#include "platform_identity.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <winreg.h>

#include <algorithm>

#include "secure_buffer.h"

namespace mi::platform {

namespace {

struct ScopedNcryptProvider {
  NCRYPT_PROV_HANDLE handle{0};
  ~ScopedNcryptProvider() {
    if (handle) {
      NCryptFreeObject(handle);
      handle = 0;
    }
  }
};

struct ScopedNcryptKey {
  NCRYPT_KEY_HANDLE handle{0};
  ~ScopedNcryptKey() {
    if (handle) {
      NCryptFreeObject(handle);
      handle = 0;
    }
  }
};

std::string ReadMachineGuid() {
  char buf[128] = {};
  DWORD size = static_cast<DWORD>(sizeof(buf));
  const LONG rc = RegGetValueA(
      HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", "MachineGuid",
      RRF_RT_REG_SZ, nullptr, buf, &size);
  if (rc != ERROR_SUCCESS || size == 0) {
    return {};
  }
  std::string guid(buf);
  while (!guid.empty() &&
         (guid.back() == '\0' || guid.back() == '\r' || guid.back() == '\n')) {
    guid.pop_back();
  }
  return guid;
}

bool OpenTpmKey(ScopedNcryptProvider& provider, ScopedNcryptKey& key,
                bool allow_create, std::string& error) {
  error.clear();
  SECURITY_STATUS status =
      NCryptOpenStorageProvider(&provider.handle, MS_PLATFORM_CRYPTO_PROVIDER, 0);
  if (status != ERROR_SUCCESS) {
    error = "tpm provider unavailable";
    return false;
  }

  status = NCryptOpenKey(provider.handle, &key.handle, L"mi_e2ee_identity", 0, 0);
  if (status == NTE_BAD_KEYSET || status == NTE_NO_KEY) {
    if (!allow_create) {
      error = "tpm key missing";
      return false;
    }
    status = NCryptCreatePersistedKey(provider.handle, &key.handle,
                                      NCRYPT_RSA_ALGORITHM, L"mi_e2ee_identity",
                                      0, 0);
    if (status != ERROR_SUCCESS) {
      error = "tpm key create failed";
      return false;
    }
    const DWORD key_len = 2048;
    status = NCryptSetProperty(
        key.handle, NCRYPT_LENGTH_PROPERTY,
        reinterpret_cast<PBYTE>(const_cast<DWORD*>(&key_len)), sizeof(key_len),
        0);
    if (status != ERROR_SUCCESS) {
      error = "tpm key length set failed";
      return false;
    }
    DWORD usage = NCRYPT_ALLOW_ALL_USAGES;
    status = NCryptSetProperty(key.handle, NCRYPT_KEY_USAGE_PROPERTY,
                               reinterpret_cast<PBYTE>(&usage), sizeof(usage),
                               0);
    if (status != ERROR_SUCCESS) {
      error = "tpm key usage set failed";
      return false;
    }
    status = NCryptFinalizeKey(key.handle, 0);
    if (status != ERROR_SUCCESS) {
      error = "tpm key finalize failed";
      return false;
    }
  } else if (status != ERROR_SUCCESS) {
    error = "tpm key open failed";
    return false;
  }
  return true;
}

}  // namespace

std::string MachineId() {
  return ReadMachineGuid();
}

bool TpmSupported() {
  return true;
}

bool TpmWrapKey(const std::array<std::uint8_t, 32>& key_bytes,
                std::vector<std::uint8_t>& out_wrapped,
                std::string& error) {
  out_wrapped.clear();
  ScopedNcryptProvider provider;
  ScopedNcryptKey key;
  if (!OpenTpmKey(provider, key, true, error)) {
    return false;
  }

  BCRYPT_OAEP_PADDING_INFO padding{};
  padding.pszAlgId = const_cast<wchar_t*>(BCRYPT_SHA256_ALGORITHM);
  padding.pbLabel = nullptr;
  padding.cbLabel = 0;

  DWORD out_len = 0;
  SECURITY_STATUS status = NCryptEncrypt(
      key.handle, const_cast<PBYTE>(key_bytes.data()),
      static_cast<DWORD>(key_bytes.size()), &padding, nullptr, 0, &out_len,
      NCRYPT_PAD_OAEP_FLAG);
  if (status != ERROR_SUCCESS || out_len == 0) {
    error = "tpm encrypt failed";
    return false;
  }

  out_wrapped.resize(out_len);
  status = NCryptEncrypt(key.handle, const_cast<PBYTE>(key_bytes.data()),
                         static_cast<DWORD>(key_bytes.size()), &padding,
                         out_wrapped.data(), out_len, &out_len,
                         NCRYPT_PAD_OAEP_FLAG);
  if (status != ERROR_SUCCESS || out_len == 0) {
    out_wrapped.clear();
    error = "tpm encrypt failed";
    return false;
  }
  out_wrapped.resize(out_len);
  return true;
}

bool TpmUnwrapKey(const std::vector<std::uint8_t>& wrapped,
                  std::array<std::uint8_t, 32>& out_key,
                  std::string& error) {
  out_key.fill(0);
  ScopedNcryptProvider provider;
  ScopedNcryptKey key;
  if (!OpenTpmKey(provider, key, false, error)) {
    return false;
  }

  BCRYPT_OAEP_PADDING_INFO padding{};
  padding.pszAlgId = const_cast<wchar_t*>(BCRYPT_SHA256_ALGORITHM);
  padding.pbLabel = nullptr;
  padding.cbLabel = 0;

  DWORD out_len = 0;
  SECURITY_STATUS status = NCryptDecrypt(
      key.handle, const_cast<PBYTE>(wrapped.data()),
      static_cast<DWORD>(wrapped.size()), &padding, nullptr, 0, &out_len,
      NCRYPT_PAD_OAEP_FLAG);
  if (status != ERROR_SUCCESS || out_len == 0) {
    error = "tpm decrypt failed";
    return false;
  }

  std::vector<std::uint8_t> buf;
  buf.resize(out_len);
  [[maybe_unused]] mi::common::ScopedWipe wipe_buf(buf);
  status = NCryptDecrypt(key.handle, const_cast<PBYTE>(wrapped.data()),
                         static_cast<DWORD>(wrapped.size()), &padding,
                         buf.data(), static_cast<DWORD>(buf.size()), &out_len,
                         NCRYPT_PAD_OAEP_FLAG);
  if (status != ERROR_SUCCESS || out_len != out_key.size()) {
    error = "tpm decrypt failed";
    return false;
  }
  std::copy_n(buf.begin(), out_key.size(), out_key.begin());
  return true;
}

}  // namespace mi::platform
