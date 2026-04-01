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

  capture_scene() {
    local mode="$1"
    local output="$2"
    adb shell am force-stop mi.e2ee.android.ui || true
    adb shell am start -W \
      -n mi.e2ee.android.ui/mi.e2ee.android.MainActivity \
      --es mi.e2ee.android.extra.SCREENSHOT_MODE "$mode" || true
    sleep 3
    adb exec-out screencap -p > "$output"
  }

  prime_android_device
  install_apk_with_retry "$workspace/android/app/build/outputs/apk/debug/app-debug.apk" mi.e2ee.android.ui
  install_apk_with_retry "$workspace/android/rootapp/build/outputs/apk/debug/rootapp-debug.apk" mi.e2ee.rootauth
  capture_scene login "$workspace/build/android_ui_artifacts/android-login.png"
  capture_scene chats "$workspace/build/android_ui_artifacts/android-chats.png"
  capture_scene detail "$workspace/build/android_ui_artifacts/android-detail.png"
  capture_scene calls "$workspace/build/android_ui_artifacts/android-calls.png"
  capture_scene settings "$workspace/build/android_ui_artifacts/android-settings.png"

  local missing=0
  local required=(
    "$workspace/build/android_ui_artifacts/android-login.png"
    "$workspace/build/android_ui_artifacts/android-chats.png"
    "$workspace/build/android_ui_artifacts/android-detail.png"
    "$workspace/build/android_ui_artifacts/android-calls.png"
    "$workspace/build/android_ui_artifacts/android-settings.png"
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
