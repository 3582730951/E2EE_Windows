#include "protected_store.h"

#include <algorithm>
#include <cstdint>
#include <limits>

#include "platform_secure_store.h"

namespace mi::server {

namespace {
constexpr std::uint8_t kDpapiMagic[8] = {'M', 'I', 'D', 'P',
                                         'A', 'P', 'I', '1'};
constexpr std::size_t kDpapiHeaderBytes = 12;

bool IsDpapiBlob(const std::vector<std::uint8_t>& data) {
  return data.size() >= kDpapiHeaderBytes &&
         std::equal(std::begin(kDpapiMagic), std::end(kDpapiMagic),
                    data.begin());
}

mi::platform::SecureStoreScope ScopeForKeyProtection(KeyProtectionMode mode) {
  return mode == KeyProtectionMode::kDpapiMachine
             ? mi::platform::SecureStoreScope::kMachine
             : mi::platform::SecureStoreScope::kUser;
}
}  // namespace

bool EncodeProtectedFileBytes(const std::vector<std::uint8_t>& plain,
                              KeyProtectionMode mode,
                              std::vector<std::uint8_t>& out,
                              std::string& error) {
  error.clear();
  if (mode == KeyProtectionMode::kNone) {
    out = plain;
    return true;
  }
  if (!mi::platform::SecureStoreSupported()) {
    error = "secure store unsupported";
    return false;
  }
  std::vector<std::uint8_t> blob;
  if (!mi::platform::ProtectSecureBlobScoped(
          plain, nullptr, 0, ScopeForKeyProtection(mode), blob, error)) {
    return false;
  }
  if (blob.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "secure store blob too large";
    return false;
  }
  const std::uint32_t len = static_cast<std::uint32_t>(blob.size());
  out.clear();
  out.reserve(kDpapiHeaderBytes + blob.size());
  out.insert(out.end(), std::begin(kDpapiMagic), std::end(kDpapiMagic));
  out.push_back(static_cast<std::uint8_t>(len & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len >> 24) & 0xFF));
  out.insert(out.end(), blob.begin(), blob.end());
  return true;
}

bool DecodeProtectedFileBytes(const std::vector<std::uint8_t>& file_bytes,
                              KeyProtectionMode mode,
                              std::vector<std::uint8_t>& out_plain,
                              bool& was_protected,
                              std::string& error) {
  error.clear();
  was_protected = false;
  if (!IsDpapiBlob(file_bytes)) {
    out_plain = file_bytes;
    return true;
  }
  if (file_bytes.size() < kDpapiHeaderBytes) {
    error = "secure store blob invalid";
    return false;
  }
  was_protected = true;
  const std::uint32_t len =
      static_cast<std::uint32_t>(file_bytes[8]) |
      (static_cast<std::uint32_t>(file_bytes[9]) << 8) |
      (static_cast<std::uint32_t>(file_bytes[10]) << 16) |
      (static_cast<std::uint32_t>(file_bytes[11]) << 24);
  if (len == 0 || file_bytes.size() != kDpapiHeaderBytes + len) {
    error = "secure store blob size invalid";
    return false;
  }
  const std::vector<std::uint8_t> blob(file_bytes.begin() + kDpapiHeaderBytes,
                                       file_bytes.end());
  return mi::platform::UnprotectSecureBlobScoped(
      blob, nullptr, 0, ScopeForKeyProtection(mode), out_plain, error);
}

bool DecodeProtectedFileBytes(const std::vector<std::uint8_t>& file_bytes,
                              std::vector<std::uint8_t>& out_plain,
                              std::string& error) {
  bool was_protected = false;
  return DecodeProtectedFileBytes(file_bytes, KeyProtectionMode::kNone,
                                  out_plain, was_protected, error);
}

}  // namespace mi::server
