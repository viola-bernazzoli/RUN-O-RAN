/*
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 */

#pragma once

#include "srsran/adt/span.h"
#include "srsran/adt/complex.h"
#include <cstdint>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>

namespace srsran {

/// Structure to hold SRS data for one antenna
struct srs_antenna_data {
  uint32_t antenna_id;
  std::vector<cf_t> generated_srs;  // Expected SRS sequence
  std::vector<cf_t> received_srs;   // Received SRS
};

/// Structure to hold SRS data for one UE
struct ue_srs_data {
  uint32_t rnti;
  uint64_t timestamp;  // Microseconds since epoch
  std::vector<srs_antenna_data> antenna_data;
};

/// Stream item to transmit over E2 (either generated or received SRS for one antenna).
struct srs_stream_item {
  uint32_t          rnti;
  uint64_t          timestamp;
  uint32_t          antenna_id;
  bool              generated;
  std::vector<cf_t> srs;
};

/// Structure for gNB configuration
struct gnb_srs_config {
  uint32_t num_rx_antennas;
  uint32_t srs_bandwidth_prb;
  uint32_t num_srs_symbols;
  double sampling_frequency_hz;
  double subcarrier_spacing_khz;
  double carrier_frequency_hz;
};

/// SRS data collector - collects SRS from all antennas for each UE
class srs_data_collector {
public:
  srs_data_collector(gnb_srs_config config);
  
  /// Add SRS data for a specific UE and antenna
  void add_srs_data(uint32_t rnti, 
                    uint32_t antenna_id,
                    span<const cf_t> generated_srs,
                    span<const cf_t> received_srs,
                    uint64_t timestamp);
  
  /// Check if we have received SRS from all antennas for a UE
  bool is_complete(uint32_t rnti) const;
  
  /// Get complete SRS data for a UE and remove it from collector
  ue_srs_data get_and_clear_ue_data(uint32_t rnti);
  
  /// Get all complete UE SRS data
  std::vector<ue_srs_data> get_all_complete_ue_data();
  
  /// Clear old incomplete data (timeout mechanism)
  void cleanup_old_data(uint64_t timeout_us = 100000); // Default 100ms timeout
  
  /// Get configuration
  const gnb_srs_config& get_config() const { return config_; }

private:
  gnb_srs_config config_;
  std::map<uint32_t, ue_srs_data> pending_data_;
  mutable std::mutex pending_data_mutex_;
  
  uint64_t get_current_timestamp_us() const;
};

/// SRS E2 message serializer using Protocol Buffers
class srs_e2_serializer {
public:
  /// Serialize SRS indication message to bytes.
  static std::vector<uint8_t> serialize_srs_indication(const std::vector<srs_stream_item>& items);
  
  /// Serialize gNB configuration message to bytes
  static std::vector<uint8_t> serialize_gnb_config(const gnb_srs_config& config, uint64_t request_id, bool success = true);
  
  /// Deserialize SRS indication message from bytes
  static std::vector<srs_stream_item> deserialize_srs_indication(span<const uint8_t> data);
  
  /// Deserialize gNB configuration from bytes
  static gnb_srs_config deserialize_gnb_config(span<const uint8_t> data);
  
  /// Deserialize gNB configuration request from bytes
  static std::pair<uint64_t, std::string> deserialize_gnb_config_request(span<const uint8_t> data);
};

  /// Registers the live SRS E2 collector used by the PHY callback path.
  void register_srs_e2_data_collector(srs_data_collector* collector);

  /// Returns the currently registered live SRS E2 collector, if any.
  srs_data_collector* get_srs_e2_data_collector();

} // namespace srsran
