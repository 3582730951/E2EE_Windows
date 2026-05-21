#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: package_posix.sh [--platform linux|macos] [--workspace PATH] [--dist PATH] [--openssl PATH]
                        [--client-build PATH] [--server-build PATH]
                        [--server-mode demo|mysql] [--mysql-username USER] [--mysql-password PASS]
                        [--client-server-host HOST_OR_IP] [--client-server-port PORT]
                        [--codesign-id ID] [--codesign-entitlements PATH]
                        [--notary-profile PROFILE] [--notary-bundle-id ID] [--notary-wait]

Build prerequisites:
  - build/client contains libmi_e2ee_client_sdk.(so|dylib)
  - build/server contains mi_e2ee_server and tools (mi_e2ee_kt_keygen, ...)

Examples:
  bash tools/package_posix.sh --platform linux
  bash tools/package_posix.sh --platform macos --openssl "$(brew --prefix openssl@3)/bin/openssl"
EOF
}

parse_bool() {
  local v="${1:-}"
  v="$(echo "$v" | tr '[:upper:]' '[:lower:]')"
  case "$v" in
    1|true|on|yes) echo 1 ;;
    0|false|off|no) echo 0 ;;
    *) echo 0 ;;
  esac
}

platform=""
workspace=""
dist_root=""
openssl_bin=""
client_build=""
server_build=""
server_mode="mysql"
mysql_username=""
mysql_password=""
client_server_host="${MI_E2EE_PACKAGE_CLIENT_SERVER_HOST:-}"
client_server_port="${MI_E2EE_PACKAGE_CLIENT_SERVER_PORT:-}"
codesign_id=""
codesign_entitlements=""
notary_profile=""
notary_bundle_id=""
notary_wait=0

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
    --client-build)
      client_build="${2:-}"
      shift 2
      ;;
    --server-build)
      server_build="${2:-}"
      shift 2
      ;;
    --server-mode)
      server_mode="${2:-}"
      shift 2
      ;;
    --mysql-username)
      mysql_username="${2:-}"
      shift 2
      ;;
    --mysql-password)
      mysql_password="${2:-}"
      shift 2
      ;;
    --client-server-host)
      client_server_host="${2:-}"
      shift 2
      ;;
    --client-server-port)
      client_server_port="${2:-}"
      shift 2
      ;;
    --codesign-id)
      codesign_id="${2:-}"
      shift 2
      ;;
    --codesign-entitlements)
      codesign_entitlements="${2:-}"
      shift 2
      ;;
    --notary-profile)
      notary_profile="${2:-}"
      shift 2
      ;;
    --notary-bundle-id)
      notary_bundle_id="${2:-}"
      shift 2
      ;;
    --notary-wait)
      notary_wait=1
      shift
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
if [[ -z "$client_server_host" ]]; then
  echo "client server host required: pass --client-server-host or set MI_E2EE_PACKAGE_CLIENT_SERVER_HOST" >&2
  exit 1
fi
if [[ -z "$client_server_port" || "$client_server_port" == "0" ]]; then
  echo "client server port required: pass --client-server-port or set MI_E2EE_PACKAGE_CLIENT_SERVER_PORT" >&2
  exit 1
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

if [[ -z "$codesign_id" ]]; then
  codesign_id="${MI_E2EE_MAC_CODESIGN_IDENTITY:-}"
fi
if [[ -z "$codesign_entitlements" ]]; then
  codesign_entitlements="${MI_E2EE_MAC_CODESIGN_ENTITLEMENTS:-}"
fi
if [[ -z "$notary_profile" ]]; then
  notary_profile="${MI_E2EE_MAC_NOTARY_PROFILE:-}"
fi
if [[ -z "$notary_bundle_id" ]]; then
  notary_bundle_id="${MI_E2EE_MAC_NOTARY_BUNDLE_ID:-}"
fi
if [[ "$notary_wait" -eq 0 && -n "${MI_E2EE_MAC_NOTARY_WAIT:-}" ]]; then
  notary_wait="$(parse_bool "$MI_E2EE_MAC_NOTARY_WAIT")"
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

case "$server_mode" in
  demo|mysql)
    ;;
  *)
    echo "unsupported --server-mode: $server_mode" >&2
    exit 1
    ;;
esac

if [[ -z "$mysql_username" ]]; then
  mysql_username="${MI_E2EE_MYSQL_USERNAME:-}"
fi
if [[ -z "$mysql_password" ]]; then
  mysql_password="${MI_E2EE_MYSQL_PASSWORD:-}"
fi
if [[ "$server_mode" == "mysql" ]]; then
  if [[ -z "$mysql_username" || -z "$mysql_password" ]]; then
    echo "mysql credentials required for --server-mode mysql (use --mysql-username/--mysql-password or MI_E2EE_MYSQL_USERNAME/MI_E2EE_MYSQL_PASSWORD)" >&2
    exit 1
  fi
  if [[ "$mysql_username" == "root" &&
        ( "$mysql_password" == "123456" || "$mysql_password" == "pass" ) ]]; then
    echo "weak mysql credentials are forbidden in package config" >&2
    exit 1
  fi
fi

if [[ -z "$client_build" ]]; then
  client_build="${MI_E2EE_CLIENT_BUILD_DIR:-$workspace/build/client}"
fi
if [[ -z "$server_build" ]]; then
  server_build="${MI_E2EE_SERVER_BUILD_DIR:-$workspace/build/server}"
fi
if [[ ! -d "$client_build" || ! -d "$server_build" ]]; then
  echo "build output missing: $client_build or $server_build" >&2
  exit 1
fi

client_root="$dist_root/mi_e2ee_client"
server_root="$dist_root/mi_e2ee_server"
client_lib="$client_root/lib"
server_lib="$server_root/lib"
keys_dir="$dist_root/keys"

rm -rf "$client_root" "$server_root" "$keys_dir"
mkdir -p "$client_lib" "$client_root/config" "$client_root/database" \
         "$client_root/bindings/python" "$client_root/bindings/rust" "$client_root/sdk" \
         "$client_root/tools" \
         "$server_lib" "$server_root/config" "$server_root/database/offline_store" "$server_root/tools"

for forbidden in \
  "$client_root/mi_e2ee_client" \
  "$client_root/e2ee_login" \
  "$client_root/e2ee_main_list" \
  "$client_root/e2ee_group_chat" \
  "$client_root/e2ee_chat_empty"; do
  rm -f "$forbidden"
done

sdk_lib="$(find "$client_build" -type f -name "libmi_e2ee_client_sdk.${sdk_ext}" | head -n 1 || true)"
if [[ -z "$sdk_lib" ]]; then
  echo "libmi_e2ee_client_sdk.${sdk_ext} not found under $client_build" >&2
  exit 1
fi

client_config_tool="$(find "$client_build" -type f -name "mi_e2ee_client_config_tool" -perm -111 | head -n 1 || true)"
if [[ -z "$client_config_tool" ]]; then
  echo "mi_e2ee_client_config_tool not found under $client_build" >&2
  exit 1
fi
cp "$client_config_tool" "$client_root/"
cp "$client_config_tool" "$client_root/tools/"
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

for tool in mi_e2ee_kt_keygen mi_e2ee_kt_pubinfo mi_e2ee_business_stress; do
  tool_path="$(find "$server_build" -type f -name "$tool" -perm -111 | head -n 1 || true)"
  if [[ -n "$tool_path" ]]; then
    cp "$tool_path" "$server_root/tools/"
    if [[ "$tool" == "mi_e2ee_business_stress" ]]; then
      cp "$tool_path" "$server_root/"
    fi
  fi
done

for helper in configure_server.sh start_server.sh stress_server.sh stress_server.py verify_server_config.py; do
  if [[ -f "$workspace/tools/$helper" ]]; then
    cp "$workspace/tools/$helper" "$server_root/"
    cp "$workspace/tools/$helper" "$server_root/tools/"
  fi
done
chmod +x "$server_root/configure_server.sh" "$server_root/start_server.sh" \
  "$server_root/stress_server.sh" "$server_root/stress_server.py" \
  "$server_root/verify_server_config.py" 2>/dev/null || true

kt_keygen="$(find "$server_build" -type f -name "mi_e2ee_kt_keygen" -perm -111 | head -n 1 || true)"
if [[ -z "$kt_keygen" ]]; then
  echo "mi_e2ee_kt_keygen not found under $server_build" >&2
  exit 1
fi
keys_dir="$dist_root/keys"
mkdir -p "$keys_dir"
"$kt_keygen" --out-dir "$keys_dir" --force

cert_dir="$server_root/config"
cert_san="DNS:localhost"
if [[ -n "$client_server_host" && "$client_server_host" != "localhost" ]]; then
  if [[ "$client_server_host" =~ ^[0-9]+(\.[0-9]+){3}$ ]]; then
    cert_san="DNS:localhost,IP:$client_server_host"
  else
    cert_san="DNS:localhost,DNS:$client_server_host"
  fi
fi
"$openssl_bin" req -x509 -newkey rsa:2048 -sha256 -nodes -days 3650 \
  -subj "/CN=MI_E2EE_Server" \
  -addext "subjectAltName=$cert_san" \
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

if [[ "$server_mode" == "mysql" ]]; then
cat > "$server_root/config/config.ini" <<EOF
[mode]
mode=0
[mysql]
mysql_ip=localhost
mysql_port=3306
mysql_database=mi_e2ee
mysql_username=$mysql_username
mysql_password=$mysql_password
[server]
list_port=9000
rotation_threshold=10000
offline_dir=database/offline_store
debug_log=0
offline_blob_temp_budget_bytes=4294967296
tls_enable=1
require_tls=1
tls_cert=config/mi_e2ee_server.pem
kt_signing_key=kt_signing_key.bin
state_protection=none
[kcp]
enable=0
allow_insecure=0
EOF
else
cat > "$server_root/config/config.ini" <<EOF
[mode]
mode=1
[server]
list_port=9000
rotation_threshold=10000
offline_dir=database/offline_store
debug_log=0
offline_blob_temp_budget_bytes=4294967296
tls_enable=1
require_tls=1
tls_cert=config/mi_e2ee_server.pem
kt_signing_key=kt_signing_key.bin
state_protection=none
[kcp]
enable=0
allow_insecure=0
EOF
fi

"$client_config_tool" \
  --config "$client_root/config/client_config.ini" \
  --server "$client_server_host" \
  --port "$client_server_port" \
  --tls-mode pin \
  --pinned-fingerprint "$fingerprint" \
  --trust-store server_trust.ini \
  --kt-root-pub kt_root_pub.bin \
  --non-interactive >/dev/null

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
  local magic
  magic="$(LC_ALL=C head -c 4 "$bin" 2>/dev/null || true)"
  if [[ "$platform" == "linux" && "$magic" != $'\177ELF' ]]; then
    return
  fi
  if command -v file >/dev/null 2>&1 &&
     ! (file -b "$bin" | grep -Eq 'ELF|Mach-O'); then
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

if [[ "$platform" == "macos" && -n "$codesign_id" ]]; then
  if ! command -v codesign >/dev/null 2>&1; then
    echo "codesign not found; cannot sign macOS artifacts" >&2
    exit 1
  fi
  if [[ -n "$codesign_entitlements" && ! -f "$codesign_entitlements" ]]; then
    echo "codesign entitlements not found: $codesign_entitlements" >&2
    exit 1
  fi

  sign_item() {
    local target="$1"
    local entitlements="$2"
    local -a args=()
    if [[ -n "$entitlements" ]]; then
      args+=(--entitlements "$entitlements")
    fi
    codesign --force --options runtime --timestamp --sign "$codesign_id" \
      "${args[@]}" "$target"
  }

  shopt -s nullglob
  for lib in "$client_lib"/*.dylib "$server_lib"/*.dylib; do
    sign_item "$lib" ""
  done
  shopt -u nullglob

  if [[ -f "$client_lib/libmi_e2ee_client_sdk.dylib" ]]; then
    sign_item "$client_lib/libmi_e2ee_client_sdk.dylib" ""
  fi
  if [[ -f "$server_root/mi_e2ee_server" ]]; then
    sign_item "$server_root/mi_e2ee_server" "$codesign_entitlements"
  fi
  for tool in "$server_root/tools/"*; do
    if [[ -x "$tool" ]]; then
      sign_item "$tool" "$codesign_entitlements"
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

if [[ "$platform" == "macos" && -n "$notary_profile" ]]; then
  if [[ -z "$codesign_id" ]]; then
    echo "notary_profile set but codesign_id missing" >&2
    exit 1
  fi
  if ! command -v xcrun >/dev/null 2>&1; then
    echo "xcrun not found; cannot submit for notarization" >&2
    exit 1
  fi
  if ! command -v ditto >/dev/null 2>&1; then
    echo "ditto not found; cannot create notarization zips" >&2
    exit 1
  fi
  client_zip="$dist_root/mi_e2ee_client_${platform}_notary.zip"
  server_zip="$dist_root/mi_e2ee_server_${platform}_notary.zip"
  rm -f "$client_zip" "$server_zip"
  ditto -c -k --keepParent "$client_root" "$client_zip"
  ditto -c -k --keepParent "$server_root" "$server_zip"

  submit_notary() {
    local zip="$1"
    local -a args=(
      xcrun notarytool submit "$zip" --keychain-profile "$notary_profile"
    )
    if [[ -n "$notary_bundle_id" ]]; then
      args+=(--bundle-id "$notary_bundle_id")
    fi
    if [[ "$notary_wait" -eq 1 ]]; then
      args+=(--wait)
    fi
    "${args[@]}"
  }

  submit_notary "$client_zip"
  submit_notary "$server_zip"
fi
