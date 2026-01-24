#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: package_posix.sh [--platform linux|macos] [--workspace PATH] [--dist PATH] [--openssl PATH]

Build prerequisites:
  - build/client contains libmi_e2ee_client_sdk.(so|dylib)
  - build/server contains mi_e2ee_server and tools (mi_e2ee_kt_keygen, ...)

Examples:
  bash tools/package_posix.sh --platform linux
  bash tools/package_posix.sh --platform macos --openssl "$(brew --prefix openssl@3)/bin/openssl"
EOF
}

platform=""
workspace=""
dist_root=""
openssl_bin=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --platform)
      platform="${2:-}"
      shift 2
      ;;
    --workspace)
      workspace="${2:-}"
      shift 2
      ;;
    --dist)
      dist_root="${2:-}"
      shift 2
      ;;
    --openssl)
      openssl_bin="${2:-}"
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
if [[ -z "$workspace" ]]; then
  workspace="$(cd "$script_dir/.." && pwd)"
fi
if [[ -z "$platform" ]]; then
  uname_s="$(uname -s | tr '[:upper:]' '[:lower:]')"
  case "$uname_s" in
    linux*) platform="linux" ;;
    darwin*) platform="macos" ;;
    *) echo "unsupported platform: $uname_s" >&2; exit 1 ;;
  esac
fi
if [[ -z "$dist_root" ]]; then
  dist_root="$workspace/dist"
fi
if [[ -z "$openssl_bin" ]]; then
  openssl_bin="$(command -v openssl || true)"
fi
if [[ -z "$openssl_bin" ]]; then
  echo "openssl not found; pass --openssl PATH" >&2
  exit 1
fi

case "$platform" in
  linux)
    sdk_ext="so"
    ;;
  macos)
    sdk_ext="dylib"
    ;;
  *)
    echo "unsupported --platform: $platform" >&2
    exit 1
    ;;
esac

client_build="$workspace/build/client"
server_build="$workspace/build/server"
if [[ ! -d "$client_build" || ! -d "$server_build" ]]; then
  echo "build output missing: $client_build or $server_build" >&2
  exit 1
fi

client_root="$dist_root/mi_e2ee_client"
server_root="$dist_root/mi_e2ee_server"
client_lib="$client_root/lib"
server_lib="$server_root/lib"

mkdir -p "$client_lib" "$client_root/config" "$client_root/database" \
         "$client_root/bindings/python" "$client_root/bindings/rust" "$client_root/sdk" \
         "$server_lib" "$server_root/config" "$server_root/database/offline_store" "$server_root/tools"

sdk_lib="$(find "$client_build" -type f -name "libmi_e2ee_client_sdk.${sdk_ext}" | head -n 1 || true)"
if [[ -z "$sdk_lib" ]]; then
  echo "libmi_e2ee_client_sdk.${sdk_ext} not found under $client_build" >&2
  exit 1
fi
cp "$sdk_lib" "$client_lib/"

core_lib="$(find "$client_build" -type f -name "libmi_e2ee_client_core.a" | head -n 1 || true)"
if [[ -n "$core_lib" ]]; then
  cp "$core_lib" "$client_lib/"
fi

server_bin="$(find "$server_build" -type f -name "mi_e2ee_server" -perm -111 | head -n 1 || true)"
if [[ -z "$server_bin" ]]; then
  echo "mi_e2ee_server not found under $server_build" >&2
  exit 1
fi
cp "$server_bin" "$server_root/mi_e2ee_server"

demo_users="$(find "$server_build" -type f -name "test_user.txt" | head -n 1 || true)"
if [[ -n "$demo_users" ]]; then
  cp "$demo_users" "$server_root/"
fi

for tool in mi_e2ee_kt_keygen mi_e2ee_kt_pubinfo mi_e2ee_perf_baseline mi_e2ee_ops_health_view mi_e2ee_third_party_audit; do
  tool_path="$(find "$server_build" -type f -name "$tool" -perm -111 | head -n 1 || true)"
  if [[ -n "$tool_path" ]]; then
    cp "$tool_path" "$server_root/tools/"
  fi
done

kt_keygen="$(find "$server_build" -type f -name "mi_e2ee_kt_keygen" -perm -111 | head -n 1 || true)"
if [[ -z "$kt_keygen" ]]; then
  echo "mi_e2ee_kt_keygen not found under $server_build" >&2
  exit 1
fi
keys_dir="$dist_root/keys"
mkdir -p "$keys_dir"
"$kt_keygen" --out-dir "$keys_dir" --force

cert_dir="$server_root/config"
"$openssl_bin" req -x509 -newkey rsa:2048 -sha256 -nodes -days 3650 \
  -subj "/CN=MI_E2EE_Server" \
  -keyout "$cert_dir/mi_e2ee_server.key" \
  -out "$cert_dir/mi_e2ee_server.crt"
cat "$cert_dir/mi_e2ee_server.key" "$cert_dir/mi_e2ee_server.crt" > "$cert_dir/mi_e2ee_server.pem"
fingerprint="$("$openssl_bin" x509 -in "$cert_dir/mi_e2ee_server.crt" -noout -fingerprint -sha256 | cut -d= -f2 | tr -d ':' | tr 'A-F' 'a-f')"
rm -f "$cert_dir/mi_e2ee_server.key" "$cert_dir/mi_e2ee_server.crt"

if [[ ${#fingerprint} -ne 64 ]]; then
  echo "pinned_fingerprint invalid: $fingerprint" >&2
  exit 1
fi

cp "$keys_dir/kt_signing_key.bin" "$server_root/config/"
cp "$keys_dir/kt_root_pub.bin" "$server_root/config/"
cp "$keys_dir/kt_root_pub.bin" "$client_root/config/"
: > "$client_root/database/server_trust.ini"

cat > "$server_root/config/config.ini" <<EOF
[mode]
mode=1
[server]
list_port=9000
rotation_threshold=10000
offline_dir=database/offline_store
debug_log=0
tls_enable=1
require_tls=1
tls_cert=config/mi_e2ee_server.pem
kt_signing_key=kt_signing_key.bin
EOF

cat > "$client_root/config/client_config.ini" <<EOF
[client]
server_ip=127.0.0.1
server_port=9000
use_tls=1
require_tls=1
trust_store=server_trust.ini
require_pinned_fingerprint=1
pinned_fingerprint=$fingerprint
tls_verify_mode=pin
tls_ca_bundle_path=
tls_verify_hostname=1
auth_mode=opaque

[proxy]
type=none
host=
port=0
username=
password=

[device_sync]
enabled=0
role=primary
key_path=e2ee_state/device_sync_key.bin

[kt]
require_signature=1
root_pubkey_path=kt_root_pub.bin
EOF

cp "$workspace/sdk/c_api_client.h" "$client_root/sdk/"
cp "$workspace/bindings/python/mi_e2ee_client.py" "$workspace/bindings/python/example_basic.py" "$client_root/bindings/python/"
cp "$workspace/bindings/rust/Cargo.toml" "$workspace/bindings/rust/Cargo.lock" "$workspace/bindings/rust/build.rs" "$client_root/bindings/rust/"
cp -R "$workspace/bindings/rust/src" "$workspace/bindings/rust/examples" "$client_root/bindings/rust/"
if [[ -f "$workspace/bindings/README.md" ]]; then
  cp "$workspace/bindings/README.md" "$client_root/bindings/"
fi

collect_deps_linux() {
  ldd "$1" 2>/dev/null | awk '{for (i=1;i<=NF;i++) if ($i ~ /^\//) print $i}' \
    | grep -vE 'linux-vdso|ld-linux' | sort -u
}

collect_deps_macos() {
  local target="$1"
  local -a seen_list=()
  has_seen() {
    local needle="$1"
    local item
    for item in "${seen_list[@]-}"; do
      if [[ "$item" == "$needle" ]]; then
        return 0
      fi
    done
    return 1
  }
  _walk() {
    local bin="$1"
    if [[ -z "$bin" || ! -f "$bin" ]]; then
      return
    fi
    while IFS= read -r dep; do
      if [[ -z "$dep" ]]; then
        continue
      fi
      if [[ "$dep" != /* ]]; then
        continue
      fi
      if [[ "$dep" == /usr/lib/* || "$dep" == /System/Library/* ]]; then
        continue
      fi
      if has_seen "$dep"; then
        continue
      fi
      seen_list+=("$dep")
      echo "$dep"
      _walk "$dep"
    done < <(otool -L "$bin" | tail -n +2 | awk '{print $1}')
  }
  _walk "$target"
}

copy_deps() {
  local bin="$1"
  local dest="$2"
  if [[ -z "$bin" ]]; then
    return
  fi
  case "$platform" in
    linux) collect_deps_linux "$bin" ;;
    macos) collect_deps_macos "$bin" ;;
  esac | while read -r lib; do
    local base
    base="$(basename "$lib")"
    if [[ -f "$dest/$base" ]]; then
      continue
    fi
    cp -L "$lib" "$dest/$base"
  done
}

copy_deps "$server_bin" "$server_lib"
copy_deps "$sdk_lib" "$client_lib"
for tool in "$server_root/tools/"*; do
  if [[ -x "$tool" ]]; then
    copy_deps "$tool" "$server_lib"
  fi
done

if [[ "$platform" == "macos" ]]; then
  if ! command -v install_name_tool >/dev/null 2>&1; then
    echo "install_name_tool not found; cannot fix macOS dylib paths" >&2
    exit 1
  fi

  ensure_rpath() {
    local bin="$1"
    local rpath="$2"
    local existing
    existing="$(otool -l "$bin" | awk '/LC_RPATH/{getline; getline; print $2}')"
    if echo "$existing" | grep -Fxq "$rpath"; then
      return
    fi
    install_name_tool -add_rpath "$rpath" "$bin"
  }

  rewrite_deps() {
    local bin="$1"
    otool -L "$bin" | tail -n +2 | awk '{print $1}' | while read -r dep; do
      if [[ -z "$dep" ]]; then
        continue
      fi
      if [[ "$dep" == /usr/lib/* || "$dep" == /System/Library/* ]]; then
        continue
      fi
      if [[ "$dep" == @* ]]; then
        continue
      fi
      local base
      base="$(basename "$dep")"
      install_name_tool -change "$dep" "@rpath/$base" "$bin"
    done
  }

  fixup_dylib() {
    local lib="$1"
    local base
    base="$(basename "$lib")"
    install_name_tool -id "@rpath/$base" "$lib"
    ensure_rpath "$lib" "@loader_path"
    rewrite_deps "$lib"
  }

  fixup_binary() {
    local bin="$1"
    local rpath="$2"
    ensure_rpath "$bin" "$rpath"
    rewrite_deps "$bin"
  }

  shopt -s nullglob
  for lib in "$client_lib"/*.dylib "$server_lib"/*.dylib; do
    fixup_dylib "$lib"
  done
  shopt -u nullglob

  if [[ -f "$server_root/mi_e2ee_server" ]]; then
    fixup_binary "$server_root/mi_e2ee_server" "@loader_path/lib"
  fi
  for tool in "$server_root/tools/"*; do
    if [[ -x "$tool" ]]; then
      fixup_binary "$tool" "@loader_path/../lib"
    fi
  done
fi

cat > "$client_root/env.sh" <<EOF
#!/usr/bin/env bash
set -euo pipefail
ROOT="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
EOF
if [[ "$platform" == "macos" ]]; then
  cat >> "$client_root/env.sh" <<'EOF'
export DYLD_LIBRARY_PATH="$ROOT/lib:${DYLD_LIBRARY_PATH:-}"
export MI_E2EE_CLIENT_LIB_DIR="$ROOT/lib"
export MI_E2EE_CLIENT_DLL="$ROOT/lib/libmi_e2ee_client_sdk.dylib"
EOF
else
  cat >> "$client_root/env.sh" <<'EOF'
export LD_LIBRARY_PATH="$ROOT/lib:${LD_LIBRARY_PATH:-}"
export MI_E2EE_CLIENT_LIB_DIR="$ROOT/lib"
export MI_E2EE_CLIENT_DLL="$ROOT/lib/libmi_e2ee_client_sdk.so"
EOF
fi
chmod +x "$client_root/env.sh"

cat > "$server_root/run_server.sh" <<EOF
#!/usr/bin/env bash
set -euo pipefail
ROOT="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
EOF
if [[ "$platform" == "macos" ]]; then
  cat >> "$server_root/run_server.sh" <<'EOF'
export DYLD_LIBRARY_PATH="$ROOT/lib:${DYLD_LIBRARY_PATH:-}"
exec "$ROOT/mi_e2ee_server" "$ROOT/config/config.ini"
EOF
else
  cat >> "$server_root/run_server.sh" <<'EOF'
export LD_LIBRARY_PATH="$ROOT/lib:${LD_LIBRARY_PATH:-}"
exec "$ROOT/mi_e2ee_server" "$ROOT/config/config.ini"
EOF
fi
chmod +x "$server_root/run_server.sh"

hash_file() {
  local file="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$file" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$file" | awk '{print $1}'
  else
    "$openssl_bin" dgst -sha256 "$file" | awk '{print $NF}'
  fi
}

write_manifest() {
  local root="$1"
  local out="$root/manifest.sha256"
  (cd "$root" && find . -type f ! -name "manifest.sha256" -print0 | sort -z | while IFS= read -r -d '' f; do
    rel="${f#./}"
    hash="$(hash_file "$root/$rel")"
    printf "%s  %s\n" "$hash" "$rel"
  done) > "$out"
}

write_manifest "$client_root"
write_manifest "$server_root"

tar -C "$dist_root" -czf "$dist_root/mi_e2ee_client_${platform}.tar.gz" mi_e2ee_client
tar -C "$dist_root" -czf "$dist_root/mi_e2ee_server_${platform}.tar.gz" mi_e2ee_server
