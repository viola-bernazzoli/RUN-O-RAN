/*
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 */

#pragma once

#include "srsran/adt/byte_buffer.h"
#include "srsran/e2/e2_srs_handler.h"
#include "srsran/srslog/srslog.h"

namespace srsran {

/// Handler for custom SRS-related E2 messages
class e2_srs_message_handler
{
public:
  e2_srs_message_handler(srslog::basic_logger& logger_, srs_data_collector& srs_collector_);
  
  /// Handle gNB configuration request
  /// Returns serialized GnbConfigMessage response
  byte_buffer handle_config_request(const byte_buffer& request_data);
  
  /// Send SRS indication to xApp
  /// Returns serialized SrsIndicationMessage
  byte_buffer create_srs_indication();

private:
  srslog::basic_logger& logger;
  srs_data_collector&   srs_collector;
};

} // namespace srsran
