/*
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 */

#include "e2_srs_message_handler.h"
#include "srsran/support/srsran_assert.h"

using namespace srsran;

e2_srs_message_handler::e2_srs_message_handler(srslog::basic_logger& logger_,
                                               srs_data_collector& srs_collector_) :
  logger(logger_),
  srs_collector(srs_collector_)
{
  logger.info("E2 SRS message handler initialized");
}

byte_buffer e2_srs_message_handler::handle_config_request(const byte_buffer& request_data)
{
  logger.info("=================================================================");
  logger.info("E2: Handling gNB Config Request");
  logger.info("=================================================================");
  
  // Step 1: Deserialize config request - convert byte_buffer to vector
  std::vector<uint8_t> request_vec;
  request_vec.reserve(request_data.length());
  for (const uint8_t& byte : request_data) {
    request_vec.push_back(byte);
  }
  
  auto [request_id, xapp_id] = srs_e2_serializer::deserialize_gnb_config_request(request_vec);
  
  logger.info("  Request ID: {}", request_id);
  logger.info("  xApp ID: {}", xapp_id);
  
  // Step 2: Get current gNB configuration from SRS collector
  const auto& config = srs_collector.get_config();
  
  logger.info("  Current gNB Config:");
  logger.info("    - RX Antennas: {}", config.num_rx_antennas);
  logger.info("    - SRS Bandwidth: {} PRB", config.srs_bandwidth_prb);
  logger.info("    - SRS Symbols: {}", config.num_srs_symbols);
  logger.info("    - Sampling Freq: {:.2f} MHz", config.sampling_frequency_hz / 1e6);
  logger.info("    - Subcarrier Spacing: {} kHz", config.subcarrier_spacing_khz);
  
  // Step 3: Serialize config response
  std::vector<uint8_t> response_vec = srs_e2_serializer::serialize_gnb_config(
    config,
    request_id,
    true  // success
  );
  
  logger.info("  Response size: {} bytes", response_vec.size());
  logger.info("=================================================================");
  
  // Step 4: Convert to byte_buffer and return
  byte_buffer response;
  if (!response.append(response_vec)) {
    logger.error("Failed to append response data to byte_buffer");
    return byte_buffer();
  }
  return response;
}

byte_buffer e2_srs_message_handler::create_srs_indication()
{
  // Get all complete UE SRS data
  std::vector<ue_srs_data> complete_data = srs_collector.get_all_complete_ue_data();
  
  if (complete_data.empty()) {
    return byte_buffer();
  }
  
  logger.info("Creating SRS indication for {} UEs", complete_data.size());

  std::vector<srs_stream_item> stream_items;
  for (const auto& ue : complete_data) {
    for (const auto& antenna : ue.antenna_data) {
      stream_items.push_back(
          srs_stream_item{ue.rnti, ue.timestamp, antenna.antenna_id, true, antenna.generated_srs});
      stream_items.push_back(
          srs_stream_item{ue.rnti, ue.timestamp, antenna.antenna_id, false, antenna.received_srs});
    }
  }

  // Serialize SRS indication message.
  std::vector<uint8_t> indication_vec = srs_e2_serializer::serialize_srs_indication(stream_items);
  
  logger.info("SRS indication size: {} bytes", indication_vec.size());
  
  // Convert to byte_buffer
  byte_buffer indication;
  if (!indication.append(indication_vec)) {
    logger.error("Failed to append indication data to byte_buffer");
    return byte_buffer();
  }
  return indication;
}
