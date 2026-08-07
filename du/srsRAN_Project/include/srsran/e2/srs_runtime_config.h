/*
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 */

#pragma once

#include <cstdint>
#include <mutex>

namespace srsran {
namespace e2 {
namespace srs {

struct srs_runtime_defaults {
  bool     configured_by_xapp      = false;
  uint64_t last_request_id          = 0;
  uint16_t num_rx_antennas          = 2;
  double   sampling_frequency_hz    = 30.72e6;
  double   subcarrier_spacing_khz   = 30.0;
  double   carrier_frequency_hz     = 3.5e9;
  uint8_t  tx_comb                  = 2;
  uint8_t  num_occupied_ofdm_symbols = 1;
  bool     frequency_hopping_enabled = false;
  uint8_t  b_srs                    = 0;
  bool     use_max_available_c_srs  = true;
  uint8_t  c_srs                    = 0;
  uint16_t ul_bandwidth_prb         = 0;
  uint16_t periodicity_slots        = 0;
  uint16_t periodicity_offset       = 0;
  bool     enforce_periodicity_offset = false;
};

inline srs_runtime_defaults& get_srs_runtime_defaults()
{
  static srs_runtime_defaults cfg;
  return cfg;
}

inline std::mutex& get_srs_runtime_defaults_mutex()
{
  static std::mutex mtx;
  return mtx;
}

} // namespace srs
} // namespace e2
} // namespace srsran
