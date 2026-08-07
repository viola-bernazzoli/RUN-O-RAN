set(SOURCES
        procedures/e2_setup_procedure.cpp
        procedures/e2_subscription_setup_procedure.cpp
        procedures/e2_subscription_delete_procedure.cpp
        procedures/e2_indication_procedure.cpp
        procedures/e2_ric_control_procedure.cpp
        procedures/e2_connection_update_procedure.cpp
        common/e2_connection_handler.cpp
        common/e2_cu_cp_factory.cpp
        common/e2_cu_up_factory.cpp
        common/e2_du_factory.cpp
        common/e2_impl.cpp
        common/e2_entity.cpp
        common/e2_subscription_manager_impl.cpp
        common/e2_du_metrics_connector.cpp
        common/e2_cu_metrics_connector.cpp
        e2sm/e2sm_ccc/e2sm_ccc_asn1_packer.cpp
        e2sm/e2sm_ccc/e2sm_ccc_impl.cpp
        e2sm/e2sm_ccc/e2sm_ccc_control_service_impl.cpp
        e2sm/e2sm_ccc/e2sm_ccc_control_action_du_executor.cpp
        e2sm/e2sm_kpm/e2sm_kpm_impl.cpp
        e2sm/e2sm_kpm/e2sm_kpm_asn1_packer.cpp
        e2sm/e2sm_rc/e2sm_rc_asn1_packer.cpp
        e2sm/e2sm_rc/e2sm_rc_impl.cpp
        e2sm/e2sm_rc/e2sm_rc_control_service_impl.cpp
        e2sm/e2sm_rc/e2sm_rc_control_action_du_executor.cpp
        e2sm/e2sm_rc/e2sm_rc_control_action_cu_executor.cpp
        e2sm/e2sm_runOran/ta_indication_store.cpp
        e2sm/e2sm_manager.cpp
        e2sm/e2sm_kpm/e2sm_kpm_utils.cpp
        e2sm/e2sm_kpm/e2sm_kpm_report_service_impl.cpp
        e2sm/e2sm_kpm/e2sm_kpm_du_meas_provider_impl.cpp
        e2sm/e2sm_kpm/e2sm_kpm_cu_meas_provider_impl.cpp
        e2sm/e2sm_runOran/e2sm_runOran_impl.cpp
        e2_srs_handler.cpp
        e2_srs_message_handler.cpp
        e2sm/e2sm_runOran/srs_messages.pb.cc
        gateways/e2_network_client_factory.cpp
)

add_library(srsran_e2 STATIC ${SOURCES})
find_package(Protobuf REQUIRED)
target_link_libraries(srsran_e2 e2ap_asn1 srsran_support srsran_ran protobuf::libprotobuf)

# Protobuf 3.12 (the version packaged by Ubuntu 22.04) emits constructors with
# an extra trailing semicolon. srsRAN builds with -Werror, so keep this generated
# code warning in the E2 target while retaining warnings elsewhere.
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(srsran_e2 PRIVATE -Wno-extra-semi)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(srsran_e2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/e2sm/e2sm_runOran)
