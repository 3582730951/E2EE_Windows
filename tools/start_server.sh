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
CONFIGURE_SCRIPT="$SCRIPT_DIR/configure_server.sh"
STRESS_SCRIPT="$SCRIPT_DIR/stress_server.py"
if [[ ! -f "$VERIFY_SCRIPT" && -f "$ROOT/tools/verify_server_config.py" ]]; then
  VERIFY_SCRIPT="$ROOT/tools/verify_server_config.py"
  CONFIGURE_SCRIPT="$ROOT/tools/configure_server.sh"
  STRESS_SCRIPT="$ROOT/tools/stress_server.py"
fi

print_menu() {
  cat <<'MENU'
MI E2EE Server Launcher

1) Start server
2) Configure then start
3) Validate configuration
4) Stress test server
5) Exit

Select [1-5]:
MENU
}

usage() {
  print_menu
  cat <<'USAGE'

Non-interactive:
  start_server.sh --non-interactive --action verify --config config/config.ini
  start_server.sh --non-interactive --action stress --host 127.0.0.1 --port 9000 --dry-run
  start_server.sh --print-menu
USAGE
}

find_server_exe() {
  local configured="${MI_E2EE_SERVER_EXE:-}"
  if [[ -n "$configured" ]]; then
    printf '%s' "$configured"
    return
  fi
  for candidate in "$ROOT/mi_e2ee_server" "$ROOT/mi_e2ee_server_app" "$ROOT/build/server/mi_e2ee_server"; do
    if [[ -x "$candidate" ]]; then
      printf '%s' "$candidate"
      return
    fi
  done
  printf '%s' "mi_e2ee_server"
}

run_start() {
  local config="$1"
  if [[ ! -f "$config" ]]; then
    echo "No config/config.ini found."
    echo "1) Run first-time setup now"
    echo "2) Exit"
    read -r -p "Select [1-2]: " setup_choice
    if [[ "${setup_choice:-2}" == "1" ]]; then
      "$CONFIGURE_SCRIPT"
    else
      return 1
    fi
  fi
  local server_exe
  server_exe="$(find_server_exe)"
  exec "$server_exe" "$config"
}

run_action() {
  local action="$1"
  local config="$2"
  local host="$3"
  local port="$4"
  local dry_run="$5"
  local output_dir="$6"
  case "$action" in
    verify)
      (cd "$ROOT" && "$PYTHON_BIN" "$VERIFY_SCRIPT" --config "$config" --privacy-strict)
      ;;
    start)
      run_start "$config"
      ;;
    stress)
      args=("$PYTHON_BIN" "$STRESS_SCRIPT" --host "$host" --port "$port" --output-dir "$output_dir")
      if [[ "$dry_run" == "1" ]]; then
        args+=(--dry-run)
      fi
      "${args[@]}"
      ;;
    configure)
      "$CONFIGURE_SCRIPT"
      run_start "$config"
      ;;
    *) echo "unknown action: $action" >&2; return 2 ;;
  esac
}

action="verify"
config="$ROOT/config/config.ini"
host="127.0.0.1"
port="9000"
dry_run=0
output_dir="stress_results"
non_interactive=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --print-menu) print_menu; exit 0 ;;
    --help|-h) usage; exit 0 ;;
    --non-interactive) non_interactive=1; shift ;;
    --action) action="$2"; shift 2 ;;
    --config) config="$2"; shift 2 ;;
    --host) host="$2"; shift 2 ;;
    --port) port="$2"; shift 2 ;;
    --output-dir) output_dir="$2"; shift 2 ;;
    --dry-run) dry_run=1; shift ;;
    *) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [[ "$non_interactive" == "1" ]]; then
  run_action "$action" "$config" "$host" "$port" "$dry_run" "$output_dir"
  exit $?
fi

while true; do
  print_menu
  read -r choice
  case "${choice:-5}" in
    1) run_action start "$config" "$host" "$port" "$dry_run" "$output_dir" ;;
    2) run_action configure "$config" "$host" "$port" "$dry_run" "$output_dir" ;;
    3) run_action verify "$config" "$host" "$port" "$dry_run" "$output_dir" ;;
    4) run_action stress "$config" "$host" "$port" "$dry_run" "$output_dir" ;;
    5|q|Q) exit 0 ;;
    *) echo "Invalid selection" >&2 ;;
  esac
done
