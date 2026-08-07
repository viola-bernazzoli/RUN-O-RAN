# runOran DU patch notes

The upstream DU-side runOran changes are the following:

- `apps/du_low/CMakeLists.txt`: link `srsran_e2` into `srsdu_low`.
- `lib/e2/CMakeLists.txt`: link `srsran_ran` into `srsran_e2` and point at `e2sm_runOran` sources.
- `lib/e2/e2sm/e2sm_runOran/CMakeLists.txt`: define the runOran service-model target.
- `lib/e2/e2sm/e2sm_runOran/e2sm_runOran_impl.cpp`: enable the SRS and timing advance report-service wiring.
- `lib/e2/e2sm/e2sm_runOran/e2sm_runOran_impl.h`: renamed runOran interface and packer declarations.
- `lib/e2/e2sm/e2sm_runOran/ta_indication_store.cpp`: repository-local helper for TA buffering.
- `include/srsran/e2/e2sm/e2sm_runOran/ta_indication_store.h`: public header for TA buffering.
- `lib/e2/common/e2_du_factory.cpp`: hook runOran into DU E2 module registration.
- `lib/e2/common/e2ap_asn1_helpers.h`: expose runOran in the E2 setup request.
- `lib/phy/upper/signal_processors/CMakeLists.txt`: link `srsran_e2` into the SRS estimator.
- `Orun_gnb.sh` and `run_gnb.sh`: add USRP auto-detection and the `ru_sdr` launch arguments needed by the modified DU.
- `run_piradio.sh`: removed from the source workspace and also cleaned up by `setup_your_gnb.sh` if it exists in the target checkout.

The generated patch bundle is `du/patches/runOran_srsRAN.patch`.

These files are provided here as the change manifest for anyone applying the same adaptation to a separate srsRAN checkout.
