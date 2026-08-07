
#pragma once

#include "srsran/e2/e2.h"
#include "srsran/e2/e2sm/e2sm.h"
#include "srsran/e2/e2_srs_handler.h"
#include <map>

namespace srsran {

/// runOran: Custom service model for SRS-based location estimation
/// Handles config requests and SRS indications
class e2sm_runOran_impl : public e2sm_interface
{
public:
  e2sm_runOran_impl(srslog::basic_logger& logger_,
                    e2sm_handler& e2sm_packer_,
                    srs_data_collector& srs_collector_);

  e2sm_handler& get_e2sm_packer() override;

  bool action_supported(const asn1::e2ap::ric_action_to_be_setup_item_s& ric_action) override;

  std::unique_ptr<e2sm_report_service> get_e2sm_report_service(const srsran::byte_buffer& action_definition) override;
  e2sm_control_service*                get_e2sm_control_service(const e2sm_ric_control_request& request) override;

  bool add_e2sm_control_service(std::unique_ptr<e2sm_control_service> control_service) override;
  
  /// Handle gNB configuration request from xApp
  byte_buffer handle_gnb_config_request(const byte_buffer& request_data);
  
  /// Get SRS data collector for sending indications
  srs_data_collector& get_srs_collector() { return srs_collector; }

private:
  srslog::basic_logger&   logger;
  e2sm_handler&           e2sm_packer;
  srs_data_collector&     srs_collector;
};

/// Simple runOran packer (minimal implementation)
class e2sm_runOran_packer : public e2sm_handler
{
public:
  static const std::string short_name;
  static const std::string oid;
  static const std::string func_description;
  static const uint32_t    ran_func_id;
  static const uint32_t    revision;

  e2sm_runOran_packer(srslog::basic_logger& logger_);

  e2sm_action_definition handle_packed_e2sm_action_definition(const srsran::byte_buffer& buf) override;
  e2sm_event_trigger_definition
  handle_packed_event_trigger_definition(const srsran::byte_buffer& buf) override;
  e2sm_ric_control_request handle_packed_ric_control_request(const asn1::e2ap::ric_ctrl_request_s& req) override;
  e2_ric_control_response pack_ric_control_response(const e2sm_ric_control_response& e2sm_response) override;
  asn1::unbounded_octstring<true> pack_ran_function_description() override;

private:
  srslog::basic_logger& logger;
};

} // namespace srsran
