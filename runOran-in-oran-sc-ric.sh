#!/usr/bin/env bash

# Install the RUN-O-RAN xApp assets into an existing ORAN-SC RIC checkout.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ric_dir="${1:-${ORAN_SC_RIC_DIR:-$repo_root/../oran-sc-ric}}"

if [[ ! -d "$ric_dir" ]]; then
  cat <<EOF
ORAN-SC RIC was not found at:
  $ric_dir

Install or clone an ORAN-SC RIC checkout first, then rerun this command with its path:
  git clone <your-ORAN-SC-RIC-repository-URL> oran-sc-ric
  $repo_root/runOran-in-oran-sc-ric.sh /path/to/oran-sc-ric

If the checkout is already available elsewhere, either pass that path as the first argument or set ORAN_SC_RIC_DIR.
EOF
  exit 1
fi

if [[ ! -d "$ric_dir/xApps/python" || ! -d "$ric_dir/e2-agents/srsRAN" ]]; then
  echo "Error: '$ric_dir' is not the expected ORAN-SC RIC layout (xApps/python and e2-agents/srsRAN are required)."
  exit 1
fi

echo "Generating Protocol Buffers bindings"
"$repo_root/compile_proto.sh" python
"$repo_root/compile_proto.sh" c

xapp_dir="$ric_dir/xApps/python"
xapp_lib_dir="$xapp_dir/lib"
agent_model_dir="$ric_dir/e2-agents/srsRAN/e2sm_runOran"

mkdir -p "$xapp_lib_dir" "$agent_model_dir"

install -m 0755 "$repo_root/python/simple_runoran_xapp.py" "$xapp_dir/simple_runoran_xapp.py"
install -m 0644 "$repo_root/proto/srs_messages.proto" "$xapp_lib_dir/srs_messages.proto"
install -m 0644 "$repo_root/python/lib/srs_messages_pb2.py" "$xapp_lib_dir/srs_messages_pb2.py"

# Keep a self-contained copy of the C++ service-model sources beside the ORAN-SC srsRAN agent configuration.
cp -a "$repo_root/du/srsRAN_Project/lib/e2/e2sm/e2sm_runOran/." "$agent_model_dir/"
install -m 0644 "$repo_root/proto/srs_messages.proto" "$agent_model_dir/srs_messages.proto"

echo "Installed RUN-O-RAN assets into: $ric_dir"
echo "  xApp:       $xapp_dir/simple_runoran_xapp.py"
echo "  Python PB:  $xapp_lib_dir/srs_messages_pb2.py"
echo "  C++ E2SM:   $agent_model_dir"
echo "Build the DU separately with: $repo_root/setup_your_gnb.sh"
