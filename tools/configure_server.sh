#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ "$(basename "$SCRIPT_DIR")" == "tools" ]]; then
  ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  ROOT="$SCRIPT_DIR"
fi
PYTHON_BIN="${PYTHON_BIN:-python3}"
VERIFY_SCRIPT="$SCRIPT_DIR/verify_server_config.py"
if [[ ! -f "$VERIFY_SCRIPT" && -f "$ROOT/tools/verify_server_config.py" ]]; then
  VERIFY_SCRIPT="$ROOT/tools/verify_server_config.py"
fi

CONFIG_DIR="$ROOT/config"
CONFIG_PATH="$CONFIG_DIR/config.ini"
OFFLINE_DIR="database/offline_store"
TLS_CERT_REL="config/mi_e2ee_server.pem"
TLS_CERT_PATH="$ROOT/$TLS_CERT_REL"
CLIENT_CONFIG_DEFAULT="$ROOT/../mi_e2ee_client/config/client_config.ini"

print_menu() {
  cat <<'MENU'
MI E2EE Server Configuration

1) First-time setup
2) Reconfigure server
3) Generate self-signed certificate and pin
4) Import CA/server certificate
5) Rotate client pinned fingerprint
6) Initialize / rotate Key Transparency key
7) Validate current configuration
8) Show current certificate fingerprint and SAS
9) Exit

Select [1-9]:
MENU
}

usage() {
  print_menu
  cat <<'USAGE'

Non-interactive test/automation:
  configure_server.sh --non-interactive --mode mysql --mysql-user USER --mysql-password PASS --output config/config.ini
  configure_server.sh --non-interactive --mode demo --output config/config.ini
  configure_server.sh --print-menu
USAGE
}

prompt_default() {
  local label="$1"
  local default="$2"
  local answer
  read -r -p "$label [$default]: " answer
  printf '%s' "${answer:-$default}"
}

prompt_secret_confirm() {
  local label="$1"
  local first second
  while true; do
    read -r -s -p "$label: " first
    printf '\n'
    read -r -s -p "Confirm $label: " second
    printf '\n'
    if [[ "$first" == "$second" ]]; then
      printf '%s' "$first"
      return 0
    fi
    echo "Values did not match." >&2
  done
}

is_weak_mysql_credentials() {
  local user="$1"
  local password="$2"
  user="$(printf '%s' "$user" | tr '[:upper:]' '[:lower:]')"
  password="$(printf '%s' "$password" | tr '[:upper:]' '[:lower:]')"
  if [[ "$user" != "root" && "$user" != "admin" ]]; then
    return 1
  fi
  case "$password" in
    123456|admin|demo|mysql|pass|password|root|test)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

validate_mysql_credentials() {
  local user="$1"
  local password="$2"
  if [[ -z "$user" || -z "$password" ]]; then
    echo "mysql credentials required for --mode mysql (use --mysql-user/--mysql-password or MI_E2EE_MYSQL_USERNAME/MI_E2EE_MYSQL_PASSWORD)" >&2
    return 1
  fi
  if is_weak_mysql_credentials "$user" "$password"; then
    echo "weak mysql credentials are forbidden in server config" >&2
    return 1
  fi
  return 0
}

find_kt_keygen() {
  local candidate
  for candidate in \
    "$ROOT/tools/mi_e2ee_kt_keygen" \
    "$ROOT/mi_e2ee_kt_keygen" \
    "$ROOT/build/server/tools/mi_e2ee_kt_keygen" \
    "$ROOT/build/server/mi_e2ee_kt_keygen"; do
    if [[ -x "$candidate" ]]; then
      printf '%s' "$candidate"
      return 0
    fi
  done
  command -v mi_e2ee_kt_keygen || true
}

ensure_dirs() {
  mkdir -p "$CONFIG_DIR" "$ROOT/database/offline_store"
}

generate_kt_key() {
  local force="$1"
  ensure_dirs
  local sk="$CONFIG_DIR/kt_signing_key.bin"
  local pk="$CONFIG_DIR/kt_root_pub.bin"
  if [[ "$force" != "1" && -f "$sk" && -f "$pk" ]]; then
    echo "KT key already exists: $sk"
    return 0
  fi
  local keygen
  keygen="$(find_kt_keygen)"
  if [[ -n "$keygen" ]]; then
    if [[ "$force" == "1" ]]; then
      "$keygen" --out-dir "$CONFIG_DIR" --force
    else
      "$keygen" --out-dir "$CONFIG_DIR"
    fi
    chmod 600 "$sk" "$pk" 2>/dev/null || true
    return 0
  fi
  "$PYTHON_BIN" - "$sk" "$pk" <<'PY'
import os
import sys
from pathlib import Path
sk = Path(sys.argv[1])
pk = Path(sys.argv[2])
sk.write_bytes(os.urandom(4096))
pk.write_bytes(os.urandom(1952))
PY
  chmod 600 "$sk" "$pk" 2>/dev/null || true
  echo "WARNING: mi_e2ee_kt_keygen not found; wrote placeholder-format key material for local validation only." >&2
}

cert_fingerprint() {
  "$PYTHON_BIN" "$VERIFY_SCRIPT" --show-cert "$1" | awk -F= '/^sha256=/{print $2}'
}

show_cert() {
  local cert="${1:-$TLS_CERT_PATH}"
  if [[ ! -f "$cert" ]]; then
    echo "Certificate missing: $cert" >&2
    return 1
  fi
  echo "Certificate:"
  echo "$cert"
  "$PYTHON_BIN" "$VERIFY_SCRIPT" --show-cert "$cert"
}

generate_self_signed() {
  ensure_dirs
  local target="${1:-localhost}"
  local san="DNS:localhost"
  if [[ "$target" == dns:* ]]; then
    san="DNS:${target#dns:}"
  elif [[ "$target" == ip:* ]]; then
    san="IP:${target#ip:}"
  fi
  local key_tmp="$CONFIG_DIR/mi_e2ee_server.key"
  local crt_tmp="$CONFIG_DIR/mi_e2ee_server.crt"
  if command -v openssl >/dev/null 2>&1; then
    openssl req -x509 -newkey rsa:2048 -sha256 -nodes -days 3650 \
      -subj "/CN=MI_E2EE_Server" \
      -addext "subjectAltName=$san" \
      -keyout "$key_tmp" \
      -out "$crt_tmp" >/dev/null 2>&1
    cat "$key_tmp" "$crt_tmp" > "$TLS_CERT_PATH"
    rm -f "$key_tmp" "$crt_tmp"
  else
    "$PYTHON_BIN" - "$TLS_CERT_PATH" <<'PY'
import os
import sys
from pathlib import Path
Path(sys.argv[1]).write_bytes(b"MI_E2EE_DEV_CERT_PLACEHOLDER\n" + os.urandom(256))
PY
    echo "WARNING: openssl not found; wrote placeholder certificate for local validation only." >&2
  fi
  chmod 600 "$TLS_CERT_PATH" 2>/dev/null || true
  local fp
  fp="$(cert_fingerprint "$TLS_CERT_PATH")"
  printf '%s\n' "$fp" > "$CONFIG_DIR/server_fingerprint.txt"
  update_server_tls_cert "$TLS_CERT_REL"
  echo "sha256=$fp"
}

write_config() {
  local mode="$1"
  local output="$2"
  local port="$3"
  local offline_dir="$4"
  local rotation_threshold="$5"
  local tls_cert="$6"
  local mysql_host="$7"
  local mysql_port="$8"
  local mysql_db="$9"
  local mysql_user="${10}"
  local mysql_password="${11}"
  local kt_key="kt_signing_key.bin"
  if [[ "$(basename "$(dirname "$output")")" != "config" ]]; then
    kt_key="config/kt_signing_key.bin"
  fi
  if [[ "$mode" == "mysql" ]]; then
    validate_mysql_credentials "$mysql_user" "$mysql_password" || return 1
  fi

  mkdir -p "$(dirname "$output")"
  if [[ "$mode" == "mysql" ]]; then
    cat > "$output" <<EOF
[mode]
mode=0
[mysql]
mysql_ip=$mysql_host
mysql_port=$mysql_port
mysql_database=$mysql_db
mysql_username=$mysql_user
mysql_password=$mysql_password
[server]
list_port=$port
rotation_threshold=$rotation_threshold
offline_dir=$offline_dir
debug_log=0
offline_blob_temp_budget_bytes=4294967296
tls_enable=1
require_tls=1
tls_cert=$tls_cert
kt_signing_key=$kt_key
state_protection=none
ops_enable=0
ops_allow_remote=0
[kcp]
enable=0
allow_insecure=0
listen_port=0
EOF
  else
    cat > "$output" <<EOF
[mode]
mode=1
[server]
list_port=$port
rotation_threshold=$rotation_threshold
offline_dir=$offline_dir
debug_log=0
offline_blob_temp_budget_bytes=4294967296
tls_enable=1
require_tls=1
tls_cert=$tls_cert
kt_signing_key=$kt_key
state_protection=none
ops_enable=0
ops_allow_remote=0
[kcp]
enable=0
allow_insecure=0
listen_port=0
EOF
  fi
  chmod 600 "$output" 2>/dev/null || true
}

validate_config() {
  (cd "$ROOT" && "$PYTHON_BIN" "$VERIFY_SCRIPT" --config "$1" --privacy-strict)
}

update_server_tls_cert() {
  local rel_path="$1"
  if [[ ! -f "$CONFIG_PATH" ]]; then
    return 0
  fi
  "$PYTHON_BIN" - "$CONFIG_PATH" "$rel_path" <<'PY'
import configparser
import sys
from pathlib import Path

path = Path(sys.argv[1])
rel_path = sys.argv[2]
parser = configparser.ConfigParser()
parser.optionxform = str
parser.read(path, encoding="utf-8")
if not parser.has_section("server"):
    parser.add_section("server")
parser.set("server", "tls_enable", "1")
parser.set("server", "require_tls", "1")
parser.set("server", "tls_cert", rel_path)
with path.open("w", encoding="utf-8", newline="\n") as handle:
    parser.write(handle, space_around_delimiters=False)
PY
}

update_server_auth_mode() {
  local mode="$1"
  local mysql_host="$2"
  local mysql_port="$3"
  local mysql_db="$4"
  local mysql_user="$5"
  local mysql_password="$6"
  if [[ "$mode" == "mysql" ]]; then
    validate_mysql_credentials "$mysql_user" "$mysql_password" || return 1
  fi
  "$PYTHON_BIN" - "$CONFIG_PATH" "$mode" "$mysql_host" "$mysql_port" "$mysql_db" "$mysql_user" "$mysql_password" <<'PY'
import configparser
import sys
from pathlib import Path

path = Path(sys.argv[1])
mode, host, port, db, user, password = sys.argv[2:8]
parser = configparser.ConfigParser()
parser.optionxform = str
parser.read(path, encoding="utf-8")
if not parser.has_section("mode"):
    parser.add_section("mode")
parser.set("mode", "mode", "0" if mode == "mysql" else "1")
if mode == "mysql":
    if not parser.has_section("mysql"):
        parser.add_section("mysql")
    parser.set("mysql", "mysql_ip", host)
    parser.set("mysql", "mysql_port", port)
    parser.set("mysql", "mysql_database", db)
    parser.set("mysql", "mysql_username", user)
    parser.set("mysql", "mysql_password", password)
elif parser.has_section("mysql"):
    parser.remove_section("mysql")
with path.open("w", encoding="utf-8", newline="\n") as handle:
    parser.write(handle, space_around_delimiters=False)
PY
}

update_server_ops() {
  local enabled="$1"
  local allow_remote="$2"
  "$PYTHON_BIN" - "$CONFIG_PATH" "$enabled" "$allow_remote" <<'PY'
import configparser
import secrets
import sys
from pathlib import Path

path = Path(sys.argv[1])
enabled, allow_remote = sys.argv[2:4]
parser = configparser.ConfigParser()
parser.optionxform = str
parser.read(path, encoding="utf-8")
if not parser.has_section("server"):
    parser.add_section("server")
parser.set("server", "ops_enable", enabled)
parser.set("server", "ops_allow_remote", allow_remote)
if enabled == "1":
    current = parser.get("server", "ops_token", fallback="")
    if len(current) < 16 or current == "change_me_to_a_random_secret":
        parser.set("server", "ops_token", secrets.token_hex(24))
else:
    parser.remove_option("server", "ops_token")
with path.open("w", encoding="utf-8", newline="\n") as handle:
    parser.write(handle, space_around_delimiters=False)
PY
}

update_client_pin_prompt() {
  local fp="$1"
  echo "Update client_config.ini pinned_fingerprint?"
  echo "1) Yes"
  echo "2) No, only print fingerprint"
  read -r -p "Select [1-2]: " choice
  if [[ "${choice:-2}" == "1" ]]; then
    local client_config
    client_config="$(prompt_default "Client config path" "$CLIENT_CONFIG_DEFAULT")"
    "$PYTHON_BIN" "$VERIFY_SCRIPT" --client-config "$client_config" --rotate-client-pin "$fp"
  fi
}

first_time_setup() {
  ensure_dirs
  echo "Auth mode:"
  echo "1) MySQL mode"
  echo "2) Demo mode (test only)"
  read -r -p "Select [1-2]: " auth_choice
  local mode="mysql" mysql_host="127.0.0.1" mysql_port="3306" mysql_db="mi_e2ee" mysql_user="" mysql_password=""
  if [[ "${auth_choice:-1}" != "2" ]]; then
    mysql_host="$(prompt_default "MySQL host" "$mysql_host")"
    mysql_port="$(prompt_default "MySQL port" "$mysql_port")"
    mysql_db="$(prompt_default "MySQL database" "$mysql_db")"
    read -r -p "MySQL username: " mysql_user
    mysql_password="$(prompt_secret_confirm "MySQL password")"
  else
    mode="demo"
    local demo_user demo_pass
    demo_user="$(prompt_default "Demo username" "alice")"
    demo_pass="$(prompt_secret_confirm "Demo password")"
    printf '%s:%s\n' "$demo_user" "$demo_pass" > "$ROOT/test_user.txt"
    chmod 600 "$ROOT/test_user.txt" 2>/dev/null || true
  fi
  local port offline_dir ops_choice
  port="$(prompt_default "Server listen port" "9000")"
  offline_dir="$(prompt_default "Offline store directory" "$OFFLINE_DIR")"
  echo "Enable ops health endpoint?"
  echo "1) No"
  echo "2) Yes, loopback only"
  read -r -p "Select [1-2]: " ops_choice
  echo "TLS certificate:"
  echo "1) Generate self-signed certificate and use pin"
  echo "2) Import existing certificate"
  read -r -p "Select [1-2]: " tls_choice
  if [[ "${tls_choice:-1}" == "2" ]]; then
    import_certificate
  else
    generate_self_signed "localhost"
  fi
  generate_kt_key 0
  write_config "$mode" "$CONFIG_PATH" "$port" "$offline_dir" "10000" "$TLS_CERT_REL" \
    "$mysql_host" "$mysql_port" "$mysql_db" "$mysql_user" "$mysql_password"
  if [[ "${ops_choice:-1}" == "2" ]]; then
    update_server_ops "1" "0"
  fi
  validate_config "$CONFIG_PATH"
}

reconfigure_server() {
  if [[ ! -f "$CONFIG_PATH" ]]; then
    echo "No config found; running first-time setup."
    first_time_setup
    return
  fi
  echo "Existing config found: config/config.ini"
  echo "1) Change auth mode"
  echo "2) Change listen port"
  echo "3) Change offline directory"
  echo "4) Change TLS certificate"
  echo "5) Change ops health"
  echo "6) Reset to first-time setup"
  echo "7) Back"
  read -r -p "Select [1-7]: " choice
  cp "$CONFIG_PATH" "$CONFIG_PATH.bak.$(date +%Y%m%d%H%M%S)"
  case "${choice:-7}" in
    1)
      echo "Auth mode:"
      echo "1) MySQL mode"
      echo "2) Demo mode (test only)"
      read -r -p "Select [1-2]: " auth_choice
      if [[ "${auth_choice:-1}" != "2" ]]; then
        local mysql_host="127.0.0.1" mysql_port="3306" mysql_db="mi_e2ee" mysql_user mysql_password
        mysql_host="$(prompt_default "MySQL host" "$mysql_host")"
        mysql_port="$(prompt_default "MySQL port" "$mysql_port")"
        mysql_db="$(prompt_default "MySQL database" "$mysql_db")"
        read -r -p "MySQL username: " mysql_user
        mysql_password="$(prompt_secret_confirm "MySQL password")"
        update_server_auth_mode "mysql" "$mysql_host" "$mysql_port" "$mysql_db" "$mysql_user" "$mysql_password"
      else
        local demo_user demo_pass
        demo_user="$(prompt_default "Demo username" "alice")"
        demo_pass="$(prompt_secret_confirm "Demo password")"
        printf '%s:%s\n' "$demo_user" "$demo_pass" > "$ROOT/test_user.txt"
        chmod 600 "$ROOT/test_user.txt" 2>/dev/null || true
        update_server_auth_mode "demo" "" "" "" "" ""
      fi
      ;;
    6) first_time_setup ;;
    2)
      local port
      port="$(prompt_default "Server listen port" "9000")"
      "$PYTHON_BIN" - "$CONFIG_PATH" "$port" <<'PY'
import configparser, sys
p, port = sys.argv[1], sys.argv[2]
c = configparser.ConfigParser(); c.read(p)
c.set("server", "list_port", port)
with open(p, "w", encoding="utf-8", newline="\n") as f: c.write(f, space_around_delimiters=False)
PY
      ;;
    3)
      local dir
      dir="$(prompt_default "Offline store directory" "$OFFLINE_DIR")"
      "$PYTHON_BIN" - "$CONFIG_PATH" "$dir" <<'PY'
import configparser, sys
p, d = sys.argv[1], sys.argv[2]
c = configparser.ConfigParser(); c.read(p)
c.set("server", "offline_dir", d)
with open(p, "w", encoding="utf-8", newline="\n") as f: c.write(f, space_around_delimiters=False)
PY
      ;;
    4)
      echo "TLS certificate:"
      echo "1) Generate self-signed certificate"
      echo "2) Import existing certificate"
      read -r -p "Select [1-2]: " tls_choice
      if [[ "${tls_choice:-1}" == "2" ]]; then
        import_certificate
      else
        generate_self_signed_menu
      fi
      ;;
    5)
      echo "Ops health endpoint:"
      echo "1) Disable"
      echo "2) Enable, loopback only"
      echo "3) Enable, allow remote"
      read -r -p "Select [1-3]: " ops_choice
      case "${ops_choice:-1}" in
        2) update_server_ops "1" "0" ;;
        3) update_server_ops "1" "1" ;;
        *) update_server_ops "0" "0" ;;
      esac
      ;;
    7) return ;;
  esac
  validate_config "$CONFIG_PATH"
}

generate_self_signed_menu() {
  echo "Self-signed certificate target:"
  echo "1) localhost / 127.0.0.1"
  echo "2) Custom DNS name"
  echo "3) Custom IP address"
  read -r -p "Select [1-3]: " choice
  local target="localhost"
  if [[ "$choice" == "2" ]]; then
    read -r -p "DNS name: " dns
    target="dns:$dns"
  elif [[ "$choice" == "3" ]]; then
    read -r -p "IP address: " ip
    target="ip:$ip"
  fi
  local fp
  fp="$(generate_self_signed "$target" | awk -F= '/^sha256=/{print $2}')"
  show_cert "$TLS_CERT_PATH"
  update_client_pin_prompt "$fp"
}

import_certificate() {
  ensure_dirs
  echo "Certificate format:"
  echo "1) PEM fullchain + private key"
  echo "2) Combined PEM"
  echo "3) PFX / P12"
  read -r -p "Select [1-3]: " fmt
  if [[ "${fmt:-2}" == "1" ]]; then
    read -r -p "Certificate/fullchain path: " cert
    read -r -p "Private key path: " key
    cat "$key" "$cert" > "$TLS_CERT_PATH"
    chmod 600 "$TLS_CERT_PATH" 2>/dev/null || true
  elif [[ "$fmt" == "3" ]]; then
    read -r -p "PFX/P12 path: " pfx
    read -r -s -p "PFX password: " _pfx_password
    printf '\n'
    cp "$pfx" "$CONFIG_DIR/mi_e2ee_server.pfx"
    TLS_CERT_REL="config/mi_e2ee_server.pfx"
    TLS_CERT_PATH="$CONFIG_DIR/mi_e2ee_server.pfx"
    chmod 600 "$TLS_CERT_PATH" 2>/dev/null || true
  else
    read -r -p "Combined PEM path: " pem
    cp "$pem" "$TLS_CERT_PATH"
    chmod 600 "$TLS_CERT_PATH" 2>/dev/null || true
  fi
  update_server_tls_cert "$TLS_CERT_REL"
  show_cert "$TLS_CERT_PATH" || true
  echo "Client TLS verification mode:"
  echo "1) Pin only"
  echo "2) CA only"
  echo "3) Hybrid CA + pin"
  echo "4) Skip client update"
  read -r -p "Select [1-4]: " client_mode
  if [[ "${client_mode:-4}" == "4" ]]; then
    return
  fi
  local client_config ca_bundle mode_arg
  client_config="$(prompt_default "Client config path" "$CLIENT_CONFIG_DEFAULT")"
  case "$client_mode" in
    1)
      "$PYTHON_BIN" "$VERIFY_SCRIPT" --client-config "$client_config" --set-client-tls pin --cert "$TLS_CERT_PATH"
      ;;
    2)
      mode_arg="ca"
      ca_bundle="$(prompt_default "CA bundle path" "$TLS_CERT_PATH")"
      "$PYTHON_BIN" "$VERIFY_SCRIPT" --client-config "$client_config" --set-client-tls "$mode_arg" --tls-ca-bundle-path "$ca_bundle"
      ;;
    3)
      mode_arg="hybrid"
      ca_bundle="$(prompt_default "CA bundle path" "$TLS_CERT_PATH")"
      "$PYTHON_BIN" "$VERIFY_SCRIPT" --client-config "$client_config" --set-client-tls "$mode_arg" --cert "$TLS_CERT_PATH" --tls-ca-bundle-path "$ca_bundle"
      ;;
  esac
}

rotate_client_pin() {
  local server_config client_config cert fp
  server_config="$(prompt_default "Server config path" "$CONFIG_PATH")"
  client_config="$(prompt_default "Client config path" "$CLIENT_CONFIG_DEFAULT")"
  cert="$(awk -F= '/^[[:space:]]*tls_cert[[:space:]]*=/{gsub(/[[:space:]]/,"",$2); print $2; exit}' "$server_config")"
  if [[ "$cert" != /* ]]; then
    cert="$ROOT/$cert"
  fi
  fp="$(cert_fingerprint "$cert")"
  echo "Current certificate fingerprint:"
  echo "$fp"
  echo "1) Update client pin"
  echo "2) Print only"
  echo "3) Back"
  read -r -p "Select [1-3]: " choice
  if [[ "${choice:-2}" == "1" ]]; then
    "$PYTHON_BIN" "$VERIFY_SCRIPT" --client-config "$client_config" --rotate-client-pin "$fp"
  fi
}

kt_menu() {
  echo "KT key action:"
  echo "1) Generate if missing"
  echo "2) Rotate existing key"
  echo "3) Back"
  read -r -p "Select [1-3]: " choice
  case "${choice:-1}" in
    1) generate_kt_key 0 ;;
    2)
      read -r -p "Type OVERWRITE to continue: " confirm
      [[ "$confirm" == "OVERWRITE" ]] && generate_kt_key 1
      ;;
    3) return ;;
  esac
  echo "Copy kt_root_pub.bin to client config directory?"
  echo "1) Yes"
  echo "2) No"
  read -r -p "Select [1-2]: " copy_choice
  if [[ "${copy_choice:-2}" == "1" ]]; then
    local client_dir
    client_dir="$(prompt_default "Client config directory" "$ROOT/../mi_e2ee_client/config")"
    mkdir -p "$client_dir"
    cp "$CONFIG_DIR/kt_root_pub.bin" "$client_dir/"
  fi
}

post_action() {
  echo
  echo "Done."
  echo
  echo "1) Return to main menu"
  echo "2) Exit"
  read -r -p "Select [1-2]: " choice
  [[ "${choice:-1}" == "2" ]] && exit 0
}

mode="mysql"
output="$CONFIG_PATH"
port="9000"
offline_dir="$OFFLINE_DIR"
rotation_threshold="10000"
mysql_host="127.0.0.1"
mysql_port="3306"
mysql_db="mi_e2ee"
mysql_user="${MI_E2EE_MYSQL_USERNAME:-}"
mysql_password="${MI_E2EE_MYSQL_PASSWORD:-}"
non_interactive=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --print-menu) print_menu; exit 0 ;;
    --help|-h) usage; exit 0 ;;
    --non-interactive) non_interactive=1; shift ;;
    --mode) mode="$2"; shift 2 ;;
    --output) output="$2"; shift 2 ;;
    --port) port="$2"; shift 2 ;;
    --offline-dir) offline_dir="$2"; shift 2 ;;
    --rotation-threshold) rotation_threshold="$2"; shift 2 ;;
    --mysql-host) mysql_host="$2"; shift 2 ;;
    --mysql-port) mysql_port="$2"; shift 2 ;;
    --mysql-db) mysql_db="$2"; shift 2 ;;
    --mysql-user) mysql_user="$2"; shift 2 ;;
    --mysql-password) mysql_password="$2"; shift 2 ;;
    *) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [[ "$non_interactive" == "1" ]]; then
  if [[ "$mode" == "mysql" ]]; then
    validate_mysql_credentials "$mysql_user" "$mysql_password" || exit 1
  fi
  mkdir -p "$(dirname "$output")"
  output_dir="$(cd "$(dirname "$output")" && pwd)"
  if [[ "$(basename "$output_dir")" == "config" ]]; then
    ROOT="$(cd "$output_dir/.." && pwd)"
  else
    ROOT="$output_dir"
  fi
  CONFIG_DIR="$ROOT/config"
  CONFIG_PATH="$CONFIG_DIR/config.ini"
  TLS_CERT_REL="config/mi_e2ee_server.pem"
  TLS_CERT_PATH="$ROOT/$TLS_CERT_REL"
  ensure_dirs
  generate_self_signed "localhost" >/dev/null
  generate_kt_key 0
  write_config "$mode" "$output" "$port" "$offline_dir" "$rotation_threshold" "$TLS_CERT_REL" \
    "$mysql_host" "$mysql_port" "$mysql_db" "$mysql_user" "$mysql_password"
  validate_config "$output"
  exit 0
fi

while true; do
  print_menu
  read -r choice
  case "${choice:-9}" in
    1) first_time_setup; post_action ;;
    2) reconfigure_server; post_action ;;
    3) generate_self_signed_menu; post_action ;;
    4) import_certificate; post_action ;;
    5) rotate_client_pin; post_action ;;
    6) kt_menu; post_action ;;
    7) validate_config "$CONFIG_PATH"; post_action ;;
    8) show_cert "$TLS_CERT_PATH"; post_action ;;
    9|q|Q) exit 0 ;;
    *) echo "Invalid selection" >&2 ;;
  esac
done
