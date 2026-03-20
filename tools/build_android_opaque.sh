#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: build_android_opaque.sh --out PATH [--abis "arm64-v8a x86_64"] [--profile release|debug]

Builds Rust OPAQUE static libraries for Android and installs them as:
  <out>/<abi>/libmi_opaque_pake.a

Defaults:
  abis: "arm64-v8a x86_64"
  profile: release
EOF
}

out_root=""
abis="arm64-v8a x86_64"
profile="release"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out)
      out_root="${2:-}"
      shift 2
      ;;
    --abis)
      abis="${2:-}"
      shift 2
      ;;
    --profile)
      profile="${2:-}"
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

if [[ -z "$out_root" ]]; then
  echo "--out is required" >&2
  exit 1
fi

case "$profile" in
  release|debug) ;;
  *)
    echo "unsupported --profile: $profile" >&2
    exit 1
    ;;
esac

if ! command -v cargo >/dev/null 2>&1; then
  echo "cargo not found" >&2
  exit 1
fi
if ! command -v rustup >/dev/null 2>&1; then
  echo "rustup not found" >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
crate_dir="$repo_root/shard/opaque_pake"

if [[ ! -f "$crate_dir/Cargo.toml" ]]; then
  echo "opaque pake crate not found: $crate_dir" >&2
  exit 1
fi

mkdir -p "$out_root"

resolve_target() {
  case "$1" in
    arm64-v8a) echo "aarch64-linux-android" ;;
    armeabi-v7a) echo "armv7-linux-androideabi" ;;
    x86) echo "i686-linux-android" ;;
    x86_64) echo "x86_64-linux-android" ;;
    *)
      echo "unsupported ABI: $1" >&2
      exit 1
      ;;
  esac
}

for abi in $abis; do
  target="$(resolve_target "$abi")"
  rustup target add "$target"
  build_args=(build --manifest-path "$crate_dir/Cargo.toml" --target "$target")
  if [[ "$profile" == "release" ]]; then
    build_args+=(--release)
    built_lib="$crate_dir/target/$target/release/libmi_opaque_pake.a"
  else
    built_lib="$crate_dir/target/$target/debug/libmi_opaque_pake.a"
  fi
  cargo "${build_args[@]}"
  if [[ ! -f "$built_lib" ]]; then
    echo "built library missing: $built_lib" >&2
    exit 1
  fi
  abi_out="$out_root/$abi"
  mkdir -p "$abi_out"
  cp "$built_lib" "$abi_out/libmi_opaque_pake.a"
done

echo "Android OPAQUE install root: $out_root"
