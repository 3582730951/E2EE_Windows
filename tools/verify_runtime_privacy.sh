#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: verify_runtime_privacy.sh --root PATH [--root PATH ...]

Scans post-e2e runtime directories for persistent logs, diagnostics, crash
artifacts, telemetry/metrics/audit outputs, plaintext keys/tokens/payloads, and
local user paths.
EOF
}

roots=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --root)
      roots+=("${2:-}")
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ "${#roots[@]}" -eq 0 ]]; then
  echo "at least one --root is required" >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

scan_names() {
  local root="$1"
  local hit
  hit="$(
    find "$root" -print 2>/dev/null | while IFS= read -r p; do
      base="$(basename "$p" | tr '[:upper:]' '[:lower:]')"
      case "$base" in
        *.log|*.log.*|*.dmp|*.dump|core|core.*|*crash*|*telemetry*|*diagnostic*|*diagnostics*|*metrics*|*audit*)
          printf '%s\n' "$p"
          break
          ;;
      esac
    done
  )"
  if [[ -n "$hit" ]]; then
    echo "runtime contains forbidden privacy artifact: $hit" >&2
    exit 1
  fi
}

scan_content() {
  local root="$1"
  python3 "$script_dir/privacy_scan_text.py" \
    --mode runtime --root "$root" --label runtime
}

for root in "${roots[@]}"; do
  if [[ -z "$root" || ! -d "$root" ]]; then
    echo "runtime root missing: $root" >&2
    exit 1
  fi
  scan_names "$root"
  scan_content "$root"
done
