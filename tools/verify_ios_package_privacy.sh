#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: verify_ios_package_privacy.sh [--root PATH ...] [--ipa PATH ...]

Scans expanded iOS app/IPA artifacts for privacy-forbidden logs, telemetry/crash
SDKs, plaintext secret markers, local user paths, App Transport Security gaps,
file-sharing exposure, and missing NSFileProtectionComplete defaults.
EOF
}

roots=()
ipas=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --root)
      roots+=("${2:-}")
      shift 2
      ;;
    --ipa)
      ipas+=("${2:-}")
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

if [[ "${#roots[@]}" -eq 0 && "${#ipas[@]}" -eq 0 ]]; then
  echo "at least one --root or --ipa is required" >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

scan_names() {
  local root="$1"
  local label="$2"
  local hit
  hit="$(
    find "$root" -print 2>/dev/null | while IFS= read -r path; do
      base="$(basename "$path" | tr '[:upper:]' '[:lower:]')"
      if privacy_name_matches "$base" \
        '*.log' '*.log.*' '*.dmp' '*.dump' '*.crash' '*.ips' '*.dsym' \
        'crash' 'crashes' 'telemetry' 'diagnostic' 'diagnostics' \
        'metrics' 'metric' 'audit' 'audits' 'ops_health' \
        '*crashlytics*' '*firebase*' '*firebase-analytics*' \
        '*telemetry*' '*diagnostic*' '*diagnostics*' '*metrics*' \
        '*ops_health*' '*audit*' '*sentry*' '*bugsnag*' '*datadog*' \
        '*newrelic*' '*appcenter*'; then
        printf '%s\n' "$path"
        break
      fi
    done
  )"
  if [[ -n "$hit" ]]; then
    echo "$label contains forbidden iOS privacy artifact: $hit" >&2
    exit 1
  fi
}

privacy_name_matches() {
  local base="$1"
  shift
  local pattern
  for pattern in "$@"; do
    if [[ "$base" == $pattern ]]; then
      return 0
    fi
  done
  return 1
}

scan_secret_content() {
  local root="$1"
  local label="$2"
  python3 "$script_dir/privacy_scan_text.py" \
    --mode package --root "$root" --label "$label" \
    --skip-ext .png --skip-ext .jpg --skip-ext .jpeg --skip-ext .webp \
    --skip-ext .gif --skip-ext .heic --skip-ext .car
}

scan_telemetry_content() {
  local root="$1"
  local label="$2"
  local hit
  hit="$(
    grep -a -R -n -E -i \
      'firebase|firebaseanalytics|firebase_analytics|firebase-analytics|crashlytics|io[./]sentry|sentrysdk|bugsnag|datadog|newrelic|appcenter|msappcenter|telemetry|analytics' \
      "$root" 2>/dev/null | head -n 1 || true
  )"
  if [[ -n "$hit" ]]; then
    echo "$label contains forbidden iOS telemetry/crash SDK marker: $hit" >&2
    exit 1
  fi
}

plist_flat_text() {
  local plist="$1"
  if [[ ! -f "$plist" ]]; then
    return 1
  fi
  if grep -q '<plist' "$plist" 2>/dev/null; then
    tr '\n' ' ' <"$plist"
    return 0
  fi
  if command -v plutil >/dev/null 2>&1; then
    plutil -convert xml1 -o - "$plist" 2>/dev/null | tr '\n' ' ' && return 0
  fi
  strings "$plist" 2>/dev/null | tr '\n' ' '
}

plist_true_for_key() {
  local flat="$1"
  local key="$2"
  echo "$flat" | grep -Eq "<key>${key}</key>[[:space:]]*<true[ /]*>"
}

require_info_plist_policy() {
  local app_root="$1"
  local label="$2"
  local plist="$app_root/Info.plist"
  if [[ ! -f "$plist" ]]; then
    echo "$label missing Info.plist" >&2
    exit 1
  fi
  local flat
  flat="$(plist_flat_text "$plist")"
  if ! echo "$flat" | grep -q 'NSFileProtectionKey'; then
    echo "$label Info.plist missing NSFileProtectionKey" >&2
    exit 1
  fi
  if ! echo "$flat" | grep -q 'NSFileProtectionComplete'; then
    echo "$label Info.plist missing NSFileProtectionComplete" >&2
    exit 1
  fi
  if plist_true_for_key "$flat" "NSAllowsArbitraryLoads"; then
    echo "$label Info.plist allows arbitrary network loads" >&2
    exit 1
  fi
  if plist_true_for_key "$flat" "UIFileSharingEnabled"; then
    echo "$label Info.plist enables user file sharing" >&2
    exit 1
  fi
  if plist_true_for_key "$flat" "LSSupportsOpeningDocumentsInPlace"; then
    echo "$label Info.plist enables in-place document exposure" >&2
    exit 1
  fi
}

scan_app_root() {
  local app_root="$1"
  local label="$2"
  if [[ -z "$app_root" || ! -d "$app_root" ]]; then
    echo "$label app root missing: $app_root" >&2
    exit 1
  fi
  require_info_plist_policy "$app_root" "$label"
}

scan_expanded_root() {
  local root="$1"
  local label="$2"
  if [[ -z "$root" || ! -d "$root" ]]; then
    echo "$label root missing: $root" >&2
    exit 1
  fi
  scan_names "$root" "$label"
  scan_secret_content "$root" "$label"
  scan_telemetry_content "$root" "$label"

  local app_count=0
  if [[ -f "$root/Info.plist" && "$root" == *.app ]]; then
    scan_app_root "$root" "$label"
    app_count=1
  fi
  while IFS= read -r app; do
    scan_app_root "$app" "$label"
    app_count=$((app_count + 1))
  done < <(find "$root/Payload" -maxdepth 1 -type d -name '*.app' 2>/dev/null || true)
  if [[ "$app_count" -eq 0 ]]; then
    while IFS= read -r app; do
      scan_app_root "$app" "$label"
      app_count=$((app_count + 1))
    done < <(find "$root" -maxdepth 3 -type d -name '*.app' 2>/dev/null || true)
  fi
  if [[ "$app_count" -eq 0 ]]; then
    echo "$label contains no iOS .app bundle" >&2
    exit 1
  fi
}

scan_ipa() {
  local ipa="$1"
  if [[ -z "$ipa" || ! -f "$ipa" ]]; then
    echo "IPA missing: $ipa" >&2
    exit 1
  fi
  if ! command -v unzip >/dev/null 2>&1; then
    echo "unzip is required for IPA privacy scanning" >&2
    exit 1
  fi
  local tmp
  tmp="$(mktemp -d)"
  unzip -qq "$ipa" -d "$tmp"
  scan_expanded_root "$tmp" "$ipa"
  rm -rf "$tmp"
}

for root in "${roots[@]}"; do
  scan_expanded_root "$root" "$root"
done

for ipa in "${ipas[@]}"; do
  scan_ipa "$ipa"
done
