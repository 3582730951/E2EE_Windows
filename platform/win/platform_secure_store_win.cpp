#include "platform_secure_store.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>

namespace mi::platform {

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
