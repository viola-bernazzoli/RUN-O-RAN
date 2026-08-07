/*
 *
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "srsran/e2/e2_du_factory.h"
#include "e2_entity.h"
#include "e2_impl.h"
#include "e2sm/e2sm_ccc/e2sm_ccc_asn1_packer.h"
#include "e2sm/e2sm_ccc/e2sm_ccc_control_action_du_executor.h"
#include "e2sm/e2sm_ccc/e2sm_ccc_control_service_impl.h"
#include "e2sm/e2sm_ccc/e2sm_ccc_impl.h"
#include "e2sm/e2sm_kpm/e2sm_kpm_asn1_packer.h"
#include "e2sm/e2sm_kpm/e2sm_kpm_du_meas_provider_impl.h"
#include "e2sm/e2sm_kpm/e2sm_kpm_impl.h"
#include "e2sm/e2sm_rc/e2sm_rc_asn1_packer.h"
#include "e2sm/e2sm_rc/e2sm_rc_control_action_du_executor.h"
#include "e2sm/e2sm_rc/e2sm_rc_control_service_impl.h"
#include "e2sm/e2sm_rc/e2sm_rc_impl.h"
#include "e2sm/e2sm_runOran/e2sm_runOran_impl.h"
#include "srsran/e2/e2_agent_dependencies.h"
#include "srsran/e2/e2_srs_handler.h"
#include "srsran/e2/srs_runtime_config.h"

using namespace srsran;

std::unique_ptr<e2_agent> srsran::create_e2_du_agent(const e2ap_configuration&      e2ap_cfg_,
                                                     e2_connection_client&          e2_client_,
                                                     e2_du_metrics_interface*       e2_metrics_,
                                                     srs_du::f1ap_ue_id_translator* f1ap_ue_id_translator_,
                                                     srs_du::du_configurator*       du_configurator_,
                                                     timer_factory                  timers_,
                                                     task_executor&                 e2_exec_,
                                                     const std::optional<e2_du_radio_config>& radio_cfg)
{
  srslog::basic_logger& logger = srslog::fetch_basic_logger("E2-DU");
  e2_agent_dependencies dependencies;
  dependencies.logger    = &logger;
  dependencies.cfg       = e2ap_cfg_;
  dependencies.e2_client = &e2_client_;
  dependencies.timers    = &timers_;
  dependencies.task_exec = &e2_exec_;

  // E2SM-KPM
  auto e2sm_kpm_meas_provider = std::make_unique<e2sm_kpm_du_meas_provider_impl>(*f1ap_ue_id_translator_);
  std::unique_ptr<e2sm_kpm_asn1_packer> e2sm_kpm_packer =
      std::make_unique<e2sm_kpm_asn1_packer>(*e2sm_kpm_meas_provider);
  std::unique_ptr<e2sm_kpm_impl> e2sm_kpm_iface =
      std::make_unique<e2sm_kpm_impl>(logger, *e2sm_kpm_packer, *e2sm_kpm_meas_provider);
  e2_metrics_->connect_e2_du_meas_provider(std::move(e2sm_kpm_meas_provider));

  dependencies.e2sm_modules.emplace_back(e2sm_module{e2sm_kpm_asn1_packer::ran_func_id,
                                                     e2sm_kpm_asn1_packer::oid,
                                                     std::move(e2sm_kpm_packer),
                                                     std::move(e2sm_kpm_iface)});

  // E2SM-RC
  auto e2sm_rc_packer = std::make_unique<e2sm_rc_asn1_packer>();
  auto e2sm_rc_iface  = std::make_unique<e2sm_rc_impl>(logger, *e2sm_rc_packer);
  // Add Supported Control Styles.
  int                                   control_service_style_id = 2;
  std::unique_ptr<e2sm_control_service> rc_control_service_style =
      std::make_unique<e2sm_rc_control_service>(control_service_style_id);
  std::unique_ptr<e2sm_control_action_executor> rc_control_action_executor =
      std::make_unique<e2sm_rc_control_action_2_6_du_executor>(*du_configurator_, *f1ap_ue_id_translator_);
  rc_control_service_style->add_e2sm_rc_control_action_executor(std::move(rc_control_action_executor));
  e2sm_rc_packer->add_e2sm_control_service(rc_control_service_style.get());
  e2sm_rc_iface->add_e2sm_control_service(std::move(rc_control_service_style));
  dependencies.e2sm_modules.emplace_back(e2sm_module{
      e2sm_rc_asn1_packer::ran_func_id, e2sm_rc_asn1_packer::oid, std::move(e2sm_rc_packer), std::move(e2sm_rc_iface)});

  // E2SM-CCC
  auto e2sm_ccc_packer = std::make_unique<e2sm_ccc_asn1_packer>();
  auto e2sm_ccc_iface  = std::make_unique<e2sm_ccc_impl>(logger, *e2sm_ccc_packer);
  // Add Supported Control Styles.
  // RIC Style Type 2: Cell Configuration and Control
  std::unique_ptr<e2sm_control_service> ccc_control_service_style =
      std::make_unique<e2sm_ccc_control_service_style_2>();
  std::unique_ptr<e2sm_control_action_executor> ccc_control_action_executor =
      std::make_unique<e2sm_ccc_control_o_rrm_policy_ratio_executor>(*du_configurator_);
  ccc_control_service_style->add_e2sm_rc_control_action_executor(std::move(ccc_control_action_executor));
  e2sm_ccc_packer->add_e2sm_control_service(ccc_control_service_style.get());
  e2sm_ccc_iface->add_e2sm_control_service(std::move(ccc_control_service_style));
  dependencies.e2sm_modules.emplace_back(e2sm_module{e2sm_ccc_asn1_packer::ran_func_id,
                                                     e2sm_ccc_asn1_packer::oid,
                                                     std::move(e2sm_ccc_packer),
                                                     std::move(e2sm_ccc_iface)});

  // runOran E2SM
  if (e2ap_cfg_.e2sm_srs_enabled) {
    const gnb_srs_config srs_cfg = {
        radio_cfg.has_value() && radio_cfg->num_rx_antennas != 0 ? radio_cfg->num_rx_antennas : 2U,
        52U,
        1U,
        (radio_cfg.has_value() && radio_cfg->sampling_frequency_hz > 0.0) ? radio_cfg->sampling_frequency_hz : 30.72e6,
        (radio_cfg.has_value() && radio_cfg->subcarrier_spacing_khz > 0.0) ? radio_cfg->subcarrier_spacing_khz : 30.0,
        (radio_cfg.has_value() && radio_cfg->carrier_frequency_hz > 0.0) ? radio_cfg->carrier_frequency_hz : 3.5e9};
    {
      std::lock_guard<std::mutex> lock(srsran::e2::srs::get_srs_runtime_defaults_mutex());
      auto& runtime_cfg = srsran::e2::srs::get_srs_runtime_defaults();
      runtime_cfg.num_rx_antennas = static_cast<uint16_t>(srs_cfg.num_rx_antennas);
      runtime_cfg.sampling_frequency_hz = srs_cfg.sampling_frequency_hz;
      runtime_cfg.subcarrier_spacing_khz = srs_cfg.subcarrier_spacing_khz;
      runtime_cfg.carrier_frequency_hz = srs_cfg.carrier_frequency_hz;
      runtime_cfg.ul_bandwidth_prb = (radio_cfg.has_value() && radio_cfg->ul_bandwidth_prb != 0)
                                         ? radio_cfg->ul_bandwidth_prb
                                         : (runtime_cfg.ul_bandwidth_prb != 0 ? runtime_cfg.ul_bandwidth_prb
                                                                              : static_cast<uint16_t>(srs_cfg.srs_bandwidth_prb));
    }
    static srs_data_collector srs_collector(srs_cfg);
    register_srs_e2_data_collector(&srs_collector);
    auto srs_packer          = std::make_unique<e2sm_runOran_packer>(logger);
    auto e2sm_runOran_iface  = std::make_unique<e2sm_runOran_impl>(logger, *srs_packer, srs_collector);
    dependencies.e2sm_modules.emplace_back(e2sm_module{e2sm_runOran_packer::ran_func_id,
                                                        e2sm_runOran_packer::oid,
                                                        std::move(srs_packer),
                                                        std::move(e2sm_runOran_iface)});
  }

  auto e2_ext = std::make_unique<e2_entity>(std::move(dependencies));
  return e2_ext;
}
