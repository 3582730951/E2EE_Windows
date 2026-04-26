#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: verify_android_package_privacy.sh [--root PATH ...] [--apk PATH ...]

Scans Android package artifacts for privacy-forbidden logs, telemetry/crash SDKs,
plaintext secret markers, local user paths, cleartext traffic, and backup/data
extraction policy gaps.
EOF
}

roots=()
apks=()
root_count=0
apk_count=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --root)
      roots+=("${2:-}")
      root_count=$((root_count + 1))
      shift 2
      ;;
    --apk)
      apks+=("${2:-}")
      apk_count=$((apk_count + 1))
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

if [[ "$root_count" -eq 0 && "$apk_count" -eq 0 ]]; then
  echo "at least one --root or --apk is required" >&2
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
        '*.log' '*.log.*' '*.dmp' '*.dump' \
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
    echo "$label contains forbidden Android privacy artifact: $hit" >&2
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
    --skip-ext .dex --skip-ext .so --skip-ext .arsc \
    --skip-ext .png --skip-ext .jpg --skip-ext .jpeg --skip-ext .webp \
    --skip-name isolate_snapshot_data --skip-name vm_snapshot_data \
    --skip-name kernel_blob.bin
}

scan_telemetry_content() {
  local root="$1"
  local label="$2"
  local hit
  hit="$(
    grep -a -R -n -E -i \
      'com[./]google[./]firebase|firebase-crashlytics|firebase_analytics|firebase-analytics|crashlytics|io[./]sentry|com[./]bugsnag|com[./]datadog|com[./]newrelic|com[./]microsoft[./]appcenter|appcenter-crashes|com[./]google[./]android[./]gms[./]measurement' \
      "$root" 2>/dev/null | head -n 1 || true
  )"
  if [[ -n "$hit" ]]; then
    echo "$label contains forbidden Android telemetry/crash SDK marker: $hit" >&2
    exit 1
  fi
}

require_manifest_text_policy() {
  local manifest="$1"
  local label="$2"
  if [[ ! -f "$manifest" ]]; then
    echo "$label missing AndroidManifest.xml" >&2
    exit 1
  fi
  for needle in \
    'android:allowBackup="false"' \
    'android:fullBackupContent="@xml/no_backup_rules"' \
    'android:dataExtractionRules="@xml/no_data_extraction"' \
    'android:usesCleartextTraffic="false"'; do
    if ! grep -qF "$needle" "$manifest"; then
      echo "$label manifest missing privacy policy: $needle" >&2
      exit 1
    fi
  done
}

require_rule_text_policy() {
  local path="$1"
  local label="$2"
  if [[ ! -f "$path" ]]; then
    echo "$label missing rules file: $path" >&2
    exit 1
  fi
  for needle in 'domain="file"' 'domain="database"' 'domain="sharedpref"' 'domain="external"' 'path="."'; do
    if ! grep -qF "$needle" "$path"; then
      echo "$label rules file missing: $needle in $path" >&2
      exit 1
    fi
  done
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
  if [[ -f "$root/AndroidManifest.xml" ]] &&
     grep -q 'android:' "$root/AndroidManifest.xml" 2>/dev/null; then
    require_manifest_text_policy "$root/AndroidManifest.xml" "$label"
    require_rule_text_policy "$root/res/xml/no_backup_rules.xml" "$label"
    require_rule_text_policy "$root/res/xml/no_data_extraction.xml" "$label"
  fi
}

find_aapt() {
  if command -v aapt >/dev/null 2>&1; then
    command -v aapt
    return 0
  fi
  find /usr/lib/android-sdk/build-tools -maxdepth 2 -type f -name aapt 2>/dev/null |
    sort -r | head -n 1
}

require_xmltree_attr() {
  local dump="$1"
  local attr="$2"
  local label="$3"
  if ! grep -q "$attr" "$dump"; then
    echo "$label APK manifest missing attribute: $attr" >&2
    exit 1
  fi
}

require_xmltree_false() {
  local dump="$1"
  local attr="$2"
  local label="$3"
  if ! grep "$attr" "$dump" | grep -Eq '0x0|false'; then
    echo "$label APK manifest must set $attr=false" >&2
    exit 1
  fi
}

require_xmltree_rules() {
  local dump="$1"
  local path="$2"
  local label="$3"
  for needle in 'file' 'database' 'sharedpref' 'external'; do
    if ! grep -q "$needle" "$dump"; then
      echo "$label APK rules missing domain '$needle' in $path" >&2
      exit 1
    fi
  done
  if ! grep -q 'path="."' "$dump"; then
    echo "$label APK rules missing path='.' exclusions in $path" >&2
    exit 1
  fi
}

find_apk_xml_resource_path() {
  local apk="$1"
  local resource_name="$2"
  local label="$3"
  local dump
  dump="$(mktemp)"
  if command -v aapt2 >/dev/null 2>&1; then
    aapt2 dump resources "$apk" >"$dump"
  else
    local aapt_bin
    aapt_bin="$(find_aapt || true)"
    if [[ -z "$aapt_bin" || ! -x "$aapt_bin" ]]; then
      echo "aapt or aapt2 is required for APK resource privacy verification" >&2
      rm -f "$dump"
      exit 1
    fi
    "$aapt_bin" dump resources "$apk" >"$dump"
  fi

  local path
  path="$(
    awk -v resource="$resource_name" '
      $0 ~ "resource .* " resource "$" {
        found = 1
        next
      }
      found && $0 ~ /\(\) \(file\)/ {
        for (i = 1; i <= NF; i++) {
          if ($i ~ /^res\/.*\.xml$/) {
            print $i
            exit
          }
        }
      }
      found && $0 ~ /^    resource / {
        found = 0
      }
    ' "$dump"
  )"
  rm -f "$dump"
  if [[ -z "$path" ]]; then
    echo "$label APK resource path not found for $resource_name" >&2
    exit 1
  fi
  printf '%s\n' "$path"
}

scan_apk_manifest() {
  local apk="$1"
  local label="$2"
  local aapt_bin
  aapt_bin="$(find_aapt || true)"
  if [[ -z "$aapt_bin" || ! -x "$aapt_bin" ]]; then
    echo "aapt is required for APK manifest privacy verification" >&2
    exit 1
  fi

  manifest_dump="$(mktemp)"
  rules_backup_dump="$(mktemp)"
  rules_extract_dump="$(mktemp)"

  "$aapt_bin" dump xmltree "$apk" AndroidManifest.xml >"$manifest_dump"
  require_xmltree_false "$manifest_dump" "allowBackup" "$label"
  require_xmltree_false "$manifest_dump" "usesCleartextTraffic" "$label"
  require_xmltree_attr "$manifest_dump" "fullBackupContent" "$label"
  require_xmltree_attr "$manifest_dump" "dataExtractionRules" "$label"

  backup_rules_path="$(find_apk_xml_resource_path "$apk" "xml/no_backup_rules" "$label")"
  data_extraction_path="$(find_apk_xml_resource_path "$apk" "xml/no_data_extraction" "$label")"
  "$aapt_bin" dump xmltree "$apk" "$backup_rules_path" >"$rules_backup_dump"
  "$aapt_bin" dump xmltree "$apk" "$data_extraction_path" >"$rules_extract_dump"
  require_xmltree_rules "$rules_backup_dump" "$backup_rules_path" "$label"
  require_xmltree_rules "$rules_extract_dump" "$data_extraction_path" "$label"
  rm -f "$manifest_dump" "$rules_backup_dump" "$rules_extract_dump"
}

scan_apk() {
  local apk="$1"
  if [[ -z "$apk" || ! -f "$apk" ]]; then
    echo "APK missing: $apk" >&2
    exit 1
  fi
  if ! command -v unzip >/dev/null 2>&1; then
    echo "unzip is required for APK privacy scanning" >&2
    exit 1
  fi
  local tmp
  tmp="$(mktemp -d)"
  unzip -qq "$apk" -d "$tmp"
  scan_expanded_root "$tmp" "$apk"
  scan_apk_manifest "$apk" "$apk"
  rm -rf "$tmp"
}

if [[ "$root_count" -gt 0 ]]; then
  for root in "${roots[@]}"; do
    scan_expanded_root "$root" "$root"
  done
fi

if [[ "$apk_count" -gt 0 ]]; then
  for apk in "${apks[@]}"; do
    scan_apk "$apk"
  done
fi
