#include "client_core.h"
#include "client_core_helpers.h"
#include "file_blob.h"
#include "media_service.h"
#include "transport_service.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "monocypher.h"
#include "miniz.h"
#include "opaque_pake.h"
#include "ikcp.h"

extern "C" {
int PQCLEAN_MLKEM768_CLEAN_crypto_kem_keypair(std::uint8_t* pk,
                                             std::uint8_t* sk);
int PQCLEAN_MLKEM768_CLEAN_crypto_kem_dec(std::uint8_t* ss,
                                         const std::uint8_t* ct,
                                         const std::uint8_t* sk);
}

#include "c_api.h"
#include "crypto.h"
#include "frame.h"
#include "key_transparency.h"
#include "protocol.h"
#include "chat_history_store.h"
#include "config_service.h"
#include "client_config.h"
#include "hex_utils.h"
#include "payload_padding.h"
#include "secure_buffer.h"
#include "security_service.h"
#include "trust_store.h"
#include "platform_net.h"
#include "platform_random.h"
#include "platform_fs.h"
#include "platform_sys.h"
#include "platform_tls.h"
#include "platform_time.h"

namespace mi::client {

namespace pfs = mi::platform::fs;

namespace {

constexpr std::size_t kMaxDeviceSyncKeyFileBytes = 64u * 1024u;

bool RandomBytes(std::uint8_t* out, std::size_t len) {
  return mi::platform::RandomBytes(out, len);
}

bool RandomUint32(std::uint32_t& out) {
  return mi::platform::RandomUint32(out);
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

std::size_t LargestPowerOfTwoLessThan(std::size_t n) {
  if (n <= 1) {
    return 0;
  }
  std::size_t k = 1;
  while ((k << 1) < n) {
    k <<= 1;
  }
  return k;
}

mi::server::Sha256Hash HashNode(const mi::server::Sha256Hash& left,
                                const mi::server::Sha256Hash& right) {
  std::uint8_t buf[1 + 32 + 32];
  buf[0] = 0x01;
  std::memcpy(buf + 1, left.data(), left.size());
  std::memcpy(buf + 1 + 32, right.data(), right.size());
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(buf, sizeof(buf), d);
  return d.bytes;
}

mi::server::Sha256Hash HashLeaf(const std::vector<std::uint8_t>& leaf_data) {
  std::vector<std::uint8_t> buf;
  buf.reserve(1 + leaf_data.size());
  buf.push_back(0x00);
  buf.insert(buf.end(), leaf_data.begin(), leaf_data.end());
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(buf.data(), buf.size(), d);
  return d.bytes;
}

mi::server::Sha256Hash KtLeafHashFromBundle(const std::string& username,
                                            const std::vector<std::uint8_t>& bundle,
                                            std::string& error) {
  error.clear();
  if (username.empty()) {
    error = "username empty";
    return {};
  }
  if (bundle.size() <
      1 + mi::server::kKtIdentitySigPublicKeyBytes +
          mi::server::kKtIdentityDhPublicKeyBytes) {
    error = "bundle invalid";
    return {};
  }

  std::array<std::uint8_t, mi::server::kKtIdentitySigPublicKeyBytes> id_sig_pk{};
  std::array<std::uint8_t, mi::server::kKtIdentityDhPublicKeyBytes> id_dh_pk{};
  std::memcpy(id_sig_pk.data(), bundle.data() + 1, id_sig_pk.size());
  std::memcpy(id_dh_pk.data(), bundle.data() + 1 + id_sig_pk.size(),
              id_dh_pk.size());

  std::vector<std::uint8_t> leaf_data;
  static constexpr char kPrefix[] = "mi_e2ee_kt_leaf_v1";
  leaf_data.reserve(sizeof(kPrefix) - 1 + 1 + username.size() + 1 +
                    id_sig_pk.size() + id_dh_pk.size());
  leaf_data.insert(leaf_data.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  leaf_data.push_back(0);
  leaf_data.insert(leaf_data.end(), username.begin(), username.end());
  leaf_data.push_back(0);
  leaf_data.insert(leaf_data.end(), id_sig_pk.begin(), id_sig_pk.end());
  leaf_data.insert(leaf_data.end(), id_dh_pk.begin(), id_dh_pk.end());
  return HashLeaf(leaf_data);
}

bool RootFromAuditPath(const mi::server::Sha256Hash& leaf_hash,
                       std::size_t leaf_index, std::size_t tree_size,
                       const std::vector<mi::server::Sha256Hash>& audit_path,
                       mi::server::Sha256Hash& out_root) {
  out_root = {};
  if (tree_size == 0 || leaf_index >= tree_size) {
    return false;
  }

  std::size_t remaining = audit_path.size();
  const auto rec = [&](auto&& self, const mi::server::Sha256Hash& leaf,
                       std::size_t m, std::size_t n, std::size_t& end,
                       mi::server::Sha256Hash& out) -> bool {
    if (n == 1) {
      if (end != 0) {
        return false;
      }
      out = leaf;
      return true;
    }
    if (end == 0) {
      return false;
    }
    const std::size_t k = LargestPowerOfTwoLessThan(n);
    if (k == 0) {
      return false;
    }
    const mi::server::Sha256Hash sibling = audit_path[end - 1];
    end--;
    if (m < k) {
      mi::server::Sha256Hash left{};
      if (!self(self, leaf, m, k, end, left)) {
        return false;
      }
      out = HashNode(left, sibling);
      return true;
    }
    mi::server::Sha256Hash right{};
    if (!self(self, leaf, m - k, n - k, end, right)) {
      return false;
    }
    out = HashNode(sibling, right);
    return true;
  };

  std::size_t end = remaining;
  if (!rec(rec, leaf_hash, leaf_index, tree_size, end, out_root)) {
    return false;
  }
  return end == 0;
}

bool ReconstructConsistencySubproof(
    std::size_t m, std::size_t n, bool b,
    const mi::server::Sha256Hash& old_root,
    const std::vector<mi::server::Sha256Hash>& proof,
    std::size_t& end_index,
    mi::server::Sha256Hash& out_old,
    mi::server::Sha256Hash& out_new) {
  if (m == 0 || n == 0 || m > n) {
    return false;
  }
  if (m == n) {
    if (b) {
      out_old = old_root;
      out_new = old_root;
      return true;
    }
    if (end_index == 0) {
      return false;
    }
    const mi::server::Sha256Hash node = proof[end_index - 1];
    end_index--;
    out_old = node;
    out_new = node;
    return true;
  }
  const std::size_t k = LargestPowerOfTwoLessThan(n);
  if (k == 0 || end_index == 0) {
    return false;
  }
  if (m <= k) {
    const mi::server::Sha256Hash right = proof[end_index - 1];
    end_index--;
    mi::server::Sha256Hash left_old{};
    mi::server::Sha256Hash left_new{};
    if (!ReconstructConsistencySubproof(m, k, b, old_root, proof, end_index,
                                        left_old, left_new)) {
      return false;
    }
    out_old = left_old;
    out_new = HashNode(left_new, right);
    return true;
  }

  const mi::server::Sha256Hash left = proof[end_index - 1];
  end_index--;
  mi::server::Sha256Hash right_old{};
  mi::server::Sha256Hash right_new{};
  if (!ReconstructConsistencySubproof(m - k, n - k, false, old_root, proof,
                                      end_index, right_old, right_new)) {
    return false;
  }
  out_old = HashNode(left, right_old);
  out_new = HashNode(left, right_new);
  return true;
}

bool VerifyConsistencyProof(std::size_t old_size, std::size_t new_size,
                            const mi::server::Sha256Hash& old_root,
                            const mi::server::Sha256Hash& new_root,
                            const std::vector<mi::server::Sha256Hash>& proof) {
  if (old_size == 0 || new_size == 0 || old_size > new_size) {
    return false;
  }
  if (old_size == new_size) {
    return proof.empty() && old_root == new_root;
  }
  std::size_t end = proof.size();
  mi::server::Sha256Hash calc_old{};
  mi::server::Sha256Hash calc_new{};
  if (!ReconstructConsistencySubproof(old_size, new_size, true, old_root, proof,
                                      end, calc_old, calc_new)) {
    return false;
  }
  return end == 0 && calc_old == old_root && calc_new == new_root;
}

constexpr std::uint8_t kGossipMagic[8] = {'M', 'I', 'K', 'T', 'G', 'S', 'P', '1'};

std::vector<std::uint8_t> WrapWithGossip(const std::vector<std::uint8_t>& plain,
                                         std::uint64_t tree_size,
                                         const std::array<std::uint8_t, 32>& root) {
  std::vector<std::uint8_t> out;
  out.reserve(sizeof(kGossipMagic) + 8 + root.size() + 4 + plain.size());
  out.insert(out.end(), std::begin(kGossipMagic), std::end(kGossipMagic));
  mi::server::proto::WriteUint64(tree_size, out);
  out.insert(out.end(), root.begin(), root.end());
  mi::server::proto::WriteUint32(static_cast<std::uint32_t>(plain.size()), out);
  out.insert(out.end(), plain.begin(), plain.end());
  return out;
}

bool UnwrapGossip(const std::vector<std::uint8_t>& in,
                  std::uint64_t& out_tree_size,
                  std::array<std::uint8_t, 32>& out_root,
                  std::vector<std::uint8_t>& out_plain) {
  out_tree_size = 0;
  out_root.fill(0);
  out_plain.clear();
  if (in.size() < sizeof(kGossipMagic) + 8 + 32 + 4) {
    return false;
  }
  if (std::memcmp(in.data(), kGossipMagic, sizeof(kGossipMagic)) != 0) {
    return false;
  }
  std::size_t off = sizeof(kGossipMagic);
  std::uint64_t size = 0;
  if (off + 8 > in.size()) {
    return false;
  }
  for (int i = 0; i < 8; ++i) {
    size |= (static_cast<std::uint64_t>(in[off + static_cast<std::size_t>(i)])
             << (i * 8));
  }
  off += 8;
  if (off + out_root.size() > in.size()) {
    return false;
  }
  std::memcpy(out_root.data(), in.data() + off, out_root.size());
  off += out_root.size();
  if (off + 4 > in.size()) {
    return false;
  }
  std::uint32_t len = static_cast<std::uint32_t>(in[off]) |
                      (static_cast<std::uint32_t>(in[off + 1]) << 8) |
                      (static_cast<std::uint32_t>(in[off + 2]) << 16) |
                      (static_cast<std::uint32_t>(in[off + 3]) << 24);
  off += 4;
  if (off + len != in.size()) {
    return false;
  }
  out_tree_size = size;
  out_plain.assign(in.begin() + off, in.end());
  return true;
}

}  // namespace

namespace {

std::uint64_t NowUnixSeconds() {
  return mi::platform::NowUnixSeconds();
}

bool ParsePairingCodeSecret16(const std::string& pairing_code,
                             std::array<std::uint8_t, 16>& out_secret) {
  out_secret.fill(0);
  const std::string norm = security::NormalizeCode(pairing_code);
  std::vector<std::uint8_t> bytes;
  const bool ok = mi::common::HexToBytes(norm, bytes);
  [[maybe_unused]] mi::common::ScopedWipe wipe_bytes(bytes);
  if (!ok || bytes.size() != out_secret.size()) {
    return false;
  }
  std::memcpy(out_secret.data(), bytes.data(), out_secret.size());
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
  [[maybe_unused]] mi::common::ScopedWipe wipe_buf(buf);
  const std::string digest = mi::common::Sha256Hex(buf.data(), buf.size());
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
  mi::common::ScopedWipe wipe_plain(out_plaintext);
  const int rc =
      crypto_aead_unlock(out_plaintext.data(), mac, key.data(), nonce, ad,
                         kAdSize, ctext, ctext_len);
  if (rc != 0) {
    out_plaintext.clear();
    return false;
  }
  wipe_plain.Release();
  return true;
}

bool WriteFixed16(const std::array<std::uint8_t, 16>& v,
                  std::vector<std::uint8_t>& out);

bool ReadFixed16(const std::vector<std::uint8_t>& data, std::size_t& offset,
                 std::array<std::uint8_t, 16>& out);

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

constexpr std::uint8_t kChatMagic[4] = {'M', 'I', 'C', 'H'};
constexpr std::uint8_t kChatVersion = 1;
constexpr std::uint8_t kChatTypeText = 1;
constexpr std::uint8_t kChatTypeAck = 2;
constexpr std::uint8_t kChatTypeFile = 3;
constexpr std::uint8_t kChatTypeGroupText = 4;
constexpr std::uint8_t kChatTypeGroupInvite = 5;
constexpr std::uint8_t kChatTypeGroupFile = 6;
constexpr std::uint8_t kChatTypeGroupSenderKeyDist = 7;
constexpr std::uint8_t kChatTypeGroupSenderKeyReq = 8;
constexpr std::uint8_t kChatTypeRich = 9;
constexpr std::uint8_t kChatTypeReadReceipt = 10;
constexpr std::uint8_t kChatTypeTyping = 11;
constexpr std::uint8_t kChatTypeSticker = 12;
constexpr std::uint8_t kChatTypePresence = 13;
constexpr std::uint8_t kChatTypeGroupCallKeyDist = 14;
constexpr std::uint8_t kChatTypeGroupCallKeyReq = 15;

constexpr std::uint8_t kGroupCallOpCreate = 1;
constexpr std::uint8_t kGroupCallOpJoin = 2;
constexpr std::uint8_t kGroupCallOpLeave = 3;
constexpr std::uint8_t kGroupCallOpEnd = 4;
constexpr std::uint8_t kGroupCallOpUpdate = 5;
constexpr std::uint8_t kGroupCallOpPing = 6;

constexpr std::size_t kChatHeaderSize = sizeof(kChatMagic) + 1 + 1 + 16;
constexpr std::size_t kChatSeenLimit = 4096;
constexpr std::size_t kPendingGroupCipherLimit = 512;

constexpr std::uint8_t kDeviceSyncEventSendPrivate = 1;
constexpr std::uint8_t kDeviceSyncEventSendGroup = 2;
constexpr std::uint8_t kDeviceSyncEventMessage = 3;
constexpr std::uint8_t kDeviceSyncEventDelivery = 4;
constexpr std::uint8_t kDeviceSyncEventGroupNotice = 5;
constexpr std::uint8_t kDeviceSyncEventRotateKey = 6;
constexpr std::uint8_t kDeviceSyncEventHistorySnapshot = 7;

constexpr std::uint8_t kGroupNoticeJoin = 1;
constexpr std::uint8_t kGroupNoticeLeave = 2;
constexpr std::uint8_t kGroupNoticeKick = 3;
constexpr std::uint8_t kGroupNoticeRoleSet = 4;

constexpr std::uint8_t kHistorySnapshotKindEnvelope = 1;
constexpr std::uint8_t kHistorySnapshotKindSystem = 2;

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

struct DeviceSyncEvent {
  std::uint8_t type{0};
  bool is_group{false};
  bool outgoing{false};
  bool is_read{false};
  std::string conv_id;
  std::string sender;
  std::vector<std::uint8_t> envelope;
  std::array<std::uint8_t, 16> msg_id{};
  std::array<std::uint8_t, 32> new_key{};
  std::string target_device_id;
  std::vector<ChatHistoryMessage> history;
};

bool EncodeDeviceSyncSendPrivate(const std::string& peer_username,
                                const std::vector<std::uint8_t>& envelope,
                                std::vector<std::uint8_t>& out) {
  out.clear();
  out.push_back(kDeviceSyncEventSendPrivate);
  return mi::server::proto::WriteString(peer_username, out) &&
         mi::server::proto::WriteBytes(envelope, out);
}

bool EncodeDeviceSyncSendGroup(const std::string& group_id,
                              const std::vector<std::uint8_t>& envelope,
                              std::vector<std::uint8_t>& out) {
  out.clear();
  out.push_back(kDeviceSyncEventSendGroup);
  return mi::server::proto::WriteString(group_id, out) &&
         mi::server::proto::WriteBytes(envelope, out);
}

bool EncodeDeviceSyncMessage(bool is_group, bool outgoing,
                             const std::string& conv_id,
                             const std::string& sender,
                             const std::vector<std::uint8_t>& envelope,
                             std::vector<std::uint8_t>& out) {
  out.clear();
  out.push_back(kDeviceSyncEventMessage);
  std::uint8_t flags = 0;
  if (is_group) {
    flags |= 0x01;
  }
  if (outgoing) {
    flags |= 0x02;
  }
  out.push_back(flags);
  return mi::server::proto::WriteString(conv_id, out) &&
         mi::server::proto::WriteString(sender, out) &&
         mi::server::proto::WriteBytes(envelope, out);
}

bool EncodeDeviceSyncDelivery(bool is_group, bool is_read,
                              const std::string& conv_id,
                              const std::array<std::uint8_t, 16>& msg_id,
                              std::vector<std::uint8_t>& out) {
  out.clear();
  out.push_back(kDeviceSyncEventDelivery);
  std::uint8_t flags = 0;
  if (is_group) {
    flags |= 0x01;
  }
  if (is_read) {
    flags |= 0x02;
  }
  out.push_back(flags);
  return mi::server::proto::WriteString(conv_id, out) &&
         WriteFixed16(msg_id, out);
}

bool EncodeDeviceSyncGroupNotice(const std::string& group_id,
                                  const std::string& actor_username,
                                  const std::vector<std::uint8_t>& payload,
                                  std::vector<std::uint8_t>& out) {
  out.clear();
  out.push_back(kDeviceSyncEventGroupNotice);
  return mi::server::proto::WriteString(group_id, out) &&
         mi::server::proto::WriteString(actor_username, out) &&
         mi::server::proto::WriteBytes(payload, out);
}

bool EncodeDeviceSyncRotateKey(const std::array<std::uint8_t, 32>& key,
                               std::vector<std::uint8_t>& out) {
  out.clear();
  out.push_back(kDeviceSyncEventRotateKey);
  out.insert(out.end(), key.begin(), key.end());
  return true;
}

bool EncodeHistorySnapshotEntry(const ChatHistoryMessage& msg,
                                std::vector<std::uint8_t>& out) {
  out.clear();
  if (msg.conv_id.empty()) {
    return false;
  }
  if (msg.is_system) {
    if (msg.system_text_utf8.empty()) {
      return false;
    }
    out.push_back(kHistorySnapshotKindSystem);
  } else {
    if (msg.sender.empty() || msg.envelope.empty()) {
      return false;
    }
    out.push_back(kHistorySnapshotKindEnvelope);
  }
  std::uint8_t flags = 0;
  if (msg.is_group) {
    flags |= 0x01;
  }
  if (msg.outgoing) {
    flags |= 0x02;
  }
  out.push_back(flags);

  const std::uint8_t st = static_cast<std::uint8_t>(msg.status);
  if (st > static_cast<std::uint8_t>(ChatHistoryStatus::kFailed)) {
    return false;
  }
  out.push_back(st);

  mi::server::proto::WriteUint64(msg.timestamp_sec, out);
  mi::server::proto::WriteString(msg.conv_id, out);
  if (msg.is_system) {
    mi::server::proto::WriteString(msg.system_text_utf8, out);
    return true;
  }
  return mi::server::proto::WriteString(msg.sender, out) &&
         mi::server::proto::WriteBytes(msg.envelope, out);
}

bool DecodeDeviceSyncEvent(const std::vector<std::uint8_t>& plain,
                           DeviceSyncEvent& out) {
  out = DeviceSyncEvent{};
  if (plain.empty()) {
    return false;
  }
  std::size_t off = 0;
  out.type = plain[off++];
  if (out.type == kDeviceSyncEventSendPrivate) {
    if (!mi::server::proto::ReadString(plain, off, out.conv_id) ||
        !mi::server::proto::ReadBytes(plain, off, out.envelope) ||
        off != plain.size()) {
      return false;
    }
    return true;
  }
  if (out.type == kDeviceSyncEventSendGroup) {
    if (!mi::server::proto::ReadString(plain, off, out.conv_id) ||
        !mi::server::proto::ReadBytes(plain, off, out.envelope) ||
        off != plain.size()) {
      return false;
    }
    return true;
  }
  if (out.type == kDeviceSyncEventMessage) {
    if (off >= plain.size()) {
      return false;
    }
    const std::uint8_t flags = plain[off++];
    out.is_group = (flags & 0x01) != 0;
    out.outgoing = (flags & 0x02) != 0;
    if (!mi::server::proto::ReadString(plain, off, out.conv_id) ||
        !mi::server::proto::ReadString(plain, off, out.sender) ||
        !mi::server::proto::ReadBytes(plain, off, out.envelope) ||
        off != plain.size()) {
      return false;
    }
    return true;
  }
  if (out.type == kDeviceSyncEventDelivery) {
    if (off >= plain.size()) {
      return false;
    }
    const std::uint8_t flags = plain[off++];
    out.is_group = (flags & 0x01) != 0;
    out.is_read = (flags & 0x02) != 0;
    if (!mi::server::proto::ReadString(plain, off, out.conv_id) ||
        !ReadFixed16(plain, off, out.msg_id) || off != plain.size()) {
      return false;
    }
    return true;
  }
  if (out.type == kDeviceSyncEventGroupNotice) {
    out.is_group = true;
    if (!mi::server::proto::ReadString(plain, off, out.conv_id) ||
        !mi::server::proto::ReadString(plain, off, out.sender) ||
        !mi::server::proto::ReadBytes(plain, off, out.envelope) ||
        off != plain.size()) {
      return false;
    }
    return true;
  }
  if (out.type == kDeviceSyncEventHistorySnapshot) {
    if (!mi::server::proto::ReadString(plain, off, out.target_device_id)) {
      return false;
    }
    std::uint32_t count = 0;
    if (!mi::server::proto::ReadUint32(plain, off, count)) {
      return false;
    }
    out.history.clear();
    out.history.reserve(count > 4096u ? 4096u : count);
    for (std::uint32_t i = 0; i < count; ++i) {
      if (off + 1 + 1 + 1 + 8 > plain.size()) {
        return false;
      }
      const std::uint8_t kind = plain[off++];
      const std::uint8_t flags = plain[off++];
      const bool is_group = (flags & 0x01) != 0;
      const bool outgoing = (flags & 0x02) != 0;
      const std::uint8_t st = plain[off++];
      if (st > static_cast<std::uint8_t>(ChatHistoryStatus::kFailed)) {
        return false;
      }
      std::uint64_t ts = 0;
      if (!mi::server::proto::ReadUint64(plain, off, ts)) {
        return false;
      }
      std::string conv_id;
      if (!mi::server::proto::ReadString(plain, off, conv_id) || conv_id.empty()) {
        return false;
      }

      ChatHistoryMessage m;
      m.is_group = is_group;
      m.outgoing = outgoing;
      m.status = static_cast<ChatHistoryStatus>(st);
      m.timestamp_sec = ts;
      m.conv_id = std::move(conv_id);

      if (kind == kHistorySnapshotKindEnvelope) {
        if (!mi::server::proto::ReadString(plain, off, m.sender) ||
            !mi::server::proto::ReadBytes(plain, off, m.envelope) ||
            m.sender.empty() || m.envelope.empty()) {
          return false;
        }
        m.is_system = false;
      } else if (kind == kHistorySnapshotKindSystem) {
        std::string text;
        if (!mi::server::proto::ReadString(plain, off, text) || text.empty()) {
          return false;
        }
        m.is_system = true;
        m.system_text_utf8 = std::move(text);
      } else {
        return false;
      }

      out.history.push_back(std::move(m));
    }
    return off == plain.size();
  }
  if (out.type == kDeviceSyncEventRotateKey) {
    if (off + out.new_key.size() != plain.size()) {
      return false;
    }
    std::memcpy(out.new_key.data(), plain.data() + off, out.new_key.size());
    return true;
  }
  return false;
}

bool DecodeGroupNoticePayload(const std::vector<std::uint8_t>& payload,
                             std::uint8_t& out_kind, std::string& out_target,
                             std::optional<std::uint8_t>& out_role) {
  out_kind = 0;
  out_target.clear();
  out_role = std::nullopt;
  if (payload.empty()) {
    return false;
  }
  std::size_t off = 0;
  out_kind = payload[off++];
  if (!mi::server::proto::ReadString(payload, off, out_target)) {
    return false;
  }
  if (out_kind == kGroupNoticeRoleSet) {
    if (off >= payload.size()) {
      return false;
    }
    out_role = payload[off++];
  }
  return off == payload.size();
}

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

bool HexToFixedBytes16(const std::string& hex,
                       std::array<std::uint8_t, 16>& out) {
  std::vector<std::uint8_t> tmp;
  if (!mi::common::HexToBytes(hex, tmp) || tmp.size() != out.size()) {
    return false;
  }
  std::memcpy(out.data(), tmp.data(), out.size());
  return true;
}

constexpr std::size_t kChatEnvelopeBaseBytes =
    sizeof(kChatMagic) + 1 + 1 + 16;

void ReserveChatEnvelope(std::vector<std::uint8_t>& out, std::size_t extra) {
  out.clear();
  out.reserve(kChatEnvelopeBaseBytes + extra);
}

bool EncodeChatText(const std::array<std::uint8_t, 16>& msg_id,
                    const std::string& text_utf8,
                    std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 2 + text_utf8.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeText);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  return mi::server::proto::WriteString(text_utf8, out);
}

bool EncodeChatAck(const std::array<std::uint8_t, 16>& msg_id,
                   std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 0);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeAck);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  return true;
}

bool EncodeChatReadReceipt(const std::array<std::uint8_t, 16>& msg_id,
                           std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 0);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeReadReceipt);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  return true;
}

bool EncodeChatTyping(const std::array<std::uint8_t, 16>& msg_id, bool typing,
                      std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 1);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeTyping);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  out.push_back(typing ? 1 : 0);
  return true;
}

bool EncodeChatPresence(const std::array<std::uint8_t, 16>& msg_id, bool online,
                        std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 1);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypePresence);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  out.push_back(online ? 1 : 0);
  return true;
}

bool EncodeChatSticker(const std::array<std::uint8_t, 16>& msg_id,
                       const std::string& sticker_id,
                       std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 2 + sticker_id.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeSticker);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  return mi::server::proto::WriteString(sticker_id, out);
}

bool EncodeChatGroupText(const std::array<std::uint8_t, 16>& msg_id,
                         const std::string& group_id,
                         const std::string& text_utf8,
                         std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out,
                      2 + group_id.size() + 2 + text_utf8.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeGroupText);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  return mi::server::proto::WriteString(group_id, out) &&
         mi::server::proto::WriteString(text_utf8, out);
}

bool EncodeChatGroupInvite(const std::array<std::uint8_t, 16>& msg_id,
                           const std::string& group_id,
                           std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 2 + group_id.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeGroupInvite);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  return mi::server::proto::WriteString(group_id, out);
}

std::vector<std::uint8_t> BuildGroupSenderKeyDistSigMessage(
    const std::string& group_id, std::uint32_t version, std::uint32_t iteration,
    const std::array<std::uint8_t, 32>& ck) {
  std::vector<std::uint8_t> msg;
  static constexpr char kPrefix[] = "MI_GSKD_V1";
  msg.reserve(sizeof(kPrefix) - 1 + 2 + group_id.size() + 4 + 4 + 4 + ck.size());
  msg.insert(msg.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  mi::server::proto::WriteString(group_id, msg);
  mi::server::proto::WriteUint32(version, msg);
  mi::server::proto::WriteUint32(iteration, msg);
  mi::server::proto::WriteBytes(ck.data(), ck.size(), msg);
  return msg;
}

bool EncodeChatGroupSenderKeyDist(
    const std::array<std::uint8_t, 16>& msg_id, const std::string& group_id,
    std::uint32_t version, std::uint32_t iteration,
    const std::array<std::uint8_t, 32>& ck,
    const std::vector<std::uint8_t>& sig, std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, group_id.size() + sig.size() + 50);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeGroupSenderKeyDist);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  if (!mi::server::proto::WriteString(group_id, out) ||
      !mi::server::proto::WriteUint32(version, out) ||
      !mi::server::proto::WriteUint32(iteration, out)) {
    out.clear();
    return false;
  }
  if (!mi::server::proto::WriteBytes(ck.data(), ck.size(), out) ||
      !mi::server::proto::WriteBytes(sig, out)) {
    out.clear();
    return false;
  }
  return true;
}

bool DecodeChatGroupSenderKeyDist(
    const std::vector<std::uint8_t>& payload, std::size_t& offset,
    std::string& out_group_id, std::uint32_t& out_version,
    std::uint32_t& out_iteration, std::array<std::uint8_t, 32>& out_ck,
    std::vector<std::uint8_t>& out_sig) {
  out_group_id.clear();
  out_version = 0;
  out_iteration = 0;
  out_ck.fill(0);
  out_sig.clear();
  if (!mi::server::proto::ReadString(payload, offset, out_group_id) ||
      !mi::server::proto::ReadUint32(payload, offset, out_version) ||
      !mi::server::proto::ReadUint32(payload, offset, out_iteration)) {
    return false;
  }
  std::vector<std::uint8_t> ck_bytes;
  if (!mi::server::proto::ReadBytes(payload, offset, ck_bytes) ||
      ck_bytes.size() != out_ck.size()) {
    return false;
  }
  std::memcpy(out_ck.data(), ck_bytes.data(), out_ck.size());
  if (!mi::server::proto::ReadBytes(payload, offset, out_sig)) {
    return false;
  }
  return true;
}

bool EncodeChatGroupSenderKeyReq(const std::array<std::uint8_t, 16>& msg_id,
                                 const std::string& group_id,
                                 std::uint32_t want_version,
                                 std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 2 + group_id.size() + 4);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeGroupSenderKeyReq);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  return mi::server::proto::WriteString(group_id, out) &&
         mi::server::proto::WriteUint32(want_version, out);
}

bool DecodeChatGroupSenderKeyReq(const std::vector<std::uint8_t>& payload,
                                 std::size_t& offset,
                                 std::string& out_group_id,
                                 std::uint32_t& out_want_version) {
  out_group_id.clear();
  out_want_version = 0;
  return mi::server::proto::ReadString(payload, offset, out_group_id) &&
         mi::server::proto::ReadUint32(payload, offset, out_want_version);
}

std::vector<std::uint8_t> BuildGroupCallKeyDistSigMessage(
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id,
    const std::array<std::uint8_t, 32>& call_key) {
  std::vector<std::uint8_t> msg;
  static constexpr char kPrefix[] = "MI_GCKD_V1";
  msg.reserve(sizeof(kPrefix) - 1 + 2 + group_id.size() + call_id.size() + 4 +
              2 + call_key.size());
  msg.insert(msg.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  mi::server::proto::WriteString(group_id, msg);
  msg.insert(msg.end(), call_id.begin(), call_id.end());
  mi::server::proto::WriteUint32(key_id, msg);
  mi::server::proto::WriteBytes(call_key.data(), call_key.size(), msg);
  return msg;
}

bool EncodeChatGroupCallKeyDist(const std::array<std::uint8_t, 16>& msg_id,
                                const std::string& group_id,
                                const std::array<std::uint8_t, 16>& call_id,
                                std::uint32_t key_id,
                                const std::array<std::uint8_t, 32>& call_key,
                                const std::vector<std::uint8_t>& sig,
                                std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, group_id.size() + sig.size() + 80);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeGroupCallKeyDist);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  if (!mi::server::proto::WriteString(group_id, out)) {
    out.clear();
    return false;
  }
  out.insert(out.end(), call_id.begin(), call_id.end());
  if (!mi::server::proto::WriteUint32(key_id, out)) {
    out.clear();
    return false;
  }
  if (!mi::server::proto::WriteBytes(call_key.data(), call_key.size(), out) ||
      !mi::server::proto::WriteBytes(sig, out)) {
    out.clear();
    return false;
  }
  return true;
}

bool DecodeChatGroupCallKeyDist(const std::vector<std::uint8_t>& payload,
                                std::size_t& offset,
                                std::string& out_group_id,
                                std::array<std::uint8_t, 16>& out_call_id,
                                std::uint32_t& out_key_id,
                                std::array<std::uint8_t, 32>& out_call_key,
                                std::vector<std::uint8_t>& out_sig) {
  out_group_id.clear();
  out_call_id.fill(0);
  out_key_id = 0;
  out_call_key.fill(0);
  out_sig.clear();
  if (!mi::server::proto::ReadString(payload, offset, out_group_id)) {
    return false;
  }
  if (offset + out_call_id.size() > payload.size()) {
    return false;
  }
  std::memcpy(out_call_id.data(), payload.data() + offset, out_call_id.size());
  offset += out_call_id.size();
  if (!mi::server::proto::ReadUint32(payload, offset, out_key_id)) {
    return false;
  }
  std::vector<std::uint8_t> key_bytes;
  if (!mi::server::proto::ReadBytes(payload, offset, key_bytes) ||
      key_bytes.size() != out_call_key.size()) {
    return false;
  }
  std::memcpy(out_call_key.data(), key_bytes.data(), out_call_key.size());
  if (!mi::server::proto::ReadBytes(payload, offset, out_sig)) {
    return false;
  }
  return true;
}

bool EncodeChatGroupCallKeyReq(const std::array<std::uint8_t, 16>& msg_id,
                               const std::string& group_id,
                               const std::array<std::uint8_t, 16>& call_id,
                               std::uint32_t want_key_id,
                               std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, group_id.size() + 32);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeGroupCallKeyReq);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  if (!mi::server::proto::WriteString(group_id, out)) {
    out.clear();
    return false;
  }
  out.insert(out.end(), call_id.begin(), call_id.end());
  if (!mi::server::proto::WriteUint32(want_key_id, out)) {
    out.clear();
    return false;
  }
  return true;
}

bool DecodeChatGroupCallKeyReq(const std::vector<std::uint8_t>& payload,
                               std::size_t& offset,
                               std::string& out_group_id,
                               std::array<std::uint8_t, 16>& out_call_id,
                               std::uint32_t& out_want_key_id) {
  out_group_id.clear();
  out_call_id.fill(0);
  out_want_key_id = 0;
  if (!mi::server::proto::ReadString(payload, offset, out_group_id)) {
    return false;
  }
  if (offset + out_call_id.size() > payload.size()) {
    return false;
  }
  std::memcpy(out_call_id.data(), payload.data() + offset, out_call_id.size());
  offset += out_call_id.size();
  return mi::server::proto::ReadUint32(payload, offset, out_want_key_id);
}

constexpr std::uint8_t kRichKindText = 1;
constexpr std::uint8_t kRichKindLocation = 2;
constexpr std::uint8_t kRichKindContactCard = 3;

constexpr std::uint8_t kRichFlagHasReply = 0x01;

struct RichDecoded {
  std::uint8_t kind{0};
  bool has_reply{false};
  std::array<std::uint8_t, 16> reply_to{};
  std::string reply_preview;
  std::string text;
  std::int32_t lat_e7{0};
  std::int32_t lon_e7{0};
  std::string location_label;
  std::string card_username;
  std::string card_display;
};

std::string FormatCoordE7(std::int32_t v_e7) {
  const std::int64_t v64 = static_cast<std::int64_t>(v_e7);
  const bool neg = v64 < 0;
  const std::uint64_t abs = static_cast<std::uint64_t>(neg ? -v64 : v64);
  const std::uint64_t deg = abs / 10000000ULL;
  const std::uint64_t frac = abs % 10000000ULL;
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%s%llu.%07llu", neg ? "-" : "",
                static_cast<unsigned long long>(deg),
                static_cast<unsigned long long>(frac));
  return std::string(buf);
}

bool EncodeChatRichText(const std::array<std::uint8_t, 16>& msg_id,
                        const std::string& text_utf8, bool has_reply,
                        const std::array<std::uint8_t, 16>& reply_to,
                        const std::string& reply_preview_utf8,
                        std::vector<std::uint8_t>& out) {
  std::size_t extra = 2 + 2 + text_utf8.size();
  if (has_reply) {
    extra += reply_to.size() + 2 + reply_preview_utf8.size();
  }
  ReserveChatEnvelope(out, extra);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeRich);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  out.push_back(kRichKindText);
  std::uint8_t flags = 0;
  if (has_reply) {
    flags |= kRichFlagHasReply;
  }
  out.push_back(flags);
  if (has_reply) {
    out.insert(out.end(), reply_to.begin(), reply_to.end());
    if (!mi::server::proto::WriteString(reply_preview_utf8, out)) {
      out.clear();
      return false;
    }
  }
  return mi::server::proto::WriteString(text_utf8, out);
}

bool EncodeChatRichLocation(const std::array<std::uint8_t, 16>& msg_id,
                            std::int32_t lat_e7, std::int32_t lon_e7,
                            const std::string& label_utf8,
                            std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 2 + 8 + 2 + label_utf8.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeRich);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  out.push_back(kRichKindLocation);
  out.push_back(0);
  if (!mi::server::proto::WriteUint32(static_cast<std::uint32_t>(lat_e7), out) ||
      !mi::server::proto::WriteUint32(static_cast<std::uint32_t>(lon_e7), out) ||
      !mi::server::proto::WriteString(label_utf8, out)) {
    out.clear();
    return false;
  }
  return true;
}

bool EncodeChatRichContactCard(const std::array<std::uint8_t, 16>& msg_id,
                               const std::string& card_username,
                               const std::string& card_display,
                               std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out,
                      2 + 2 + card_username.size() + 2 + card_display.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeRich);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  out.push_back(kRichKindContactCard);
  out.push_back(0);
  if (!mi::server::proto::WriteString(card_username, out) ||
      !mi::server::proto::WriteString(card_display, out)) {
    out.clear();
    return false;
  }
  return true;
}

bool DecodeChatRich(const std::vector<std::uint8_t>& payload, std::size_t& offset,
                    RichDecoded& out) {
  out = RichDecoded{};
  if (offset + 2 > payload.size()) {
    return false;
  }
  out.kind = payload[offset++];
  const std::uint8_t flags = payload[offset++];
  out.has_reply = (flags & kRichFlagHasReply) != 0;
  if (out.has_reply) {
    if (!ReadFixed16(payload, offset, out.reply_to) ||
        !mi::server::proto::ReadString(payload, offset, out.reply_preview)) {
      return false;
    }
  }

  if (out.kind == kRichKindText) {
    return mi::server::proto::ReadString(payload, offset, out.text);
  }
  if (out.kind == kRichKindLocation) {
    std::uint32_t lat_u = 0;
    std::uint32_t lon_u = 0;
    if (!mi::server::proto::ReadUint32(payload, offset, lat_u) ||
        !mi::server::proto::ReadUint32(payload, offset, lon_u) ||
        !mi::server::proto::ReadString(payload, offset, out.location_label)) {
      return false;
    }
    out.lat_e7 = static_cast<std::int32_t>(lat_u);
    out.lon_e7 = static_cast<std::int32_t>(lon_u);
    return true;
  }
  if (out.kind == kRichKindContactCard) {
    return mi::server::proto::ReadString(payload, offset, out.card_username) &&
           mi::server::proto::ReadString(payload, offset, out.card_display);
  }
  return false;
}

std::string FormatRichAsText(const RichDecoded& msg) {
  std::string out;
  if (msg.has_reply) {
    out += "【回复】";
    if (!msg.reply_preview.empty()) {
      out += msg.reply_preview;
    } else {
      out += "（引用）";
    }
    out += "\n";
  }

  if (msg.kind == kRichKindText) {
    out += msg.text;
    return out;
  }
  if (msg.kind == kRichKindLocation) {
    out += "【位置】";
    out += msg.location_label.empty() ? "（未命名）" : msg.location_label;
    out += "\nlat:";
    out += FormatCoordE7(msg.lat_e7);
    out += ", lon:";
    out += FormatCoordE7(msg.lon_e7);
    return out;
  }
  if (msg.kind == kRichKindContactCard) {
    out += "【名片】";
    out += msg.card_username.empty() ? "（空）" : msg.card_username;
    if (!msg.card_display.empty()) {
      out += " (";
      out += msg.card_display;
      out += ")";
    }
    return out;
  }
  out += "【未知消息】";
  return out;
}

struct HistorySummaryDecoded {
  ChatHistorySummaryKind kind{ChatHistorySummaryKind::kNone};
  std::string text;
  std::string file_id;
  std::string file_name;
  std::uint64_t file_size{0};
  std::string sticker_id;
  std::int32_t lat_e7{0};
  std::int32_t lon_e7{0};
  std::string location_label;
  std::string card_username;
  std::string card_display;
  std::string group_id;
};

bool DecodeHistorySummary(const std::vector<std::uint8_t>& payload,
                          HistorySummaryDecoded& out) {
  out = HistorySummaryDecoded{};
  const std::size_t header_len = kHistorySummaryMagic.size() + 2;
  if (payload.size() < header_len) {
    return false;
  }
  if (std::memcmp(payload.data(), kHistorySummaryMagic.data(),
                  kHistorySummaryMagic.size()) != 0) {
    return false;
  }
  std::size_t off = kHistorySummaryMagic.size();
  const std::uint8_t version = payload[off++];
  if (version != kHistorySummaryVersion) {
    return false;
  }
  out.kind = static_cast<ChatHistorySummaryKind>(payload[off++]);

  if (out.kind == ChatHistorySummaryKind::kText) {
    return mi::server::proto::ReadString(payload, off, out.text) &&
           off == payload.size();
  }
  if (out.kind == ChatHistorySummaryKind::kFile) {
    return mi::server::proto::ReadUint64(payload, off, out.file_size) &&
           mi::server::proto::ReadString(payload, off, out.file_name) &&
           mi::server::proto::ReadString(payload, off, out.file_id) &&
           off == payload.size();
  }
  if (out.kind == ChatHistorySummaryKind::kSticker) {
    return mi::server::proto::ReadString(payload, off, out.sticker_id) &&
           off == payload.size();
  }
  if (out.kind == ChatHistorySummaryKind::kLocation) {
    std::uint32_t lat_u = 0;
    std::uint32_t lon_u = 0;
    if (!mi::server::proto::ReadUint32(payload, off, lat_u) ||
        !mi::server::proto::ReadUint32(payload, off, lon_u) ||
        !mi::server::proto::ReadString(payload, off, out.location_label) ||
        off != payload.size()) {
      return false;
    }
    out.lat_e7 = static_cast<std::int32_t>(lat_u);
    out.lon_e7 = static_cast<std::int32_t>(lon_u);
    return true;
  }
  if (out.kind == ChatHistorySummaryKind::kContactCard) {
    return mi::server::proto::ReadString(payload, off, out.card_username) &&
           mi::server::proto::ReadString(payload, off, out.card_display) &&
           off == payload.size();
  }
  if (out.kind == ChatHistorySummaryKind::kGroupInvite) {
    return mi::server::proto::ReadString(payload, off, out.group_id) &&
           off == payload.size();
  }
  return false;
}

std::string FormatSummaryAsText(const HistorySummaryDecoded& summary) {
  if (summary.kind == ChatHistorySummaryKind::kLocation ||
      summary.kind == ChatHistorySummaryKind::kContactCard) {
    RichDecoded rich;
    rich.kind = (summary.kind == ChatHistorySummaryKind::kLocation)
                    ? kRichKindLocation
                    : kRichKindContactCard;
    rich.location_label = summary.location_label;
    rich.lat_e7 = summary.lat_e7;
    rich.lon_e7 = summary.lon_e7;
    rich.card_username = summary.card_username;
    rich.card_display = summary.card_display;
    return FormatRichAsText(rich);
  }
  if (summary.kind == ChatHistorySummaryKind::kGroupInvite) {
    return summary.group_id.empty()
               ? std::string("Group invite")
               : (std::string("Group invite: ") + summary.group_id);
  }
  return summary.text;
}

bool ApplyHistorySummary(const std::vector<std::uint8_t>& summary,
                         ClientCore::HistoryEntry& entry) {
  HistorySummaryDecoded decoded;
  if (!DecodeHistorySummary(summary, decoded)) {
    return false;
  }
  if (decoded.kind == ChatHistorySummaryKind::kText) {
    entry.kind = ClientCore::HistoryKind::kText;
    entry.text_utf8 = std::move(decoded.text);
    return true;
  }
  if (decoded.kind == ChatHistorySummaryKind::kFile) {
    entry.kind = ClientCore::HistoryKind::kFile;
    entry.file_id = std::move(decoded.file_id);
    entry.file_name = std::move(decoded.file_name);
    entry.file_size = decoded.file_size;
    return true;
  }
  if (decoded.kind == ChatHistorySummaryKind::kSticker) {
    entry.kind = ClientCore::HistoryKind::kSticker;
    entry.sticker_id = std::move(decoded.sticker_id);
    return true;
  }
  if (decoded.kind == ChatHistorySummaryKind::kLocation ||
      decoded.kind == ChatHistorySummaryKind::kContactCard ||
      decoded.kind == ChatHistorySummaryKind::kGroupInvite) {
    entry.kind = ClientCore::HistoryKind::kText;
    entry.text_utf8 = FormatSummaryAsText(decoded);
    return true;
  }
  return false;
}

bool DecodeChatHeader(const std::vector<std::uint8_t>& payload,
                      std::uint8_t& out_type,
                      std::array<std::uint8_t, 16>& out_id,
                      std::size_t& offset) {
  offset = 0;
  if (payload.size() < kChatHeaderSize) {
    return false;
  }
  if (std::memcmp(payload.data(), kChatMagic, sizeof(kChatMagic)) != 0) {
    return false;
  }
  offset = sizeof(kChatMagic);
  const std::uint8_t version = payload[offset++];
  if (version != kChatVersion) {
    return false;
  }
  out_type = payload[offset++];
  std::memcpy(out_id.data(), payload.data() + offset, out_id.size());
  offset += out_id.size();
  return true;
}

constexpr std::uint8_t kGroupCipherMagic[4] = {'M', 'I', 'G', 'C'};
constexpr std::uint8_t kGroupCipherVersion = 1;
constexpr std::size_t kGroupCipherNonceBytes = 24;
constexpr std::size_t kGroupCipherMacBytes = 16;
constexpr std::size_t kMaxGroupSkippedMessageKeys = 2048;
constexpr std::size_t kMaxGroupSkip = 4096;
constexpr std::uint64_t kGroupSenderKeyRotationThreshold = 10000;
constexpr std::uint64_t kGroupSenderKeyRotationIntervalSec =
    7ull * 24ull * 60ull * 60ull;

bool KdfGroupCk(const std::array<std::uint8_t, 32>& ck,
                std::array<std::uint8_t, 32>& out_ck,
                std::array<std::uint8_t, 32>& out_mk) {
  std::array<std::uint8_t, 64> buf{};
  static constexpr char kInfo[] = "mi_e2ee_group_sender_ck_v1";
  if (!mi::server::crypto::HkdfSha256(
          ck.data(), ck.size(), nullptr, 0,
          reinterpret_cast<const std::uint8_t*>(kInfo), std::strlen(kInfo),
          buf.data(), buf.size())) {
    return false;
  }
  std::memcpy(out_ck.data(), buf.data(), 32);
  std::memcpy(out_mk.data(), buf.data() + 32, 32);
  return true;
}

template <typename State>
void EnforceGroupSkippedLimit(State& state) {
  while (state.skipped_mks.size() > kMaxGroupSkippedMessageKeys) {
    if (state.skipped_order.empty()) {
      state.skipped_mks.clear();
      return;
    }
    const auto n = state.skipped_order.front();
    state.skipped_order.pop_front();
    state.skipped_mks.erase(n);
  }
}

template <typename State>
bool DeriveGroupMessageKey(State& state, std::uint32_t iteration,
                           std::array<std::uint8_t, 32>& out_mk) {
  out_mk.fill(0);
  if (iteration < state.next_iteration) {
    const auto it = state.skipped_mks.find(iteration);
    if (it == state.skipped_mks.end()) {
      return false;
    }
    out_mk = it->second;
    state.skipped_mks.erase(it);
    return true;
  }

  if (iteration - state.next_iteration > kMaxGroupSkip) {
    return false;
  }

  while (state.next_iteration < iteration) {
    std::array<std::uint8_t, 32> next_ck{};
    std::array<std::uint8_t, 32> mk{};
    if (!KdfGroupCk(state.ck, next_ck, mk)) {
      return false;
    }
    state.skipped_mks.emplace(state.next_iteration, mk);
    state.skipped_order.push_back(state.next_iteration);
    state.ck = next_ck;
    state.next_iteration++;
    EnforceGroupSkippedLimit(state);
  }

  std::array<std::uint8_t, 32> next_ck{};
  if (!KdfGroupCk(state.ck, next_ck, out_mk)) {
    return false;
  }
  state.ck = next_ck;
  state.next_iteration++;
  return true;
}

std::string MakeGroupSenderKeyMapKey(const std::string& group_id,
                                    const std::string& sender_username) {
  return group_id + "|" + sender_username;
}

std::string MakeGroupCallKeyMapKey(const std::string& group_id,
                                   const std::array<std::uint8_t, 16>& call_id) {
  const std::string call_hex =
      BytesToHexLower(call_id.data(), call_id.size());
  return group_id + "|" + call_hex;
}

std::string HashGroupMembers(std::vector<std::string> members) {
  std::sort(members.begin(), members.end());
  std::string joined;
  for (const auto& m : members) {
    joined.append(m);
    joined.push_back('\n');
  }
  return mi::common::Sha256Hex(
      reinterpret_cast<const std::uint8_t*>(joined.data()), joined.size());
}

void BuildGroupCipherAd(const std::string& group_id,
                        const std::string& sender_username,
                        std::uint32_t sender_key_version,
                        std::uint32_t sender_key_iteration,
                        std::vector<std::uint8_t>& out) {
  out.clear();
  static constexpr char kPrefix[] = "MI_GMSG_AD_V1";
  out.reserve(sizeof(kPrefix) - 1 + 2 + group_id.size() + 2 +
              sender_username.size() + 4 + 4);
  out.insert(out.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  mi::server::proto::WriteString(group_id, out);
  mi::server::proto::WriteString(sender_username, out);
  mi::server::proto::WriteUint32(sender_key_version, out);
  mi::server::proto::WriteUint32(sender_key_iteration, out);
}

bool EncodeGroupCipherNoSig(const std::string& group_id,
                            const std::string& sender_username,
                            std::uint32_t sender_key_version,
                            std::uint32_t sender_key_iteration,
                            const std::array<std::uint8_t, 24>& nonce,
                            const std::array<std::uint8_t, 16>& mac,
                            const std::vector<std::uint8_t>& cipher,
                            std::vector<std::uint8_t>& out) {
  out.clear();
  out.reserve(sizeof(kGroupCipherMagic) + 1 + 4 + 4 +
              2 + group_id.size() + 2 + sender_username.size() +
              4 + nonce.size() + 4 + mac.size() + 4 + cipher.size());
  out.insert(out.end(), kGroupCipherMagic,
             kGroupCipherMagic + sizeof(kGroupCipherMagic));
  out.push_back(kGroupCipherVersion);
  mi::server::proto::WriteUint32(sender_key_version, out);
  mi::server::proto::WriteUint32(sender_key_iteration, out);
  if (!mi::server::proto::WriteString(group_id, out) ||
      !mi::server::proto::WriteString(sender_username, out)) {
    out.clear();
    return false;
  }
  if (!mi::server::proto::WriteBytes(nonce.data(), nonce.size(), out) ||
      !mi::server::proto::WriteBytes(mac.data(), mac.size(), out) ||
      !mi::server::proto::WriteBytes(cipher, out)) {
    out.clear();
    return false;
  }
  return true;
}

bool DecodeGroupCipher(const std::vector<std::uint8_t>& payload,
                       std::uint32_t& out_sender_key_version,
                       std::uint32_t& out_sender_key_iteration,
                       std::string& out_group_id,
                       std::string& out_sender_username,
                       std::array<std::uint8_t, 24>& out_nonce,
                       std::array<std::uint8_t, 16>& out_mac,
                       std::vector<std::uint8_t>& out_cipher,
                       std::vector<std::uint8_t>& out_sig,
                       std::size_t& out_sig_offset) {
  out_sender_key_version = 0;
  out_sender_key_iteration = 0;
  out_group_id.clear();
  out_sender_username.clear();
  out_nonce.fill(0);
  out_mac.fill(0);
  out_cipher.clear();
  out_sig.clear();
  out_sig_offset = 0;

  if (payload.size() < sizeof(kGroupCipherMagic) + 1) {
    return false;
  }
  if (std::memcmp(payload.data(), kGroupCipherMagic,
                  sizeof(kGroupCipherMagic)) != 0) {
    return false;
  }
  std::size_t off = sizeof(kGroupCipherMagic);
  const std::uint8_t version = payload[off++];
  if (version != kGroupCipherVersion) {
    return false;
  }
  if (!mi::server::proto::ReadUint32(payload, off, out_sender_key_version) ||
      !mi::server::proto::ReadUint32(payload, off, out_sender_key_iteration) ||
      !mi::server::proto::ReadString(payload, off, out_group_id) ||
      !mi::server::proto::ReadString(payload, off, out_sender_username)) {
    return false;
  }
  std::vector<std::uint8_t> nonce_bytes;
  std::vector<std::uint8_t> mac_bytes;
  if (!mi::server::proto::ReadBytes(payload, off, nonce_bytes) ||
      nonce_bytes.size() != kGroupCipherNonceBytes ||
      !mi::server::proto::ReadBytes(payload, off, mac_bytes) ||
      mac_bytes.size() != kGroupCipherMacBytes ||
      !mi::server::proto::ReadBytes(payload, off, out_cipher)) {
    return false;
  }
  std::memcpy(out_nonce.data(), nonce_bytes.data(), out_nonce.size());
  std::memcpy(out_mac.data(), mac_bytes.data(), out_mac.size());
  out_sig_offset = off;
  if (!mi::server::proto::ReadBytes(payload, off, out_sig) ||
      off != payload.size()) {
    return false;
  }
  return true;
}

constexpr std::uint8_t kFileBlobMagic[4] = {'M', 'I', 'F', '1'};
constexpr std::uint8_t kFileBlobVersionV1 = 1;
constexpr std::uint8_t kFileBlobVersionV2 = 2;
constexpr std::uint8_t kFileBlobVersionV3 = 3;
constexpr std::uint8_t kFileBlobVersionV4 = 4;
constexpr std::uint8_t kFileBlobAlgoRaw = 0;
constexpr std::uint8_t kFileBlobAlgoDeflate = 1;
constexpr std::uint8_t kFileBlobFlagDoubleCompression = 0x01;
constexpr std::size_t kFileBlobV1PrefixSize = sizeof(kFileBlobMagic) + 1 + 3;
constexpr std::size_t kFileBlobV1HeaderSize = kFileBlobV1PrefixSize + 24 + 16;
constexpr std::size_t kFileBlobV2PrefixSize =
    sizeof(kFileBlobMagic) + 1 + 1 + 1 + 1 + 8 + 8 + 8;
constexpr std::size_t kFileBlobV2HeaderSize = kFileBlobV2PrefixSize + 24 + 16;
constexpr std::size_t kFileBlobV3PrefixSize =
    sizeof(kFileBlobMagic) + 1 + 1 + 1 + 1 + 4 + 8 + 24;
constexpr std::size_t kFileBlobV3HeaderSize = kFileBlobV3PrefixSize;
constexpr std::size_t kFileBlobV4BaseHeaderSize =
    sizeof(kFileBlobMagic) + 1 + 1 + 1 + 1 + 4 + 8 + 24;
constexpr std::size_t kMaxChatFileBytes = 300u * 1024u * 1024u;
constexpr std::size_t kMaxChatFileBlobBytes = 320u * 1024u * 1024u;
constexpr std::uint32_t kFileBlobV3ChunkBytes = 256u * 1024u;
constexpr std::uint32_t kFileBlobV4PlainChunkBytes = 128u * 1024u;
constexpr std::uint32_t kE2eeBlobChunkBytes = 4u * 1024u * 1024u;
constexpr std::size_t kFileBlobV4PadBuckets[] = {
    64u * 1024u,
    96u * 1024u,
    128u * 1024u,
    160u * 1024u,
    192u * 1024u,
    256u * 1024u,
    384u * 1024u
};

bool LooksLikeAlreadyCompressedFileName(const std::string& file_name) {
  if (file_name.empty()) {
    return false;
  }
  std::string ext;
  const auto dot = file_name.find_last_of('.');
  if (dot != std::string::npos && dot + 1 < file_name.size()) {
    ext = file_name.substr(dot + 1);
  } else {
    return false;
  }
  for (auto& c : ext) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }

  static const std::unordered_set<std::string> kCompressed = {
      "jpg",  "jpeg", "png", "gif", "webp", "bmp", "ico",  "heic",
      "mp4",  "mkv",  "mov", "webm","avi",  "flv", "m4v",
      "mp3",  "m4a",  "aac", "ogg", "opus", "flac", "wav",
      "zip",  "rar",  "7z",  "gz",  "bz2",  "xz",  "zst",
      "pdf",  "docx", "xlsx","pptx"
  };
  return kCompressed.find(ext) != kCompressed.end();
}

std::size_t SelectFileChunkTarget(std::size_t min_len) {
  if (min_len == 0 || min_len > (kE2eeBlobChunkBytes - 16u)) {
    return 0;
  }
  for (const auto bucket : kFileBlobV4PadBuckets) {
    if (bucket >= min_len) {
      if (bucket == min_len) {
        return bucket;
      }
      std::uint32_t r = 0;
      if (!RandomUint32(r)) {
        return bucket;
      }
      const std::size_t span = bucket - min_len;
      return min_len + (static_cast<std::size_t>(r) % (span + 1));
    }
  }
  const std::size_t round = ((min_len + 4095) / 4096) * 4096;
  if (round < min_len || round > (kE2eeBlobChunkBytes - 16u)) {
    return 0;
  }
  std::uint32_t r = 0;
  if (!RandomUint32(r)) {
    return round;
  }
  const std::size_t span = round - min_len;
  return min_len + (static_cast<std::size_t>(r) % (span + 1));
}

bool DeflateCompress(const std::uint8_t* data, std::size_t len, int level,
                     std::vector<std::uint8_t>& out) {
  out.clear();
  if (!data || len == 0) {
    return false;
  }
  if (len >
      static_cast<std::size_t>((std::numeric_limits<mz_ulong>::max)())) {
    return false;
  }

  const mz_ulong src_len = static_cast<mz_ulong>(len);
  const mz_ulong bound = mz_compressBound(src_len);
  std::vector<std::uint8_t> buf;
  buf.resize(static_cast<std::size_t>(bound));
  mz_ulong out_len = bound;
  const int status = mz_compress2(buf.data(), &out_len, data, src_len, level);
  if (status != MZ_OK) {
    crypto_wipe(buf.data(), buf.size());
    return false;
  }
  buf.resize(static_cast<std::size_t>(out_len));
  out = std::move(buf);
  return true;
}

bool DeflateDecompress(const std::uint8_t* data, std::size_t len,
                       std::size_t expected_len,
                       std::vector<std::uint8_t>& out) {
  out.clear();
  if (!data || len == 0 || expected_len == 0) {
    return false;
  }
  if (expected_len >
      static_cast<std::size_t>((std::numeric_limits<mz_ulong>::max)())) {
    return false;
  }
  if (len > static_cast<std::size_t>((std::numeric_limits<mz_ulong>::max)())) {
    return false;
  }

  std::vector<std::uint8_t> buf;
  buf.resize(expected_len);
  mz_ulong out_len = static_cast<mz_ulong>(expected_len);
  const int status = mz_uncompress(buf.data(), &out_len, data,
                                  static_cast<mz_ulong>(len));
  if (status != MZ_OK || out_len != static_cast<mz_ulong>(expected_len)) {
    crypto_wipe(buf.data(), buf.size());
    return false;
  }
  out = std::move(buf);
  return true;
}

bool EncodeChatFile(const std::array<std::uint8_t, 16>& msg_id,
                    std::uint64_t file_size,
                    const std::string& file_name,
                    const std::string& file_id,
                    const std::array<std::uint8_t, 32>& file_key,
                    std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out,
                      8 + 2 + file_name.size() + 2 + file_id.size() +
                          file_key.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeFile);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  if (!mi::server::proto::WriteUint64(file_size, out) ||
      !mi::server::proto::WriteString(file_name, out) ||
      !mi::server::proto::WriteString(file_id, out)) {
    out.clear();
    return false;
  }
  out.insert(out.end(), file_key.begin(), file_key.end());
  return true;
}

bool EncodeChatGroupFile(const std::array<std::uint8_t, 16>& msg_id,
                         const std::string& group_id,
                         std::uint64_t file_size,
                         const std::string& file_name,
                         const std::string& file_id,
                         const std::array<std::uint8_t, 32>& file_key,
                         std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out,
                      2 + group_id.size() + 8 + 2 + file_name.size() + 2 +
                          file_id.size() + file_key.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeGroupFile);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  if (!mi::server::proto::WriteString(group_id, out) ||
      !mi::server::proto::WriteUint64(file_size, out) ||
      !mi::server::proto::WriteString(file_name, out) ||
      !mi::server::proto::WriteString(file_id, out)) {
    out.clear();
    return false;
  }
  out.insert(out.end(), file_key.begin(), file_key.end());
  return true;
}

bool DecodeChatFile(const std::vector<std::uint8_t>& payload,
                    std::size_t& offset,
                    std::uint64_t& out_file_size,
                    std::string& out_file_name,
                    std::string& out_file_id,
                    std::array<std::uint8_t, 32>& out_file_key) {
  out_file_size = 0;
  out_file_name.clear();
  out_file_id.clear();
  out_file_key.fill(0);
  if (!mi::server::proto::ReadUint64(payload, offset, out_file_size) ||
      !mi::server::proto::ReadString(payload, offset, out_file_name) ||
      !mi::server::proto::ReadString(payload, offset, out_file_id)) {
    return false;
  }
  if (offset + out_file_key.size() != payload.size()) {
    return false;
  }
  std::memcpy(out_file_key.data(), payload.data() + offset, out_file_key.size());
  offset += out_file_key.size();
  return true;
}

bool DecodeChatGroupFile(const std::vector<std::uint8_t>& payload,
                         std::size_t& offset,
                         std::string& out_group_id,
                         std::uint64_t& out_file_size,
                         std::string& out_file_name,
                         std::string& out_file_id,
                         std::array<std::uint8_t, 32>& out_file_key) {
  out_group_id.clear();
  if (!mi::server::proto::ReadString(payload, offset, out_group_id)) {
    return false;
  }
  return DecodeChatFile(payload, offset, out_file_size, out_file_name,
                        out_file_id, out_file_key);
}

bool EncryptFileBlobAdaptive(const std::vector<std::uint8_t>& plaintext,
                             const std::array<std::uint8_t, 32>& key,
                             const std::string& file_name,
                             std::vector<std::uint8_t>& out_blob) {
  out_blob.clear();
  if (plaintext.empty()) {
    return false;
  }
  if (plaintext.size() > kMaxChatFileBytes) {
    return false;
  }

  const bool skip_compress = LooksLikeAlreadyCompressedFileName(file_name);

  if (skip_compress) {
    std::vector<std::uint8_t> header;
    header.reserve(kFileBlobV2PrefixSize);
    header.insert(header.end(), kFileBlobMagic,
                  kFileBlobMagic + sizeof(kFileBlobMagic));
    header.push_back(kFileBlobVersionV2);
    header.push_back(0);
    header.push_back(kFileBlobAlgoRaw);
    header.push_back(0);
    mi::server::proto::WriteUint64(static_cast<std::uint64_t>(plaintext.size()),
                                   header);
    mi::server::proto::WriteUint64(0, header);
    mi::server::proto::WriteUint64(static_cast<std::uint64_t>(plaintext.size()),
                                   header);
    if (header.size() != kFileBlobV2PrefixSize) {
      return false;
    }

    std::array<std::uint8_t, 24> nonce{};
    if (!RandomBytes(nonce.data(), nonce.size())) {
      return false;
    }

    out_blob.resize(header.size() + nonce.size() + 16 + plaintext.size());
    std::memcpy(out_blob.data(), header.data(), header.size());
    std::memcpy(out_blob.data() + header.size(), nonce.data(), nonce.size());
    std::uint8_t* mac = out_blob.data() + header.size() + nonce.size();
    std::uint8_t* cipher = mac + 16;
    crypto_aead_lock(cipher, mac, key.data(), nonce.data(), header.data(),
                     header.size(), plaintext.data(), plaintext.size());
    return true;
  }

  std::vector<std::uint8_t> stage1;
  if (!DeflateCompress(plaintext.data(), plaintext.size(), 1, stage1)) {
    return false;
  }
  if (stage1.size() >= plaintext.size()) {
    crypto_wipe(stage1.data(), stage1.size());
    std::vector<std::uint8_t> header;
    header.reserve(kFileBlobV2PrefixSize);
    header.insert(header.end(), kFileBlobMagic,
                  kFileBlobMagic + sizeof(kFileBlobMagic));
    header.push_back(kFileBlobVersionV2);
    header.push_back(0);
    header.push_back(kFileBlobAlgoRaw);
    header.push_back(0);
    mi::server::proto::WriteUint64(static_cast<std::uint64_t>(plaintext.size()),
                                   header);
    mi::server::proto::WriteUint64(0, header);
    mi::server::proto::WriteUint64(static_cast<std::uint64_t>(plaintext.size()),
                                   header);
    if (header.size() != kFileBlobV2PrefixSize) {
      return false;
    }

    std::array<std::uint8_t, 24> nonce{};
    if (!RandomBytes(nonce.data(), nonce.size())) {
      return false;
    }

    out_blob.resize(header.size() + nonce.size() + 16 + plaintext.size());
    std::memcpy(out_blob.data(), header.data(), header.size());
    std::memcpy(out_blob.data() + header.size(), nonce.data(), nonce.size());
    std::uint8_t* mac = out_blob.data() + header.size() + nonce.size();
    std::uint8_t* cipher = mac + 16;
    crypto_aead_lock(cipher, mac, key.data(), nonce.data(), header.data(),
                     header.size(), plaintext.data(), plaintext.size());
    return true;
  }

  std::vector<std::uint8_t> stage2;
  if (!DeflateCompress(stage1.data(), stage1.size(), 9, stage2)) {
    crypto_wipe(stage1.data(), stage1.size());
    return false;
  }

  std::vector<std::uint8_t> header;
  header.reserve(kFileBlobV2PrefixSize);
  header.insert(header.end(), kFileBlobMagic,
                kFileBlobMagic + sizeof(kFileBlobMagic));
  header.push_back(kFileBlobVersionV2);
  header.push_back(kFileBlobFlagDoubleCompression);
  header.push_back(kFileBlobAlgoDeflate);
  header.push_back(0);
  mi::server::proto::WriteUint64(static_cast<std::uint64_t>(plaintext.size()),
                                 header);
  mi::server::proto::WriteUint64(static_cast<std::uint64_t>(stage1.size()),
                                 header);
  mi::server::proto::WriteUint64(static_cast<std::uint64_t>(stage2.size()),
                                 header);
  if (header.size() != kFileBlobV2PrefixSize) {
    crypto_wipe(stage1.data(), stage1.size());
    crypto_wipe(stage2.data(), stage2.size());
    return false;
  }

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    crypto_wipe(stage1.data(), stage1.size());
    crypto_wipe(stage2.data(), stage2.size());
    return false;
  }

  out_blob.resize(header.size() + nonce.size() + 16 + stage2.size());
  std::memcpy(out_blob.data(), header.data(), header.size());
  std::memcpy(out_blob.data() + header.size(), nonce.data(), nonce.size());
  std::uint8_t* mac = out_blob.data() + header.size() + nonce.size();
  std::uint8_t* cipher = mac + 16;
  crypto_aead_lock(cipher, mac, key.data(), nonce.data(), header.data(),
                   header.size(), stage2.data(), stage2.size());

  crypto_wipe(stage1.data(), stage1.size());
  crypto_wipe(stage2.data(), stage2.size());
  return true;
}

bool DecryptFileBlob(const std::vector<std::uint8_t>& blob,
                     const std::array<std::uint8_t, 32>& key,
                     std::vector<std::uint8_t>& out_plaintext) {
  out_plaintext.clear();
  if (blob.size() < kFileBlobV1HeaderSize) {
    return false;
  }
  if (std::memcmp(blob.data(), kFileBlobMagic, sizeof(kFileBlobMagic)) != 0) {
    return false;
  }
  const std::uint8_t version = blob[sizeof(kFileBlobMagic)];

  std::size_t header_len = 0;
  std::size_t header_size = 0;
  std::uint8_t flags = 0;
  std::uint8_t algo = 0;
  std::uint64_t original_size = 0;
  std::uint64_t stage1_size = 0;
  std::uint64_t stage2_size = 0;
  if (version == kFileBlobVersionV1) {
    header_len = kFileBlobV1PrefixSize;
    header_size = kFileBlobV1HeaderSize;
  } else if (version == kFileBlobVersionV2) {
    header_len = kFileBlobV2PrefixSize;
    header_size = kFileBlobV2HeaderSize;
    if (blob.size() < header_size) {
      return false;
    }
    std::size_t off = sizeof(kFileBlobMagic) + 1;
    if (off + 3 > blob.size()) {
      return false;
    }
    flags = blob[off++];
    algo = blob[off++];
    off++;  // reserved
    if (!mi::server::proto::ReadUint64(blob, off, original_size) ||
        !mi::server::proto::ReadUint64(blob, off, stage1_size) ||
        !mi::server::proto::ReadUint64(blob, off, stage2_size) ||
        off != kFileBlobV2PrefixSize) {
      return false;
    }
    if (original_size == 0 ||
        original_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
      return false;
    }
    if (stage2_size == 0 ||
        stage2_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
      return false;
    }
  } else if (version == kFileBlobVersionV3) {
    header_len = kFileBlobV3PrefixSize;
    header_size = kFileBlobV3HeaderSize;
    if (blob.size() < header_size + 16 + 1) {
      return false;
    }

    std::size_t off = sizeof(kFileBlobMagic) + 1;
    if (off + 3 > blob.size()) {
      return false;
    }
    flags = blob[off++];
    algo = blob[off++];
    off++;  // reserved
    std::uint32_t chunk_size = 0;
    if (!mi::server::proto::ReadUint32(blob, off, chunk_size) ||
        !mi::server::proto::ReadUint64(blob, off, original_size) ||
        off + 24 != kFileBlobV3PrefixSize) {
      return false;
    }
    if (algo != kFileBlobAlgoRaw) {
      return false;
    }
    if (chunk_size == 0 || chunk_size > (kE2eeBlobChunkBytes - 16u)) {
      return false;
    }
    if (original_size == 0 ||
        original_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
      return false;
    }
    const std::uint64_t chunks =
        (original_size + chunk_size - 1) / chunk_size;
    if (chunks == 0 || chunks > (1ull << 31)) {
      return false;
    }
    const std::uint64_t expect =
        static_cast<std::uint64_t>(kFileBlobV3PrefixSize) +
        chunks * 16u + original_size;
    if (expect == 0 ||
        expect > static_cast<std::uint64_t>(kMaxChatFileBlobBytes) ||
        expect != static_cast<std::uint64_t>(blob.size())) {
      return false;
    }

    std::array<std::uint8_t, 24> base_nonce{};
    std::memcpy(base_nonce.data(), blob.data() + off, base_nonce.size());

    out_plaintext.resize(static_cast<std::size_t>(original_size));
    const std::uint8_t* header = blob.data();
    std::size_t blob_off = kFileBlobV3PrefixSize;
    std::uint64_t out_off = 0;
    for (std::uint64_t idx = 0; idx < chunks; ++idx) {
      const std::size_t want =
          static_cast<std::size_t>(std::min<std::uint64_t>(
              chunk_size, original_size - out_off));
      if (want == 0 || blob_off + 16 + want > blob.size()) {
        out_plaintext.clear();
        return false;
      }

      std::array<std::uint8_t, 24> nonce = base_nonce;
      for (int i = 0; i < 8; ++i) {
        nonce[16 + i] = static_cast<std::uint8_t>((idx >> (8 * i)) & 0xFF);
      }

      const std::uint8_t* mac = blob.data() + blob_off;
      const std::uint8_t* cipher = blob.data() + blob_off + 16;
      const int ok = crypto_aead_unlock(out_plaintext.data() + out_off, mac,
                                        key.data(), nonce.data(), header,
                                        header_len, cipher, want);
      if (ok != 0) {
        crypto_wipe(out_plaintext.data(), out_plaintext.size());
        out_plaintext.clear();
        return false;
      }
      blob_off += 16 + want;
      out_off += want;
    }
    if (out_off != original_size || blob_off != blob.size()) {
      crypto_wipe(out_plaintext.data(), out_plaintext.size());
      out_plaintext.clear();
      return false;
    }
    (void)flags;
    return true;
  } else {
    return false;
  }

  const std::uint8_t* header = blob.data();
  const std::uint8_t* nonce = blob.data() + header_len;
  const std::uint8_t* mac = blob.data() + header_len + 24;
  const std::size_t cipher_off = header_size;
  const std::size_t cipher_len = blob.size() - cipher_off;
  if (version == kFileBlobVersionV2 &&
      cipher_len != static_cast<std::size_t>(stage2_size)) {
    return false;
  }

  std::vector<std::uint8_t> stage2_plain;
  stage2_plain.resize(cipher_len);
  const int ok = crypto_aead_unlock(
      stage2_plain.data(), mac, key.data(), nonce, header, header_len,
      blob.data() + cipher_off, cipher_len);
  if (ok != 0) {
    crypto_wipe(stage2_plain.data(), stage2_plain.size());
    return false;
  }

  if (version == kFileBlobVersionV1) {
    out_plaintext = std::move(stage2_plain);
    return true;
  }

  if ((flags & kFileBlobFlagDoubleCompression) == 0) {
    if (original_size != static_cast<std::uint64_t>(stage2_plain.size())) {
      crypto_wipe(stage2_plain.data(), stage2_plain.size());
      return false;
    }
    out_plaintext = std::move(stage2_plain);
    return true;
  }
  if (algo != kFileBlobAlgoDeflate) {
    crypto_wipe(stage2_plain.data(), stage2_plain.size());
    return false;
  }
  if (stage1_size == 0 ||
      stage1_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
    crypto_wipe(stage2_plain.data(), stage2_plain.size());
    return false;
  }

  std::vector<std::uint8_t> stage1_plain;
  if (!DeflateDecompress(stage2_plain.data(), stage2_plain.size(),
                         static_cast<std::size_t>(stage1_size),
                         stage1_plain)) {
    crypto_wipe(stage2_plain.data(), stage2_plain.size());
    return false;
  }
  crypto_wipe(stage2_plain.data(), stage2_plain.size());

  std::vector<std::uint8_t> original;
  if (!DeflateDecompress(stage1_plain.data(), stage1_plain.size(),
                         static_cast<std::size_t>(original_size), original)) {
    crypto_wipe(stage1_plain.data(), stage1_plain.size());
    return false;
  }
  crypto_wipe(stage1_plain.data(), stage1_plain.size());

  out_plaintext = std::move(original);
  return true;
}

}  // namespace

std::vector<std::uint8_t> ClientCore::BuildGroupCallKeyDistSigMessage(
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id,
    const std::array<std::uint8_t, 32>& call_key) {
  return ::mi::client::BuildGroupCallKeyDistSigMessage(group_id, call_id, key_id,
                                                       call_key);
}

bool ClientCore::EncodeGroupCallKeyDist(
    const std::array<std::uint8_t, 16>& msg_id,
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id,
    const std::array<std::uint8_t, 32>& call_key,
    const std::vector<std::uint8_t>& sig,
    std::vector<std::uint8_t>& out) {
  return EncodeChatGroupCallKeyDist(msg_id, group_id, call_id, key_id, call_key,
                                    sig, out);
}

bool ClientCore::DecodeGroupCallKeyDist(
    const std::vector<std::uint8_t>& payload,
    std::size_t& offset,
    std::string& out_group_id,
    std::array<std::uint8_t, 16>& out_call_id,
    std::uint32_t& out_key_id,
    std::array<std::uint8_t, 32>& out_call_key,
    std::vector<std::uint8_t>& out_sig) {
  return DecodeChatGroupCallKeyDist(payload, offset, out_group_id, out_call_id,
                                    out_key_id, out_call_key, out_sig);
}

bool ClientCore::EncodeGroupCallKeyReq(
    const std::array<std::uint8_t, 16>& msg_id,
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t want_key_id,
    std::vector<std::uint8_t>& out) {
  return EncodeChatGroupCallKeyReq(msg_id, group_id, call_id, want_key_id, out);
}

bool ClientCore::DecodeGroupCallKeyReq(
    const std::vector<std::uint8_t>& payload,
    std::size_t& offset,
    std::string& out_group_id,
    std::array<std::uint8_t, 16>& out_call_id,
    std::uint32_t& out_want_key_id) {
  return DecodeChatGroupCallKeyReq(payload, offset, out_group_id, out_call_id,
                                   out_want_key_id);
}

bool ClientCore::Init(const std::string& config_path) {
  config_path_ = config_path;
  ClientConfig cfg;
  ConfigService config_service;
  SecurityService security_service;
  std::string err;
  security_service.StartEndpointHardening();
  const bool loaded = config_service.Load(config_path_, cfg, err);
  remote_mode_ = loaded;
  const std::filesystem::path config_dir = config_service.config_dir();
  const std::filesystem::path data_dir = config_service.data_dir();
  if (!loaded) {
    last_error_ = err;
    if (err == "client section missing") {
      last_error_.clear();
      remote_mode_ = false;
    } else {
      return false;
    }
  }
  if (remote_mode_) {
    server_ip_ = cfg.server_ip;
    use_tls_ = cfg.use_tls;
    require_tls_ = cfg.require_tls;
    tls_verify_mode_ = cfg.tls_verify_mode;
    tls_verify_hostname_ = cfg.tls_verify_hostname;
    tls_ca_bundle_path_.clear();
    if (!cfg.tls_ca_bundle_path.empty()) {
      std::filesystem::path ca_path = cfg.tls_ca_bundle_path;
      if (!ca_path.is_absolute()) {
        ca_path = config_dir / ca_path;
      }
      tls_ca_bundle_path_ = ca_path.string();
    }
    if (tls_verify_mode_ != TlsVerifyMode::kPin &&
        !tls_ca_bundle_path_.empty()) {
      std::error_code ec;
      const std::filesystem::path ca_path(tls_ca_bundle_path_);
      if (!std::filesystem::exists(ca_path, ec) || ec) {
        last_error_ = "tls ca bundle missing";
        return false;
      }
    }
    use_kcp_ = cfg.kcp.enable;
    kcp_cfg_ = cfg.kcp;
    media_config_ = cfg.media;
    if (use_kcp_) {
      use_tls_ = false;
      require_tls_ = false;
    }
    if (use_tls_) {
      if (mi::platform::tls::IsStubbed()) {
        last_error_ = "tls stub build";
        return false;
      }
      if (!mi::platform::tls::IsSupported()) {
        last_error_ = "tls unsupported";
        return false;
      }
    }
    server_port_ = use_kcp_ && cfg.kcp.server_port != 0 ? cfg.kcp.server_port
                                                        : cfg.server_port;
    transport_kind_ = use_kcp_
                          ? mi::server::TransportKind::kKcp
                          : (use_tls_ ? mi::server::TransportKind::kTls
                                      : mi::server::TransportKind::kTcp);
    auth_mode_ = cfg.auth_mode;
    proxy_ = cfg.proxy;
    device_sync_enabled_ = cfg.device_sync.enabled;
    device_sync_is_primary_ =
        (cfg.device_sync.role == mi::client::DeviceSyncRole::kPrimary);
    device_sync_rotate_interval_sec_ = cfg.device_sync.rotate_interval_sec;
    device_sync_rotate_message_limit_ = cfg.device_sync.rotate_message_limit;
    device_sync_ratchet_enable_ = cfg.device_sync.ratchet_enable;
    device_sync_ratchet_max_skip_ = cfg.device_sync.ratchet_max_skip;
    device_sync_last_rotate_ms_ = 0;
    device_sync_send_count_ = 0;
    device_sync_send_counter_ = 0;
    device_sync_recv_counter_ = 0;
    device_sync_prev_key_.fill(0);
    device_sync_prev_key_until_ms_ = 0;
    device_sync_prev_recv_counter_ = 0;
    identity_policy_.rotation_days = cfg.identity.rotation_days;
    identity_policy_.legacy_retention_days = cfg.identity.legacy_retention_days;
    identity_policy_.tpm_enable = cfg.identity.tpm_enable;
    identity_policy_.tpm_require = cfg.identity.tpm_require;
    pqc_precompute_pool_ = cfg.perf.pqc_precompute_pool;
    cover_traffic_enabled_ = core_helpers::ResolveCoverTrafficEnabled(cfg.traffic);
    cover_traffic_interval_sec_ = cfg.traffic.cover_traffic_interval_sec;
    cover_traffic_last_sent_ms_ = 0;
    trust_store_path_.clear();
    trust_store_tls_required_ = false;
    const bool allow_pinned_fingerprint =
        (tls_verify_mode_ != TlsVerifyMode::kCa);
    require_pinned_fingerprint_ =
        (tls_verify_mode_ == TlsVerifyMode::kPin);
    pinned_server_fingerprint_.clear();
    pending_server_fingerprint_.clear();
    pending_server_pin_.clear();
    if (!use_kcp_) {
      std::string security_err;
      if (!security_service.LoadTrustFromConfig(
              cfg, data_dir, server_ip_, server_port_, require_tls_,
              allow_pinned_fingerprint, trust_store_path_,
              pinned_server_fingerprint_, trust_store_tls_required_,
              security_err)) {
        last_error_ =
            security_err.empty() ? "trust store init failed" : security_err;
        return false;
      }
      if (!allow_pinned_fingerprint) {
        pinned_server_fingerprint_.clear();
      }
    } else {
      require_pinned_fingerprint_ = false;
      trust_store_path_.clear();
      pinned_server_fingerprint_.clear();
    }
    if (local_handle_) {
      mi_server_destroy(local_handle_);
      local_handle_ = nullptr;
    }
    token_.clear();
    last_error_.clear();
    send_seq_ = 0;

    e2ee_ = mi::client::e2ee::Engine{};
    e2ee_.SetPqcPoolSize(pqc_precompute_pool_);
    e2ee_inited_ = false;
    prekey_published_ = false;
    std::filesystem::path base = data_dir;
    if (base.empty()) {
      base = config_dir;
    }
    if (base.empty()) {
      base = std::filesystem::path{"."};
    }
    e2ee_state_dir_ = base / "e2ee_state";
    kt_state_path_ = e2ee_state_dir_ / "kt_state.bin";
    kt_require_signature_ = cfg.kt.require_signature;
    kt_gossip_alert_threshold_ = cfg.kt.gossip_alert_threshold;
    kt_root_pubkey_.clear();
    kt_root_pubkey_loaded_ = false;
    kt_gossip_mismatch_count_ = 0;
    kt_gossip_alerted_ = false;
    if (kt_require_signature_) {
      std::vector<std::uint8_t> key_bytes;
      if (!cfg.kt.root_pubkey_path.empty()) {
        std::filesystem::path key_path = cfg.kt.root_pubkey_path;
        if (!key_path.is_absolute()) {
          key_path = config_dir / key_path;
        }
        std::string key_err;
        if (!core_helpers::ReadFileBytes(key_path, key_bytes, key_err)) {
          last_error_ = key_err.empty() ? "kt root pubkey load failed" : key_err;
          return false;
        }
      } else if (!cfg.kt.root_pubkey_hex.empty()) {
        if (!mi::common::HexToBytes(cfg.kt.root_pubkey_hex, key_bytes)) {
          last_error_ = "kt root pubkey hex invalid";
          return false;
        }
      } else {
        std::string key_err;
        if (!core_helpers::TryLoadKtRootPubkeyFromLoopback(
                config_dir, server_ip_, key_bytes, key_err)) {
          std::string data_err;
          if (!core_helpers::TryLoadKtRootPubkeyFromLoopback(
                  data_dir, server_ip_, key_bytes, data_err)) {
            if (data_err.empty()) {
              data_err = key_err;
            }
            last_error_ = data_err.empty() ? "kt root pubkey missing" : data_err;
            return false;
          }
        }
      }
      if (key_bytes.size() != mi::server::kKtSthSigPublicKeyBytes) {
        last_error_ = "kt root pubkey size invalid";
        return false;
      }
      kt_root_pubkey_ = std::move(key_bytes);
      kt_root_pubkey_loaded_ = true;
    }
      if (!cfg.device_sync.key_path.empty()) {
        std::filesystem::path kp = cfg.device_sync.key_path;
        if (!kp.is_absolute()) {
          kp = data_dir / kp;
        }
        device_sync_key_path_ = kp;
      } else {
        device_sync_key_path_ = e2ee_state_dir_ / "device_sync_key.bin";
      }
    LoadKtState();
    if (!LoadOrCreateDeviceId() || device_id_.empty()) {
      if (last_error_.empty()) {
        last_error_ = "device id unavailable";
      }
      return false;
    }
    if (device_sync_enabled_ && !LoadDeviceSyncKey()) {
      if (device_sync_is_primary_) {
        return false;
      }
      last_error_.clear();
    }
    if (require_tls_ && !use_tls_) {
      last_error_ = "require_tls=1 but use_tls=0";
      return false;
    }
    if (trust_store_tls_required_ && !use_tls_) {
      last_error_ = "tls downgrade detected";
      return false;
    }
    return !server_ip_.empty() && server_port_ != 0;
  }

  server_ip_.clear();
  server_port_ = 0;
  use_tls_ = false;
  require_tls_ = true;
  tls_verify_mode_ = TlsVerifyMode::kPin;
  tls_verify_hostname_ = true;
  tls_ca_bundle_path_.clear();
  use_kcp_ = false;
  kcp_cfg_ = KcpConfig{};
  media_config_ = ClientConfig{}.media;
  transport_kind_ = mi::server::TransportKind::kLocal;
  auth_mode_ = AuthMode::kLegacy;
  proxy_ = ProxyConfig{};
  device_sync_enabled_ = false;
  device_sync_is_primary_ = true;
  device_sync_key_loaded_ = false;
  device_sync_key_.fill(0);
  device_sync_key_path_.clear();
  device_sync_rotate_interval_sec_ =
      ClientConfig{}.device_sync.rotate_interval_sec;
  device_sync_rotate_message_limit_ =
      ClientConfig{}.device_sync.rotate_message_limit;
  device_sync_ratchet_enable_ = ClientConfig{}.device_sync.ratchet_enable;
  device_sync_ratchet_max_skip_ = ClientConfig{}.device_sync.ratchet_max_skip;
  device_sync_last_rotate_ms_ = 0;
  device_sync_send_count_ = 0;
  device_sync_send_counter_ = 0;
  device_sync_recv_counter_ = 0;
  device_sync_prev_key_.fill(0);
  device_sync_prev_key_until_ms_ = 0;
  device_sync_prev_recv_counter_ = 0;
  device_id_.clear();
  trust_store_path_.clear();
  trust_store_tls_required_ = false;
  require_pinned_fingerprint_ = true;
  pinned_server_fingerprint_.clear();
  pending_server_fingerprint_.clear();
  pending_server_pin_.clear();
  identity_policy_ = mi::client::e2ee::IdentityPolicy{};
  pqc_precompute_pool_ = ClientConfig{}.perf.pqc_precompute_pool;
  cover_traffic_enabled_ =
      core_helpers::ResolveCoverTrafficEnabled(ClientConfig{}.traffic);
  cover_traffic_interval_sec_ = ClientConfig{}.traffic.cover_traffic_interval_sec;
  cover_traffic_last_sent_ms_ = 0;
  last_error_.clear();
  if (local_handle_) {
    mi_server_destroy(local_handle_);
    local_handle_ = nullptr;
  }

  e2ee_ = mi::client::e2ee::Engine{};
  e2ee_.SetPqcPoolSize(pqc_precompute_pool_);
  e2ee_inited_ = false;
  prekey_published_ = false;
  std::filesystem::path base = data_dir;
  if (base.empty()) {
    base = config_dir;
  }
  if (base.empty()) {
    base = std::filesystem::path{"."};
  }
  e2ee_state_dir_ = base / "e2ee_state";
  kt_state_path_ = e2ee_state_dir_ / "kt_state.bin";
  kt_require_signature_ = false;
  kt_gossip_alert_threshold_ = 3;
  kt_root_pubkey_.clear();
  kt_root_pubkey_loaded_ = false;
  kt_gossip_mismatch_count_ = 0;
  kt_gossip_alerted_ = false;
  device_sync_key_path_ = e2ee_state_dir_ / "device_sync_key.bin";
  LoadKtState();
  if (!LoadOrCreateDeviceId() || device_id_.empty()) {
    if (last_error_.empty()) {
      last_error_ = "device id unavailable";
    }
    return false;
  }
  local_handle_ = mi_server_create(config_path.c_str());
  return local_handle_ != nullptr;
}
















bool ClientCore::EnsurePreKeyPublished() {
  if (!EnsureE2ee()) {
    return false;
  }
  bool rotated = false;
  std::string rotate_err;
  if (!e2ee_.MaybeRotatePreKeys(rotated, rotate_err)) {
    last_error_ = rotate_err.empty() ? "prekey rotation failed" : rotate_err;
    return false;
  }
  if (rotated) {
    prekey_published_ = false;
  }
  if (prekey_published_) {
    return true;
  }
  if (!PublishPreKeyBundle()) {
    return false;
  }
  prekey_published_ = true;
  return true;
}

bool ClientCore::MaybeSendCoverTraffic() {
  if (!cover_traffic_enabled_ || cover_traffic_interval_sec_ == 0) {
    return true;
  }
  const std::uint64_t now_ms = mi::platform::NowSteadyMs();
  const std::uint64_t interval_ms =
      static_cast<std::uint64_t>(cover_traffic_interval_sec_) * 1000;
  if (cover_traffic_last_sent_ms_ != 0 &&
      now_ms - cover_traffic_last_sent_ms_ < interval_ms) {
    return true;
  }
  std::vector<std::uint8_t> payload;
  std::string pad_err;
  if (!padding::PadPayload({}, payload, pad_err)) {
    return false;
  }
  const std::string saved_err = last_error_;
  std::vector<std::uint8_t> ignore;
  const bool ok = ProcessEncrypted(mi::server::FrameType::kHeartbeat, payload,
                                   ignore);
  last_error_ = saved_err;
  if (ok) {
    cover_traffic_last_sent_ms_ = now_ms;
  }
  return ok;
}

bool ClientCore::FetchPreKeyBundle(const std::string& peer_username,
                                  std::vector<std::uint8_t>& out_bundle) {
  out_bundle.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(peer_username, plain);
  mi::server::proto::WriteUint64(kt_tree_size_, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kPreKeyFetch, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "prekey fetch failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "prekey response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, err);
    last_error_ = err.empty() ? "prekey fetch failed" : err;
    return false;
  }
  std::size_t off = 1;
  if (!mi::server::proto::ReadBytes(resp_payload, off, out_bundle)) {
    last_error_ = "prekey response invalid";
    out_bundle.clear();
    return false;
  }
  if (off < resp_payload.size()) {
    std::uint32_t kt_version = 0;
    if (!mi::server::proto::ReadUint32(resp_payload, off, kt_version)) {
      last_error_ = "kt response invalid";
      return false;
    }
    if (kt_version == 1) {
      std::uint64_t tree_size = 0;
      std::vector<std::uint8_t> root_bytes;
      std::uint64_t leaf_index = 0;
      std::uint32_t audit_count = 0;
      std::uint32_t cons_count = 0;
      if (!mi::server::proto::ReadUint64(resp_payload, off, tree_size) ||
          !mi::server::proto::ReadBytes(resp_payload, off, root_bytes) ||
          !mi::server::proto::ReadUint64(resp_payload, off, leaf_index) ||
          !mi::server::proto::ReadUint32(resp_payload, off, audit_count)) {
        last_error_ = "kt response invalid";
        return false;
      }
      if (root_bytes.size() != 32 || tree_size == 0 ||
          leaf_index >= tree_size) {
        last_error_ = "kt response invalid";
        return false;
      }

      std::vector<mi::server::Sha256Hash> audit_path;
      audit_path.reserve(audit_count);
      for (std::uint32_t i = 0; i < audit_count; ++i) {
        std::vector<std::uint8_t> node;
        if (!mi::server::proto::ReadBytes(resp_payload, off, node) ||
            node.size() != 32) {
          last_error_ = "kt response invalid";
          return false;
        }
        mi::server::Sha256Hash h{};
        std::copy_n(node.begin(), h.size(), h.begin());
        audit_path.push_back(h);
      }
      if (!mi::server::proto::ReadUint32(resp_payload, off, cons_count)) {
        last_error_ = "kt response invalid";
        return false;
      }
      std::vector<mi::server::Sha256Hash> cons_path;
      cons_path.reserve(cons_count);
      for (std::uint32_t i = 0; i < cons_count; ++i) {
        std::vector<std::uint8_t> node;
        if (!mi::server::proto::ReadBytes(resp_payload, off, node) ||
            node.size() != 32) {
          last_error_ = "kt response invalid";
          return false;
        }
        mi::server::Sha256Hash h{};
        std::copy_n(node.begin(), h.size(), h.begin());
        cons_path.push_back(h);
      }
      std::vector<std::uint8_t> sth_sig;
      if (!mi::server::proto::ReadBytes(resp_payload, off, sth_sig)) {
        last_error_ = "kt response invalid";
        return false;
      }
      if (off != resp_payload.size()) {
        last_error_ = "kt response invalid";
        return false;
      }

      mi::server::Sha256Hash root{};
      std::copy_n(root_bytes.begin(), root.size(), root.begin());

      std::string leaf_err;
      const auto leaf_hash = KtLeafHashFromBundle(peer_username, out_bundle, leaf_err);
      if (!leaf_err.empty()) {
        last_error_ = leaf_err;
        return false;
      }
      mi::server::Sha256Hash computed_root{};
      if (!RootFromAuditPath(leaf_hash, static_cast<std::size_t>(leaf_index),
                             static_cast<std::size_t>(tree_size), audit_path,
                             computed_root) ||
          computed_root != root) {
        RecordKtGossipMismatch("kt inclusion proof invalid");
        return false;
      }

      if (kt_tree_size_ > 0) {
        if (tree_size < kt_tree_size_) {
          RecordKtGossipMismatch("kt tree rolled back");
          return false;
        }
        if (tree_size == kt_tree_size_) {
          if (root != kt_root_) {
            RecordKtGossipMismatch("kt split view");
            return false;
          }
        } else {
          if (!VerifyConsistencyProof(static_cast<std::size_t>(kt_tree_size_),
                                      static_cast<std::size_t>(tree_size),
                                      kt_root_, root, cons_path)) {
            RecordKtGossipMismatch("kt consistency proof invalid");
            return false;
          }
        }
      }

      if (kt_require_signature_) {
        if (!kt_root_pubkey_loaded_) {
          last_error_ = "kt root pubkey missing";
          return false;
        }
        if (sth_sig.size() != mi::server::kKtSthSigBytes) {
          RecordKtGossipMismatch("kt signature size invalid");
          return false;
        }
        mi::server::KeyTransparencySth sth;
        sth.tree_size = tree_size;
        sth.root = root;
        sth.signature = sth_sig;
        const auto sig_msg = mi::server::BuildKtSthSignatureMessage(sth);
        std::string sig_err;
        if (!mi::client::e2ee::Engine::VerifyDetached(sig_msg, sth_sig,
                                                      kt_root_pubkey_, sig_err)) {
          RecordKtGossipMismatch(sig_err.empty() ? "kt signature invalid" : sig_err);
          return false;
        }
      }
      kt_gossip_mismatch_count_ = 0;
      kt_gossip_alerted_ = false;
      kt_tree_size_ = tree_size;
      kt_root_ = root;
      SaveKtState();
      return true;
    }
    last_error_ = "kt version unsupported";
    return false;
  }
  return true;
}








bool ClientCore::ProcessEncrypted(mi::server::FrameType type,
                                  const std::vector<std::uint8_t>& plain,
                                  std::vector<std::uint8_t>& out_plain) {
  return TransportService().ProcessEncrypted(*this, type, plain, out_plain);
}

bool ClientCore::Heartbeat() {
  last_error_.clear();
  std::vector<std::uint8_t> ignore;
  if (!ProcessEncrypted(mi::server::FrameType::kHeartbeat, {}, ignore)) {
    if (last_error_.empty()) {
      last_error_ = "heartbeat failed";
    }
    return false;
  }
  return true;
}

std::vector<ClientCore::DeviceEntry> ClientCore::ListDevices() {
  std::vector<DeviceEntry> out;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return out;
  }
  if (device_id_.empty()) {
    LoadOrCreateDeviceId();
  }
  if (device_id_.empty()) {
    last_error_ = "device id unavailable";
    return out;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(device_id_, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kDeviceList, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "device list failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    last_error_ = "device list response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "device list failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "device list response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::string dev;
    std::uint32_t age = 0;
    if (!mi::server::proto::ReadString(resp_payload, off, dev) ||
        !mi::server::proto::ReadUint32(resp_payload, off, age)) {
      last_error_ = "device list response invalid";
      out.clear();
      return out;
    }
    DeviceEntry e;
    e.device_id = std::move(dev);
    e.last_seen_sec = age;
    out.push_back(std::move(e));
  }
  if (off != resp_payload.size()) {
    last_error_ = "device list response invalid";
    out.clear();
    return out;
  }
  return out;
}

bool ClientCore::KickDevice(const std::string& target_device_id) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (device_id_.empty()) {
    LoadOrCreateDeviceId();
  }
  if (device_id_.empty()) {
    last_error_ = "device id unavailable";
    return false;
  }
  if (target_device_id.empty()) {
    last_error_ = "device id empty";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(device_id_, plain);
  mi::server::proto::WriteString(target_device_id, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kDeviceKick, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "device kick failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "device kick response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "device kick failed" : server_err;
    return false;
  }
  if (resp_payload.size() != 1) {
    last_error_ = "device kick response invalid";
    return false;
  }

  if (device_sync_enabled_) {
    if (!device_sync_key_loaded_) {
      LoadDeviceSyncKey();
    }
    if (device_sync_key_loaded_) {
      std::array<std::uint8_t, 32> next_key{};
      if (RandomBytes(next_key.data(), next_key.size())) {
        std::vector<std::uint8_t> event_plain;
        if (EncodeDeviceSyncRotateKey(next_key, event_plain)) {
          std::vector<std::uint8_t> event_cipher;
          if (EncryptDeviceSync(event_plain, event_cipher) &&
              PushDeviceSyncCiphertext(event_cipher)) {
            StoreDeviceSyncKey(next_key);
          }
        }
      }
      last_error_.clear();
    }
  }
  return true;
}
































bool ClientCore::PublishPreKeyBundle() {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (!EnsureE2ee()) {
    return false;
  }

  std::vector<std::uint8_t> bundle;
  std::string err;
  if (!e2ee_.BuildPublishBundle(bundle, err)) {
    last_error_ = err.empty() ? "build prekey bundle failed" : err;
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteBytes(bundle, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kPreKeyPublish, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "prekey publish failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "prekey publish response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "prekey publish failed" : server_err;
    return false;
  }
  prekey_published_ = true;
  return true;
}
















bool ClientCore::DeriveMediaRoot(
    const std::string& peer_username,
    const std::array<std::uint8_t, 16>& call_id,
    std::array<std::uint8_t, 32>& out_media_root,
    std::string& out_error) {
  return MediaService().DeriveMediaRoot(*this, peer_username, call_id,
                                        out_media_root, out_error);
}






































}  // namespace mi::client
