#include "sync_service.h"

#include "client_core.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "crypto.h"
#include "secure_buffer.h"
#include "secure_store_util.h"
#include "hex_utils.h"
#include "monocypher.h"
#include "path_security.h"
#include "platform_fs.h"
#include "platform_random.h"
#include "protocol.h"
#include "trust_store.h"

namespace mi::client {

using DevicePairingRequest = ClientCore::DevicePairingRequest;

namespace pfs = mi::platform::fs;

namespace {

constexpr std::size_t kMaxDeviceSyncKeyFileBytes = 64u * 1024u;


std::string BytesToHexLower(const std::uint8_t* data, std::size_t len) {
  static constexpr char kHex[] = "0123456789abcdef";
  if (!data || len == 0) {
    return {};
  }
  std::string out;
  out.resize(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    out[i * 2] = kHex[data[i] >> 4];
    out[i * 2 + 1] = kHex[data[i] & 0x0F];
  }
  return out;
}

bool RandomBytes(std::uint8_t* out, std::size_t len) {
  return mi::platform::RandomBytes(out, len);
}

bool IsAllZero(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return true;
  }
  std::uint8_t acc = 0;
  for (std::size_t i = 0; i < len; ++i) {
    acc |= data[i];
  }
  return acc == 0;
}

bool HexToFixedBytes16(const std::string& hex,
                       std::array<std::uint8_t, 16>& out) {
  std::vector<std::uint8_t> tmp;
  if (!mi::common::HexToBytes(hex, tmp) || tmp.size() != out.size()) {
    return false;
  }
  std::memcpy(out.data(), tmp.data(), out.size());
  return true;
}

bool WriteFixed16(const std::array<std::uint8_t, 16>& v,
                  std::vector<std::uint8_t>& out) {
  out.insert(out.end(), v.begin(), v.end());
  return true;
}

bool ReadFixed16(const std::vector<std::uint8_t>& data, std::size_t& offset,
                 std::array<std::uint8_t, 16>& out) {
  if (offset + out.size() > data.size()) {
    return false;
  }
  std::memcpy(out.data(), data.data() + offset, out.size());
  offset += out.size();
  return true;
}

bool ParsePairingCodeSecret16(const std::string& pairing_code,
                              std::array<std::uint8_t, 16>& out_secret) {
  out_secret.fill(0);
  const std::string norm = security::NormalizeCode(pairing_code);
  std::vector<std::uint8_t> bytes;
  if (!mi::common::HexToBytes(norm, bytes) ||
      bytes.size() != out_secret.size()) {
    if (!bytes.empty()) {
      crypto_wipe(bytes.data(), bytes.size());
    }
    return false;
  }
  std::memcpy(out_secret.data(), bytes.data(), out_secret.size());
  crypto_wipe(bytes.data(), bytes.size());
  return true;
}

bool DerivePairingIdAndKey(const std::array<std::uint8_t, 16>& secret,
                           std::string& out_pairing_id_hex,
                           std::array<std::uint8_t, 32>& out_key) {
  out_pairing_id_hex.clear();
  out_key.fill(0);
  static constexpr char kIdPrefix[] = "mi_e2ee_pairing_id_v1";
  std::vector<std::uint8_t> buf;
  buf.reserve(sizeof(kIdPrefix) - 1 + secret.size());
  buf.insert(buf.end(),
             reinterpret_cast<const std::uint8_t*>(kIdPrefix),
             reinterpret_cast<const std::uint8_t*>(kIdPrefix) +
                 (sizeof(kIdPrefix) - 1));
  buf.insert(buf.end(), secret.begin(), secret.end());
  const std::string digest = mi::common::Sha256Hex(buf.data(), buf.size());
  if (!buf.empty()) {
    crypto_wipe(buf.data(), buf.size());
  }
  if (digest.size() < 32) {
    return false;
  }
  out_pairing_id_hex = digest.substr(0, 32);

  static constexpr char kInfo[] = "mi_e2ee_pairing_key_v1";
  if (!mi::server::crypto::HkdfSha256(
          secret.data(), secret.size(), nullptr, 0,
          reinterpret_cast<const std::uint8_t*>(kInfo), std::strlen(kInfo),
          out_key.data(), out_key.size())) {
    out_pairing_id_hex.clear();
    out_key.fill(0);
    return false;
  }
  return true;
}

bool EncryptPairingPayload(const std::array<std::uint8_t, 32>& key,
                           const std::vector<std::uint8_t>& plaintext,
                           std::vector<std::uint8_t>& out_cipher) {
  out_cipher.clear();
  if (plaintext.empty()) {
    return false;
  }
  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'P', 'Y'};
  static constexpr std::uint8_t kVer = 1;
  std::uint8_t ad[5];
  std::memcpy(ad + 0, kMagic, sizeof(kMagic));
  ad[4] = kVer;

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    return false;
  }

  out_cipher.resize(sizeof(ad) + nonce.size() + 16 + plaintext.size());
  std::memcpy(out_cipher.data(), ad, sizeof(ad));
  std::memcpy(out_cipher.data() + sizeof(ad), nonce.data(), nonce.size());
  std::uint8_t* mac = out_cipher.data() + sizeof(ad) + nonce.size();
  std::uint8_t* cipher = mac + 16;

  crypto_aead_lock(cipher, mac, key.data(), nonce.data(), ad, sizeof(ad),
                   plaintext.data(), plaintext.size());
  return true;
}

bool DecryptPairingPayload(const std::array<std::uint8_t, 32>& key,
                           const std::vector<std::uint8_t>& cipher,
                           std::vector<std::uint8_t>& out_plaintext) {
  out_plaintext.clear();
  if (cipher.size() < (5 + 24 + 16 + 1)) {
    return false;
  }
  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'P', 'Y'};
  if (std::memcmp(cipher.data(), kMagic, sizeof(kMagic)) != 0) {
    return false;
  }
  if (cipher[4] != 1) {
    return false;
  }

  const std::uint8_t* ad = cipher.data();
  static constexpr std::size_t kAdSize = 5;
  const std::uint8_t* nonce = cipher.data() + kAdSize;
  const std::uint8_t* mac = nonce + 24;
  const std::uint8_t* ctext = mac + 16;
  const std::size_t ctext_len = cipher.size() - kAdSize - 24 - 16;

  out_plaintext.resize(ctext_len);
  const int rc =
      crypto_aead_unlock(out_plaintext.data(), mac, key.data(), nonce, ad,
                         kAdSize, ctext, ctext_len);
  if (rc != 0) {
    out_plaintext.clear();
    return false;
  }
  return true;
}

bool EncodePairingRequestPlain(const std::string& device_id,
                               const std::array<std::uint8_t, 16>& request_id,
                               std::vector<std::uint8_t>& out) {
  out.clear();
  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'P', 'R'};
  static constexpr std::uint8_t kVer = 1;
  out.insert(out.end(), kMagic, kMagic + sizeof(kMagic));
  out.push_back(kVer);
  WriteFixed16(request_id, out);
  return mi::server::proto::WriteString(device_id, out);
}

bool DecodePairingRequestPlain(const std::vector<std::uint8_t>& plain,
                               std::string& out_device_id,
                               std::array<std::uint8_t, 16>& out_request_id) {
  out_device_id.clear();
  out_request_id.fill(0);
  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'P', 'R'};
  if (plain.size() < (sizeof(kMagic) + 1 + out_request_id.size())) {
    return false;
  }
  std::size_t off = 0;
  if (std::memcmp(plain.data(), kMagic, sizeof(kMagic)) != 0) {
    return false;
  }
  off += sizeof(kMagic);
  if (plain[off++] != 1) {
    return false;
  }
  if (!ReadFixed16(plain, off, out_request_id)) {
    return false;
  }
  return mi::server::proto::ReadString(plain, off, out_device_id) &&
         off == plain.size();
}

bool EncodePairingResponsePlain(const std::array<std::uint8_t, 16>& request_id,
                                const std::array<std::uint8_t, 32>& device_sync_key,
                                std::vector<std::uint8_t>& out) {
  out.clear();
  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'P', 'S'};
  static constexpr std::uint8_t kVer = 1;
  out.insert(out.end(), kMagic, kMagic + sizeof(kMagic));
  out.push_back(kVer);
  WriteFixed16(request_id, out);
  out.insert(out.end(), device_sync_key.begin(), device_sync_key.end());
  return true;
}

bool DecodePairingResponsePlain(const std::vector<std::uint8_t>& plain,
                                std::array<std::uint8_t, 16>& out_request_id,
                                std::array<std::uint8_t, 32>& out_device_sync_key) {
  out_request_id.fill(0);
  out_device_sync_key.fill(0);
  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'P', 'S'};
  if (plain.size() != (sizeof(kMagic) + 1 + out_request_id.size() +
                       out_device_sync_key.size())) {
    return false;
  }
  std::size_t off = 0;
  if (std::memcmp(plain.data(), kMagic, sizeof(kMagic)) != 0) {
    return false;
  }
  off += sizeof(kMagic);
  if (plain[off++] != 1) {
    return false;
  }
  if (!ReadFixed16(plain, off, out_request_id)) {
    return false;
  }
  if (off + out_device_sync_key.size() != plain.size()) {
    return false;
  }
  std::memcpy(out_device_sync_key.data(), plain.data() + off,
              out_device_sync_key.size());
  return true;
}

}  // namespace

bool SyncService::LoadDeviceSyncKey(ClientCore& core) const {
  core.device_sync_key_loaded_ = false;
  core.device_sync_key_.fill(0);
  if (!core.device_sync_enabled_) {
    return true;
  }
  if (core.device_sync_key_path_.empty()) {
    core.last_error_ = "device sync key path empty";
    return false;
  }

  std::error_code ec;
  std::uint64_t size = 0;
  bool exists = false;
  if (pfs::Exists(core.device_sync_key_path_, ec)) {
    exists = true;
    if (ec) {
      core.last_error_ = "device sync key path error";
      return false;
    }
    size = pfs::FileSize(core.device_sync_key_path_, ec);
    if (ec) {
      core.last_error_ = "device sync key size stat failed";
      return false;
    }
    if (size > kMaxDeviceSyncKeyFileBytes) {
      core.last_error_ = "device sync key too large";
      return false;
    }
    std::string perm_err;
    if (!mi::shard::security::CheckPathNotWorldWritable(
            core.device_sync_key_path_, perm_err)) {
      core.last_error_ = perm_err.empty()
                             ? "device sync key permissions insecure"
                             : perm_err;
      return false;
    }
  } else if (ec) {
    core.last_error_ = "device sync key path error";
    return false;
  }

  std::vector<std::uint8_t> bytes;
  if (exists) {
    std::ifstream f(core.device_sync_key_path_, std::ios::binary);
    if (!f.is_open()) {
      core.last_error_ = "device sync key read failed";
      return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    if (!bytes.empty()) {
      f.read(reinterpret_cast<char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
      if (!f || f.gcount() != static_cast<std::streamsize>(bytes.size())) {
        core.last_error_ = "device sync key read failed";
        return false;
      }
    }
  }

  if (!bytes.empty()) {
    std::vector<std::uint8_t> plain;
    bool was_dpapi = false;
    static constexpr char kMagic[] = "MI_E2EE_DEVICE_SYNC_KEY_DPAPI1";
    static constexpr char kEntropy[] = "MI_E2EE_DEVICE_SYNC_KEY_ENTROPY_V1";
    std::string dpapi_err;
    if (!MaybeUnprotectSecureStore(bytes, kMagic, kEntropy, plain, was_dpapi,
                             dpapi_err)) {
      core.last_error_ =
          dpapi_err.empty() ? "device sync key unprotect failed" : dpapi_err;
      return false;
    }
    if (plain.size() != core.device_sync_key_.size()) {
      core.last_error_ = "device sync key size invalid";
      return false;
    }
    std::array<std::uint8_t, 32> key{};
    std::memcpy(key.data(), plain.data(), key.size());
    if (!was_dpapi) {
      if (!core.StoreDeviceSyncKey(key)) {
        return false;
      }
      return true;
    }
    core.device_sync_key_ = key;
    core.device_sync_key_loaded_ = true;
    return true;
  }

  if (!core.device_sync_is_primary_) {
    core.last_error_ = "device sync key missing (linked device)";
    return false;
  }

  std::array<std::uint8_t, 32> k{};
  if (!RandomBytes(k.data(), k.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  return core.StoreDeviceSyncKey(k);
}

bool SyncService::StoreDeviceSyncKey(ClientCore& core, const std::array<std::uint8_t, 32>& key) const {
  core.last_error_.clear();
  if (!core.device_sync_enabled_) {
    core.last_error_ = "device sync disabled";
    return false;
  }
  if (core.device_sync_key_path_.empty()) {
    core.last_error_ = "device sync key path empty";
    return false;
  }
  if (IsAllZero(key.data(), key.size())) {
    core.last_error_ = "device sync key invalid";
    return false;
  }

  std::error_code ec;
  const auto dir = core.device_sync_key_path_.has_parent_path()
                       ? core.device_sync_key_path_.parent_path()
                       : std::filesystem::path{};
  if (!dir.empty()) {
    pfs::CreateDirectories(dir, ec);
  }
  std::string perm_err;
  if (!mi::shard::security::CheckPathNotWorldWritable(core.device_sync_key_path_,
                                                      perm_err)) {
    core.last_error_ = perm_err.empty() ? "device sync key permissions insecure"
                                   : perm_err;
    return false;
  }

  std::vector<std::uint8_t> plain(key.begin(), key.end());
  mi::common::ScopedWipe plain_wipe(plain);
  static constexpr char kMagic[] = "MI_E2EE_DEVICE_SYNC_KEY_DPAPI1";
  static constexpr char kEntropy[] = "MI_E2EE_DEVICE_SYNC_KEY_ENTROPY_V1";
  std::string wrap_err;
  std::vector<std::uint8_t> wrapped;
  if (!ProtectSecureStore(plain, kMagic, kEntropy, wrapped, wrap_err)) {
    core.last_error_ = wrap_err.empty() ? "device sync key protect failed" : wrap_err;
    return false;
  }
  std::vector<std::uint8_t> out_bytes = std::move(wrapped);

  std::error_code write_ec;
  if (!pfs::AtomicWrite(core.device_sync_key_path_, out_bytes.data(),
                        out_bytes.size(), write_ec)) {
    core.last_error_ = "device sync key write failed";
    return false;
  }
#ifndef _WIN32
  {
    std::error_code perm_ec;
    std::filesystem::permissions(
        core.device_sync_key_path_,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace, perm_ec);
  }
#endif

  if (!IsAllZero(core.device_sync_key_.data(), core.device_sync_key_.size())) {
    crypto_wipe(core.device_sync_key_.data(), core.device_sync_key_.size());
  }
  core.device_sync_key_ = key;
  core.device_sync_key_loaded_ = true;
  return true;
}

bool SyncService::EncryptDeviceSync(ClientCore& core, const std::vector<std::uint8_t>& plaintext,
                                   std::vector<std::uint8_t>& out_cipher) const {
  out_cipher.clear();
  if (!core.device_sync_enabled_) {
    core.last_error_ = "device sync disabled";
    return false;
  }
  if (!core.device_sync_key_loaded_) {
    core.last_error_ = "device sync key missing";
    return false;
  }
  if (plaintext.empty()) {
    core.last_error_ = "device sync plaintext empty";
    return false;
  }

  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'S', 'Y'};
  static constexpr std::uint8_t kVer = 1;
  std::uint8_t ad[5];
  std::memcpy(ad + 0, kMagic, sizeof(kMagic));
  ad[4] = kVer;

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    core.last_error_ = "rng failed";
    return false;
  }

  out_cipher.resize(sizeof(ad) + nonce.size() + 16 + plaintext.size());
  std::memcpy(out_cipher.data(), ad, sizeof(ad));
  std::memcpy(out_cipher.data() + sizeof(ad), nonce.data(), nonce.size());
  std::uint8_t* mac = out_cipher.data() + sizeof(ad) + nonce.size();
  std::uint8_t* cipher = mac + 16;

  crypto_aead_lock(cipher, mac, core.device_sync_key_.data(), nonce.data(), ad,
                   sizeof(ad), plaintext.data(), plaintext.size());
  return true;
}

bool SyncService::DecryptDeviceSync(ClientCore& core, const std::vector<std::uint8_t>& cipher,
                                   std::vector<std::uint8_t>& out_plaintext) const {
  out_plaintext.clear();
  if (!core.device_sync_enabled_) {
    core.last_error_ = "device sync disabled";
    return false;
  }
  if (!core.device_sync_key_loaded_) {
    core.last_error_ = "device sync key missing";
    return false;
  }
  if (cipher.size() < (5 + 24 + 16 + 1)) {
    core.last_error_ = "device sync cipher invalid";
    return false;
  }
  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'S', 'Y'};
  if (std::memcmp(cipher.data(), kMagic, sizeof(kMagic)) != 0) {
    core.last_error_ = "device sync magic mismatch";
    return false;
  }
  if (cipher[4] != 1) {
    core.last_error_ = "device sync version mismatch";
    return false;
  }

  const std::uint8_t* ad = cipher.data();
  static constexpr std::size_t kAdSize = 5;
  const std::uint8_t* nonce = cipher.data() + kAdSize;
  const std::uint8_t* mac = nonce + 24;
  const std::uint8_t* ctext = mac + 16;
  const std::size_t ctext_len = cipher.size() - kAdSize - 24 - 16;

  out_plaintext.resize(ctext_len);
  const int rc = crypto_aead_unlock(out_plaintext.data(), mac,
                                    core.device_sync_key_.data(), nonce, ad, kAdSize,
                                    ctext, ctext_len);
  if (rc != 0) {
    out_plaintext.clear();
    core.last_error_ = "device sync auth failed";
    return false;
  }
  return true;
}

bool SyncService::PushDeviceSyncCiphertext(ClientCore& core, const std::vector<std::uint8_t>& cipher) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (!core.device_sync_enabled_) {
    core.last_error_ = "device sync disabled";
    return false;
  }
  if (!core.LoadOrCreateDeviceId()) {
    if (core.last_error_.empty()) {
      core.last_error_ = "device id unavailable";
    }
    return false;
  }
  if (cipher.empty()) {
    core.last_error_ = "payload empty";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(core.device_id_, plain);
  mi::server::proto::WriteBytes(cipher, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kDeviceSyncPush, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "device sync push failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "device sync push response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "device sync push failed" : server_err;
    return false;
  }
  return true;
}

std::vector<std::vector<std::uint8_t>> SyncService::PullDeviceSyncCiphertexts(ClientCore& core) const {
  std::vector<std::vector<std::uint8_t>> out;
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return out;
  }
  if (!core.device_sync_enabled_) {
    core.last_error_ = "device sync disabled";
    return out;
  }
  if (!core.LoadOrCreateDeviceId()) {
    if (core.last_error_.empty()) {
      core.last_error_ = "device id unavailable";
    }
    return out;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(core.device_id_, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kDeviceSyncPull, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "device sync pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "device sync pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "device sync pull failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    core.last_error_ = "device sync pull response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::vector<std::uint8_t> msg;
    if (!mi::server::proto::ReadBytes(resp_payload, off, msg)) {
      out.clear();
      core.last_error_ = "device sync pull response invalid";
      return out;
    }
    out.push_back(std::move(msg));
  }
  if (off != resp_payload.size()) {
    out.clear();
    core.last_error_ = "device sync pull response invalid";
    return out;
  }
  return out;
}

bool SyncService::BeginDevicePairingPrimary(ClientCore& core, std::string& out_pairing_code) const {
  out_pairing_code.clear();
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (!core.device_sync_enabled_) {
    core.last_error_ = "device sync disabled";
    return false;
  }
  if (!core.device_sync_is_primary_) {
    core.last_error_ = "not primary device";
    return false;
  }
  if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
    return false;
  }
  if (!core.device_sync_key_loaded_) {
    core.last_error_ = "device sync key missing";
    return false;
  }

  std::array<std::uint8_t, 16> secret{};
  if (!RandomBytes(secret.data(), secret.size())) {
    core.last_error_ = "rng failed";
    return false;
  }

  std::string pairing_id;
  std::array<std::uint8_t, 32> key{};
  if (!DerivePairingIdAndKey(secret, pairing_id, key)) {
    core.last_error_ = "pairing derive failed";
    crypto_wipe(secret.data(), secret.size());
    return false;
  }

  out_pairing_code =
      mi::common::GroupHex4(BytesToHexLower(secret.data(), secret.size()));
  crypto_wipe(secret.data(), secret.size());

  core.pairing_active_ = true;
  core.pairing_is_primary_ = true;
  core.pairing_wait_response_ = false;
  core.pairing_id_hex_ = std::move(pairing_id);
  core.pairing_key_ = key;
  core.pairing_request_id_.fill(0);
  return true;
}

std::vector<ClientCore::DevicePairingRequest> SyncService::PollDevicePairingRequests(ClientCore& core) const {
  std::vector<DevicePairingRequest> out;
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return out;
  }
  if (!core.pairing_active_ || !core.pairing_is_primary_ || core.pairing_id_hex_.empty() ||
      IsAllZero(core.pairing_key_.data(), core.pairing_key_.size())) {
    core.last_error_ = "pairing not active";
    return out;
  }

  std::vector<std::uint8_t> plain;
  plain.push_back(0);  // pull requests
  mi::server::proto::WriteString(core.pairing_id_hex_, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kDevicePairingPull, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "pairing pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "pairing pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "pairing pull failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    core.last_error_ = "pairing pull response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::vector<std::uint8_t> msg;
    if (!mi::server::proto::ReadBytes(resp_payload, off, msg)) {
      out.clear();
      core.last_error_ = "pairing pull response invalid";
      return out;
    }
    std::vector<std::uint8_t> plain_msg;
    if (!DecryptPairingPayload(core.pairing_key_, msg, plain_msg)) {
      continue;
    }
    std::string device_id;
    std::array<std::uint8_t, 16> request_id{};
    if (!DecodePairingRequestPlain(plain_msg, device_id, request_id)) {
      continue;
    }
    if (device_id.empty() || device_id == core.device_id_) {
      continue;
    }
    DevicePairingRequest r;
    r.device_id = std::move(device_id);
    r.request_id_hex = BytesToHexLower(request_id.data(), request_id.size());
    out.push_back(std::move(r));
  }
  if (off != resp_payload.size()) {
    out.clear();
    core.last_error_ = "pairing pull response invalid";
    return out;
  }
  return out;
}

bool SyncService::ApproveDevicePairingRequest(ClientCore& core, const DevicePairingRequest& request) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (!core.pairing_active_ || !core.pairing_is_primary_ || core.pairing_id_hex_.empty() ||
      IsAllZero(core.pairing_key_.data(), core.pairing_key_.size())) {
    core.last_error_ = "pairing not active";
    return false;
  }
  if (!core.device_sync_enabled_ || !core.device_sync_is_primary_) {
    core.last_error_ = "device sync not primary";
    return false;
  }
  if (request.device_id.empty() || request.request_id_hex.empty()) {
    core.last_error_ = "invalid request";
    return false;
  }
  if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
    return false;
  }
  if (!core.device_sync_key_loaded_) {
    core.last_error_ = "device sync key missing";
    return false;
  }

  std::array<std::uint8_t, 16> req_id{};
  if (!HexToFixedBytes16(security::NormalizeCode(request.request_id_hex),
                         req_id)) {
    core.last_error_ = "invalid request id";
    return false;
  }

  std::vector<std::uint8_t> plain_resp;
  if (!EncodePairingResponsePlain(req_id, core.device_sync_key_, plain_resp)) {
    core.last_error_ = "pairing encode failed";
    return false;
  }

  std::vector<std::uint8_t> cipher_resp;
  if (!EncryptPairingPayload(core.pairing_key_, plain_resp, cipher_resp)) {
    core.last_error_ = "pairing encrypt failed";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(core.pairing_id_hex_, plain);
  mi::server::proto::WriteString(request.device_id, plain);
  mi::server::proto::WriteBytes(cipher_resp, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kDevicePairingRespond, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "pairing respond failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "pairing respond response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "pairing respond failed" : server_err;
    return false;
  }
  if (resp_payload.size() != 1) {
    core.last_error_ = "pairing respond response invalid";
    return false;
  }

  {
    const std::string saved_err = core.last_error_;
    core.BestEffortBroadcastDeviceSyncHistorySnapshot(request.device_id);
    core.last_error_ = saved_err;
  }
  core.CancelDevicePairing();
  return true;
}

bool SyncService::BeginDevicePairingLinked(ClientCore& core, const std::string& pairing_code) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (!core.device_sync_enabled_) {
    core.last_error_ = "device sync disabled";
    return false;
  }
  if (core.device_sync_key_loaded_) {
    core.last_error_ = "device sync key already present";
    return false;
  }
  if (pairing_code.empty()) {
    core.last_error_ = "pairing code empty";
    return false;
  }

  std::array<std::uint8_t, 16> secret{};
  if (!ParsePairingCodeSecret16(pairing_code, secret)) {
    core.last_error_ = "pairing code invalid";
    return false;
  }
  std::string pairing_id;
  std::array<std::uint8_t, 32> key{};
  if (!DerivePairingIdAndKey(secret, pairing_id, key)) {
    crypto_wipe(secret.data(), secret.size());
    core.last_error_ = "pairing derive failed";
    return false;
  }
  crypto_wipe(secret.data(), secret.size());

  if (!core.LoadOrCreateDeviceId() || core.device_id_.empty()) {
    core.last_error_ = core.last_error_.empty() ? "device id unavailable" : core.last_error_;
    return false;
  }
  {
    const std::string saved_err = core.last_error_;
    (void)core.PullDeviceSyncCiphertexts();
    core.last_error_ = saved_err;
  }

  std::array<std::uint8_t, 16> request_id{};
  if (!RandomBytes(request_id.data(), request_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }

  std::vector<std::uint8_t> req_plain;
  if (!EncodePairingRequestPlain(core.device_id_, request_id, req_plain)) {
    core.last_error_ = "pairing encode failed";
    return false;
  }
  std::vector<std::uint8_t> req_cipher;
  if (!EncryptPairingPayload(key, req_plain, req_cipher)) {
    core.last_error_ = "pairing encrypt failed";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(pairing_id, plain);
  mi::server::proto::WriteBytes(req_cipher, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kDevicePairingRequest, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "pairing request failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "pairing request response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "pairing request failed" : server_err;
    return false;
  }
  if (resp_payload.size() != 1) {
    core.last_error_ = "pairing request response invalid";
    return false;
  }

  core.pairing_active_ = true;
  core.pairing_is_primary_ = false;
  core.pairing_wait_response_ = true;
  core.pairing_id_hex_ = std::move(pairing_id);
  core.pairing_key_ = key;
  core.pairing_request_id_ = request_id;
  return true;
}

bool SyncService::PollDevicePairingLinked(ClientCore& core, bool& out_completed) const {
  out_completed = false;
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (!core.pairing_active_ || core.pairing_is_primary_ || !core.pairing_wait_response_ ||
      core.pairing_id_hex_.empty() ||
      IsAllZero(core.pairing_key_.data(), core.pairing_key_.size()) ||
      IsAllZero(core.pairing_request_id_.data(), core.pairing_request_id_.size())) {
    core.last_error_ = "pairing not pending";
    return false;
  }
  if (core.device_id_.empty()) {
    core.LoadOrCreateDeviceId();
  }
  if (core.device_id_.empty()) {
    core.last_error_ = "device id unavailable";
    return false;
  }

  std::vector<std::uint8_t> plain;
  plain.push_back(1);  // pull responses
  mi::server::proto::WriteString(core.pairing_id_hex_, plain);
  mi::server::proto::WriteString(core.device_id_, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kDevicePairingPull, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "pairing pull failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "pairing pull response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "pairing pull failed" : server_err;
    return false;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    core.last_error_ = "pairing pull response invalid";
    return false;
  }

  for (std::uint32_t i = 0; i < count; ++i) {
    std::vector<std::uint8_t> msg;
    if (!mi::server::proto::ReadBytes(resp_payload, off, msg)) {
      core.last_error_ = "pairing pull response invalid";
      return false;
    }
    std::vector<std::uint8_t> plain_msg;
    if (!DecryptPairingPayload(core.pairing_key_, msg, plain_msg)) {
      continue;
    }
    std::array<std::uint8_t, 16> req_id{};
    std::array<std::uint8_t, 32> sync_key{};
    if (!DecodePairingResponsePlain(plain_msg, req_id, sync_key)) {
      continue;
    }
    if (req_id != core.pairing_request_id_) {
      continue;
    }
    if (!core.StoreDeviceSyncKey(sync_key)) {
      return false;
    }
    core.CancelDevicePairing();
    out_completed = true;
    return true;
  }
  if (off != resp_payload.size()) {
    core.last_error_ = "pairing pull response invalid";
    return false;
  }

  return true;
}

void SyncService::CancelDevicePairing(ClientCore& core) const {
  core.pairing_active_ = false;
  core.pairing_is_primary_ = false;
  core.pairing_wait_response_ = false;
  core.pairing_id_hex_.clear();
  if (!IsAllZero(core.pairing_key_.data(), core.pairing_key_.size())) {
    crypto_wipe(core.pairing_key_.data(), core.pairing_key_.size());
  }
  core.pairing_key_.fill(0);
  if (!IsAllZero(core.pairing_request_id_.data(), core.pairing_request_id_.size())) {
    crypto_wipe(core.pairing_request_id_.data(), core.pairing_request_id_.size());
  }
  core.pairing_request_id_.fill(0);
}

}  // namespace mi::client

