#include "metadata_protector.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>

#include "crypto.h"
#include "hex_utils.h"
#include "monocypher.h"
#include "platform_fs.h"
#include "protected_store.h"

namespace mi::server {

namespace {

constexpr std::uint8_t kMetaMagic[4] = {'M', 'I', 'M', 'D'};
constexpr std::uint8_t kMetaVersion = 1;
constexpr std::size_t kMetaHeaderBytes = 4 + 1;
constexpr std::size_t kMetaNonceBytes = 24;
constexpr std::size_t kMetaTagBytes = 16;

void SetOwnerOnlyPermissions(const std::filesystem::path& path) {
#ifdef _WIN32
  std::string acl_err;
  (void)mi::shard::security::HardenPathAcl(path, acl_err);
#else
  std::error_code ec;
  std::filesystem::permissions(
      path, std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace, ec);
#endif
}

bool ParseHexKey(const std::string& hex,
                 std::array<std::uint8_t, 32>& out) {
  std::vector<std::uint8_t> bytes;
  if (!mi::common::HexToBytes(hex, bytes) || bytes.size() != out.size()) {
    return false;
  }
  std::copy(bytes.begin(), bytes.end(), out.begin());
  return true;
}

std::string ToHex(const std::uint8_t* data, std::size_t len) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    out[i * 2] = kHex[(data[i] >> 4) & 0x0F];
    out[i * 2 + 1] = kHex[data[i] & 0x0F];
  }
  return out;
}

}  // namespace

bool LoadOrCreateMetadataKey(const MetadataKeyConfig& cfg,
                             std::array<std::uint8_t, 32>& out_key,
                             std::string& error) {
  error.clear();
  out_key.fill(0);

  if (!cfg.key_hex.empty()) {
    if (!ParseHexKey(cfg.key_hex, out_key)) {
      error = "metadata_key_hex invalid";
      return false;
    }
    return true;
  }

  if (cfg.key_path.empty()) {
    error = "metadata_key path empty";
    return false;
  }

  std::error_code ec;
  if (std::filesystem::exists(cfg.key_path, ec)) {
    std::ifstream ifs(cfg.key_path, std::ios::binary);
    if (!ifs) {
      error = "metadata_key read failed";
      return false;
    }
    ifs.seekg(0, std::ios::end);
    const std::streamsize size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    if (size <= 0 ||
        size > static_cast<std::streamsize>(
                   (std::numeric_limits<std::size_t>::max)())) {
      error = "metadata_key size invalid";
      return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!ifs.read(reinterpret_cast<char*>(bytes.data()), size)) {
      error = "metadata_key read failed";
      return false;
    }
    std::vector<std::uint8_t> plain;
    std::string protect_err;
    if (!DecodeProtectedFileBytes(bytes, cfg.protection, plain, protect_err)) {
      error = protect_err.empty() ? "metadata_key decode failed" : protect_err;
      return false;
    }
    if (plain.size() != out_key.size()) {
      error = "metadata_key size invalid";
      return false;
    }
    std::copy(plain.begin(), plain.end(), out_key.begin());
    return true;
  }

  if (!crypto::RandomBytes(out_key.data(), out_key.size())) {
    error = "metadata_key rng failed";
    return false;
  }
  std::vector<std::uint8_t> plain(out_key.begin(), out_key.end());
  std::vector<std::uint8_t> protected_bytes;
  std::string protect_err;
  if (!EncodeProtectedFileBytes(plain, cfg.protection, protected_bytes,
                                protect_err)) {
    error =
        protect_err.empty() ? "metadata_key protect failed" : protect_err;
    return false;
  }
  std::error_code write_ec;
  if (!mi::platform::fs::AtomicWrite(cfg.key_path, protected_bytes.data(),
                                     protected_bytes.size(), write_ec) ||
      write_ec) {
    error = "metadata_key write failed";
    return false;
  }
  SetOwnerOnlyPermissions(cfg.key_path);
  return true;
}

MetadataProtector::MetadataProtector(
    const std::array<std::uint8_t, 32>& key)
    : key_(key) {}

std::string MetadataProtector::HashId(const std::string& id) const {
  crypto::Sha256Digest digest;
  crypto::HmacSha256(key_.data(), key_.size(),
                     reinterpret_cast<const std::uint8_t*>(id.data()),
                     id.size(), digest);
  return ToHex(digest.bytes.data(), digest.bytes.size());
}

bool MetadataProtector::EncryptBlob(const std::vector<std::uint8_t>& plain,
                                    std::vector<std::uint8_t>& out,
                                    std::string& error) const {
  error.clear();
  std::array<std::uint8_t, kMetaNonceBytes> nonce{};
  if (!crypto::RandomBytes(nonce.data(), nonce.size())) {
    error = "metadata encrypt rng failed";
    return false;
  }

  std::vector<std::uint8_t> cipher;
  cipher.resize(plain.size());
  std::array<std::uint8_t, kMetaTagBytes> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), key_.data(), nonce.data(), nullptr,
                   0, plain.data(), plain.size());

  out.clear();
  out.reserve(kMetaHeaderBytes + nonce.size() + mac.size() + cipher.size());
  out.insert(out.end(), kMetaMagic, kMetaMagic + sizeof(kMetaMagic));
  out.push_back(kMetaVersion);
  out.insert(out.end(), nonce.begin(), nonce.end());
  out.insert(out.end(), mac.begin(), mac.end());
  out.insert(out.end(), cipher.begin(), cipher.end());
  return true;
}

bool MetadataProtector::DecryptBlob(const std::vector<std::uint8_t>& in,
                                    std::vector<std::uint8_t>& out,
                                    std::string& error) const {
  error.clear();
  out.clear();
  if (in.size() < kMetaHeaderBytes + kMetaNonceBytes + kMetaTagBytes) {
    out = in;
    return true;
  }
  if (std::memcmp(in.data(), kMetaMagic, sizeof(kMetaMagic)) != 0) {
    out = in;
    return true;
  }
  if (in[kMetaHeaderBytes - 1] != kMetaVersion) {
    error = "metadata blob version invalid";
    return false;
  }
  std::size_t off = kMetaHeaderBytes;
  std::array<std::uint8_t, kMetaNonceBytes> nonce{};
  std::memcpy(nonce.data(), in.data() + off, nonce.size());
  off += nonce.size();
  std::array<std::uint8_t, kMetaTagBytes> mac{};
  std::memcpy(mac.data(), in.data() + off, mac.size());
  off += mac.size();
  const std::size_t cipher_len = in.size() - off;
  std::vector<std::uint8_t> cipher;
  cipher.resize(cipher_len);
  if (cipher_len != 0) {
    std::memcpy(cipher.data(), in.data() + off, cipher_len);
  }

  out.resize(cipher_len);
  const int ok = crypto_aead_unlock(out.data(), mac.data(), key_.data(),
                                    nonce.data(), nullptr, 0,
                                    cipher.data(), cipher.size());
  if (ok != 0) {
    out.clear();
    error = "metadata blob decrypt failed";
    return false;
  }
  return true;
}

}  // namespace mi::server
