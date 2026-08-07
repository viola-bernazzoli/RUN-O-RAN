# DU-side runOran changes

The current workspace includes a small DU-side adaptation set for the runOran E2 service model.

Relevant files in `srsRAN_Project/`:

- `apps/du_low/CMakeLists.txt`
- `lib/e2/CMakeLists.txt`
- `lib/e2/e2sm/e2sm_runOran/CMakeLists.txt`
- `lib/e2/e2sm/e2sm_runOran/e2sm_runOran_impl.cpp`
- `lib/e2/e2sm/e2sm_runOran/e2sm_runOran_impl.h`
- `lib/e2/e2sm/e2sm_runOran/ta_indication_store.cpp`
- `include/srsran/e2/e2sm/e2sm_runOran/ta_indication_store.h`
- `lib/e2/common/e2_du_factory.cpp`
- `lib/e2/common/e2ap_asn1_helpers.h`
- `lib/phy/upper/signal_processors/CMakeLists.txt`
- `Orun_gnb.sh`
- `run_gnb.sh`

The repo intentionally does not vendor the full srsRAN tree. Apply the same changes on top of the upstream checkout that you build from, or use `du/patches/runOran_srsRAN.patch` as the formal diff bundle.
