#include "platform_secure_store.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "monocypher.h"
#include "platform_fs.h"
#include "platform_random.h"

namespace mi::platform {

namespace {
constexpr char kBlobMagic[] = "MI_E2EE_SECURE_STORE_V1";
constexpr std::size_t kKeyBytes = 32;
constexpr std::size_t kNonceBytes = 24;
constexpr std::size_t kTagBytes = 16;
constexpr char kKeyFileName[] = "mi_e2ee_secure_store.key";

std::filesystem::path ResolveBaseDir() {
  if (const char* env = std::getenv("MI_E2EE_DATA_DIR")) {
    if (*env != '\0') {
      return std::filesystem::path(env);
    }
  }
  std::error_code ec;
  auto cwd = fs::CurrentPath(ec);
  if (!cwd.empty()) {
    return cwd;
  }
  return std::filesystem::path{"."};
}

std::filesystem::path KeyPath() {
  return ResolveBaseDir() / kKeyFileName;
}

bool ReadKeyFile(std::array<std::uint8_t, kKeyBytes>& key, std::string& error) {
  error.clear();
  const auto path = KeyPath();
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    error = "secure store key not found";
    return false;
  }
  f.read(reinterpret_cast<char*>(key.data()), key.size());
  if (!f || f.gcount() != static_cast<std::streamsize>(key.size())) {
    error = "secure store key read failed";
    return false;
  }
  return true;
}

bool WriteKeyFile(const std::array<std::uint8_t, kKeyBytes>& key,
                  std::string& error) {
  error.clear();
  const auto path = KeyPath();
  std::error_code ec;
  if (path.has_parent_path()) {
    fs::CreateDirectories(path.parent_path(), ec);
    if (ec) {
      error = "secure store dir create failed";
      return false;
    }
  }
  if (!fs::AtomicWrite(path, key.data(), key.size(), ec)) {
    error = "secure store key write failed";
    return false;
  }
  return true;
}

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

  std::string read_err;
  if (!ReadKeyFile(key, read_err)) {
    if (!RandomBytes(key.data(), key.size())) {
      error = "secure store rng failed";
      return false;
    }
    std::string write_err;
    if (!WriteKeyFile(key, write_err)) {
      error = write_err.empty() ? "secure store key write failed" : write_err;
      return false;
    }
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
  return true;
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
