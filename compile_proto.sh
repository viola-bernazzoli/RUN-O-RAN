#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
proto_dir="$repo_root/proto"
proto_file="$proto_dir/srs_messages.proto"
cpp_output_dir="$repo_root/du/srsRAN_Project/lib/e2/e2sm/e2sm_runOran"

choose_target() {
  local choice="${1:-}"
  if [[ -z "$choice" ]]; then
    printf 'Choose protobuf target [python/c]: '
    read -r choice
  fi
  printf '%s' "${choice,,}"
}

target="$(choose_target "${1:-}")"

if ! command -v protoc >/dev/null 2>&1; then
  echo "Error: protoc is not installed or not available on PATH."
  exit 1
fi

case "$target" in
  python|py)
    python_build_dir="$repo_root/generated/python"
    mkdir -p "$python_build_dir" "$repo_root/python/lib"
    protoc -I"$proto_dir" --python_out="$python_build_dir" "$proto_file"
    cp "$python_build_dir/srs_messages_pb2.py" "$repo_root/python/lib/srs_messages_pb2.py"
    touch "$repo_root/python/lib/__init__.py"
    echo "Generated Python bindings at python/lib/srs_messages_pb2.py"
    ;;
  c|cpp)
    mkdir -p "$cpp_output_dir"
    protoc -I"$proto_dir" --cpp_out="$cpp_output_dir" "$proto_file"
    echo "Generated C++ bindings under du/srsRAN_Project/lib/e2/e2sm/e2sm_runOran/"
    ;;
  *)
    echo "Error: expected 'python' or 'c'"
    exit 1
    ;;
esac
