#include "platform_secure_store.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "monocypher.h"
#include "platform_identity.h"
#include "platform_random.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>

namespace mi::platform {

namespace {

constexpr char kTpmMagic[] = "MI_E2EE_SECURE_STORE_TPM1";
constexpr std::size_t kKeyBytes = 32;
constexpr std::size_t kNonceBytes = 24;
constexpr std::size_t kTagBytes = 16;

bool StartsWithBytes(const std::vector<std::uint8_t>& data,
                     const char* prefix,
                     std::size_t prefix_len) {
  if (!prefix || prefix_len == 0 || data.size() < prefix_len) {
    return false;
  }
  return std::memcmp(data.data(), prefix, prefix_len) == 0;
}

bool WriteLe32(std::uint32_t v, std::vector<std::uint8_t>& out) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  return true;
}

bool ReadLe32(const std::vector<std::uint8_t>& data,
              std::size_t& offset,
              std::uint32_t& out) {
  if (offset + 4 > data.size()) {
    return false;
  }
  out = static_cast<std::uint32_t>(data[offset]) |
        (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
        (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
        (static_cast<std::uint32_t>(data[offset + 3]) << 24);
  offset += 4;
  return true;
}

bool TryProtectWithTpm(const std::vector<std::uint8_t>& plain,
                       const std::uint8_t* entropy,
                       std::size_t entropy_len,
                       std::vector<std::uint8_t>& out,
                       std::string& error) {
  error.clear();
  out.clear();
  if (!TpmSupported()) {
    return false;
  }
  std::array<std::uint8_t, kKeyBytes> data_key{};
  if (!RandomBytes(data_key.data(), data_key.size())) {
    error = "secure store rng failed";
    return false;
  }
  std::vector<std::uint8_t> wrapped_key;
  if (!TpmWrapKey(data_key, wrapped_key, error)) {
    return false;
  }
  std::array<std::uint8_t, kNonceBytes> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    error = "secure store rng failed";
    return false;
  }
  std::vector<std::uint8_t> cipher(plain.size());
  std::array<std::uint8_t, kTagBytes> tag{};
  const std::uint8_t* ad = entropy_len > 0 ? entropy : nullptr;
  const std::size_t ad_len = entropy_len > 0 ? entropy_len : 0;
  crypto_aead_lock(cipher.data(), tag.data(), data_key.data(), nonce.data(), ad,
                   ad_len, plain.data(), plain.size());

  const std::size_t magic_len = sizeof(kTpmMagic) - 1;
  if (wrapped_key.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "secure store key wrap too large";
    return false;
  }
  out.reserve(magic_len + 4 + wrapped_key.size() + nonce.size() + tag.size() +
              cipher.size());
  out.insert(out.end(), kTpmMagic, kTpmMagic + magic_len);
  WriteLe32(static_cast<std::uint32_t>(wrapped_key.size()), out);
  out.insert(out.end(), wrapped_key.begin(), wrapped_key.end());
  out.insert(out.end(), nonce.begin(), nonce.end());
  out.insert(out.end(), tag.begin(), tag.end());
  out.insert(out.end(), cipher.begin(), cipher.end());
  return true;
}

bool TryUnprotectWithTpm(const std::vector<std::uint8_t>& blob,
                         const std::uint8_t* entropy,
                         std::size_t entropy_len,
                         std::vector<std::uint8_t>& out,
                         std::string& error) {
  error.clear();
  out.clear();
  const std::size_t magic_len = sizeof(kTpmMagic) - 1;
  if (!StartsWithBytes(blob, kTpmMagic, magic_len)) {
    return false;
  }
  std::size_t off = magic_len;
  std::uint32_t wrapped_len = 0;
  if (!ReadLe32(blob, off, wrapped_len)) {
    error = "secure store blob invalid";
    return false;
  }
  if (wrapped_len == 0 ||
      wrapped_len > blob.size() ||
      off + wrapped_len + kNonceBytes + kTagBytes > blob.size()) {
    error = "secure store blob invalid";
    return false;
  }
  const std::size_t wrapped_end = off + wrapped_len;
  std::vector<std::uint8_t> wrapped(blob.begin() + static_cast<std::ptrdiff_t>(off),
                                    blob.begin() + static_cast<std::ptrdiff_t>(wrapped_end));
  off = wrapped_end;
  std::array<std::uint8_t, kNonceBytes> nonce{};
  std::memcpy(nonce.data(), blob.data() + off, nonce.size());
  off += nonce.size();
  std::array<std::uint8_t, kTagBytes> tag{};
  std::memcpy(tag.data(), blob.data() + off, tag.size());
  off += tag.size();
  std::vector<std::uint8_t> cipher(blob.begin() + static_cast<std::ptrdiff_t>(off),
                                   blob.end());

  std::array<std::uint8_t, kKeyBytes> data_key{};
  if (!TpmUnwrapKey(wrapped, data_key, error)) {
    return false;
  }
  out.resize(cipher.size());
  const std::uint8_t* ad = entropy_len > 0 ? entropy : nullptr;
  const std::size_t ad_len = entropy_len > 0 ? entropy_len : 0;
  const int rc = crypto_aead_unlock(out.data(), tag.data(), data_key.data(),
                                    nonce.data(), ad, ad_len, cipher.data(),
                                    cipher.size());
  if (rc != 0) {
    out.clear();
    error = "secure store auth failed";
    return false;
  }
  return true;
}

}  // namespace

bool SecureStoreSupported() {
  return true;
}

bool ProtectSecureBlobScoped(const std::vector<std::uint8_t>& plain,
                             const std::uint8_t* entropy,
                             std::size_t entropy_len,
                             SecureStoreScope scope,
                             std::vector<std::uint8_t>& out,
                             std::string& error) {
  error.clear();
  out.clear();
  if (plain.empty()) {
    error = "secure store plain empty";
    return false;
  }

  if (scope == SecureStoreScope::kMachine) {
    std::string tpm_err;
    if (TryProtectWithTpm(plain, entropy, entropy_len, out, tpm_err)) {
      return true;
    }
  }

  DATA_BLOB blob_in;
  blob_in.cbData = static_cast<DWORD>(plain.size());
  blob_in.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plain.data()));

  DATA_BLOB entropy_blob;
  entropy_blob.cbData = static_cast<DWORD>(entropy ? entropy_len : 0);
  const BYTE* entropy_bytes =
      entropy_blob.cbData > 0 ? reinterpret_cast<const BYTE*>(entropy) : nullptr;
  entropy_blob.pbData = const_cast<BYTE*>(entropy_bytes);

  DATA_BLOB blob_out;
  blob_out.cbData = 0;
  blob_out.pbData = nullptr;
  DWORD flags = CRYPTPROTECT_UI_FORBIDDEN;
  if (scope == SecureStoreScope::kMachine) {
    flags |= CRYPTPROTECT_LOCAL_MACHINE;
  }
  const BOOL ok =
      CryptProtectData(&blob_in, nullptr, &entropy_blob, nullptr, nullptr,
                       flags, &blob_out);
  if (!ok || !blob_out.pbData || blob_out.cbData == 0) {
    error = "CryptProtectData failed";
    if (blob_out.pbData) {
      LocalFree(blob_out.pbData);
    }
    return false;
  }

  out.assign(blob_out.pbData, blob_out.pbData + blob_out.cbData);
  LocalFree(blob_out.pbData);
  return true;
}

bool UnprotectSecureBlobScoped(const std::vector<std::uint8_t>& blob,
                               const std::uint8_t* entropy,
                               std::size_t entropy_len,
                               SecureStoreScope /*scope*/,
                               std::vector<std::uint8_t>& out,
                               std::string& error) {
  error.clear();
  out.clear();
  if (blob.empty()) {
    error = "secure store blob empty";
    return false;
  }

  std::string tpm_err;
  if (TryUnprotectWithTpm(blob, entropy, entropy_len, out, tpm_err)) {
    return true;
  }
  if (!tpm_err.empty()) {
    error = tpm_err;
    return false;
  }

  DATA_BLOB blob_in;
  blob_in.cbData = static_cast<DWORD>(blob.size());
  blob_in.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(blob.data()));

  DATA_BLOB entropy_blob;
  entropy_blob.cbData = static_cast<DWORD>(entropy ? entropy_len : 0);
  const BYTE* entropy_bytes =
      entropy_blob.cbData > 0 ? reinterpret_cast<const BYTE*>(entropy) : nullptr;
  entropy_blob.pbData = const_cast<BYTE*>(entropy_bytes);

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
  out.assign(blob_out.pbData, blob_out.pbData + blob_out.cbData);
  LocalFree(blob_out.pbData);
  return true;
}

bool ProtectSecureBlob(const std::vector<std::uint8_t>& plain,
                       const std::uint8_t* entropy,
                       std::size_t entropy_len,
                       std::vector<std::uint8_t>& out,
                       std::string& error) {
  return ProtectSecureBlobScoped(plain, entropy, entropy_len,
                                 SecureStoreScope::kUser, out, error);
}

bool UnprotectSecureBlob(const std::vector<std::uint8_t>& blob,
                         const std::uint8_t* entropy,
                         std::size_t entropy_len,
                         std::vector<std::uint8_t>& out,
                         std::string& error) {
  return UnprotectSecureBlobScoped(blob, entropy, entropy_len,
                                   SecureStoreScope::kUser, out, error);
}

}  // namespace mi::platform
