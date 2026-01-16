#include "dpapi_util.h"
#include "secure_store_util.h"

#include <cstring>

#include "platform_secure_store.h"

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

  std::vector<std::uint8_t> blob(in.begin() + static_cast<std::ptrdiff_t>(off),
                                 in.end());
  const std::uint8_t* entropy_ptr =
      reinterpret_cast<const std::uint8_t*>(entropy);
  const std::size_t entropy_len = entropy ? std::strlen(entropy) : 0;
  if (!mi::platform::UnprotectSecureBlob(blob, entropy_ptr, entropy_len,
                                         out_plain, error)) {
    return false;
  }
  out_was_dpapi = true;
  return true;
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
  const std::uint8_t* entropy_ptr =
      reinterpret_cast<const std::uint8_t*>(entropy);
  const std::size_t entropy_len = entropy ? std::strlen(entropy) : 0;
  std::vector<std::uint8_t> blob;
  if (!mi::platform::ProtectSecureBlob(plain, entropy_ptr, entropy_len, blob,
                                       error)) {
    return false;
  }
  const std::size_t magic_len = std::strlen(magic);
  out_wrapped.reserve(magic_len + 4 + blob.size());
  out_wrapped.insert(out_wrapped.end(), magic, magic + magic_len);
  const std::uint32_t len = static_cast<std::uint32_t>(blob.size());
  out_wrapped.push_back(static_cast<std::uint8_t>(len & 0xFF));
  out_wrapped.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
  out_wrapped.push_back(static_cast<std::uint8_t>((len >> 16) & 0xFF));
  out_wrapped.push_back(static_cast<std::uint8_t>((len >> 24) & 0xFF));
  out_wrapped.insert(out_wrapped.end(), blob.begin(), blob.end());
  return true;
}

bool MaybeUnprotectSecureStore(const std::vector<std::uint8_t>& in,
                               const char* magic,
                               const char* entropy,
                               std::vector<std::uint8_t>& out_plain,
                               bool& out_was_wrapped,
                               std::string& error) {
  return MaybeUnprotectDpapi(in, magic, entropy, out_plain, out_was_wrapped,
                             error);
}

bool ProtectSecureStore(const std::vector<std::uint8_t>& plain,
                        const char* magic,
                        const char* entropy,
                        std::vector<std::uint8_t>& out_wrapped,
                        std::string& error) {
  return ProtectDpapi(plain, magic, entropy, out_wrapped, error);
}

}  // namespace mi::client
