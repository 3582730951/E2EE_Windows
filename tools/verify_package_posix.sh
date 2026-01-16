#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: verify_package_posix.sh [--platform linux|macos] [--dist PATH]
EOF
}

platform=""
dist_root=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --platform)
      platform="${2:-}"
      shift 2
      ;;
    --dist)
      dist_root="${2:-}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage
      exit 1
      ;;
  esac
done

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
workspace="$(cd "$script_dir/.." && pwd)"
if [[ -z "$dist_root" ]]; then
  dist_root="$workspace/dist"
fi
if [[ -z "$platform" ]]; then
  uname_s="$(uname -s | tr '[:upper:]' '[:lower:]')"
  case "$uname_s" in
    linux*) platform="linux" ;;
    darwin*) platform="macos" ;;
    *) echo "unsupported platform: $uname_s" >&2; exit 1 ;;
  esac
fi

case "$platform" in
  linux) sdk_ext="so" ;;
  macos) sdk_ext="dylib" ;;
  *) echo "unsupported --platform: $platform" >&2; exit 1 ;;
esac

client_root="$dist_root/mi_e2ee_client"
server_root="$dist_root/mi_e2ee_server"

require_file() {
  local path="$1"
  if [[ ! -f "$path" ]]; then
    echo "missing file: $path" >&2
    exit 1
  fi
}

require_dir() {
  local path="$1"
  if [[ ! -d "$path" ]]; then
    echo "missing dir: $path" >&2
    exit 1
  fi
}

require_dir "$client_root"
require_dir "$server_root"
require_dir "$client_root/lib"
require_dir "$server_root/lib"
require_dir "$server_root/tools"

require_file "$client_root/lib/libmi_e2ee_client_sdk.${sdk_ext}"
require_file "$client_root/config/client_config.ini"
require_file "$client_root/sdk/c_api_client.h"
require_file "$client_root/bindings/python/mi_e2ee_client.py"
require_file "$client_root/bindings/rust/Cargo.toml"
require_file "$client_root/env.sh"

require_file "$server_root/mi_e2ee_server"
require_file "$server_root/config/config.ini"
require_file "$server_root/config/mi_e2ee_server.pem"
require_file "$server_root/config/kt_signing_key.bin"
require_file "$server_root/config/kt_root_pub.bin"
require_file "$server_root/tools/mi_e2ee_kt_keygen"
require_file "$server_root/tools/mi_e2ee_kt_pubinfo"
require_file "$server_root/tools/mi_e2ee_perf_baseline"
require_file "$server_root/tools/mi_e2ee_ops_health_view"
require_file "$server_root/tools/mi_e2ee_third_party_audit"
require_file "$server_root/test_user.txt"
require_file "$server_root/run_server.sh"

fingerprint="$(grep -E '^pinned_fingerprint=' "$client_root/config/client_config.ini" | head -n 1 | cut -d= -f2 | tr -d '[:space:]')"
if [[ ! "$fingerprint" =~ ^[0-9a-f]{64}$ ]]; then
  echo "pinned_fingerprint invalid: $fingerprint" >&2
  exit 1
fi

if ! grep -q "^tls_cert=config/mi_e2ee_server.pem" "$server_root/config/config.ini"; then
  echo "server config missing tls_cert=config/mi_e2ee_server.pem" >&2
  exit 1
fi

verify_manifest() {
  local root="$1"
  local manifest="$root/manifest.sha256"
  if [[ ! -f "$manifest" ]]; then
    echo "missing manifest: $manifest" >&2
    exit 1
  fi
  if command -v sha256sum >/dev/null 2>&1; then
    (cd "$root" && sha256sum -c "manifest.sha256")
  elif command -v shasum >/dev/null 2>&1; then
    (cd "$root" && shasum -a 256 -c "manifest.sha256")
  else
    echo "sha256 tool not found; skipping manifest verification for $root" >&2
  fi
}

check_linux_deps() {
  local bin="$1"
  local lib_dir="$2"
  local output
  if ! output="$(LD_LIBRARY_PATH="$lib_dir" ldd "$bin" 2>&1)"; then
    echo "ldd failed: $bin" >&2
    echo "$output" >&2
    exit 1
  fi
  if echo "$output" | grep -q "not found"; then
    echo "missing dependencies for $bin" >&2
    echo "$output" >&2
    exit 1
  fi
}

verify_deps_linux() {
  local lib_dir="$1"
  shift
  local bin
  for bin in "$@"; do
    check_linux_deps "$bin" "$lib_dir"
  done
  shopt -s nullglob
  local lib
  for lib in "$lib_dir"/*.so*; do
    check_linux_deps "$lib" "$lib_dir"
  done
  shopt -u nullglob
}

check_macos_deps() {
  local bin="$1"
  local lib_dir="$2"
  local bin_dir
  bin_dir="$(cd "$(dirname "$bin")" && pwd)"
  while IFS= read -r dep; do
    if [[ -z "$dep" ]]; then
      continue
    fi
    case "$dep" in
      /usr/lib/*|/System/Library/*)
        continue
        ;;
      @rpath/*)
        local base
        base="$(basename "$dep")"
        if [[ ! -f "$lib_dir/$base" ]]; then
          echo "missing @rpath dep for $bin: $dep" >&2
          exit 1
        fi
        ;;
      @loader_path/*)
        local rel="${dep#@loader_path/}"
        if [[ ! -f "$bin_dir/$rel" ]]; then
          echo "missing @loader_path dep for $bin: $dep" >&2
          exit 1
        fi
        ;;
      @executable_path/*)
        local rel="${dep#@executable_path/}"
        if [[ ! -f "$bin_dir/$rel" ]]; then
          echo "missing @executable_path dep for $bin: $dep" >&2
          exit 1
        fi
        ;;
      /*)
        echo "unexpected absolute dep for $bin: $dep" >&2
        exit 1
        ;;
      *)
        echo "unsupported dep for $bin: $dep" >&2
        exit 1
        ;;
    esac
  done < <(otool -L "$bin" | tail -n +2 | awk '{print $1}')
}

verify_deps_macos() {
  local lib_dir="$1"
  shift
  local bin
  for bin in "$@"; do
    check_macos_deps "$bin" "$lib_dir"
  done
  shopt -s nullglob
  local lib
  for lib in "$lib_dir"/*.dylib; do
    check_macos_deps "$lib" "$lib_dir"
  done
  shopt -u nullglob
}

if [[ "$platform" == "linux" ]]; then
  verify_deps_linux "$client_root/lib" "$client_root/lib/libmi_e2ee_client_sdk.${sdk_ext}"
  verify_deps_linux "$server_root/lib" \
    "$server_root/mi_e2ee_server" \
    "$server_root/tools/mi_e2ee_kt_keygen" \
    "$server_root/tools/mi_e2ee_kt_pubinfo" \
    "$server_root/tools/mi_e2ee_perf_baseline" \
    "$server_root/tools/mi_e2ee_ops_health_view" \
    "$server_root/tools/mi_e2ee_third_party_audit"
elif [[ "$platform" == "macos" ]]; then
  verify_deps_macos "$client_root/lib" "$client_root/lib/libmi_e2ee_client_sdk.${sdk_ext}"
  verify_deps_macos "$server_root/lib" \
    "$server_root/mi_e2ee_server" \
    "$server_root/tools/mi_e2ee_kt_keygen" \
    "$server_root/tools/mi_e2ee_kt_pubinfo" \
    "$server_root/tools/mi_e2ee_perf_baseline" \
    "$server_root/tools/mi_e2ee_ops_health_view" \
    "$server_root/tools/mi_e2ee_third_party_audit"
fi

verify_manifest "$client_root"
verify_manifest "$server_root"
