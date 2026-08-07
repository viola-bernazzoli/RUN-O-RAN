
#include "e2sm_runOran_impl.h"
#include "srsran/e2/e2sm/e2sm_runOran/ta_indication_store.h"
#include "srsran/support/srsran_assert.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <algorithm>
#include <unordered_set>
#include <vector>

using namespace srsran;

namespace {

uint64_t get_timestamp_us()
{
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count());
}

void append_varint(std::vector<uint8_t>& bytes, uint64_t value)
{
  while (value >= 0x80U) {
    bytes.push_back(static_cast<uint8_t>(value) | 0x80U);
    value >>= 7U;
  }
  bytes.push_back(static_cast<uint8_t>(value));
}

void append_key(std::vector<uint8_t>& bytes, uint32_t field_number, uint32_t wire_type)
{
  append_varint(bytes, (static_cast<uint64_t>(field_number) << 3U) | wire_type);
}

std::vector<uint8_t> serialize_ta_indication(const std::vector<ta_indication_record>& records)
{
  std::vector<uint8_t> bytes;
  for (const auto& record : records) {
    std::vector<uint8_t> entry;

    append_key(entry, 1, 0);
    append_varint(entry, record.rnti);

    append_key(entry, 2, 0);
    append_varint(entry, record.timestamp_us);

    append_key(entry, 3, 0);
    append_varint(entry, record.timing_advance);

    append_key(entry, 4, 0);
    append_varint(entry, record.source);

    append_key(entry, 5, 0);
    append_varint(entry, record.attached ? 1U : 0U);

    append_key(entry, 6, 0);
    append_varint(entry, static_cast<uint64_t>(record.ue_index));

    append_key(bytes, 1, 2);
    append_varint(bytes, entry.size());
    bytes.insert(bytes.end(), entry.begin(), entry.end());
  }

  return bytes;
}

class ta_report_service final : public e2sm_report_service
{
public:
  bool collect_measurements() override
  {
    records = drain_ta_indications();
    return not records.empty();
  }

  bool is_ind_msg_ready() override { return not records.empty(); }

  srsran::byte_buffer get_indication_message() override
  {
    srsran::byte_buffer buffer;
    const auto          bytes = serialize_ta_indication(records);
    if (!buffer.append(bytes)) {
      return {};
    }
    records.clear();
    return buffer;
  }

  srsran::byte_buffer get_indication_header() override
  {
    srsran::byte_buffer buffer;
    std::vector<uint8_t> header;
    append_varint(header, get_timestamp_us());
    if (!buffer.append(header)) {
      return {};
    }
    return buffer;
  }

private:
  std::vector<ta_indication_record> records;
};

class srs_report_service final : public e2sm_report_service
{
public:
  explicit srs_report_service(srs_data_collector& srs_collector_) : srs_collector(srs_collector_) {}

  bool collect_measurements() override
  {
    std::vector<ue_srs_data> records = srs_collector.get_all_complete_ue_data();
    for (const auto& ue : records) {
      std::vector<srs_antenna_data> sorted_antennas = ue.antenna_data;
      std::sort(sorted_antennas.begin(),
                sorted_antennas.end(),
                [](const srs_antenna_data& lhs, const srs_antenna_data& rhs) {
                  return lhs.antenna_id < rhs.antenna_id;
                });

      // Enqueue generated/reference streams first in deterministic antenna order.
      for (const auto& antenna : sorted_antennas) {
        const uint64_t key = (static_cast<uint64_t>(ue.rnti) << 32U) | static_cast<uint64_t>(antenna.antenna_id);
        if (generated_sent_keys.insert(key).second) {
          pending_items.push_back(
              srs_stream_item{ue.rnti, ue.timestamp, antenna.antenna_id, true, antenna.generated_srs});
        }
      }

      // Enqueue received streams for every snapshot, also in deterministic antenna order.
      for (const auto& antenna : sorted_antennas) {
        pending_items.push_back(
            srs_stream_item{ue.rnti, ue.timestamp, antenna.antenna_id, false, antenna.received_srs});
      }
    }
    return not pending_items.empty();
  }

  bool is_ind_msg_ready() override { return not pending_items.empty(); }

  srsran::byte_buffer get_indication_message() override
  {
    constexpr size_t max_indication_payload_bytes = 8000;
    constexpr size_t max_streams_per_pdu          = 2;

    if (pending_items.empty()) {
      return {};
    }

    const uint32_t batch_rnti      = pending_items.front().rnti;
    const uint64_t batch_timestamp = pending_items.front().timestamp;
    const bool     batch_generated = pending_items.front().generated;

    std::vector<srs_stream_item> batch;
    std::vector<uint8_t>         serialized;

    while (!pending_items.empty() && pending_items.front().rnti == batch_rnti &&
           pending_items.front().timestamp == batch_timestamp &&
           pending_items.front().generated == batch_generated && batch.size() < max_streams_per_pdu) {
      batch.push_back(pending_items.front());
      std::vector<uint8_t> candidate = srs_e2_serializer::serialize_srs_indication(batch);

      if (candidate.size() > max_indication_payload_bytes && batch.size() > 1) {
        batch.pop_back();
        break;
      }

      serialized = std::move(candidate);
      pending_items.pop_front();

      if (serialized.size() >= max_indication_payload_bytes) {
        break;
      }
    }

    if (serialized.empty() && !batch.empty()) {
      serialized = srs_e2_serializer::serialize_srs_indication(batch);
    }

    srsran::byte_buffer buffer;
    if (!buffer.append(serialized)) {
      return {};
    }
    return buffer;
  }

  srsran::byte_buffer get_indication_header() override
  {
    srsran::byte_buffer buffer;
    std::vector<uint8_t> header;
    append_varint(header, get_timestamp_us());
    if (!buffer.append(header)) {
      return {};
    }
    return buffer;
  }

private:
  srs_data_collector&      srs_collector;
  std::deque<srs_stream_item> pending_items;
  std::unordered_set<uint64_t> generated_sent_keys;
};

class null_report_service final : public e2sm_report_service
{
public:
  bool collect_measurements() override { return false; }
  bool is_ind_msg_ready() override { return false; }
  srsran::byte_buffer get_indication_message() override { return {}; }
  srsran::byte_buffer get_indication_header() override { return {}; }
};

} // namespace

const std::string srsran::e2sm_runOran_packer::short_name       = "ORAN runOran E2 Service Model";
const std::string srsran::e2sm_runOran_packer::oid              = "1.3.6.1.4.1.53148.1.7.2.1";
const std::string srsran::e2sm_runOran_packer::func_description = "runOran and Timing Advance";
const uint32_t    srsran::e2sm_runOran_packer::ran_func_id      = 5;
const uint32_t    srsran::e2sm_runOran_packer::revision         = 0;

// runOran Implementation
e2sm_runOran_impl::e2sm_runOran_impl(srslog::basic_logger& logger_,
                                     e2sm_handler& e2sm_packer_,
                                     srs_data_collector& srs_collector_) :
  logger(logger_),
  e2sm_packer(e2sm_packer_),
  srs_collector(srs_collector_)
{
  logger.info("runOran service model initialized");
}

e2sm_handler& e2sm_runOran_impl::get_e2sm_packer()
{
  return e2sm_packer;
}

bool e2sm_runOran_impl::action_supported(const asn1::e2ap::ric_action_to_be_setup_item_s& ric_action)
{
  // Support SRS indication subscriptions
  return true;  // Accept all SRS-related actions for now
}

std::unique_ptr<e2sm_report_service>
e2sm_runOran_impl::get_e2sm_report_service(const srsran::byte_buffer& action_definition)
{
  // Custom action definition:
  //   0x01 => Timing-advance stream
  //   0x02 => SRS stream
  if (!action_definition.empty() && *action_definition.begin() == 0x01U) {
    logger.info("runOran report service selected: timing advance stream");
    return std::make_unique<ta_report_service>();
  }
  if (!action_definition.empty() && *action_definition.begin() == 0x02U) {
    logger.info("runOran report service selected: SRS stream");
    return std::make_unique<srs_report_service>(srs_collector);
  }
  logger.warning("Unsupported runOran action definition; no report service will be created");
  return std::make_unique<null_report_service>();
}

e2sm_control_service*
e2sm_runOran_impl::get_e2sm_control_service(const e2sm_ric_control_request& request)
{
  // No control services for SRS model (read-only)
  return nullptr;
}

bool e2sm_runOran_impl::add_e2sm_control_service(std::unique_ptr<e2sm_control_service> control_service)
{
  // No control services needed
  return false;
}

byte_buffer e2sm_runOran_impl::handle_gnb_config_request(const byte_buffer& request_data)
{
  logger.info("Handling gNB config request");
  
  // Deserialize request - convert byte_buffer to vector
  std::vector<uint8_t> request_vec;
  request_vec.reserve(request_data.length());
  for (const uint8_t& byte : request_data) {
    request_vec.push_back(byte);
  }
  
  auto [request_id, xapp_id] = srs_e2_serializer::deserialize_gnb_config_request(request_vec);
  
  logger.info("Config request from xApp: {}, request_id: {}", xapp_id, request_id);
  
  // Get current gNB configuration
  const auto& config = srs_collector.get_config();
  
  // Serialize response
  std::vector<uint8_t> response_vec = srs_e2_serializer::serialize_gnb_config(
    config, 
    request_id, 
    true  // success
  );
  
  logger.info("Sending gNB config response: {} antennas, {} PRB", 
              config.num_rx_antennas, config.srs_bandwidth_prb);
  
  // Convert to byte_buffer
  byte_buffer response;
  if (!response.append(response_vec)) {
    logger.error("Failed to append response to byte_buffer");
    return byte_buffer();
  }
  return response;
}


// runOran Packer Implementation
e2sm_runOran_packer::e2sm_runOran_packer(srslog::basic_logger& logger_) : logger(logger_)
{
  logger.info("runOran packer initialized");
}

e2sm_action_definition e2sm_runOran_packer::handle_packed_e2sm_action_definition(const srsran::byte_buffer& buf)
{
  (void)buf;
  // Action definition is currently consumed directly by the report service.
  return {};
}

e2sm_event_trigger_definition
e2sm_runOran_packer::handle_packed_event_trigger_definition(const srsran::byte_buffer& buf)
{
  e2sm_event_trigger_definition trigger = {};
  trigger.ric_service_type              = e2sm_event_trigger_definition::REPORT;
  trigger.report_period                 = 20;

  // Custom event trigger format: first byte is report period in ms.
  if (!buf.empty()) {
    const uint64_t period_ms = static_cast<uint64_t>(*buf.begin());
    if (period_ms > 0) {
      trigger.report_period = period_ms;
    }
  }
  return trigger;
}

e2sm_ric_control_request
e2sm_runOran_packer::handle_packed_ric_control_request(const asn1::e2ap::ric_ctrl_request_s& req)
{
  e2sm_ric_control_request result;
  // Minimal implementation - control not used for SRS
  return result;
}

e2_ric_control_response
e2sm_runOran_packer::pack_ric_control_response(const e2sm_ric_control_response& e2sm_response)
{
  e2_ric_control_response result;
  // Minimal implementation
  return result;
}

asn1::unbounded_octstring<true> e2sm_runOran_packer::pack_ran_function_description()
{
  asn1::unbounded_octstring<true> ran_function_description;
  std::string description = short_name + ":" + func_description;
  ran_function_description.from_string(description);
  return ran_function_description;
}
