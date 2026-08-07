# RUN-O-RAN E2 Service Model

This repository is part of the RUN-O-RAN publication. If you use this code, its service model, or results obtained with it, please cite the publication below.

> V. Bernazzoli, P. Morri, E. Moro, M. Brambilla, I. Filippini, and M. Nicoli, “Robust Uplink Ranging in 5G Networks: An Integrated O-RAN Approach,” *2025 IEEE 22nd International Conference on Mobile Ad-Hoc and Smart Systems (MASS)*, 2025. [Publication record](https://hdl.handle.net/11311/1307598).

Copyright © 2026 RUN-O-RAN contributors. See the licences of the bundled third-party projects, including srsRAN and ORAN-SC, for their respective terms.

## What this repository provides

RUN-O-RAN defines an E2 service model (E2SM) for exchanging sounding reference signal (SRS) and timing-advance (TA) data between a gNB and a RIC xApp. The model is implemented with [Protocol Buffers](https://protobuf.dev/), a language-neutral and platform-neutral format developed by Google LLC for efficiently serialising structured data. The shared schema is [`proto/srs_messages.proto`](proto/srs_messages.proto); it produces both the C++ DU bindings and the Python xApp bindings from one wire contract.

The model defines these message payloads:

- **`SRS_conf`** — reproduces the 3GPP `SRS-Resource` configuration needed by the serving gNB. The xApp sends it in an E2AP RIC Control message to configure SRS transmission.
- **`SRS_param`** — contains the subset of `SRS_conf` that neighbouring gNBs need to identify the time-frequency resources used by the target UE's uplink SRS.
- **`SRS`** — carries received baseband SRS samples, the hashed C-RNTI (`UE_ID`), a timestamp, and measured SNR. Each complex sample is represented in a floating-point array that packs the in-phase component in the upper 16 bits and the quadrature component in the lower 16 bits. The gNB serialises this payload in an E2AP RIC Indication when the SRS reception trigger is active.
- **`TA_k`** — carries a timing-advance value, its associated `UE_ID`, and a timestamp. The gNB emits it in an E2AP RIC Indication when a timing-advance update or UE-attachment trigger occurs.

## Workspace layout

The three main folders in the parent workspace serve different roles:

```text
RUN-O-RAN/
├── e2-runOran-sm/       # Shared protobuf schema, minimal xApp, DU overlay, and installers.
├── srsRAN_Project/      # gNB/DU source tree used to build and run the radio side.
└── oran-sc-ric/         # Suggested and tested ORAN-SC RIC deployment, including Python xApps.
```

`e2-runOran-sm` is RIC-agnostic at the protocol level: the xApp and `e2sm_runOran` can be adapted to another RIC implementation. The included deployment helper targets ORAN-SC RIC because that is the implementation tested with this project.

## Installation and first run

These steps are suitable for a new Linux user. Use an Ubuntu/Debian-like system with internet access and `sudo` permission. Before building the gNB, install and configure an ORAN-SC RIC checkout: it is the tested RIC implementation and must be reachable by the gNB. An existing srsRAN checkout is optional—the gNB builder installs missing packages and clones srsRAN automatically.

Keep the three sibling folders shown in the [workspace layout](#workspace-layout): `e2-runOran-sm`, `oran-sc-ric`, and (optionally) `srsRAN_Project`. The project helpers populate the missing DU/build files; the RIC itself must already be installed using its normal ORAN-SC procedure. A compatible radio and gNB configuration are required to receive real indications.

1. Clone RUN-O-RAN and enter the cloned repository:

   ```bash
   git clone https://github.com/viola-bernazzoli/RUN-O-RAN.git
   cd RUN-O-RAN/e2-runOran-sm
   ```

2. Build the DU/gNB side. The builder installs missing build dependencies, clones the compatible `release_25_10` srsRAN source if it is not already available, generates the C++ Protocol Buffers code, copies the DU overlay, and builds `gnb`:

   ```bash
   ./setup_your_gnb.sh
   ```

   By default the prepared checkout is the sibling folder `../srsRAN_Project`, matching the workspace layout above. The upstream repository's default branch is now an archive notice, so the builder explicitly uses `release_25_10`. To use an existing checkout elsewhere, set `SRSRAN_DIR` to its full path; to select another compatible source ref, set `SRSRAN_BRANCH`.

3. Install the xApp files into the installed ORAN-SC RIC checkout. From this directory, the sibling checkout is detected automatically; alternatively pass its path explicitly:

   ```bash
   ./runOran-in-oran-sc-ric.sh ../oran-sc-ric
   ```

   The helper copies the minimal Python xApp and generated Python protobuf binding into `oran-sc-ric/xApps/python/`. It also copies the C++ `e2sm_runOran` source bundle to `oran-sc-ric/e2-agents/srsRAN/e2sm_runOran/` for reference. The gNB is still built by `setup_your_gnb.sh`.

4. Start the RIC using its normal ORAN-SC deployment procedure, then start the minimal xApp from the ORAN-SC Python xApp directory:

   ```bash
   cd ../oran-sc-ric/xApps/python
   python3 simple_runoran_xapp.py --e2-node-id <e2-node-id>
   ```

   Replace `<e2-node-id>` with the identifier of the gNB that connects to the RIC.

## Generate Protocol Buffers manually

The installer runs C++ generation automatically. Run these commands yourself only when editing the schema:

```bash
./compile_proto.sh python  # writes python/lib/srs_messages_pb2.py
./compile_proto.sh c       # writes DU C++ bindings under e2sm_runOran/
```

## DU patch and overlay

`du/srsRAN_Project/` is the complete DU overlay used by `setup_your_gnb.sh`, including the `e2sm_srs_enabled` configuration wiring and SRS collector support. `du/patches/runOran_srsRAN.patch` is the formal rename/enablement patch for the pre-existing `e2sm_srs` DU baseline. Apply it only to that matching baseline; for a fresh srsRAN checkout, use `setup_your_gnb.sh`.
