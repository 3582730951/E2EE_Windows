#include "dpapi_util.h"

#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

namespace mi::client {

namespace {

bool StartsWithBytes(const std::vector<std::uint8_t>& data,
                     const char* prefix,
                     std::size_t prefix_len) {
  if (!prefix || prefix_len == 0 || data.size() < prefix_len) {
    return false;
  }
  return std::memcmp(data.data(), prefix, prefix_len) == 0;
}

}  // namespace

bool MaybeUnprotectDpapi(const std::vector<std::uint8_t>& in,
                         const char* magic,
                         const char* entropy,
                         std::vector<std::uint8_t>& out_plain,
                         bool& out_was_dpapi,
                         std::string& error) {
  error.clear();
  out_plain.clear();
  out_was_dpapi = false;
  if (!magic || std::strlen(magic) == 0) {
    error = "dpapi magic empty";
    return false;
  }
#ifdef _WIN32
  const std::size_t magic_len = std::strlen(magic);
  if (!StartsWithBytes(in, magic, magic_len)) {
    out_plain = in;
    return true;
  }
  if (in.size() < (magic_len + 4)) {
    error = "dpapi header truncated";
    return false;
  }
  std::size_t off = magic_len;
  const std::uint32_t blob_len =
      static_cast<std::uint32_t>(in[off]) |
      (static_cast<std::uint32_t>(in[off + 1]) << 8) |
      (static_cast<std::uint32_t>(in[off + 2]) << 16) |
      (static_cast<std::uint32_t>(in[off + 3]) << 24);
  off += 4;
  if (off + blob_len != in.size()) {
    error = "dpapi size invalid";
    return false;
  }

  DATA_BLOB blob_in;
  blob_in.cbData = blob_len;
  blob_in.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(in.data() + off));

  DATA_BLOB entropy_blob;
  entropy_blob.cbData = static_cast<DWORD>(entropy ? std::strlen(entropy) : 0);
  entropy_blob.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(entropy ? entropy : ""));

  DATA_BLOB blob_out;
  blob_out.cbData = 0;
  blob_out.pbData = nullptr;
  const BOOL ok =
      CryptUnprotectData(&blob_in, nullptr, &entropy_blob, nullptr, nullptr,
                         CRYPTPROTECT_UI_FORBIDDEN, &blob_out);
  if (!ok || !blob_out.pbData || blob_out.cbData == 0) {
    error = "CryptUnprotectData failed";
    if (blob_out.pbData) {
      LocalFree(blob_out.pbData);
    }
    return false;
  }
  out_plain.assign(blob_out.pbData, blob_out.pbData + blob_out.cbData);
  LocalFree(blob_out.pbData);
  out_was_dpapi = true;
  return true;
#else
  out_plain = in;
  return true;
#endif
}

bool ProtectDpapi(const std::vector<std::uint8_t>& plain,
                  const char* magic,
                  const char* entropy,
                  std::vector<std::uint8_t>& out_wrapped,
                  std::string& error) {
  error.clear();
  out_wrapped.clear();
  if (plain.empty()) {
    error = "dpapi plain empty";
    return false;
  }
  if (!magic || std::strlen(magic) == 0) {
    error = "dpapi magic empty";
    return false;
  }
#ifdef _WIN32
  DATA_BLOB blob_in;
  blob_in.cbData = static_cast<DWORD>(plain.size());
  blob_in.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plain.data()));

  DATA_BLOB entropy_blob;
  entropy_blob.cbData = static_cast<DWORD>(entropy ? std::strlen(entropy) : 0);
  entropy_blob.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(entropy ? entropy : ""));

  DATA_BLOB blob_out;
  blob_out.cbData = 0;
  blob_out.pbData = nullptr;
  const BOOL ok =
      CryptProtectData(&blob_in, nullptr, &entropy_blob, nullptr, nullptr,
                       CRYPTPROTECT_UI_FORBIDDEN, &blob_out);
  if (!ok || !blob_out.pbData || blob_out.cbData == 0) {
    error = "CryptProtectData failed";
    if (blob_out.pbData) {
      LocalFree(blob_out.pbData);
    }
    return false;
  }

  const std::size_t magic_len = std::strlen(magic);
  out_wrapped.reserve(magic_len + 4 + blob_out.cbData);
  out_wrapped.insert(out_wrapped.end(), magic, magic + magic_len);
  const std::uint32_t len = static_cast<std::uint32_t>(blob_out.cbData);
  out_wrapped.push_back(static_cast<std::uint8_t>(len & 0xFF));
  out_wrapped.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
  out_wrapped.push_back(static_cast<std::uint8_t>((len >> 16) & 0xFF));
  out_wrapped.push_back(static_cast<std::uint8_t>((len >> 24) & 0xFF));
  out_wrapped.insert(out_wrapped.end(), blob_out.pbData,
                     blob_out.pbData + blob_out.cbData);
  LocalFree(blob_out.pbData);
  return true;
#else
  out_wrapped = plain;
  return true;
#endif
}

}  // namespace mi::client

