#include "platform_secure_store.h"

#include <array>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "monocypher.h"
#include "platform_random.h"

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#elif defined(__linux__)
#include <libsecret/secret.h>
#include <unistd.h>
#endif

namespace mi::platform {

namespace {

constexpr char kStoreLabel[] = "mi_e2ee secure store key";
constexpr char kStoreService[] = "mi_e2ee_secure_store";
constexpr char kStoreAccount[] = "default";
constexpr char kBlobMagic[] = "MI_E2EE_SECURE_STORE_V1";
constexpr std::size_t kKeyBytes = 32;
constexpr std::size_t kNonceBytes = 24;
constexpr std::size_t kTagBytes = 16;

bool RandomKey(std::array<std::uint8_t, kKeyBytes>& key) {
  return RandomBytes(key.data(), key.size());
}

std::string BytesToHexLower(const std::uint8_t* data, std::size_t len) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    out[i * 2] = kHex[data[i] >> 4];
    out[i * 2 + 1] = kHex[data[i] & 0x0F];
  }
  return out;
}

int HexNibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

bool HexToBytes(const std::string& hex, std::vector<std::uint8_t>& out) {
  out.clear();
  if (hex.empty() || (hex.size() % 2) != 0) {
    return false;
  }
  out.reserve(hex.size() / 2);
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    const int hi = HexNibble(hex[i]);
    const int lo = HexNibble(hex[i + 1]);
    if (hi < 0 || lo < 0) {
      out.clear();
      return false;
    }
    out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
  }
  return true;
}

#if defined(__APPLE__)

std::string OsStatusToString(OSStatus status) {
  CFStringRef msg = SecCopyErrorMessageString(status, nullptr);
  if (!msg) {
    return "keychain error";
  }
  char buf[256] = {};
  std::string out;
  if (CFStringGetCString(msg, buf, sizeof(buf), kCFStringEncodingUTF8)) {
    out = buf;
  } else {
    out = "keychain error";
  }
  CFRelease(msg);
  return out;
}

bool LoadKeychainKey(std::array<std::uint8_t, kKeyBytes>& key,
                     bool& found,
                     std::string& error) {
  found = false;
  error.clear();

  const void* keys[] = {kSecClass, kSecAttrService, kSecAttrAccount,
                        kSecReturnData, kSecMatchLimit};
  const void* values[] = {kSecClassGenericPassword,
                          CFStringCreateWithCString(nullptr, kStoreService,
                                                    kCFStringEncodingUTF8),
                          CFStringCreateWithCString(nullptr, kStoreAccount,
                                                    kCFStringEncodingUTF8),
                          kCFBooleanTrue, kSecMatchLimitOne};
  CFDictionaryRef query =
      CFDictionaryCreate(nullptr, keys, values,
                         static_cast<CFIndex>(sizeof(keys) / sizeof(keys[0])),
                         &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks);
  for (const void* value : values) {
    if (value && (value == values[1] || value == values[2])) {
      CFRelease(value);
    }
  }
  if (!query) {
    error = "keychain query failed";
    return false;
  }

  CFTypeRef out = nullptr;
  const OSStatus status = SecItemCopyMatching(query, &out);
  CFRelease(query);
  if (status == errSecItemNotFound) {
    return true;
  }
  if (status != errSecSuccess) {
    error = OsStatusToString(status);
    return false;
  }
  auto* data = static_cast<CFDataRef>(out);
  if (!data || CFDataGetLength(data) != static_cast<CFIndex>(key.size())) {
    if (out) {
      CFRelease(out);
    }
    error = "keychain key invalid";
    return false;
  }
  std::memcpy(key.data(), CFDataGetBytePtr(data), key.size());
  CFRelease(out);
  found = true;
  return true;
}

bool StoreKeychainKey(const std::array<std::uint8_t, kKeyBytes>& key,
                      std::string& error) {
  error.clear();
  CFDataRef data =
      CFDataCreate(nullptr, key.data(), static_cast<CFIndex>(key.size()));
  if (!data) {
    error = "keychain key encode failed";
    return false;
  }

  const void* add_keys[] = {kSecClass, kSecAttrService, kSecAttrAccount,
                            kSecAttrAccessible, kSecValueData, kSecAttrLabel};
  const void* add_values[] = {
      kSecClassGenericPassword,
      CFStringCreateWithCString(nullptr, kStoreService, kCFStringEncodingUTF8),
      CFStringCreateWithCString(nullptr, kStoreAccount, kCFStringEncodingUTF8),
      kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
      data,
      CFStringCreateWithCString(nullptr, kStoreLabel, kCFStringEncodingUTF8)};
  CFDictionaryRef add =
      CFDictionaryCreate(nullptr, add_keys, add_values,
                         static_cast<CFIndex>(sizeof(add_keys) /
                                              sizeof(add_keys[0])),
                         &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks);
  for (const void* value : add_values) {
    if (value &&
        (value == add_values[1] || value == add_values[2] ||
         value == add_values[5])) {
      CFRelease(value);
    }
  }
  if (!add) {
    CFRelease(data);
    error = "keychain add failed";
    return false;
  }

  OSStatus status = SecItemAdd(add, nullptr);
  if (status == errSecDuplicateItem) {
    const void* upd_keys[] = {kSecValueData, kSecAttrLabel};
    const void* upd_values[] = {data,
                                CFStringCreateWithCString(
                                    nullptr, kStoreLabel,
                                    kCFStringEncodingUTF8)};
    CFDictionaryRef upd =
        CFDictionaryCreate(nullptr, upd_keys, upd_values,
                           static_cast<CFIndex>(sizeof(upd_keys) /
                                                sizeof(upd_keys[0])),
                           &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks);
    if (upd_values[1]) {
      CFRelease(upd_values[1]);
    }
    status = upd ? SecItemUpdate(add, upd) : errSecParam;
    if (upd) {
      CFRelease(upd);
    }
  }
  CFRelease(add);
  CFRelease(data);
  if (status != errSecSuccess) {
    error = OsStatusToString(status);
    return false;
  }
  return true;
}

#elif defined(__linux__)

const SecretSchema& SecureStoreSchema() {
  static const SecretSchema kSchema = {
      "com.mi.e2ee.secure_store",
      SECRET_SCHEMA_NONE,
      {{"name", SECRET_SCHEMA_ATTRIBUTE_STRING},
       {"uid", SECRET_SCHEMA_ATTRIBUTE_STRING},
       {nullptr, 0}}};
  return kSchema;
}

std::string CurrentUidString() {
  return std::to_string(static_cast<unsigned long>(getuid()));
}

bool LoadSecretServiceKey(std::array<std::uint8_t, kKeyBytes>& key,
                          bool& found,
                          std::string& error) {
  found = false;
  error.clear();
  GError* gerr = nullptr;
  gchar* secret = secret_password_lookup_sync(
      &SecureStoreSchema(), nullptr, &gerr, "name", kStoreService, "uid",
      CurrentUidString().c_str(), nullptr);
  if (gerr) {
    error = gerr->message ? gerr->message : "secret service error";
    g_error_free(gerr);
    return false;
  }
  if (!secret) {
    return true;
  }
  std::vector<std::uint8_t> bytes;
  if (!HexToBytes(secret, bytes) || bytes.size() != key.size()) {
    secret_password_free(secret);
    error = "secret store key invalid";
    return false;
  }
  secret_password_free(secret);
  std::memcpy(key.data(), bytes.data(), key.size());
  found = true;
  return true;
}

bool StoreSecretServiceKey(const std::array<std::uint8_t, kKeyBytes>& key,
                           std::string& error) {
  error.clear();
  const std::string hex = BytesToHexLower(key.data(), key.size());
  GError* gerr = nullptr;
  const gboolean ok = secret_password_store_sync(
      &SecureStoreSchema(), SECRET_COLLECTION_DEFAULT, kStoreLabel,
      hex.c_str(), nullptr, &gerr, "name", kStoreService, "uid",
      CurrentUidString().c_str(), nullptr);
  if (!ok) {
    error = gerr && gerr->message ? gerr->message : "secret store failed";
    if (gerr) {
      g_error_free(gerr);
    }
    return false;
  }
  return true;
}

#endif

bool GetOrCreateMasterKey(std::array<std::uint8_t, kKeyBytes>& key,
                          std::string& error) {
  error.clear();
  static bool cached = false;
  static std::array<std::uint8_t, kKeyBytes> cached_key{};
  static std::mutex cache_mu;

  {
    std::lock_guard<std::mutex> lock(cache_mu);
    if (cached) {
      key = cached_key;
      return true;
    }
  }

  bool found = false;
#if defined(__APPLE__)
  if (!LoadKeychainKey(key, found, error)) {
    return false;
  }
#elif defined(__linux__)
  if (!LoadSecretServiceKey(key, found, error)) {
    return false;
  }
#else
  error = "secure store unsupported";
  return false;
#endif

  if (!found) {
    if (!RandomKey(key)) {
      error = "secure store rng failed";
      return false;
    }
#if defined(__APPLE__)
    if (!StoreKeychainKey(key, error)) {
      return false;
    }
#elif defined(__linux__)
    if (!StoreSecretServiceKey(key, error)) {
      return false;
    }
#endif
  }

  {
    std::lock_guard<std::mutex> lock(cache_mu);
    cached_key = key;
    cached = true;
  }
  return true;
}

bool ParseEncryptedBlob(const std::vector<std::uint8_t>& blob,
                        std::array<std::uint8_t, kNonceBytes>& nonce,
                        std::array<std::uint8_t, kTagBytes>& tag,
                        std::vector<std::uint8_t>& cipher) {
  const std::size_t magic_len = sizeof(kBlobMagic) - 1;
  if (blob.size() < magic_len + nonce.size() + tag.size()) {
    return false;
  }
  if (std::memcmp(blob.data(), kBlobMagic, magic_len) != 0) {
    return false;
  }
  std::size_t off = magic_len;
  std::memcpy(nonce.data(), blob.data() + off, nonce.size());
  off += nonce.size();
  std::memcpy(tag.data(), blob.data() + off, tag.size());
  off += tag.size();
  cipher.assign(blob.begin() + static_cast<std::ptrdiff_t>(off), blob.end());
  return true;
}

}  // namespace

bool SecureStoreSupported() {
#if defined(__APPLE__) || defined(__linux__)
  return true;
#else
  return false;
#endif
}

bool ProtectSecureBlob(const std::vector<std::uint8_t>& plain,
                       const std::uint8_t* entropy,
                       std::size_t entropy_len,
                       std::vector<std::uint8_t>& out,
                       std::string& error) {
  error.clear();
  out.clear();
  if (plain.empty()) {
    error = "secure store plain empty";
    return false;
  }

  std::array<std::uint8_t, kKeyBytes> key{};
  if (!GetOrCreateMasterKey(key, error)) {
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
  crypto_aead_lock(cipher.data(), tag.data(), key.data(), nonce.data(), ad,
                   ad_len, plain.data(), plain.size());

  const std::size_t magic_len = sizeof(kBlobMagic) - 1;
  out.reserve(magic_len + nonce.size() + tag.size() + cipher.size());
  out.insert(out.end(), kBlobMagic, kBlobMagic + magic_len);
  out.insert(out.end(), nonce.begin(), nonce.end());
  out.insert(out.end(), tag.begin(), tag.end());
  out.insert(out.end(), cipher.begin(), cipher.end());
  return true;
}

bool UnprotectSecureBlob(const std::vector<std::uint8_t>& blob,
                         const std::uint8_t* entropy,
                         std::size_t entropy_len,
                         std::vector<std::uint8_t>& out,
                         std::string& error) {
  error.clear();
  out.clear();
  if (blob.empty()) {
    error = "secure store blob empty";
    return false;
  }
  std::array<std::uint8_t, kKeyBytes> key{};
  if (!GetOrCreateMasterKey(key, error)) {
    return false;
  }
  std::array<std::uint8_t, kNonceBytes> nonce{};
  std::array<std::uint8_t, kTagBytes> tag{};
  std::vector<std::uint8_t> cipher;
  if (!ParseEncryptedBlob(blob, nonce, tag, cipher)) {
    error = "secure store blob invalid";
    return false;
  }
  out.resize(cipher.size());
  const std::uint8_t* ad = entropy_len > 0 ? entropy : nullptr;
  const std::size_t ad_len = entropy_len > 0 ? entropy_len : 0;
  const int rc = crypto_aead_unlock(out.data(), tag.data(), key.data(),
                                    nonce.data(), ad, ad_len, cipher.data(),
                                    cipher.size());
  if (rc != 0) {
    out.clear();
    error = "secure store auth failed";
    return false;
  }
  return true;
}

bool ProtectSecureBlobScoped(const std::vector<std::uint8_t>& plain,
                             const std::uint8_t* entropy,
                             std::size_t entropy_len,
                             SecureStoreScope /*scope*/,
                             std::vector<std::uint8_t>& out,
                             std::string& error) {
  return ProtectSecureBlob(plain, entropy, entropy_len, out, error);
}

bool UnprotectSecureBlobScoped(const std::vector<std::uint8_t>& blob,
                               const std::uint8_t* entropy,
                               std::size_t entropy_len,
                               SecureStoreScope /*scope*/,
                               std::vector<std::uint8_t>& out,
                               std::string& error) {
  return UnprotectSecureBlob(blob, entropy, entropy_len, out, error);
}

}  // namespace mi::platform
