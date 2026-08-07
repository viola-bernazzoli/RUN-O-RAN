/*
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 */

#include "srsran/e2/e2_srs_handler.h"
#include "srs_messages.pb.h"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <mutex>

using namespace srsran;

namespace {

std::mutex              srs_e2_collector_mutex;
srs_data_collector*      srs_e2_collector = nullptr;
constexpr float          iq_quant_scale                 = 8192.0f;

uint32_t pack_iq_sample(cf_t sample)
{
  const int32_t real_q = std::clamp(static_cast<int32_t>(std::lrint(sample.real() * iq_quant_scale)), -32768, 32767);
  const int32_t imag_q = std::clamp(static_cast<int32_t>(std::lrint(sample.imag() * iq_quant_scale)), -32768, 32767);

  return (static_cast<uint32_t>(static_cast<uint16_t>(real_q)) << 16U) |
         static_cast<uint32_t>(static_cast<uint16_t>(imag_q));
}

cf_t unpack_iq_sample(uint32_t packed)
{
  const int16_t real_q = static_cast<int16_t>((packed >> 16U) & 0xffffU);
  const int16_t imag_q = static_cast<int16_t>(packed & 0xffffU);
  return cf_t(static_cast<float>(real_q) / iq_quant_scale, static_cast<float>(imag_q) / iq_quant_scale);
}

} // namespace

uint64_t srs_data_collector::get_current_timestamp_us() const
{
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

srs_data_collector::srs_data_collector(gnb_srs_config config) : config_(config) {}

void srs_data_collector::add_srs_data(uint32_t rnti,
                                       uint32_t antenna_id,
                                       span<const cf_t> generated_srs,
                                       span<const cf_t> received_srs,
                                       uint64_t timestamp)
{
  std::lock_guard<std::mutex> lock(pending_data_mutex_);
  auto& ue_data = pending_data_[rnti];

  // Keep a single SRS occasion snapshot per UE to avoid unbounded growth between report ticks.
  if (ue_data.antenna_data.size() >= config_.num_rx_antennas) {
    ue_data.antenna_data.clear();
  }

  ue_data.rnti      = rnti;
  ue_data.timestamp = timestamp;

  auto ant_it = std::find_if(ue_data.antenna_data.begin(),
                             ue_data.antenna_data.end(),
                             [antenna_id](const srs_antenna_data& ant) { return ant.antenna_id == antenna_id; });
  if (ant_it == ue_data.antenna_data.end()) {
    ue_data.antenna_data.emplace_back();
    ant_it = std::prev(ue_data.antenna_data.end());
  }

  ant_it->antenna_id = antenna_id;

  ant_it->generated_srs.assign(generated_srs.begin(), generated_srs.end());
  ant_it->received_srs.assign(received_srs.begin(), received_srs.end());
}

bool srs_data_collector::is_complete(uint32_t rnti) const
{
  std::lock_guard<std::mutex> lock(pending_data_mutex_);
  auto it = pending_data_.find(rnti);
  if (it == pending_data_.end()) {
    return false;
  }
  return it->second.antenna_data.size() >= config_.num_rx_antennas;
}

ue_srs_data srs_data_collector::get_and_clear_ue_data(uint32_t rnti)
{
  std::lock_guard<std::mutex> lock(pending_data_mutex_);
  auto it = pending_data_.find(rnti);
  if (it == pending_data_.end()) {
    return ue_srs_data{};
  }
  
  ue_srs_data data = std::move(it->second);
  pending_data_.erase(it);
  return data;
}

std::vector<ue_srs_data> srs_data_collector::get_all_complete_ue_data()
{
  std::lock_guard<std::mutex> lock(pending_data_mutex_);
  std::vector<ue_srs_data> complete_data;
  
  auto it = pending_data_.begin();
  while (it != pending_data_.end()) {
    if (it->second.antenna_data.size() >= config_.num_rx_antennas) {
      complete_data.push_back(std::move(it->second));
      it = pending_data_.erase(it);
    } else {
      ++it;
    }
  }
  
  return complete_data;
}

void srs_data_collector::cleanup_old_data(uint64_t timeout_us)
{
  std::lock_guard<std::mutex> lock(pending_data_mutex_);
  uint64_t current_time = get_current_timestamp_us();
  
  auto it = pending_data_.begin();
  while (it != pending_data_.end()) {
    if (current_time - it->second.timestamp > timeout_us) {
      it = pending_data_.erase(it);
    } else {
      ++it;
    }
  }
}

// Protocol Buffer serialization implementation
std::vector<uint8_t> srs_e2_serializer::serialize_srs_indication(const std::vector<srs_stream_item>& items)
{
  srsran::e2::srs::SrsIndicationMessage msg;

  std::map<std::pair<uint32_t, uint64_t>, std::vector<const srs_stream_item*>> grouped_items;
  for (const auto& item : items) {
    grouped_items[{item.rnti, item.timestamp}].push_back(&item);
  }

  for (const auto& [ue_key, ue_items] : grouped_items) {
    auto* ue_msg = msg.add_ue_srs_data();
    ue_msg->set_rnti(ue_key.first);
    ue_msg->set_timestamp(ue_key.second);

    for (const auto* item : ue_items) {
      auto* ant_msg = ue_msg->add_antenna_data();
      ant_msg->set_antenna_id(item->antenna_id);
      ant_msg->set_generated(item->generated);

      auto* srs_msg = ant_msg->mutable_srs();
      for (const auto& sample : item->srs) {
        srs_msg->add_iq_samples(pack_iq_sample(sample));
      }
    }
  }

  std::vector<uint8_t> serialized(msg.ByteSizeLong());
  msg.SerializeToArray(serialized.data(), serialized.size());
  return serialized;
}

std::vector<uint8_t> srs_e2_serializer::serialize_gnb_config(const gnb_srs_config& config, uint64_t request_id, bool success)
{
  srsran::e2::srs::GnbConfigMessage msg;
  
  msg.set_request_id(request_id);
  msg.set_num_rx_antennas(config.num_rx_antennas);
  msg.set_srs_bandwidth_prb(config.srs_bandwidth_prb);
  msg.set_num_srs_symbols(config.num_srs_symbols);
  msg.set_sampling_frequency_hz(config.sampling_frequency_hz);
  msg.set_subcarrier_spacing_khz(config.subcarrier_spacing_khz);
  msg.set_carrier_frequency_hz(config.carrier_frequency_hz);
  msg.set_success(success);
  
  std::vector<uint8_t> serialized(msg.ByteSizeLong());
  msg.SerializeToArray(serialized.data(), serialized.size());
  return serialized;
}

std::pair<uint64_t, std::string> srs_e2_serializer::deserialize_gnb_config_request(span<const uint8_t> data)
{
  srsran::e2::srs::GnbConfigRequest msg;
  
  if (!msg.ParseFromArray(data.data(), data.size())) {
    return {0, ""};
  }
  
  return {msg.request_id(), msg.xapp_id()};
}

std::vector<srs_stream_item> srs_e2_serializer::deserialize_srs_indication(span<const uint8_t> data)
{
  srsran::e2::srs::SrsIndicationMessage msg;
  msg.ParseFromArray(data.data(), data.size());

  std::vector<srs_stream_item> result;

  for (const auto& ue_msg : msg.ue_srs_data()) {
    for (const auto& ant_msg : ue_msg.antenna_data()) {
      srs_stream_item item;
      item.rnti      = ue_msg.rnti();
      item.timestamp = ue_msg.timestamp();
      item.antenna_id = ant_msg.antenna_id();
      item.generated  = ant_msg.generated();

      for (uint32_t packed : ant_msg.srs().iq_samples()) {
        item.srs.push_back(unpack_iq_sample(packed));
      }

      result.push_back(std::move(item));
    }
  }

  return result;
}

gnb_srs_config srs_e2_serializer::deserialize_gnb_config(span<const uint8_t> data)
{
  srsran::e2::srs::GnbConfigMessage msg;
  msg.ParseFromArray(data.data(), data.size());
  
  gnb_srs_config config;
  config.num_rx_antennas = msg.num_rx_antennas();
  config.srs_bandwidth_prb = msg.srs_bandwidth_prb();
  config.num_srs_symbols = msg.num_srs_symbols();
  config.sampling_frequency_hz = msg.sampling_frequency_hz();
  config.subcarrier_spacing_khz = msg.subcarrier_spacing_khz();
  config.carrier_frequency_hz = msg.carrier_frequency_hz() > 0.0 ? msg.carrier_frequency_hz() : 3.5e9;
  
  return config;
}

void srsran::register_srs_e2_data_collector(srs_data_collector* collector)
{
  std::lock_guard<std::mutex> lock(srs_e2_collector_mutex);
  srs_e2_collector = collector;
}

srs_data_collector* srsran::get_srs_e2_data_collector()
{
  std::lock_guard<std::mutex> lock(srs_e2_collector_mutex);
  return srs_e2_collector;
}
