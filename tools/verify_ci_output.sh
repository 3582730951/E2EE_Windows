#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: verify_ci_output.sh [--input PATH ...]

Reads stdin when --input is omitted. Fails if CI output contains high-confidence
plaintext secret, token, key, payload, or local path markers.
EOF
}

inputs=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --input)
      inputs+=("${2:-}")
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

scan_stream() {
  local label="$1"
  local hit
  hit="$(
    grep -n -E -i \
      '(^|[^[:alnum:]_])(payload_hex|file_key|message_plaintext|plaintext_payload|local_path|qr_secret_hex|mysql_password|password|token|access_token|refresh_token)[[:space:]]*[:=][[:space:]]*[^[:space:]]+' \
      2>/dev/null | head -n 1 || true
  )"
  if [[ -n "$hit" ]]; then
    echo "CI output contains forbidden secret marker in $label: $hit" >&2
    exit 1
  fi
}

scan_pat_stream() {
  local label="$1"
  local hit
  hit="$(
    grep -n -E \
      '(ghp_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{20,}|glpat-[A-Za-z0-9_-]{20,}|sk-[A-Za-z0-9_-]{20,}|Bearer[[:space:]]+[A-Za-z0-9._~+/=-]{24,})' \
      2>/dev/null | head -n 1 || true
  )"
  if [[ -n "$hit" ]]; then
    echo "CI output contains forbidden credential pattern in $label: $hit" >&2
    exit 1
  fi
}

scan_path_stream() {
  local label="$1"
  local hit
  hit="$(
    grep -n -E \
      '(^|[^[:alnum:]_])/(home|Users)/[^[:space:]/]+/|[A-Za-z]:\\Users\\[^\\[:space:]]+\\' \
      2>/dev/null | head -n 1 || true
  )"
  if [[ -n "$hit" ]]; then
    echo "CI output contains forbidden local path in $label: $hit" >&2
    exit 1
  fi
}

scan_file() {
  local path="$1"
  if [[ -z "$path" || ! -f "$path" ]]; then
    echo "input log missing: $path" >&2
    exit 1
  fi
  scan_stream "$path" < "$path"
  scan_pat_stream "$path" < "$path"
  scan_path_stream "$path" < "$path"
}

if [[ "${#inputs[@]}" -eq 0 ]]; then
  tmp="$(mktemp)"
  trap 'rm -f "$tmp"' EXIT
  cat > "$tmp"
  scan_file "$tmp"
else
  for input in "${inputs[@]}"; do
    scan_file "$input"
  done
fi
