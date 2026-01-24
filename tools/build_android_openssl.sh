#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: build_android_openssl.sh --ndk PATH --out PATH [--version VER] [--api LEVEL] [--abis "a b c"]

Builds OpenSSL for Android ABIs and installs into:
  <out>/<abi>/{include,lib}

Defaults:
  version: 3.3.2
  api: 24
  abis: "armeabi-v7a arm64-v8a x86 x86_64"
EOF
}

version="3.3.2"
api="24"
abis="armeabi-v7a arm64-v8a x86 x86_64"
ndk_root=""
out_root=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ndk)
      ndk_root="${2:-}"
      shift 2
      ;;
    --out)
      out_root="${2:-}"
      shift 2
      ;;
    --version)
      version="${2:-}"
      shift 2
      ;;
    --api)
      api="${2:-}"
      shift 2
      ;;
    --abis)
      abis="${2:-}"
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

if [[ -z "$ndk_root" ]]; then
  ndk_root="${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME:-}}"
fi
if [[ -z "$ndk_root" ]]; then
  echo "NDK root not provided. Use --ndk or ANDROID_NDK_ROOT." >&2
  exit 1
fi
if [[ ! -d "$ndk_root" ]]; then
  echo "NDK root not found: $ndk_root" >&2
  exit 1
fi

if [[ -z "$out_root" ]]; then
  out_root="$(pwd)/build/openssl/android"
fi

if ! command -v perl >/dev/null 2>&1; then
  echo "perl not found; required by OpenSSL Configure." >&2
  exit 1
fi
if ! command -v make >/dev/null 2>&1; then
  echo "make not found." >&2
  exit 1
fi

uname_s="$(uname -s | tr '[:upper:]' '[:lower:]')"
uname_m="$(uname -m | tr '[:upper:]' '[:lower:]')"
case "$uname_s" in
  linux)
    host_tag="linux-x86_64"
    ;;
  darwin)
    if [[ "$uname_m" == "arm64" ]]; then
      host_tag="darwin-arm64"
    else
      host_tag="darwin-x86_64"
    fi
    ;;
  *)
    echo "unsupported host: $uname_s/$uname_m" >&2
    exit 1
    ;;
esac

toolchain_bin="$ndk_root/toolchains/llvm/prebuilt/$host_tag/bin"
if [[ ! -d "$toolchain_bin" ]]; then
  echo "NDK toolchain not found: $toolchain_bin" >&2
  exit 1
fi

build_root="$(pwd)/build/openssl"
mkdir -p "$build_root"
tarball="$build_root/openssl-${version}.tar.gz"
src_parent="$build_root/src"
src_dir="$src_parent/openssl-${version}"

if [[ ! -f "$tarball" ]]; then
  url="https://www.openssl.org/source/openssl-${version}.tar.gz"
  echo "Downloading OpenSSL $version from $url"
  curl -L "$url" -o "$tarball"
fi
if [[ ! -d "$src_dir" ]]; then
  rm -rf "$src_parent"
  mkdir -p "$src_parent"
  tar -xf "$tarball" -C "$src_parent"
fi

if command -v nproc >/dev/null 2>&1; then
  jobs="$(nproc)"
else
  jobs="$(getconf _NPROCESSORS_ONLN || echo 4)"
fi

build_one() {
  local abi="$1"
  local target=""
  case "$abi" in
    armeabi-v7a) target="android-arm" ;;
    arm64-v8a) target="android-arm64" ;;
    x86) target="android-x86" ;;
    x86_64) target="android-x86_64" ;;
    *)
      echo "unsupported ABI: $abi" >&2
      exit 1
      ;;
  esac

  local work_dir="$build_root/work-$abi"
  local prefix="$out_root/$abi"
  rm -rf "$work_dir"
  cp -R "$src_dir" "$work_dir"

  echo "Building OpenSSL for $abi ($target)"
  pushd "$work_dir" >/dev/null
  export ANDROID_NDK_HOME="$ndk_root"
  export ANDROID_NDK_ROOT="$ndk_root"
  export PATH="$toolchain_bin:$PATH"
  ./Configure "$target" -D__ANDROID_API__="$api" no-shared no-tests --prefix="$prefix"
  make -j"$jobs"
  make install_sw
  popd >/dev/null
}

for abi in $abis; do
  build_one "$abi"
done

echo "OpenSSL install root: $out_root"
