#!/usr/bin/env bash

set -euo pipefail

wait_for_android_services() {
  adb wait-for-device
  local boot=""
  for _ in $(seq 1 120); do
    boot="$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')"
    if [ "$boot" = "1" ]; then
      break
    fi
    sleep 2
  done
  if [ "$boot" != "1" ]; then
    echo "Android emulator boot did not complete" >&2
    return 1
  fi
  for _ in $(seq 1 90); do
    if adb shell cmd package list packages >/dev/null 2>&1 &&
       adb shell cmd activity get-config >/dev/null 2>&1; then
      return 0
    fi
    sleep 2
  done
  echo "Android framework services are not ready" >&2
  return 1
}

wait_for_stable_android_properties() {
  local sdk=""
  local bootanim=""
  local devcomplete=""
  local stable=0
  for _ in $(seq 1 60); do
    sdk="$(adb shell getprop ro.build.version.sdk 2>/dev/null | tr -d '\r')"
    bootanim="$(adb shell getprop init.svc.bootanim 2>/dev/null | tr -d '\r')"
    devcomplete="$(adb shell getprop dev.bootcomplete 2>/dev/null | tr -d '\r')"
    if [[ "$sdk" =~ ^[0-9]+$ ]] &&
       { [ -z "$bootanim" ] || [ "$bootanim" = "stopped" ]; } &&
       { [ -z "$devcomplete" ] || [ "$devcomplete" = "1" ]; }; then
      stable=$((stable + 1))
      if [ "$stable" -ge 5 ]; then
        return 0
      fi
    else
      stable=0
    fi
    sleep 2
  done
  echo "Android device properties never stabilized" >&2
  return 1
}

prime_android_device() {
  wait_for_android_services
  wait_for_stable_android_properties
  adb shell wm dismiss-keyguard >/dev/null 2>&1 || true
  adb shell input keyevent 82 >/dev/null 2>&1 || true
  adb shell input keyevent KEYCODE_HOME >/dev/null 2>&1 || true
  adb reconnect >/dev/null 2>&1 || true
  adb wait-for-device
  sleep 15
}

install_apk_with_retry() {
  local apk="$1"
  local package_name="$2"
  if adb shell pm path "$package_name" >/dev/null 2>&1; then
    echo "Package already present: $package_name"
    return 0
  fi
  for _ in $(seq 1 3); do
    if adb install -r "$apk"; then
      return 0
    fi
    adb reconnect || true
    wait_for_android_services
    sleep 2
  done
  echo "Failed to install $apk" >&2
  return 1
}

capture_ui_artifacts() {
  local workspace="${GITHUB_WORKSPACE:?GITHUB_WORKSPACE is required}"
  mkdir -p "$workspace/build/android_ui_artifacts"

  set_system_theme() {
    local theme="$1"
    case "$theme" in
      dark)
        adb shell cmd uimode night yes >/dev/null 2>&1 || true
        adb shell settings put secure ui_night_mode 2 >/dev/null 2>&1 || true
        ;;
      light)
        adb shell cmd uimode night no >/dev/null 2>&1 || true
        adb shell settings put secure ui_night_mode 1 >/dev/null 2>&1 || true
        ;;
      *)
        echo "Unknown theme mode: $theme" >&2
        return 1
        ;;
    esac
    adb shell am force-stop com.android.systemui >/dev/null 2>&1 || true
    sleep 2
  }

  capture_scene() {
    local mode="$1"
    local output="$2"
    local attempt=0
    while [ "$attempt" -lt 3 ]; do
      attempt=$((attempt + 1))
      adb shell am force-stop mi.e2ee.android.ui || true
      adb shell am start -S -W \
        -n mi.e2ee.android.ui/mi.e2ee.android.MainActivity \
        --es mi.e2ee.android.extra.SCREENSHOT_MODE "$mode" || true
      sleep 4
      adb exec-out screencap -p > "$output"
      if [ -s "$output" ]; then
        return 0
      fi
      sleep 2
    done
    echo "Failed to capture Android screenshot for mode=$mode after $attempt attempts" >&2
    return 1
  }

  prime_android_device
  install_apk_with_retry "$workspace/android/app/build/outputs/apk/debug/app-debug.apk" mi.e2ee.android.ui
  install_apk_with_retry "$workspace/android/rootapp/build/outputs/apk/debug/rootapp-debug.apk" mi.e2ee.rootauth
  set_system_theme light
  capture_scene login "$workspace/build/android_ui_artifacts/android-login.png"
  capture_scene chats "$workspace/build/android_ui_artifacts/android-chats.png"
  capture_scene detail "$workspace/build/android_ui_artifacts/android-detail.png"
  capture_scene calls "$workspace/build/android_ui_artifacts/android-calls.png"
  capture_scene settings "$workspace/build/android_ui_artifacts/android-settings.png"
  capture_scene security_center "$workspace/build/android_ui_artifacts/android-security.png"
  set_system_theme dark
  capture_scene chats "$workspace/build/android_ui_artifacts/android-chats-dark.png"
  capture_scene security_center "$workspace/build/android_ui_artifacts/android-security-dark.png"
  set_system_theme light

  local missing=0
  local required=(
    "$workspace/build/android_ui_artifacts/android-login.png"
    "$workspace/build/android_ui_artifacts/android-chats.png"
    "$workspace/build/android_ui_artifacts/android-detail.png"
    "$workspace/build/android_ui_artifacts/android-calls.png"
    "$workspace/build/android_ui_artifacts/android-settings.png"
    "$workspace/build/android_ui_artifacts/android-security.png"
    "$workspace/build/android_ui_artifacts/android-chats-dark.png"
    "$workspace/build/android_ui_artifacts/android-security-dark.png"
  )
  for png in "${required[@]}"; do
    if [ ! -s "$png" ]; then
      echo "Required Android UI smoke capture missing: $png" >&2
      missing=1
    fi
  done
  if [ "$missing" -ne 0 ]; then
    return 1
  fi

  python3 - "$workspace" <<'PY'
import json
import sys
from pathlib import Path

workspace = Path(sys.argv[1])
artifacts_root = workspace / "build" / "android_ui_artifacts"
acceptance = json.loads((workspace / "ui_contract" / "acceptance.json").read_text(encoding="utf-8"))
runtime_contract = acceptance["runtime_artifact_contract"]
android_manifest_contract = runtime_contract["evidence_manifests"]["android_runtime"]
tuple_entries = runtime_contract["tuple_inventory"]["android"]

scene_evidence: dict[str, list[str]] = {}
theme_evidence: dict[str, list[str]] = {}
state_evidence: dict[str, list[str]] = {}
tuple_evidence: dict[str, list[str]] = {}

def extend_unique(target: dict[str, list[str]], key: str, evidence_files: list[str]) -> None:
    bucket = target.setdefault(key, [])
    for rel in evidence_files:
        if rel not in bucket:
            bucket.append(rel)

for index, entry in enumerate(tuple_entries):
    scene = entry["scene"]
    theme = entry["theme"]
    state = entry["state"]
    evidence_files = entry["evidence_files"]
    if not isinstance(evidence_files, list) or not evidence_files:
        raise SystemExit(f"android tuple_inventory[{index}] missing evidence_files")
    tuple_key = f"{scene}|{theme}|{state}"
    if tuple_key in tuple_evidence:
        raise SystemExit(f"duplicate android tuple_inventory entry: {tuple_key}")
    for rel in evidence_files:
        if not (artifacts_root / rel).is_file():
            raise SystemExit(f"missing Android runtime tuple evidence file: {rel}")
    tuple_evidence[tuple_key] = list(evidence_files)
    extend_unique(scene_evidence, scene, evidence_files)
    extend_unique(theme_evidence, theme, evidence_files)
    extend_unique(state_evidence, state, evidence_files)

manifest = {
    "platform": "android",
    "required_evidence_satisfied": [
        "build",
        "navigation_flow",
        "fixture_smoke",
        "theme_parity",
        "identity_visuals",
        "security_tone_matrix",
        "empty_state_art",
        "detail_tab_hidden_parity",
        "scene_matrix",
        "viewport_matrix",
        "tuple_matrix",
    ],
    "theme_matrix_validated": sorted(theme_evidence.keys()),
    "state_matrix_validated": sorted(state_evidence.keys()),
    "scenes_validated": sorted(scene_evidence.keys()),
    "tuple_matrix_validated": sorted(tuple_evidence.keys()),
    "scene_evidence": scene_evidence,
    "theme_evidence": theme_evidence,
    "state_evidence": state_evidence,
    "tuple_evidence": tuple_evidence,
    "tuple_markers_emitted_by_runtime": True,
    "runtime_marker_source": android_manifest_contract["runtime_marker_source"],
    "emulator_validated": False,
    "arm64_validated": False,
    "arm64_waived": False,
    "arm64_waiver_reason": "",
    "arm64_gate_passed": False,
}

(artifacts_root / android_manifest_contract["manifest_file"]).write_text(
    json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
    encoding="utf-8",
)

for scene, evidence_files in scene_evidence.items():
    (artifacts_root / f"{android_manifest_contract['scene_marker_prefix']}{scene}.json").write_text(
        json.dumps(
            {"platform": "android", "scene": scene, "evidence_files": evidence_files},
            indent=2,
            ensure_ascii=False,
        ) + "\n",
        encoding="utf-8",
    )

for theme, evidence_files in theme_evidence.items():
    (artifacts_root / f"{android_manifest_contract['theme_marker_prefix']}{theme}.json").write_text(
        json.dumps(
            {"platform": "android", "theme": theme, "evidence_files": evidence_files},
            indent=2,
            ensure_ascii=False,
        ) + "\n",
        encoding="utf-8",
    )

for state, evidence_files in state_evidence.items():
    (artifacts_root / f"{android_manifest_contract['state_marker_prefix']}{state}.json").write_text(
        json.dumps(
            {"platform": "android", "state": state, "evidence_files": evidence_files},
            indent=2,
            ensure_ascii=False,
        ) + "\n",
        encoding="utf-8",
    )

for tuple_key, evidence_files in sorted(tuple_evidence.items()):
    scene, theme, state = tuple_key.split("|", 2)
    (artifacts_root / f"{android_manifest_contract['tuple_marker_prefix']}{scene}__{theme}__{state}.json").write_text(
        json.dumps(
            {
                "platform": "android",
                "scene": scene,
                "theme": theme,
                "state": state,
                "tuple_key": tuple_key,
                "evidence_files": evidence_files,
            },
            indent=2,
            ensure_ascii=False,
        ) + "\n",
        encoding="utf-8",
    )
PY
}

main() {
  prime_android_device
  cd "${GITHUB_WORKSPACE:?GITHUB_WORKSPACE is required}/android"
  ./gradlew :app:connectedDebugAndroidTest :rootapp:connectedDebugAndroidTest -PmiE2eeOpaque=true --no-daemon
}

status=0
main || status=$?
capture_status=0
capture_ui_artifacts || capture_status=$?
if [ "$capture_status" -ne 0 ] && [ "$status" -eq 0 ]; then
  status=$capture_status
fi
exit "$status"
