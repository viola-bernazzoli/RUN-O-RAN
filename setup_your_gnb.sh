#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source_du_dir="$repo_root/du/srsRAN_Project"
workspace_root="$(cd "$repo_root/.." && pwd)"
# Keep the gNB source tree beside e2-runOran-sm, as documented in the workspace layout.
# SRSRAN_WORKDIR is retained as a legacy override for existing users.
if [[ -n "${SRSRAN_DIR:-}" ]]; then
  srsran_dir="$SRSRAN_DIR"
elif [[ -n "${SRSRAN_WORKDIR:-}" ]]; then
  srsran_dir="$SRSRAN_WORKDIR/srsRAN_Project"
else
  srsran_dir="$workspace_root/srsRAN_Project"
fi
checkout_root="$(dirname "$srsran_dir")"
repo_url="${SRSRAN_REPO_URL:-https://github.com/srsran/srsRAN_Project.git}"
# The repository default branch is now an archive notice. Use the last srsRAN Project release
# that retains the source-tree layout expected by this DU overlay.
srsran_ref="${SRSRAN_BRANCH:-release_25_10}"
build_dir="$srsran_dir/build"
apt_packages=(
  build-essential
  cmake
  git
  pkg-config
  protobuf-compiler
  libprotobuf-dev
  libboost-program-options-dev
  libfftw3-dev
  libmbedtls-dev
  libsctp-dev
  libyaml-cpp-dev
  libpcap-dev
)

require_cmd() {
  local cmd="$1"
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "Error: required command '$cmd' is not available on PATH."
    exit 1
  fi
}

require_cmd git
require_cmd cmake
require_cmd make
require_cmd cp
require_cmd find

if [[ $EUID -ne 0 ]] && ! command -v sudo >/dev/null 2>&1; then
  echo "Error: sudo is required to install missing packages."
  exit 1
fi

if command -v apt-get >/dev/null 2>&1; then
  missing_packages=()
  for package in "${apt_packages[@]}"; do
    if ! dpkg -s "$package" >/dev/null 2>&1; then
      missing_packages+=("$package")
    fi
  done

  if (( ${#missing_packages[@]} > 0 )); then
    echo "Installing missing dependencies: ${missing_packages[*]}"
    if [[ $EUID -eq 0 ]]; then
      apt-get update
      apt-get install -y "${missing_packages[@]}"
    else
      sudo apt-get update
      sudo apt-get install -y "${missing_packages[@]}"
    fi
  fi
else
  echo "Warning: apt-get not found; dependency bootstrap will be skipped."
fi

is_valid_srsran_checkout() {
  [[ -d "$1/.git" && -f "$1/CMakeLists.txt" ]]
}

if ! is_valid_srsran_checkout "$srsran_dir"; then
  mkdir -p "$checkout_root"
  if [[ -d "$srsran_dir" ]]; then
    echo "Warning: '$srsran_dir' is not a complete srsRAN checkout; removing it before cloning."
    rm -rf "$srsran_dir"
  fi
  echo "Cloning srsRAN_Project from $repo_url ($srsran_ref)"
  git clone --branch "$srsran_ref" --single-branch "$repo_url" "$srsran_dir"
else
  echo "Found existing srsRAN checkout at $srsran_dir"
fi

if [[ ! -f "$srsran_dir/CMakeLists.txt" ]]; then
  echo "Error: srsRAN clone completed, but '$srsran_dir/CMakeLists.txt' is missing."
  echo "Check SRSRAN_REPO_URL and rerun the script."
  exit 1
fi

echo "Generating C++ protobuf bindings for e2sm_runOran"
"$repo_root/compile_proto.sh" c

echo "Copying bundled DU-side modifications into the checkout"
while IFS= read -r -d '' source_file; do
  relative_path="${source_file#"$source_du_dir"/}"
  target_file="$srsran_dir/$relative_path"
  mkdir -p "$(dirname "$target_file")"
  cp "$source_file" "$target_file"
done < <(find "$source_du_dir" -type f -print0)

if [[ -f "$srsran_dir/run_piradio.sh" ]]; then
  rm -f "$srsran_dir/run_piradio.sh"
fi

chmod +x "$srsran_dir/Orun_gnb.sh" "$srsran_dir/run_gnb.sh" 2>/dev/null || true

for generated_source in srs_messages.pb.cc srs_messages.pb.h; do
  if [[ ! -f "$srsran_dir/lib/e2/e2sm/e2sm_runOran/$generated_source" ]]; then
    echo "Error: generated runOran protobuf source is missing: $generated_source"
    exit 1
  fi
done

echo "Configuring srsRAN build"
# Always configure after copying the overlay. This refreshes CMake targets when the
# overlay changes and also repairs build trees produced by earlier script versions.
cmake -S "$srsran_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release

echo "Building gNB with the bundled runOran E2 changes"
cmake --build "$build_dir" --target gnb -j"$(nproc)"

if [[ ! -x "$build_dir/apps/gnb/gnb" ]]; then
  echo "Error: build completed but '$build_dir/apps/gnb/gnb' is missing."
  exit 1
fi

if ! strings -a "$build_dir/apps/gnb/gnb" | grep -Fq "ORAN runOran E2 Service Model"; then
  echo "Warning: the built gNB does not appear to include the bundled runOran E2 service model."
  echo "Check that the checkout contains the copied DU files from this repository."
else
  echo "gNB build includes the bundled runOran E2 service model."
fi

echo "Done. Built binary: $build_dir/apps/gnb/gnb"
