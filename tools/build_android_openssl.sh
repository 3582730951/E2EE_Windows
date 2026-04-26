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

openssl_minor="${version%.*}"
download_urls=()
if [[ -n "${MI_E2EE_ANDROID_OPENSSL_URL:-}" ]]; then
  download_urls+=("$MI_E2EE_ANDROID_OPENSSL_URL")
fi
download_urls+=(
  "https://www.openssl.org/source/openssl-${version}.tar.gz"
  "https://www.openssl.org/source/old/${openssl_minor}/openssl-${version}.tar.gz"
  "https://github.com/openssl/openssl/releases/download/openssl-${version}/openssl-${version}.tar.gz"
)

curl_retry_all_errors=()
if curl --help all 2>/dev/null | grep -q -- "--retry-all-errors"; then
  curl_retry_all_errors+=(--retry-all-errors)
fi

archive_is_valid() {
  local archive="$1"
  [[ -s "$archive" ]] || return 1
  tar -tzf "$archive" "openssl-${version}/Configure" >/dev/null 2>&1
}

verify_archive_checksum() {
  local archive="$1"
  local expected="${MI_E2EE_ANDROID_OPENSSL_SHA256:-}"
  if [[ -z "$expected" ]]; then
    return 0
  fi
  if ! command -v sha256sum >/dev/null 2>&1; then
    echo "sha256sum not found; cannot verify MI_E2EE_ANDROID_OPENSSL_SHA256." >&2
    return 1
  fi
  local actual
  actual="$(sha256sum "$archive" | awk '{print $1}')"
  if [[ "$actual" != "$expected" ]]; then
    echo "OpenSSL archive SHA-256 mismatch for $archive" >&2
    echo "expected: $expected" >&2
    echo "actual:   $actual" >&2
    return 1
  fi
}

download_archive() {
  local url="$1"
  local tmp="${tarball}.tmp"
  local attempt
  for attempt in 1 2 3; do
    rm -f "$tmp"
    echo "Downloading OpenSSL $version from $url (attempt $attempt)"
    if curl \
      --fail \
      --location \
      --show-error \
      --connect-timeout 30 \
      --retry 5 \
      "${curl_retry_all_errors[@]}" \
      "$url" \
      -o "$tmp"; then
      if archive_is_valid "$tmp" && verify_archive_checksum "$tmp"; then
        mv "$tmp" "$tarball"
        return 0
      fi
      echo "Downloaded OpenSSL archive failed validation: $url" >&2
    else
      echo "OpenSSL download failed: $url" >&2
    fi
    rm -f "$tmp"
    sleep "$attempt"
  done
  return 1
}

if [[ -f "$tarball" ]] && ! archive_is_valid "$tarball"; then
  echo "Cached OpenSSL archive is invalid; removing $tarball" >&2
  rm -f "$tarball"
fi
if [[ ! -f "$tarball" ]]; then
  downloaded=false
  for url in "${download_urls[@]}"; do
    if download_archive "$url"; then
      downloaded=true
      break
    fi
  done
  if [[ "$downloaded" != "true" ]]; then
    echo "Failed to download a valid OpenSSL $version source archive." >&2
    exit 1
  fi
fi
if ! verify_archive_checksum "$tarball"; then
  rm -f "$tarball"
  exit 1
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
  ./Configure "$target" -D__ANDROID_API__="$api" no-shared no-tests no-apps --prefix="$prefix"
  make -j"$jobs"
  make install_sw
  popd >/dev/null
  rm -rf "$work_dir"
}

for abi in $abis; do
  build_one "$abi"
done

rm -rf "$src_parent"

echo "OpenSSL install root: $out_root"
