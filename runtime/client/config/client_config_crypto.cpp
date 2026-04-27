#include "client_config_crypto.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iterator>

#include "crypto.h"
#include "monocypher.h"
#include "platform_random.h"
#include "secure_buffer.h"

namespace mi::client {
namespace {

constexpr char kMagic[] = "MI_E2EE_CLIENT_CONFIG_V1\n";
constexpr std::size_t kMagicLen = sizeof(kMagic) - 1;
constexpr std::size_t kNonceLen = 24;
constexpr std::size_t kTagLen = 16;

constexpr std::uint8_t kDefaultKeyMaterial[] = {
    0x6d, 0x69, 0x5f, 0x65, 0x32, 0x65, 0x65, 0x5f,
    0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x5f, 0x63,
    0x6f, 0x6e, 0x66, 0x69, 0x67, 0x5f, 0x66, 0x69,
    0x78, 0x65, 0x64, 0x5f, 0x6b, 0x65, 0x79, 0x31};

constexpr std::uint8_t kFileSalt[] = "mi-e2ee-client-config-file-v1";
constexpr std::uint8_t kFileInfo[] = "xchacha20-poly1305 encrypted config";

int HexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

bool ParseKeyHex(const char* hex, std::array<std::uint8_t, 32>& out) {
  if (!hex || std::strlen(hex) == 0) {
    return false;
  }
  if (std::strlen(hex) != out.size() * 2) {
    return false;
  }
  for (std::size_t i = 0; i < out.size(); ++i) {
    const int hi = HexNibble(hex[i * 2]);
    const int lo = HexNibble(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) {
      return false;
    }
    out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
  }
  return true;
}

bool DeriveConfigKey(std::array<std::uint8_t, 32>& key, std::string& error) {
  std::array<std::uint8_t, 32> seed{};
#ifdef MI_E2EE_CLIENT_CONFIG_KEY_HEX
  constexpr const char* kBuildKeyHex = MI_E2EE_CLIENT_CONFIG_KEY_HEX;
#else
  constexpr const char* kBuildKeyHex = "";
#endif
  if (!ParseKeyHex(kBuildKeyHex, seed)) {
    static_assert(sizeof(kDefaultKeyMaterial) == 32,
                  "default client config key material must be 32 bytes");
    std::copy(std::begin(kDefaultKeyMaterial), std::end(kDefaultKeyMaterial),
              seed.begin());
  }

  const bool ok = mi::server::crypto::HkdfSha256(
      seed.data(), seed.size(), kFileSalt, sizeof(kFileSalt) - 1, kFileInfo,
      sizeof(kFileInfo) - 1, key.data(), key.size());
  mi::common::SecureWipe(seed);
  if (!ok) {
    error = "client config key derivation failed";
    return false;
  }
  return true;
}

}  // namespace

bool IsEncryptedClientConfig(const std::vector<std::uint8_t>& data) {
  return data.size() >= kMagicLen &&
         std::memcmp(data.data(), kMagic, kMagicLen) == 0;
}

bool EncryptClientConfigText(std::string_view plaintext,
                             std::vector<std::uint8_t>& out,
                             std::string& error) {
  out.clear();
  error.clear();
  std::array<std::uint8_t, 32> key{};
  if (!DeriveConfigKey(key, error)) {
    return false;
  }
  std::array<std::uint8_t, kNonceLen> nonce{};
  if (!mi::platform::RandomBytes(nonce.data(), nonce.size())) {
    mi::common::SecureWipe(key);
    error = "client config rng failed";
    return false;
  }

  std::vector<std::uint8_t> cipher(plaintext.size());
  std::array<std::uint8_t, kTagLen> tag{};
  crypto_aead_lock(
      cipher.data(), tag.data(), key.data(), nonce.data(),
      reinterpret_cast<const std::uint8_t*>(kMagic), kMagicLen,
      reinterpret_cast<const std::uint8_t*>(plaintext.data()),
      plaintext.size());

  out.reserve(kMagicLen + nonce.size() + tag.size() + cipher.size());
  out.insert(out.end(), kMagic, kMagic + kMagicLen);
  out.insert(out.end(), nonce.begin(), nonce.end());
  out.insert(out.end(), tag.begin(), tag.end());
  out.insert(out.end(), cipher.begin(), cipher.end());

  mi::common::SecureWipe(key);
  mi::common::SecureWipe(cipher);
  mi::common::SecureWipe(tag);
  return true;
}

bool DecryptClientConfigBlob(const std::vector<std::uint8_t>& blob,
                             std::string& plaintext,
                             std::string& error) {
  plaintext.clear();
  error.clear();
  if (!IsEncryptedClientConfig(blob)) {
    error = "client config blob is not encrypted";
    return false;
  }
  const std::size_t min_len = kMagicLen + kNonceLen + kTagLen;
  if (blob.size() < min_len) {
    error = "client config encrypted blob truncated";
    return false;
  }

  std::array<std::uint8_t, 32> key{};
  if (!DeriveConfigKey(key, error)) {
    return false;
  }
  const auto* nonce = blob.data() + kMagicLen;
  const auto* tag = nonce + kNonceLen;
  const auto* cipher = tag + kTagLen;
  const std::size_t cipher_len = blob.size() - min_len;

  std::vector<std::uint8_t> plain(cipher_len);
  const int rc = crypto_aead_unlock(
      plain.data(), tag, key.data(), nonce,
      reinterpret_cast<const std::uint8_t*>(kMagic), kMagicLen, cipher,
      cipher_len);
  mi::common::SecureWipe(key);
  if (rc != 0) {
    mi::common::SecureWipe(plain);
    error = "client config authentication failed";
    return false;
  }
  plaintext.assign(reinterpret_cast<const char*>(plain.data()), plain.size());
  mi::common::SecureWipe(plain);
  return true;
}

bool WriteEncryptedClientConfigText(const std::filesystem::path& path,
                                    std::string_view plaintext,
                                    std::string& error) {
  std::vector<std::uint8_t> blob;
  if (!EncryptClientConfigText(plaintext, blob, error)) {
    return false;
  }
  std::error_code ec;
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      mi::common::SecureWipe(blob);
      error = "client config directory create failed: " + ec.message();
      return false;
    }
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    mi::common::SecureWipe(blob);
    error = "client config open for write failed: " + path.string();
    return false;
  }
  out.write(reinterpret_cast<const char*>(blob.data()),
            static_cast<std::streamsize>(blob.size()));
  const bool ok = out.good();
  mi::common::SecureWipe(blob);
  if (!ok) {
    error = "client config write failed: " + path.string();
    return false;
  }
  return true;
}

EncryptedConfigField::~EncryptedConfigField() { Clear(); }

EncryptedConfigField::EncryptedConfigField(EncryptedConfigField&& other) noexcept
    : blob_(std::move(other.blob_)) {
  other.blob_.clear();
}

EncryptedConfigField& EncryptedConfigField::operator=(
    EncryptedConfigField&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  Clear();
  blob_ = std::move(other.blob_);
  other.blob_.clear();
  return *this;
}

bool EncryptedConfigField::SetPlain(std::string_view value, std::string& error) {
  Clear();
  return EncryptClientConfigText(value, blob_, error);
}

bool EncryptedConfigField::Reveal(std::string& out, std::string& error) const {
  return DecryptClientConfigBlob(blob_, out, error);
}

void EncryptedConfigField::Clear() {
  mi::common::SecureWipe(blob_);
  blob_.clear();
}

}  // namespace mi::client
