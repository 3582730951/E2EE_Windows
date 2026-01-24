#include "messaging_service.h"

#include "client_core.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "chat_history_store.h"
#include "crypto.h"
#include "hex_utils.h"
#include "key_transparency.h"
#include "monocypher.h"
#include "platform_fs.h"
#include "platform_random.h"
#include "platform_time.h"
#include "protocol.h"

namespace mi::client {

using ChatDelivery = ClientCore::ChatDelivery;
using ChatFileMessage = ClientCore::ChatFileMessage;
using ChatPollResult = ClientCore::ChatPollResult;
using ChatPresenceEvent = ClientCore::ChatPresenceEvent;
using ChatReadReceipt = ClientCore::ChatReadReceipt;
using ChatStickerMessage = ClientCore::ChatStickerMessage;
using ChatTextMessage = ClientCore::ChatTextMessage;
using ChatTypingEvent = ClientCore::ChatTypingEvent;
using FriendEntry = ClientCore::FriendEntry;
using FriendRequestEntry = ClientCore::FriendRequestEntry;
using GroupCallEvent = ClientCore::GroupCallEvent;
using GroupCallSignalResult = ClientCore::GroupCallSignalResult;
using GroupChatFileMessage = ClientCore::GroupChatFileMessage;
using GroupChatTextMessage = ClientCore::GroupChatTextMessage;
using GroupInviteMessage = ClientCore::GroupInviteMessage;
using GroupMemberInfo = ClientCore::GroupMemberInfo;
using GroupMemberRole = ClientCore::GroupMemberRole;
using GroupNotice = ClientCore::GroupNotice;
using HistoryEntry = ClientCore::HistoryEntry;
using HistoryKind = ClientCore::HistoryKind;
using HistoryStatus = ClientCore::HistoryStatus;
using MediaRelayPacket = ClientCore::MediaRelayPacket;
using OutgoingChatFileMessage = ClientCore::OutgoingChatFileMessage;
using OutgoingChatStickerMessage = ClientCore::OutgoingChatStickerMessage;
using OutgoingChatTextMessage = ClientCore::OutgoingChatTextMessage;
using OutgoingGroupChatFileMessage = ClientCore::OutgoingGroupChatFileMessage;
using OutgoingGroupChatTextMessage = ClientCore::OutgoingGroupChatTextMessage;

namespace pfs = mi::platform::fs;

namespace {

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
constexpr std::uint8_t kGroupCipherMagic[4] = {'M', 'I', 'G', 'C'};
constexpr std::uint8_t kGroupCipherVersion = 1;
constexpr std::size_t kGroupCipherNonceBytes = 24;
constexpr std::size_t kGroupCipherMacBytes = 16;
constexpr std::size_t kMaxGroupSkippedMessageKeys = 2048;
constexpr std::size_t kMaxGroupSkip = 4096;
constexpr std::uint64_t kGroupSenderKeyRotationThreshold = 10000;
constexpr std::uint64_t kGroupSenderKeyRotationIntervalSec =
    7ull * 24ull * 60ull * 60ull;
constexpr std::uint64_t kSenderKeyDistResendIntervalMs = 5 * 1000;
constexpr std::size_t kMaxChatFileBytes = 300u * 1024u * 1024u;

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

constexpr std::size_t kChatEnvelopeBaseBytes =
    sizeof(kChatMagic) + 1 + 1 + 16;

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
  out.insert(out.end(), kGossipMagic, kGossipMagic + sizeof(kGossipMagic));
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

std::uint64_t NowUnixSeconds() {
  return mi::platform::NowUnixSeconds();
}

bool RandomBytes(std::uint8_t* out, std::size_t len) {
  return mi::platform::RandomBytes(out, len);
}

constexpr std::uint8_t kPadMagic[4] = {'M', 'I', 'P', 'D'};
constexpr std::size_t kPadHeaderBytes = 8;
constexpr std::size_t kPadBuckets[] = {256, 512, 1024, 2048, 4096, 8192, 16384};

std::size_t SelectPadTarget(std::size_t min_len) {
  for (const auto bucket : kPadBuckets) {
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
  if (round <= min_len) {
    return min_len;
  }
  std::uint32_t r = 0;
  if (!RandomUint32(r)) {
    return round;
  }
  const std::size_t span = round - min_len;
  return min_len + (static_cast<std::size_t>(r) % (span + 1));
}

bool PadPayload(const std::vector<std::uint8_t>& plain,
                std::vector<std::uint8_t>& out,
                std::string& error) {
  error.clear();
  out.clear();
  if (plain.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "pad size overflow";
    return false;
  }
  const std::size_t min_len = kPadHeaderBytes + plain.size();
  const std::size_t target_len = SelectPadTarget(min_len);
  out.reserve(target_len);
  out.insert(out.end(), kPadMagic, kPadMagic + sizeof(kPadMagic));
  const std::uint32_t len32 = static_cast<std::uint32_t>(plain.size());
  out.push_back(static_cast<std::uint8_t>(len32 & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len32 >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len32 >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len32 >> 24) & 0xFF));
  out.insert(out.end(), plain.begin(), plain.end());
  if (out.size() < target_len) {
    const std::size_t pad_len = target_len - out.size();
    const std::size_t offset = out.size();
    out.resize(target_len);
    if (!RandomBytes(out.data() + offset, pad_len)) {
      error = "pad rng failed";
      return false;
    }
  }
  return true;
}

bool UnpadPayload(const std::vector<std::uint8_t>& plain,
                  std::vector<std::uint8_t>& out,
                  std::string& error) {
  error.clear();
  out.clear();
  if (plain.size() < kPadHeaderBytes ||
      std::memcmp(plain.data(), kPadMagic, sizeof(kPadMagic)) != 0) {
    out = plain;
    return true;
  }
  const std::uint32_t len =
      static_cast<std::uint32_t>(plain[4]) |
      (static_cast<std::uint32_t>(plain[5]) << 8) |
      (static_cast<std::uint32_t>(plain[6]) << 16) |
      (static_cast<std::uint32_t>(plain[7]) << 24);
  if (kPadHeaderBytes + len > plain.size()) {
    error = "pad size invalid";
    return false;
  }
  out.assign(plain.begin() + kPadHeaderBytes,
             plain.begin() + kPadHeaderBytes + len);
  return true;
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

bool HexToFixedBytes16(const std::string& hex,
                       std::array<std::uint8_t, 16>& out) {
  std::vector<std::uint8_t> tmp;
  if (!mi::common::HexToBytes(hex, tmp) || tmp.size() != out.size()) {
    return false;
  }
  std::memcpy(out.data(), tmp.data(), out.size());
  return true;
}

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
  if (!mi::server::proto::WriteString(conv_id, out)) {
    return false;
  }
  return WriteFixed16(msg_id, out);
}

bool EncodeDeviceSyncGroupNotice(const std::string& group_id,
                                 const std::string& actor,
                                 const std::vector<std::uint8_t>& payload,
                                 std::vector<std::uint8_t>& out) {
  out.clear();
  out.push_back(kDeviceSyncEventGroupNotice);
  return mi::server::proto::WriteString(group_id, out) &&
         mi::server::proto::WriteString(actor, out) &&
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
    if (!mi::server::proto::ReadString(plain, off, out.conv_id) ||
        !mi::server::proto::ReadString(plain, off, out.sender) ||
        !mi::server::proto::ReadBytes(plain, off, out.envelope) ||
        off != plain.size()) {
      return false;
    }
    return true;
  }
  if (out.type == kDeviceSyncEventHistorySnapshot) {
    if (!mi::server::proto::ReadString(plain, off, out.target_device_id) ||
        off >= plain.size()) {
      return false;
    }
    std::uint32_t count = 0;
    if (!mi::server::proto::ReadUint32(plain, off, count)) {
      return false;
    }
    out.history.clear();
    out.history.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
      if (off >= plain.size()) {
        return false;
      }
      const std::uint8_t kind = plain[off++];
      if (kind != kHistorySnapshotKindEnvelope &&
          kind != kHistorySnapshotKindSystem) {
        return false;
      }
      if (off + 2 > plain.size()) {
        return false;
      }
      const std::uint8_t flags = plain[off++];
      const bool is_group = (flags & 0x01) != 0;
      const bool outgoing = (flags & 0x02) != 0;
      const std::uint8_t status = plain[off++];
      if (status > static_cast<std::uint8_t>(ChatHistoryStatus::kFailed)) {
        return false;
      }
      std::uint64_t ts = 0;
      std::string conv_id;
      if (!mi::server::proto::ReadUint64(plain, off, ts) ||
          !mi::server::proto::ReadString(plain, off, conv_id)) {
        return false;
      }
      ChatHistoryMessage msg;
      msg.is_group = is_group;
      msg.outgoing = outgoing;
      msg.timestamp_sec = ts;
      msg.conv_id = std::move(conv_id);
      msg.status = static_cast<ChatHistoryStatus>(status);
      if (kind == kHistorySnapshotKindSystem) {
        std::string text;
        if (!mi::server::proto::ReadString(plain, off, text)) {
          return false;
        }
        msg.is_system = true;
        msg.system_text_utf8 = std::move(text);
        out.history.push_back(std::move(msg));
        continue;
      }
      std::string sender;
      std::vector<std::uint8_t> envelope;
      if (!mi::server::proto::ReadString(plain, off, sender) ||
          !mi::server::proto::ReadBytes(plain, off, envelope)) {
        return false;
      }
      msg.sender = std::move(sender);
      msg.envelope = std::move(envelope);
      out.history.push_back(std::move(msg));
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

}  // namespace

bool MessagingService::JoinGroup(ClientCore& core, const std::string& group_id) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return false;
  }
  std::vector<std::uint8_t> plain;
  plain.push_back(0);  // join action
  mi::server::proto::WriteString(group_id, plain);
  std::vector<std::uint8_t> resp_plain;
  if (!core.ProcessEncrypted(mi::server::FrameType::kGroupEvent, plain,
                        resp_plain)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "join group failed";
    }
    return false;
  }
  if (resp_plain.empty()) {
    core.last_error_ = "join group response empty";
    return false;
  }
  if (resp_plain[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_plain, off, server_err);
    core.last_error_ = server_err.empty() ? "join group failed" : server_err;
    return false;
  }
  return true;
}

bool MessagingService::LeaveGroup(ClientCore& core, const std::string& group_id) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return false;
  }
  std::vector<std::uint8_t> plain;
  plain.push_back(1);  // leave action
  mi::server::proto::WriteString(group_id, plain);
  std::vector<std::uint8_t> resp_plain;
  if (!core.ProcessEncrypted(mi::server::FrameType::kGroupEvent, plain,
                        resp_plain)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "leave group failed";
    }
    return false;
  }
  if (resp_plain.empty()) {
    core.last_error_ = "leave group response empty";
    return false;
  }
  if (resp_plain[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_plain, off, server_err);
    core.last_error_ = server_err.empty() ? "leave group failed" : server_err;
    return false;
  }
  return true;
}

std::vector<std::string> MessagingService::ListGroupMembers(ClientCore& core, 
    const std::string& group_id) const {
  std::vector<std::string> out;
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return out;
  }
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return out;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kGroupMemberList, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group member list failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "group member list response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "group member list failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    core.last_error_ = "group member list response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::string user;
    if (!mi::server::proto::ReadString(resp_payload, off, user)) {
      core.last_error_ = "group member list response invalid";
      out.clear();
      return out;
    }
    out.push_back(std::move(user));
  }
  if (off != resp_payload.size()) {
    core.last_error_ = "group member list response invalid";
    out.clear();
    return out;
  }
  return out;
}

std::vector<ClientCore::GroupMemberInfo> MessagingService::ListGroupMembersInfo(ClientCore& core, 
    const std::string& group_id) const {
  std::vector<GroupMemberInfo> out;
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return out;
  }
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return out;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kGroupMemberInfoList, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group member info failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "group member info response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "group member info failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    core.last_error_ = "group member info response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::string user;
    if (!mi::server::proto::ReadString(resp_payload, off, user) ||
        off >= resp_payload.size()) {
      core.last_error_ = "group member info response invalid";
      out.clear();
      return out;
    }
    const std::uint8_t role_u8 = resp_payload[off++];
    if (role_u8 > static_cast<std::uint8_t>(GroupMemberRole::kMember)) {
      core.last_error_ = "group member info response invalid";
      out.clear();
      return out;
    }
    GroupMemberInfo e;
    e.username = std::move(user);
    e.role = static_cast<GroupMemberRole>(role_u8);
    out.push_back(std::move(e));
  }
  if (off != resp_payload.size()) {
    core.last_error_ = "group member info response invalid";
    out.clear();
    return out;
  }
  return out;
}

bool MessagingService::SetGroupMemberRole(ClientCore& core, const std::string& group_id,
                                    const std::string& target_username,
                                    GroupMemberRole role) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty() || target_username.empty()) {
    core.last_error_ = "invalid params";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  mi::server::proto::WriteString(target_username, plain);
  plain.push_back(static_cast<std::uint8_t>(role));

  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kGroupRoleSet, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group role set failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "group role set response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "group role set failed" : server_err;
    return false;
  }
  if (resp_payload.size() != 1) {
    core.last_error_ = "group role set response invalid";
    return false;
  }
  return true;
}

bool MessagingService::KickGroupMember(ClientCore& core, const std::string& group_id,
                                 const std::string& target_username) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty() || target_username.empty()) {
    core.last_error_ = "invalid params";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  mi::server::proto::WriteString(target_username, plain);
  std::vector<std::uint8_t> resp_plain;
  if (!core.ProcessEncrypted(mi::server::FrameType::kGroupKickMember, plain,
                        resp_plain)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group kick failed";
    }
    return false;
  }
  if (resp_plain.empty()) {
    core.last_error_ = "group kick response empty";
    return false;
  }
  if (resp_plain[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_plain, off, server_err);
    core.last_error_ = server_err.empty() ? "group kick failed" : server_err;
    return false;
  }
  std::size_t off = 1;
  std::uint32_t version = 0;
  if (!mi::server::proto::ReadUint32(resp_plain, off, version) ||
      off >= resp_plain.size()) {
    core.last_error_ = "group kick response invalid";
    return false;
  }
  const std::uint8_t reason = resp_plain[off++];
  (void)version;
  (void)reason;
  if (off != resp_plain.size()) {
    core.last_error_ = "group kick response invalid";
    return false;
  }
  return true;
}

bool MessagingService::SendGroupMessage(ClientCore& core, const std::string& group_id,
                                  std::uint32_t threshold) const {
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  mi::server::proto::WriteUint32(threshold, plain);
  std::vector<std::uint8_t> resp_plain;
  if (!core.ProcessEncrypted(mi::server::FrameType::kMessage, plain, resp_plain)) {
    return false;
  }
  return !resp_plain.empty() && resp_plain[0] != 0;
}

bool MessagingService::CreateGroup(ClientCore& core, std::string& out_group_id) const {
  out_group_id.clear();
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }

  std::array<std::uint8_t, 16> group_id{};
  if (!RandomBytes(group_id.data(), group_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  out_group_id = BytesToHexLower(group_id.data(), group_id.size());
  if (out_group_id.empty()) {
    core.last_error_ = "group id generation failed";
    return false;
  }

  if (!core.JoinGroup(out_group_id)) {
    out_group_id.clear();
    if (core.last_error_.empty()) {
      core.last_error_ = "create group failed";
    }
    return false;
  }

  return true;
}

bool MessagingService::SendGroupInvite(ClientCore& core, const std::string& group_id,
                                 const std::string& peer_username,
                                 std::string& out_message_id_hex) const {
  out_message_id_hex.clear();
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    if (group_id.empty()) {
      core.last_error_ = "group id empty";
      return false;
    }
    if (peer_username.empty()) {
      core.last_error_ = "peer empty";
      return false;
    }

    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      core.last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

    std::vector<std::uint8_t> envelope;
    if (!EncodeChatGroupInvite(msg_id, group_id, envelope)) {
      core.last_error_ = "encode group invite failed";
      out_message_id_hex.clear();
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      out_message_id_hex.clear();
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      out_message_id_hex.clear();
      return false;
    }
    if (!core.PushDeviceSyncCiphertext(event_cipher)) {
      out_message_id_hex.clear();
      return false;
    }
    return true;
  }
  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return false;
  }
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }

  const auto members = core.ListGroupMembers(group_id);
  if (members.empty()) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group member list empty";
    }
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatGroupInvite(msg_id, group_id, envelope)) {
    core.last_error_ = "encode group invite failed";
    out_message_id_hex.clear();
    return false;
  }

  if (!core.SendPrivateE2ee(peer_username, envelope)) {
    out_message_id_hex.clear();
    return false;
  }
  return true;
}

bool MessagingService::SendOffline(ClientCore& core, const std::string& recipient,
                             const std::vector<std::uint8_t>& payload) const {
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(recipient, plain);
  mi::server::proto::WriteBytes(payload, plain);
  std::vector<std::uint8_t> resp_plain;
  if (!core.ProcessEncrypted(mi::server::FrameType::kOfflinePush, plain,
                        resp_plain)) {
    return false;
  }
  return !resp_plain.empty() && resp_plain[0] == 1;
}

std::vector<std::vector<std::uint8_t>> MessagingService::PullOffline(ClientCore& core) const {
  std::vector<std::vector<std::uint8_t>> messages;
  if (!core.EnsureChannel()) {
    return messages;
  }

  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kOfflinePull, {}, resp_payload)) {
    return messages;
  }
  std::size_t offset = 0;
  if (resp_payload.empty() || resp_payload[0] == 0) {
    return messages;
  }
  offset = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, offset, count)) {
    return messages;
  }
  for (std::uint32_t i = 0; i < count; ++i) {
    std::vector<std::uint8_t> msg;
    if (!mi::server::proto::ReadBytes(resp_payload, offset, msg)) {
      break;
    }
    messages.push_back(std::move(msg));
  }
  return messages;
}

std::vector<ClientCore::FriendEntry> MessagingService::ListFriends(ClientCore& core) const {
  std::vector<FriendEntry> out;
  if (!core.EnsureChannel()) {
    return out;
  }
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kFriendList, {}, resp_payload)) {
    return out;
  }
  if (resp_payload.empty() || resp_payload[0] == 0) {
    return out;
  }
  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    FriendEntry e;
    if (!mi::server::proto::ReadString(resp_payload, off, e.username)) {
      break;
    }
    if (off < resp_payload.size()) {
      std::string remark;
      if (!mi::server::proto::ReadString(resp_payload, off, remark)) {
        break;
      }
      e.remark = std::move(remark);
    }
    out.push_back(std::move(e));
  }
  return out;
}

bool MessagingService::SyncFriends(ClientCore& core, std::vector<FriendEntry>& out, bool& changed) const {
  out.clear();
  changed = false;
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteUint32(core.friend_sync_version_, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kFriendSync, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "friend sync failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "friend sync response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "friend sync failed" : server_err;
    return false;
  }
  std::size_t off = 1;
  std::uint32_t version = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, version) ||
      off >= resp_payload.size()) {
    core.last_error_ = "friend sync response invalid";
    return false;
  }
  const bool changed_flag = (resp_payload[off++] != 0);
  if (changed_flag) {
    std::uint32_t count = 0;
    if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
      core.last_error_ = "friend sync response invalid";
      return false;
    }
    out.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
      FriendEntry e;
      if (!mi::server::proto::ReadString(resp_payload, off, e.username) ||
          !mi::server::proto::ReadString(resp_payload, off, e.remark)) {
        core.last_error_ = "friend sync response invalid";
        out.clear();
        return false;
      }
      out.push_back(std::move(e));
    }
  }
  if (off != resp_payload.size()) {
    core.last_error_ = "friend sync response invalid";
    return false;
  }
  core.friend_sync_version_ = version;
  changed = changed_flag;
  return true;
}

bool MessagingService::AddFriend(ClientCore& core, const std::string& friend_username,
                           const std::string& remark) const {
  if (!core.EnsureChannel()) {
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(friend_username, plain);
  mi::server::proto::WriteString(remark, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kFriendAdd, plain,
                        resp_payload)) {
    return false;
  }
  return !resp_payload.empty() && resp_payload[0] == 1;
}

bool MessagingService::SetFriendRemark(ClientCore& core, const std::string& friend_username,
                                 const std::string& remark) const {
  if (!core.EnsureChannel()) {
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(friend_username, plain);
  mi::server::proto::WriteString(remark, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kFriendRemarkSet, plain,
                        resp_payload)) {
    return false;
  }
  return !resp_payload.empty() && resp_payload[0] == 1;
}

bool MessagingService::SendFriendRequest(ClientCore& core, const std::string& target_username,
                                   const std::string& requester_remark) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(target_username, plain);
  mi::server::proto::WriteString(requester_remark, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kFriendRequestSend, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "friend request send failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "friend request response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ =
        server_err.empty() ? "friend request send failed" : server_err;
    return false;
  }
  return true;
}

std::vector<ClientCore::FriendRequestEntry> MessagingService::ListFriendRequests(ClientCore& core) const {
  core.last_error_.clear();
  std::vector<FriendRequestEntry> out;
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return out;
  }
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kFriendRequestList, {},
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "friend request list failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "friend request list response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ =
        server_err.empty() ? "friend request list failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    core.last_error_ = "friend request list decode failed";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    FriendRequestEntry e;
    if (!mi::server::proto::ReadString(resp_payload, off, e.requester_username) ||
        !mi::server::proto::ReadString(resp_payload, off, e.requester_remark)) {
      core.last_error_ = "friend request list decode failed";
      return {};
    }
    out.push_back(std::move(e));
  }
  return out;
}

bool MessagingService::RespondFriendRequest(ClientCore& core, const std::string& requester_username,
                                      bool accept) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(requester_username, plain);
  mi::server::proto::WriteUint32(accept ? 1u : 0u, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kFriendRequestRespond, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "friend request respond failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "friend request respond response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ =
        server_err.empty() ? "friend request respond failed" : server_err;
    return false;
  }
  return true;
}

bool MessagingService::DeleteFriend(ClientCore& core, const std::string& friend_username) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(friend_username, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kFriendDelete, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "friend delete failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "friend delete response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "friend delete failed" : server_err;
    return false;
  }
  return true;
}

bool MessagingService::SetUserBlocked(ClientCore& core, const std::string& blocked_username,
                                bool blocked) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(blocked_username, plain);
  mi::server::proto::WriteUint32(blocked ? 1u : 0u, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kUserBlockSet, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "block set failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "block set response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "block set failed" : server_err;
    return false;
  }
  return true;
}

bool MessagingService::MaybeRotateDeviceSyncKey(ClientCore& core) const {
  if (!core.device_sync_enabled_ || !core.device_sync_is_primary_) {
    return false;
  }

  const std::string saved_err = core.last_error_;
  if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
    core.last_error_ = saved_err;
    return false;
  }
  if (!core.device_sync_key_loaded_) {
    core.last_error_ = saved_err;
    return false;
  }
  if (core.device_sync_rotate_interval_sec_ == 0 &&
      core.device_sync_rotate_message_limit_ == 0) {
    core.last_error_ = saved_err;
    return false;
  }

  const std::uint64_t now_ms = mi::platform::NowSteadyMs();
  bool due = false;
  if (core.device_sync_rotate_interval_sec_ != 0 &&
      core.device_sync_last_rotate_ms_ != 0) {
    const std::uint64_t interval_ms =
        static_cast<std::uint64_t>(core.device_sync_rotate_interval_sec_) *
        1000ull;
    if (interval_ms != 0 &&
        now_ms - core.device_sync_last_rotate_ms_ >= interval_ms) {
      due = true;
    }
  }
  if (!due && core.device_sync_rotate_message_limit_ != 0 &&
      core.device_sync_send_count_ >= core.device_sync_rotate_message_limit_) {
    due = true;
  }
  if (!due) {
    core.last_error_ = saved_err;
    return false;
  }

  std::array<std::uint8_t, 32> next_key{};
  if (!RandomBytes(next_key.data(), next_key.size())) {
    core.last_error_ = saved_err;
    return false;
  }
  std::vector<std::uint8_t> event_plain;
  if (!EncodeDeviceSyncRotateKey(next_key, event_plain)) {
    core.last_error_ = saved_err;
    return false;
  }
  std::vector<std::uint8_t> event_cipher;
  if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
    core.last_error_ = saved_err;
    return false;
  }
  if (!core.PushDeviceSyncCiphertext(event_cipher)) {
    core.last_error_ = saved_err;
    return false;
  }
  if (!core.StoreDeviceSyncKey(next_key)) {
    core.last_error_ = saved_err;
    return false;
  }
  core.last_error_ = saved_err;
  return true;
}

void MessagingService::BestEffortBroadcastDeviceSyncMessage(ClientCore& core, 
    bool is_group, bool outgoing, const std::string& conv_id,
    const std::string& sender, const std::vector<std::uint8_t>& envelope) const {
  if (!core.device_sync_enabled_ || !core.device_sync_is_primary_) {
    return;
  }

  const std::string saved_err = core.last_error_;
  if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
    core.last_error_ = saved_err;
    return;
  }

  MaybeRotateDeviceSyncKey(core);
  std::vector<std::uint8_t> event_plain;
  if (!EncodeDeviceSyncMessage(is_group, outgoing, conv_id, sender, envelope,
                               event_plain)) {
    core.last_error_ = saved_err;
    return;
  }

  std::vector<std::uint8_t> event_cipher;
  if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
    core.last_error_ = saved_err;
    return;
  }
  if (core.PushDeviceSyncCiphertext(event_cipher)) {
    core.device_sync_send_count_++;
  }
  core.last_error_ = saved_err;
}

void MessagingService::BestEffortBroadcastDeviceSyncDelivery(ClientCore& core, 
    bool is_group, const std::string& conv_id,
    const std::array<std::uint8_t, 16>& msg_id, bool is_read) const {
  if (!core.device_sync_enabled_ || !core.device_sync_is_primary_) {
    return;
  }

  const std::string saved_err = core.last_error_;
  if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
    core.last_error_ = saved_err;
    return;
  }

  MaybeRotateDeviceSyncKey(core);
  std::vector<std::uint8_t> event_plain;
  if (!EncodeDeviceSyncDelivery(is_group, is_read, conv_id, msg_id, event_plain)) {
    core.last_error_ = saved_err;
    return;
  }

  std::vector<std::uint8_t> event_cipher;
  if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
    core.last_error_ = saved_err;
    return;
  }
  if (core.PushDeviceSyncCiphertext(event_cipher)) {
    core.device_sync_send_count_++;
  }
  core.last_error_ = saved_err;
}

void MessagingService::BestEffortBroadcastDeviceSyncHistorySnapshot(ClientCore& core, 
    const std::string& target_device_id) const {
  if (!core.device_sync_enabled_ || !core.device_sync_is_primary_) {
    return;
  }
  if (target_device_id.empty()) {
    return;
  }
  if (!core.history_store_) {
    return;
  }

  const std::string saved_err = core.last_error_;
  if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
    core.last_error_ = saved_err;
    return;
  }

  MaybeRotateDeviceSyncKey(core);
  std::vector<ChatHistoryMessage> msgs;
  std::string hist_err;
  if (!core.history_store_->ExportRecentSnapshot(20, 50, msgs, hist_err) ||
      msgs.empty()) {
    core.last_error_ = saved_err;
    return;
  }

  static constexpr std::size_t kMaxPlain = 200u * 1024u;
  std::size_t idx = 0;
  while (idx < msgs.size()) {
    MaybeRotateDeviceSyncKey(core);
    std::vector<std::uint8_t> event_plain;
    event_plain.push_back(kDeviceSyncEventHistorySnapshot);
    mi::server::proto::WriteString(target_device_id, event_plain);
    const std::size_t count_pos = event_plain.size();
    mi::server::proto::WriteUint32(0, event_plain);

    std::uint32_t count = 0;
    while (idx < msgs.size()) {
      std::vector<std::uint8_t> entry;
      if (!EncodeHistorySnapshotEntry(msgs[idx], entry)) {
        ++idx;
        continue;
      }
      if (event_plain.size() + entry.size() > kMaxPlain) {
        if (count == 0) {
          ++idx;
        }
        break;
      }
      event_plain.insert(event_plain.end(), entry.begin(), entry.end());
      ++count;
      ++idx;
    }

    if (count == 0) {
      continue;
    }
    event_plain[count_pos + 0] = static_cast<std::uint8_t>(count & 0xFF);
    event_plain[count_pos + 1] = static_cast<std::uint8_t>((count >> 8) & 0xFF);
    event_plain[count_pos + 2] =
        static_cast<std::uint8_t>((count >> 16) & 0xFF);
    event_plain[count_pos + 3] =
        static_cast<std::uint8_t>((count >> 24) & 0xFF);

    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      break;
    }
    if (!core.PushDeviceSyncCiphertext(event_cipher)) {
      break;
    }
    core.device_sync_send_count_++;
  }

  core.last_error_ = saved_err;
}

bool MessagingService::GetPeerIdentityCached(ClientCore& core, const std::string& peer_username,
                                       ClientCore::CachedPeerIdentity& out,
                                       bool require_trust) const {
  out = ClientCore::CachedPeerIdentity{};
  if (!core.EnsureE2ee()) {
    return false;
  }
  auto it = core.peer_id_cache_.find(peer_username);
  if (it != core.peer_id_cache_.end()) {
    out = it->second;
    if (!require_trust) {
      return true;
    }
    std::string trust_err;
    if (!core.e2ee_.EnsurePeerTrusted(peer_username, out.fingerprint_hex, trust_err)) {
      core.last_error_ = trust_err.empty() ? "peer not trusted" : trust_err;
      return false;
    }
    return true;
  }

  std::vector<std::uint8_t> bundle;
  if (!core.FetchPreKeyBundle(peer_username, bundle)) {
    return false;
  }

  std::vector<std::uint8_t> id_sig_pk;
  std::array<std::uint8_t, 32> id_dh_pk{};
  std::string fingerprint;
  std::string parse_err;
  if (!core.e2ee_.ExtractPeerIdentityFromBundle(bundle, id_sig_pk, id_dh_pk,
                                          fingerprint, parse_err)) {
    core.last_error_ = parse_err.empty() ? "bundle parse failed" : parse_err;
    return false;
  }

  if (require_trust) {
    std::string trust_err;
    if (!core.e2ee_.EnsurePeerTrusted(peer_username, fingerprint, trust_err)) {
      core.last_error_ = trust_err.empty() ? "peer not trusted" : trust_err;
      return false;
    }
  }

  ClientCore::CachedPeerIdentity entry;
  entry.id_sig_pk = std::move(id_sig_pk);
  entry.id_dh_pk = id_dh_pk;
  entry.fingerprint_hex = std::move(fingerprint);
  core.peer_id_cache_[peer_username] = entry;
  out = entry;
  return true;
}

bool MessagingService::EnsureGroupSenderKeyForSend(ClientCore& core, 
    const std::string& group_id, const std::vector<std::string>& members,
    ClientCore::GroupSenderKeyState*& out_sender_key, std::string& out_warn) const {
  out_sender_key = nullptr;
  out_warn.clear();
  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return false;
  }
  if (members.empty()) {
    core.last_error_ = "group member list empty";
    return false;
  }

  const std::string sender_key_map_key =
      MakeGroupSenderKeyMapKey(group_id, core.username_);
  auto& sender_key = core.group_sender_keys_[sender_key_map_key];
  if (sender_key.group_id.empty()) {
    sender_key.group_id = group_id;
    sender_key.sender_username = core.username_;
  }

  const std::string members_hash = HashGroupMembers(members);
  const bool have_key = (sender_key.version != 0 &&
                         !IsAllZero(sender_key.ck.data(), sender_key.ck.size()));
  const std::uint64_t now_sec = NowUnixSeconds();
  if (have_key && sender_key.rotated_at == 0) {
    sender_key.rotated_at = now_sec;
  }
  const bool membership_changed =
      (!sender_key.members_hash.empty() && sender_key.members_hash != members_hash);
  const bool threshold_reached =
      (sender_key.sent_count >= kGroupSenderKeyRotationThreshold);
  const bool time_window_reached =
      (have_key && sender_key.rotated_at != 0 &&
       now_sec > sender_key.rotated_at &&
       (now_sec - sender_key.rotated_at) >= kGroupSenderKeyRotationIntervalSec);

  if (!have_key || membership_changed || threshold_reached || time_window_reached) {
    const std::uint32_t next_version =
        have_key ? (sender_key.version + 1) : 1;
    if (!RandomBytes(sender_key.ck.data(), sender_key.ck.size())) {
      core.last_error_ = "rng failed";
      return false;
    }
    sender_key.version = next_version;
    sender_key.next_iteration = 0;
    sender_key.members_hash = members_hash;
    sender_key.rotated_at = now_sec;
    sender_key.sent_count = 0;
    sender_key.skipped_mks.clear();
    sender_key.skipped_order.clear();

    for (auto it = core.pending_sender_key_dists_.begin();
         it != core.pending_sender_key_dists_.end();) {
      if (it->second.group_id == group_id) {
        it = core.pending_sender_key_dists_.erase(it);
      } else {
        ++it;
      }
    }

    std::array<std::uint8_t, 16> dist_id{};
    if (!RandomBytes(dist_id.data(), dist_id.size())) {
      core.last_error_ = "rng failed";
      return false;
    }
    const std::string dist_id_hex = BytesToHexLower(dist_id.data(), dist_id.size());

    const auto sig_msg = BuildGroupSenderKeyDistSigMessage(
        group_id, sender_key.version, sender_key.next_iteration, sender_key.ck);
    std::vector<std::uint8_t> sig;
    std::string sig_err;
    if (!core.e2ee_.SignDetached(sig_msg, sig, sig_err)) {
      core.last_error_ = sig_err.empty() ? "sign sender key failed" : sig_err;
      return false;
    }

    std::vector<std::uint8_t> dist_envelope;
    if (!EncodeChatGroupSenderKeyDist(dist_id, group_id, sender_key.version,
                                      sender_key.next_iteration, sender_key.ck,
                                      sig, dist_envelope)) {
      core.last_error_ = "encode sender key failed";
      return false;
    }

    ClientCore::PendingSenderKeyDistribution pending;
    pending.group_id = group_id;
    pending.version = sender_key.version;
    pending.envelope = dist_envelope;
    pending.last_sent_ms = mi::platform::NowSteadyMs();
    for (const auto& m : members) {
      if (!core.username_.empty() && m == core.username_) {
        continue;
      }
      pending.pending_members.insert(m);
    }
    core.pending_sender_key_dists_[dist_id_hex] = std::move(pending);

    std::string first_error;
    for (const auto& m : members) {
      if (!core.username_.empty() && m == core.username_) {
        continue;
      }
      const std::string saved_err = core.last_error_;
      if (!core.SendGroupSenderKeyEnvelope(group_id, m, dist_envelope) &&
          first_error.empty()) {
        first_error = core.last_error_;
      }
      core.last_error_ = saved_err;
    }
    out_warn = first_error;
  }

  const std::uint64_t now_ms = mi::platform::NowSteadyMs();
  for (auto& kv : core.pending_sender_key_dists_) {
    auto& pending = kv.second;
    if (pending.group_id != group_id || pending.pending_members.empty()) {
      continue;
    }
    if (pending.last_sent_ms != 0 &&
        now_ms - pending.last_sent_ms < kSenderKeyDistResendIntervalMs) {
      continue;
    }
    pending.last_sent_ms = now_ms;
    for (const auto& m : pending.pending_members) {
      const std::string saved_err = core.last_error_;
      core.SendGroupSenderKeyEnvelope(pending.group_id, m, pending.envelope);
      core.last_error_ = saved_err;
    }
  }

  out_sender_key = &sender_key;
  return true;
}

bool MessagingService::StoreGroupCallKey(ClientCore& core, 
    const std::string& group_id, const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id, const std::array<std::uint8_t, 32>& call_key) const {
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return false;
  }
  if (key_id == 0) {
    core.last_error_ = "key id invalid";
    return false;
  }
  if (IsAllZero(call_key.data(), call_key.size())) {
    core.last_error_ = "call key empty";
    return false;
  }
  const std::string map_key = MakeGroupCallKeyMapKey(group_id, call_id);
  auto& state = core.group_call_keys_[map_key];
  if (state.key_id != 0 && key_id < state.key_id) {
    return false;
  }
  state.group_id = group_id;
  state.call_id = call_id;
  state.key_id = key_id;
  state.call_key = call_key;
  state.updated_at = NowUnixSeconds();
  return true;
}

bool MessagingService::LookupGroupCallKey(const ClientCore& core, 
    const std::string& group_id, const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id, std::array<std::uint8_t, 32>& out_key) const {
  out_key.fill(0);
  if (group_id.empty() || key_id == 0) {
    return false;
  }
  const std::string map_key = MakeGroupCallKeyMapKey(group_id, call_id);
  const auto it = core.group_call_keys_.find(map_key);
  if (it == core.group_call_keys_.end()) {
    return false;
  }
  if (it->second.key_id != key_id ||
      IsAllZero(it->second.call_key.data(), it->second.call_key.size())) {
    return false;
  }
  out_key = it->second.call_key;
  return true;
}

bool MessagingService::SendGroupCallKeyEnvelope(ClientCore& core, 
    const std::string& group_id, const std::string& peer_username,
    const std::array<std::uint8_t, 16>& call_id, std::uint32_t key_id,
    const std::array<std::uint8_t, 32>& call_key) const {
  if (group_id.empty() || peer_username.empty()) {
    core.last_error_ = "invalid params";
    return false;
  }
  std::array<std::uint8_t, 16> dist_id{};
  if (!RandomBytes(dist_id.data(), dist_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  const auto sig_msg =
      core.BuildGroupCallKeyDistSigMessage(group_id, call_id, key_id, call_key);
  std::vector<std::uint8_t> sig;
  std::string sig_err;
  if (!core.e2ee_.SignDetached(sig_msg, sig, sig_err)) {
    core.last_error_ = sig_err.empty() ? "sign call key failed" : sig_err;
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatGroupCallKeyDist(dist_id, group_id, call_id, key_id, call_key,
                                  sig, envelope)) {
    core.last_error_ = "encode call key failed";
    return false;
  }
  return core.SendGroupSenderKeyEnvelope(group_id, peer_username, envelope);
}

bool MessagingService::SendGroupCallKeyRequest(ClientCore& core, 
    const std::string& group_id, const std::string& peer_username,
    const std::array<std::uint8_t, 16>& call_id, std::uint32_t key_id) const {
  if (group_id.empty() || peer_username.empty()) {
    core.last_error_ = "invalid params";
    return false;
  }
  std::array<std::uint8_t, 16> req_id{};
  if (!RandomBytes(req_id.data(), req_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> req;
  if (!EncodeChatGroupCallKeyReq(req_id, group_id, call_id, key_id, req)) {
    core.last_error_ = "encode call key req failed";
    return false;
  }
  return core.SendGroupSenderKeyEnvelope(group_id, peer_username, req);
}

void MessagingService::ResendPendingSenderKeyDistributions(ClientCore& core) const {
  if (core.pending_sender_key_dists_.empty()) {
    return;
  }
  const std::uint64_t now_ms = mi::platform::NowSteadyMs();
  for (auto it = core.pending_sender_key_dists_.begin();
       it != core.pending_sender_key_dists_.end();) {
    auto& pending = it->second;
    if (pending.pending_members.empty()) {
      it = core.pending_sender_key_dists_.erase(it);
      continue;
    }
    if (pending.last_sent_ms != 0 &&
        now_ms - pending.last_sent_ms < kSenderKeyDistResendIntervalMs) {
      ++it;
      continue;
    }
    pending.last_sent_ms = now_ms;
    for (const auto& member : pending.pending_members) {
      const std::string saved_err = core.last_error_;
      core.SendGroupSenderKeyEnvelope(pending.group_id, member, pending.envelope);
      core.last_error_ = saved_err;
    }
    ++it;
  }
}

bool MessagingService::SendGroupChatText(ClientCore& core, const std::string& group_id,
                                   const std::string& text_utf8,
                                   std::string& out_message_id_hex) const {
  out_message_id_hex.clear();
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return false;
  }
  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }

    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      core.last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

    std::vector<std::uint8_t> plain_envelope;
    if (!EncodeChatGroupText(msg_id, group_id, text_utf8, plain_envelope)) {
      core.last_error_ = "encode group text failed";
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendGroup(group_id, plain_envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = core.PushDeviceSyncCiphertext(event_cipher);
    core.BestEffortPersistHistoryEnvelope(true, true, group_id, core.username_, plain_envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }

  const auto members = core.ListGroupMembers(group_id);
  if (members.empty()) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group member list empty";
    }
    return false;
  }

  ClientCore::GroupSenderKeyState* sender_key = nullptr;
  std::string warn;
  if (!core.EnsureGroupSenderKeyForSend(group_id, members, sender_key, warn)) {
    return false;
  }
  if (!sender_key) {
    core.last_error_ = "sender key unavailable";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

  std::vector<std::uint8_t> plain_envelope;
  if (!EncodeChatGroupText(msg_id, group_id, text_utf8, plain_envelope)) {
    core.last_error_ = "encode group text failed";
    return false;
  }
  std::vector<std::uint8_t> padded_envelope;
  std::string pad_err;
  if (!PadPayload(plain_envelope, padded_envelope, pad_err)) {
    core.last_error_ = pad_err.empty() ? "pad group message failed" : pad_err;
    return false;
  }

  std::array<std::uint8_t, 32> next_ck{};
  std::array<std::uint8_t, 32> mk{};
  if (!KdfGroupCk(sender_key->ck, next_ck, mk)) {
    core.last_error_ = "kdf failed";
    return false;
  }
  const std::uint32_t iter = sender_key->next_iteration;

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> ad;
  BuildGroupCipherAd(group_id, core.username_, sender_key->version, iter, ad);

  std::vector<std::uint8_t> cipher;
  cipher.resize(padded_envelope.size());
  std::array<std::uint8_t, 16> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), mk.data(), nonce.data(), ad.data(),
                   ad.size(), padded_envelope.data(), padded_envelope.size());

  std::vector<std::uint8_t> wire_no_sig;
  if (!EncodeGroupCipherNoSig(group_id, core.username_, sender_key->version, iter,
                              nonce, mac, cipher, wire_no_sig)) {
    core.last_error_ = "encode group cipher failed";
    return false;
  }

  std::vector<std::uint8_t> msg_sig;
  std::string msg_sig_err;
  if (!core.e2ee_.SignDetached(wire_no_sig, msg_sig, msg_sig_err)) {
    core.last_error_ = msg_sig_err.empty() ? "sign group message failed" : msg_sig_err;
    return false;
  }

  std::vector<std::uint8_t> wire = std::move(wire_no_sig);
  mi::server::proto::WriteBytes(msg_sig, wire);

  const bool ok = core.SendGroupCipherMessage(group_id, wire);
  core.BestEffortPersistHistoryEnvelope(true, true, group_id, core.username_, plain_envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    out_message_id_hex.clear();
    return false;
  }

  sender_key->ck = next_ck;
  sender_key->next_iteration++;
  sender_key->sent_count++;
  core.last_error_ = warn;
  if (!out_message_id_hex.empty()) {
    const auto map_it = core.group_delivery_map_.find(out_message_id_hex);
    if (map_it == core.group_delivery_map_.end()) {
      core.group_delivery_map_[out_message_id_hex] = group_id;
      core.group_delivery_order_.push_back(out_message_id_hex);
      while (core.group_delivery_order_.size() > kChatSeenLimit) {
        core.group_delivery_map_.erase(core.group_delivery_order_.front());
        core.group_delivery_order_.pop_front();
      }
    } else {
      map_it->second = group_id;
    }
  }
  core.BestEffortBroadcastDeviceSyncMessage(true, true, group_id, core.username_,
                                      plain_envelope);
  return true;
}

bool MessagingService::ResendGroupChatText(ClientCore& core, const std::string& group_id,
                                     const std::string& message_id_hex,
                                     const std::string& text_utf8) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return false;
  }
  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    std::array<std::uint8_t, 16> msg_id{};
    if (!HexToFixedBytes16(message_id_hex, msg_id)) {
      core.last_error_ = "invalid message id";
      return false;
    }

    std::vector<std::uint8_t> plain_envelope;
    if (!EncodeChatGroupText(msg_id, group_id, text_utf8, plain_envelope)) {
      core.last_error_ = "encode group text failed";
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendGroup(group_id, plain_envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = core.PushDeviceSyncCiphertext(event_cipher);
    core.BestEffortPersistHistoryEnvelope(true, true, group_id, core.username_, plain_envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }

  const auto members = core.ListGroupMembers(group_id);
  if (members.empty()) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group member list empty";
    }
    return false;
  }

  ClientCore::GroupSenderKeyState* sender_key = nullptr;
  std::string warn;
  if (!core.EnsureGroupSenderKeyForSend(group_id, members, sender_key, warn)) {
    return false;
  }
  if (!sender_key) {
    core.last_error_ = "sender key unavailable";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    core.last_error_ = "invalid message id";
    return false;
  }

  std::vector<std::uint8_t> plain_envelope;
  if (!EncodeChatGroupText(msg_id, group_id, text_utf8, plain_envelope)) {
    core.last_error_ = "encode group text failed";
    return false;
  }
  std::vector<std::uint8_t> padded_envelope;
  std::string pad_err;
  if (!PadPayload(plain_envelope, padded_envelope, pad_err)) {
    core.last_error_ = pad_err.empty() ? "pad group message failed" : pad_err;
    return false;
  }

  std::array<std::uint8_t, 32> next_ck{};
  std::array<std::uint8_t, 32> mk{};
  if (!KdfGroupCk(sender_key->ck, next_ck, mk)) {
    core.last_error_ = "kdf failed";
    return false;
  }
  const std::uint32_t iter = sender_key->next_iteration;

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> ad;
  BuildGroupCipherAd(group_id, core.username_, sender_key->version, iter, ad);

  std::vector<std::uint8_t> cipher;
  cipher.resize(padded_envelope.size());
  std::array<std::uint8_t, 16> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), mk.data(), nonce.data(), ad.data(),
                   ad.size(), padded_envelope.data(), padded_envelope.size());

  std::vector<std::uint8_t> wire_no_sig;
  if (!EncodeGroupCipherNoSig(group_id, core.username_, sender_key->version, iter,
                              nonce, mac, cipher, wire_no_sig)) {
    core.last_error_ = "encode group cipher failed";
    return false;
  }

  std::vector<std::uint8_t> msg_sig;
  std::string msg_sig_err;
  if (!core.e2ee_.SignDetached(wire_no_sig, msg_sig, msg_sig_err)) {
    core.last_error_ = msg_sig_err.empty() ? "sign group message failed" : msg_sig_err;
    return false;
  }

  std::vector<std::uint8_t> wire = std::move(wire_no_sig);
  mi::server::proto::WriteBytes(msg_sig, wire);

  const bool ok = core.SendGroupCipherMessage(group_id, wire);
  core.BestEffortPersistHistoryEnvelope(true, true, group_id, core.username_, plain_envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }

  sender_key->ck = next_ck;
  sender_key->next_iteration++;
  sender_key->sent_count++;
  core.last_error_ = warn;
  if (!message_id_hex.empty()) {
    const auto map_it = core.group_delivery_map_.find(message_id_hex);
    if (map_it == core.group_delivery_map_.end()) {
      core.group_delivery_map_[message_id_hex] = group_id;
      core.group_delivery_order_.push_back(message_id_hex);
      while (core.group_delivery_order_.size() > kChatSeenLimit) {
        core.group_delivery_map_.erase(core.group_delivery_order_.front());
        core.group_delivery_order_.pop_front();
      }
    } else {
      map_it->second = group_id;
    }
  }
  core.BestEffortBroadcastDeviceSyncMessage(true, true, group_id, core.username_,
                                      plain_envelope);
  return true;
}

bool MessagingService::SendGroupChatFile(ClientCore& core, const std::string& group_id,
                                   const std::filesystem::path& file_path,
                                   std::string& out_message_id_hex) const {
  out_message_id_hex.clear();
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return false;
  }
  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }

    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      core.last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

    std::error_code ec;
    if (file_path.empty() || !pfs::Exists(file_path, ec) || ec) {
      core.last_error_ = "file not found";
      return false;
    }
    if (pfs::IsDirectory(file_path, ec) || ec) {
      core.last_error_ = "path is directory";
      return false;
    }
    const std::uint64_t size64 = pfs::FileSize(file_path, ec);
    if (ec || size64 == 0) {
      core.last_error_ = "file empty";
      return false;
    }
    if (size64 > kMaxChatFileBytes) {
      core.last_error_ = "file too large";
      return false;
    }

    const std::string file_name =
        file_path.has_filename() ? file_path.filename().u8string() : "file";

    std::array<std::uint8_t, 32> file_key{};
    std::string file_id;
    if (!core.UploadChatFileFromPath(file_path, size64, file_name, file_key,
                                file_id)) {
      return false;
    }

    std::vector<std::uint8_t> envelope;
    if (!EncodeChatGroupFile(msg_id, group_id, size64, file_name, file_id,
                             file_key, envelope)) {
      core.last_error_ = "encode group file failed";
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendGroup(group_id, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = core.PushDeviceSyncCiphertext(event_cipher);
    core.BestEffortPersistHistoryEnvelope(true, true, group_id, core.username_, envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }
  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }

  const auto members = core.ListGroupMembers(group_id);
  if (members.empty()) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group member list empty";
    }
    return false;
  }

  ClientCore::GroupSenderKeyState* sender_key = nullptr;
  std::string warn;
  if (!core.EnsureGroupSenderKeyForSend(group_id, members, sender_key, warn)) {
    return false;
  }
  if (!sender_key) {
    core.last_error_ = "sender key unavailable";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

  std::error_code ec;
  if (file_path.empty() || !pfs::Exists(file_path, ec) || ec) {
    core.last_error_ = "file not found";
    out_message_id_hex.clear();
    return false;
  }
  if (pfs::IsDirectory(file_path, ec) || ec) {
    core.last_error_ = "path is directory";
    out_message_id_hex.clear();
    return false;
  }
  const std::uint64_t size64 = pfs::FileSize(file_path, ec);
  if (ec || size64 == 0) {
    core.last_error_ = "file empty";
    out_message_id_hex.clear();
    return false;
  }
  if (size64 > kMaxChatFileBytes) {
    core.last_error_ = "file too large";
    out_message_id_hex.clear();
    return false;
  }

  const std::string file_name =
      file_path.has_filename() ? file_path.filename().u8string() : "file";

  std::array<std::uint8_t, 32> file_key{};
  std::string file_id;
  if (!core.UploadChatFileFromPath(file_path, size64, file_name, file_key, file_id)) {
    out_message_id_hex.clear();
    return false;
  }

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatGroupFile(msg_id, group_id, size64, file_name, file_id,
                           file_key, envelope)) {
    core.last_error_ = "encode group file failed";
    out_message_id_hex.clear();
    return false;
  }
  std::vector<std::uint8_t> padded_envelope;
  std::string pad_err;
  if (!PadPayload(envelope, padded_envelope, pad_err)) {
    core.last_error_ = pad_err.empty() ? "pad group message failed" : pad_err;
    out_message_id_hex.clear();
    return false;
  }

  std::array<std::uint8_t, 32> next_ck{};
  std::array<std::uint8_t, 32> mk{};
  if (!KdfGroupCk(sender_key->ck, next_ck, mk)) {
    core.last_error_ = "kdf failed";
    out_message_id_hex.clear();
    return false;
  }
  const std::uint32_t iter = sender_key->next_iteration;

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    core.last_error_ = "rng failed";
    out_message_id_hex.clear();
    return false;
  }
  std::vector<std::uint8_t> ad;
  BuildGroupCipherAd(group_id, core.username_, sender_key->version, iter, ad);

  std::vector<std::uint8_t> cipher;
  cipher.resize(padded_envelope.size());
  std::array<std::uint8_t, 16> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), mk.data(), nonce.data(), ad.data(),
                   ad.size(), padded_envelope.data(), padded_envelope.size());

  std::vector<std::uint8_t> wire_no_sig;
  if (!EncodeGroupCipherNoSig(group_id, core.username_, sender_key->version, iter,
                              nonce, mac, cipher, wire_no_sig)) {
    core.last_error_ = "encode group cipher failed";
    out_message_id_hex.clear();
    return false;
  }

  std::vector<std::uint8_t> msg_sig;
  std::string msg_sig_err;
  if (!core.e2ee_.SignDetached(wire_no_sig, msg_sig, msg_sig_err)) {
    core.last_error_ = msg_sig_err.empty() ? "sign group message failed" : msg_sig_err;
    out_message_id_hex.clear();
    return false;
  }

  std::vector<std::uint8_t> wire = std::move(wire_no_sig);
  mi::server::proto::WriteBytes(msg_sig, wire);

  const bool ok = core.SendGroupCipherMessage(group_id, wire);
  core.BestEffortPersistHistoryEnvelope(true, true, group_id, core.username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    out_message_id_hex.clear();
    return false;
  }

  sender_key->ck = next_ck;
  sender_key->next_iteration++;
  sender_key->sent_count++;
  core.last_error_ = warn;
  if (!out_message_id_hex.empty()) {
    const auto map_it = core.group_delivery_map_.find(out_message_id_hex);
    if (map_it == core.group_delivery_map_.end()) {
      core.group_delivery_map_[out_message_id_hex] = group_id;
      core.group_delivery_order_.push_back(out_message_id_hex);
      while (core.group_delivery_order_.size() > kChatSeenLimit) {
        core.group_delivery_map_.erase(core.group_delivery_order_.front());
        core.group_delivery_order_.pop_front();
      }
    } else {
      map_it->second = group_id;
    }
  }
  core.BestEffortBroadcastDeviceSyncMessage(true, true, group_id, core.username_, envelope);
  return true;
}

bool MessagingService::ResendGroupChatFile(ClientCore& core, const std::string& group_id,
                                     const std::string& message_id_hex,
                                     const std::filesystem::path& file_path) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return false;
  }
  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }

    std::array<std::uint8_t, 16> msg_id{};
    if (!HexToFixedBytes16(message_id_hex, msg_id)) {
      core.last_error_ = "invalid message id";
      return false;
    }

    std::error_code ec;
    if (file_path.empty() || !pfs::Exists(file_path, ec) || ec) {
      core.last_error_ = "file not found";
      return false;
    }
    if (pfs::IsDirectory(file_path, ec) || ec) {
      core.last_error_ = "path is directory";
      return false;
    }
    const std::uint64_t size64 = pfs::FileSize(file_path, ec);
    if (ec || size64 == 0) {
      core.last_error_ = "file empty";
      return false;
    }
    if (size64 > kMaxChatFileBytes) {
      core.last_error_ = "file too large";
      return false;
    }

    const std::string file_name =
        file_path.has_filename() ? file_path.filename().u8string() : "file";

    std::array<std::uint8_t, 32> file_key{};
    std::string file_id;
    if (!core.UploadChatFileFromPath(file_path, size64, file_name, file_key,
                                file_id)) {
      return false;
    }

    std::vector<std::uint8_t> envelope;
    if (!EncodeChatGroupFile(msg_id, group_id, size64, file_name, file_id,
                             file_key, envelope)) {
      core.last_error_ = "encode group file failed";
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendGroup(group_id, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = core.PushDeviceSyncCiphertext(event_cipher);
    core.BestEffortPersistHistoryEnvelope(true, true, group_id, core.username_, envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }
  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }

  const auto members = core.ListGroupMembers(group_id);
  if (members.empty()) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group member list empty";
    }
    return false;
  }

  ClientCore::GroupSenderKeyState* sender_key = nullptr;
  std::string warn;
  if (!core.EnsureGroupSenderKeyForSend(group_id, members, sender_key, warn)) {
    return false;
  }
  if (!sender_key) {
    core.last_error_ = "sender key unavailable";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    core.last_error_ = "invalid message id";
    return false;
  }

  std::error_code ec;
  if (file_path.empty() || !pfs::Exists(file_path, ec) || ec) {
    core.last_error_ = "file not found";
    return false;
  }
  if (pfs::IsDirectory(file_path, ec) || ec) {
    core.last_error_ = "path is directory";
    return false;
  }
  const std::uint64_t size64 = pfs::FileSize(file_path, ec);
  if (ec || size64 == 0) {
    core.last_error_ = "file empty";
    return false;
  }
  if (size64 > kMaxChatFileBytes) {
    core.last_error_ = "file too large";
    return false;
  }

  const std::string file_name =
      file_path.has_filename() ? file_path.filename().u8string() : "file";

  std::array<std::uint8_t, 32> file_key{};
  std::string file_id;
  if (!core.UploadChatFileFromPath(file_path, size64, file_name, file_key, file_id)) {
    return false;
  }

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatGroupFile(msg_id, group_id, size64, file_name, file_id,
                           file_key, envelope)) {
    core.last_error_ = "encode group file failed";
    return false;
  }
  std::vector<std::uint8_t> padded_envelope;
  std::string pad_err;
  if (!PadPayload(envelope, padded_envelope, pad_err)) {
    core.last_error_ = pad_err.empty() ? "pad group message failed" : pad_err;
    return false;
  }

  std::array<std::uint8_t, 32> next_ck{};
  std::array<std::uint8_t, 32> mk{};
  if (!KdfGroupCk(sender_key->ck, next_ck, mk)) {
    core.last_error_ = "kdf failed";
    return false;
  }
  const std::uint32_t iter = sender_key->next_iteration;

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> ad;
  BuildGroupCipherAd(group_id, core.username_, sender_key->version, iter, ad);

  std::vector<std::uint8_t> cipher;
  cipher.resize(padded_envelope.size());
  std::array<std::uint8_t, 16> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), mk.data(), nonce.data(), ad.data(),
                   ad.size(), padded_envelope.data(), padded_envelope.size());

  std::vector<std::uint8_t> wire_no_sig;
  if (!EncodeGroupCipherNoSig(group_id, core.username_, sender_key->version, iter,
                              nonce, mac, cipher, wire_no_sig)) {
    core.last_error_ = "encode group cipher failed";
    return false;
  }

  std::vector<std::uint8_t> msg_sig;
  std::string msg_sig_err;
  if (!core.e2ee_.SignDetached(wire_no_sig, msg_sig, msg_sig_err)) {
    core.last_error_ = msg_sig_err.empty() ? "sign group message failed" : msg_sig_err;
    return false;
  }

  std::vector<std::uint8_t> wire = std::move(wire_no_sig);
  mi::server::proto::WriteBytes(msg_sig, wire);

  const bool ok = core.SendGroupCipherMessage(group_id, wire);
  core.BestEffortPersistHistoryEnvelope(true, true, group_id, core.username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }

  sender_key->ck = next_ck;
  sender_key->next_iteration++;
  sender_key->sent_count++;
  core.last_error_ = warn;
  if (!message_id_hex.empty()) {
    const auto map_it = core.group_delivery_map_.find(message_id_hex);
    if (map_it == core.group_delivery_map_.end()) {
      core.group_delivery_map_[message_id_hex] = group_id;
      core.group_delivery_order_.push_back(message_id_hex);
      while (core.group_delivery_order_.size() > kChatSeenLimit) {
        core.group_delivery_map_.erase(core.group_delivery_order_.front());
        core.group_delivery_order_.pop_front();
      }
    } else {
      map_it->second = group_id;
    }
  }
  core.BestEffortBroadcastDeviceSyncMessage(true, true, group_id, core.username_, envelope);
  return true;
}

bool MessagingService::SendPrivateE2ee(ClientCore& core, const std::string& peer_username,
                                 const std::vector<std::uint8_t>& plaintext) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }

  const std::vector<std::uint8_t> app_plain =
      WrapWithGossip(plaintext, core.kt_tree_size_, core.kt_root_);

  std::vector<std::uint8_t> payload;
  std::string enc_err;
  if (!core.e2ee_.EncryptToPeer(peer_username, {}, app_plain, payload, enc_err)) {
    if (enc_err == "peer bundle missing") {
      std::vector<std::uint8_t> peer_bundle;
      if (!core.FetchPreKeyBundle(peer_username, peer_bundle)) {
        return false;
      }
      if (!core.e2ee_.EncryptToPeer(peer_username, peer_bundle, app_plain, payload,
                               enc_err)) {
        core.last_error_ = enc_err.empty() ? "encrypt failed" : enc_err;
        return false;
      }
    } else {
      core.last_error_ = enc_err.empty() ? "encrypt failed" : enc_err;
      return false;
    }
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(peer_username, plain);
  mi::server::proto::WriteBytes(payload, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kPrivateSend, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "private send failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "private send response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "private send failed" : server_err;
    return false;
  }
  return true;
}

std::vector<mi::client::e2ee::PrivateMessage> MessagingService::PullPrivateE2ee(ClientCore& core) const {
  std::vector<mi::client::e2ee::PrivateMessage> out;
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return out;
  }
  if (!core.EnsureE2ee()) {
    return out;
  }
  if (!core.EnsurePreKeyPublished()) {
    return out;
  }

  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kPrivatePull, {}, resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "private pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "private pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "private pull failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    core.last_error_ = "private pull response invalid";
    return out;
  }

  for (std::uint32_t i = 0; i < count; ++i) {
    std::string sender;
    std::vector<std::uint8_t> payload;
    if (!mi::server::proto::ReadString(resp_payload, off, sender) ||
        !mi::server::proto::ReadBytes(resp_payload, off, payload)) {
      core.last_error_ = "private pull response invalid";
      break;
    }

    mi::client::e2ee::PrivateMessage msg;
    std::string dec_err;
    if (core.e2ee_.DecryptFromPayload(sender, payload, msg, dec_err)) {
      std::uint64_t peer_tree_size = 0;
      std::array<std::uint8_t, 32> peer_root{};
      std::vector<std::uint8_t> inner_plain;
      if (UnwrapGossip(msg.plaintext, peer_tree_size, peer_root, inner_plain)) {
        msg.plaintext = std::move(inner_plain);
        if (peer_tree_size > 0 && core.kt_tree_size_ > 0) {
          if (peer_tree_size == core.kt_tree_size_ && peer_root != core.kt_root_) {
            core.last_error_ = "kt gossip mismatch";
          } else if (peer_tree_size > core.kt_tree_size_) {
            std::vector<std::array<std::uint8_t, 32>> proof;
            if (core.FetchKtConsistency(core.kt_tree_size_, peer_tree_size, proof) &&
                VerifyConsistencyProof(
                    static_cast<std::size_t>(core.kt_tree_size_),
                    static_cast<std::size_t>(peer_tree_size), core.kt_root_,
                    peer_root, proof)) {
              core.kt_tree_size_ = peer_tree_size;
              core.kt_root_ = peer_root;
              core.SaveKtState();
            } else if (core.last_error_.empty()) {
              core.last_error_ = "kt gossip verify failed";
            }
          }
        }
      }
      out.push_back(std::move(msg));
    } else if (core.last_error_.empty() && !dec_err.empty()) {
      core.last_error_ = dec_err;
    }
  }
  return out;
}

bool MessagingService::PushMedia(ClientCore& core, const std::string& recipient,
                           const std::array<std::uint8_t, 16>& call_id,
                           const std::vector<std::uint8_t>& packet) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (recipient.empty()) {
    core.last_error_ = "recipient empty";
    return false;
  }
  if (packet.empty()) {
    core.last_error_ = "packet empty";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(recipient, plain);
  WriteFixed16(call_id, plain);
  mi::server::proto::WriteBytes(packet, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kMediaPush, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "media push failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "media push response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "media push failed" : server_err;
    return false;
  }
  return true;
}

std::vector<ClientCore::MediaRelayPacket> MessagingService::PullMedia(ClientCore& core, 
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t max_packets,
    std::uint32_t wait_ms) const {
  std::vector<MediaRelayPacket> out;
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return out;
  }
  const auto& media_cfg = core.media_config();
  if (max_packets == 0) {
    max_packets = media_cfg.pull_max_packets;
  }
  if (max_packets == 0) {
    max_packets = 1;
  } else if (max_packets > 256) {
    max_packets = 256;
  }
  if (wait_ms > 1000) {
    wait_ms = 1000;
  }
  std::vector<std::uint8_t> plain;
  WriteFixed16(call_id, plain);
  mi::server::proto::WriteUint32(max_packets, plain);
  mi::server::proto::WriteUint32(wait_ms, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kMediaPull, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "media pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "media pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "media pull failed" : server_err;
    return out;
  }
  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    core.last_error_ = "media pull response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    MediaRelayPacket packet;
    if (!mi::server::proto::ReadString(resp_payload, off, packet.sender) ||
        !mi::server::proto::ReadBytes(resp_payload, off, packet.payload)) {
      core.last_error_ = "media pull response invalid";
      break;
    }
    out.push_back(std::move(packet));
  }
  return out;
}

ClientCore::GroupCallSignalResult MessagingService::SendGroupCallSignal(ClientCore& core, 
    std::uint8_t op, const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id, bool video,
    std::uint32_t key_id, std::uint32_t seq, std::uint64_t ts_ms,
    const std::vector<std::uint8_t>& ext) const {
  GroupCallSignalResult resp;
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    resp.error = core.last_error_;
    return resp;
  }
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    resp.error = core.last_error_;
    return resp;
  }

  std::vector<std::uint8_t> plain;
  plain.reserve(64 + group_id.size() + ext.size());
  plain.push_back(op);
  mi::server::proto::WriteString(group_id, plain);
  WriteFixed16(call_id, plain);
  const std::uint8_t media_flags = video ? static_cast<std::uint8_t>(0x01 | 0x02)
                                         : static_cast<std::uint8_t>(0x01);
  plain.push_back(media_flags);
  mi::server::proto::WriteUint32(key_id, plain);
  mi::server::proto::WriteUint32(seq, plain);
  if (ts_ms == 0) {
    ts_ms = NowUnixSeconds() * 1000ULL;
  }
  mi::server::proto::WriteUint64(ts_ms, plain);
  mi::server::proto::WriteBytes(ext, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kGroupCallSignal, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group call signal failed";
    }
    resp.error = core.last_error_;
    return resp;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "group call response empty";
    resp.error = core.last_error_;
    return resp;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "group call failed" : server_err;
    resp.error = core.last_error_;
    return resp;
  }

  std::size_t off = 1;
  if (!ReadFixed16(resp_payload, off, resp.call_id) ||
      !mi::server::proto::ReadUint32(resp_payload, off, resp.key_id)) {
    core.last_error_ = "group call response invalid";
    resp.error = core.last_error_;
    return resp;
  }
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    core.last_error_ = "group call response invalid";
    resp.error = core.last_error_;
    return resp;
  }
  resp.members.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::string member;
    if (!mi::server::proto::ReadString(resp_payload, off, member)) {
      core.last_error_ = "group call response invalid";
      resp.error = core.last_error_;
      return resp;
    }
    resp.members.push_back(std::move(member));
  }
  if (off != resp_payload.size()) {
    core.last_error_ = "group call response invalid";
    resp.error = core.last_error_;
    return resp;
  }
  resp.success = true;
  return resp;
}

bool MessagingService::StartGroupCall(ClientCore& core, const std::string& group_id,
                                bool video,
                                std::array<std::uint8_t, 16>& out_call_id,
                                std::uint32_t& out_key_id) const {
  out_call_id.fill(0);
  out_key_id = 0;
  core.last_error_.clear();
  std::array<std::uint8_t, 16> empty{};
  const auto resp =
      core.SendGroupCallSignal(kGroupCallOpCreate, group_id, empty, video);
  if (!resp.success) {
    return false;
  }
  out_call_id = resp.call_id;
  out_key_id = resp.key_id;

  std::array<std::uint8_t, 32> call_key{};
  if (!RandomBytes(call_key.data(), call_key.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  if (!core.StoreGroupCallKey(group_id, resp.call_id, resp.key_id, call_key)) {
    return false;
  }

  const auto members = core.ListGroupMembers(group_id);
  if (members.empty()) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group member list empty";
    }
    return false;
  }

  std::string first_error;
  for (const auto& member : members) {
    if (!core.username_.empty() && member == core.username_) {
      continue;
    }
    const std::string saved_err = core.last_error_;
    if (!core.SendGroupCallKeyEnvelope(group_id, member, resp.call_id, resp.key_id,
                                  call_key) &&
        first_error.empty()) {
      first_error = core.last_error_;
    }
    core.last_error_ = saved_err;
  }
  if (!first_error.empty()) {
    core.last_error_ = first_error;
  }
  return true;
}

bool MessagingService::JoinGroupCall(ClientCore& core, const std::string& group_id,
                               const std::array<std::uint8_t, 16>& call_id,
                               bool video) const {
  std::uint32_t key_id = 0;
  return core.JoinGroupCall(group_id, call_id, video, key_id);
}

bool MessagingService::JoinGroupCall(ClientCore& core, const std::string& group_id,
                               const std::array<std::uint8_t, 16>& call_id,
                               bool video,
                               std::uint32_t& out_key_id) const {
  out_key_id = 0;
  core.last_error_.clear();
  const auto resp =
      core.SendGroupCallSignal(kGroupCallOpJoin, group_id, call_id, video);
  if (!resp.success) {
    return false;
  }
  out_key_id = resp.key_id;
  std::array<std::uint8_t, 32> call_key{};
  if (!core.LookupGroupCallKey(group_id, call_id, resp.key_id, call_key)) {
    bool requested = false;
    for (const auto& member : resp.members) {
      if (!core.username_.empty() && member == core.username_) {
        continue;
      }
      const std::string saved_err = core.last_error_;
      core.SendGroupCallKeyRequest(group_id, member, call_id, resp.key_id);
      core.last_error_ = saved_err;
      requested = true;
      break;
    }
    if (!requested) {
      const std::string saved_err = core.last_error_;
      const auto members = core.ListGroupMembers(group_id);
      core.last_error_ = saved_err;
      for (const auto& member : members) {
        if (!core.username_.empty() && member == core.username_) {
          continue;
        }
        const std::string saved_err2 = core.last_error_;
        core.SendGroupCallKeyRequest(group_id, member, call_id, resp.key_id);
        core.last_error_ = saved_err2;
        break;
      }
    }
  }
  return true;
}

bool MessagingService::LeaveGroupCall(ClientCore& core, const std::string& group_id,
                                const std::array<std::uint8_t, 16>& call_id) const {
  core.last_error_.clear();
  const auto resp =
      core.SendGroupCallSignal(kGroupCallOpLeave, group_id, call_id, false);
  if (!resp.success) {
    return false;
  }
  const std::string map_key = MakeGroupCallKeyMapKey(group_id, call_id);
  core.group_call_keys_.erase(map_key);
  return true;
}

bool MessagingService::RotateGroupCallKey(ClientCore& core, 
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id,
    const std::vector<std::string>& members) const {
  core.last_error_.clear();
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return false;
  }
  if (members.empty()) {
    core.last_error_ = "group members empty";
    return false;
  }
  if (key_id == 0) {
    core.last_error_ = "key id invalid";
    return false;
  }
  std::array<std::uint8_t, 32> call_key{};
  if (!RandomBytes(call_key.data(), call_key.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  if (!core.StoreGroupCallKey(group_id, call_id, key_id, call_key)) {
    return false;
  }
  std::string first_error;
  for (const auto& member : members) {
    if (!core.username_.empty() && member == core.username_) {
      continue;
    }
    const std::string saved_err = core.last_error_;
    if (!core.SendGroupCallKeyEnvelope(group_id, member, call_id, key_id, call_key) &&
        first_error.empty()) {
      first_error = core.last_error_;
    }
    core.last_error_ = saved_err;
  }
  if (!first_error.empty()) {
    core.last_error_ = first_error;
    return false;
  }
  return true;
}

bool MessagingService::RequestGroupCallKey(ClientCore& core, 
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id,
    const std::vector<std::string>& members) const {
  core.last_error_.clear();
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return false;
  }
  if (members.empty()) {
    core.last_error_ = "group members empty";
    return false;
  }
  if (key_id == 0) {
    core.last_error_ = "key id invalid";
    return false;
  }
  bool requested = false;
  for (const auto& member : members) {
    if (!core.username_.empty() && member == core.username_) {
      continue;
    }
    const std::string saved_err = core.last_error_;
    core.SendGroupCallKeyRequest(group_id, member, call_id, key_id);
    core.last_error_ = saved_err;
    requested = true;
  }
  if (!requested) {
    core.last_error_ = "no member to request";
    return false;
  }
  return true;
}

bool MessagingService::GetGroupCallKey(const ClientCore& core, const std::string& group_id,
                                 const std::array<std::uint8_t, 16>& call_id,
                                 std::uint32_t key_id,
                                 std::array<std::uint8_t, 32>& out_key) const {
  return core.LookupGroupCallKey(group_id, call_id, key_id, out_key);
}

std::vector<ClientCore::GroupCallEvent> MessagingService::PullGroupCallEvents(ClientCore& core, 
    std::uint32_t max_events, std::uint32_t wait_ms) const {
  std::vector<GroupCallEvent> out;
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return out;
  }
  if (max_events == 0) {
    max_events = 1;
  } else if (max_events > 256) {
    max_events = 256;
  }
  if (wait_ms > 1000) {
    wait_ms = 1000;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteUint32(max_events, plain);
  mi::server::proto::WriteUint32(wait_ms, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kGroupCallSignalPull, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group call pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "group call pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "group call pull failed" : server_err;
    return out;
  }
  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    core.last_error_ = "group call pull response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    if (off >= resp_payload.size()) {
      core.last_error_ = "group call pull response invalid";
      break;
    }
    GroupCallEvent ev;
    ev.op = resp_payload[off++];
    if (!mi::server::proto::ReadString(resp_payload, off, ev.group_id) ||
        !ReadFixed16(resp_payload, off, ev.call_id) ||
        !mi::server::proto::ReadUint32(resp_payload, off, ev.key_id) ||
        !mi::server::proto::ReadString(resp_payload, off, ev.sender)) {
      core.last_error_ = "group call pull response invalid";
      break;
    }
    if (off >= resp_payload.size()) {
      core.last_error_ = "group call pull response invalid";
      break;
    }
    ev.media_flags = resp_payload[off++];
    if (!mi::server::proto::ReadUint64(resp_payload, off, ev.ts_ms)) {
      core.last_error_ = "group call pull response invalid";
      break;
    }
    out.push_back(std::move(ev));
  }
  return out;
}

bool MessagingService::PushGroupMedia(ClientCore& core, const std::string& group_id,
                                const std::array<std::uint8_t, 16>& call_id,
                                const std::vector<std::uint8_t>& packet) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return false;
  }
  if (packet.empty()) {
    core.last_error_ = "packet empty";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  WriteFixed16(call_id, plain);
  mi::server::proto::WriteBytes(packet, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kGroupMediaPush, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group media push failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "group media push response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "group media push failed" : server_err;
    return false;
  }
  return true;
}

std::vector<ClientCore::MediaRelayPacket> MessagingService::PullGroupMedia(ClientCore& core, 
    const std::array<std::uint8_t, 16>& call_id, std::uint32_t max_packets,
    std::uint32_t wait_ms) const {
  std::vector<MediaRelayPacket> out;
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return out;
  }
  const auto& media_cfg = core.media_config();
  if (max_packets == 0) {
    max_packets = media_cfg.group_pull_max_packets;
  }
  if (max_packets == 0) {
    max_packets = 1;
  } else if (max_packets > 256) {
    max_packets = 256;
  }
  if (wait_ms > 1000) {
    wait_ms = 1000;
  }
  std::vector<std::uint8_t> plain;
  WriteFixed16(call_id, plain);
  mi::server::proto::WriteUint32(max_packets, plain);
  mi::server::proto::WriteUint32(wait_ms, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kGroupMediaPull, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group media pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "group media pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "group media pull failed" : server_err;
    return out;
  }
  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    core.last_error_ = "group media pull response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    MediaRelayPacket packet;
    if (!mi::server::proto::ReadString(resp_payload, off, packet.sender) ||
        !mi::server::proto::ReadBytes(resp_payload, off, packet.payload)) {
      core.last_error_ = "group media pull response invalid";
      break;
    }
    out.push_back(std::move(packet));
  }
  return out;
}

std::vector<mi::client::e2ee::PrivateMessage> MessagingService::DrainReadyPrivateE2ee(ClientCore& core) const {
  std::vector<mi::client::e2ee::PrivateMessage> out;
  core.last_error_.clear();
  if (!core.EnsureE2ee()) {
    return out;
  }
  out = core.e2ee_.DrainReadyMessages();
  for (auto& msg : out) {
    std::uint64_t peer_tree_size = 0;
    std::array<std::uint8_t, 32> peer_root{};
    std::vector<std::uint8_t> inner_plain;
    if (UnwrapGossip(msg.plaintext, peer_tree_size, peer_root, inner_plain)) {
      msg.plaintext = std::move(inner_plain);
      if (peer_tree_size > 0 && core.kt_tree_size_ > 0) {
        if (peer_tree_size == core.kt_tree_size_ && peer_root != core.kt_root_) {
          core.last_error_ = "kt gossip mismatch";
        } else if (peer_tree_size > core.kt_tree_size_) {
          std::vector<std::array<std::uint8_t, 32>> proof;
          if (core.FetchKtConsistency(core.kt_tree_size_, peer_tree_size, proof) &&
              VerifyConsistencyProof(static_cast<std::size_t>(core.kt_tree_size_),
                                    static_cast<std::size_t>(peer_tree_size),
                                    core.kt_root_, peer_root, proof)) {
            core.kt_tree_size_ = peer_tree_size;
            core.kt_root_ = peer_root;
            core.SaveKtState();
          } else if (core.last_error_.empty()) {
            core.last_error_ = "kt gossip verify failed";
          }
        }
      }
    }
  }
  return out;
}

bool MessagingService::SendGroupCipherMessage(ClientCore& core, const std::string& group_id,
                                        const std::vector<std::uint8_t>& payload) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    core.last_error_ = "group id empty";
    return false;
  }
  if (payload.empty()) {
    core.last_error_ = "payload empty";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  mi::server::proto::WriteBytes(payload, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kGroupCipherSend, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group send failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "group send response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "group send failed" : server_err;
    return false;
  }
  return true;
}

bool MessagingService::SendGroupSenderKeyEnvelope(ClientCore& core, 
    const std::string& group_id, const std::string& peer_username,
    const std::vector<std::uint8_t>& plaintext) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }
  if (group_id.empty() || peer_username.empty()) {
    core.last_error_ = "invalid params";
    return false;
  }

  const std::vector<std::uint8_t> app_plain =
      WrapWithGossip(plaintext, core.kt_tree_size_, core.kt_root_);

  std::vector<std::uint8_t> payload;
  std::string enc_err;
  if (!core.e2ee_.EncryptToPeer(peer_username, {}, app_plain, payload, enc_err)) {
    if (enc_err == "peer bundle missing") {
      std::vector<std::uint8_t> peer_bundle;
      if (!core.FetchPreKeyBundle(peer_username, peer_bundle)) {
        return false;
      }
      if (!core.e2ee_.EncryptToPeer(peer_username, peer_bundle, app_plain, payload,
                               enc_err)) {
        core.last_error_ = enc_err.empty() ? "encrypt failed" : enc_err;
        return false;
      }
    } else {
      core.last_error_ = enc_err.empty() ? "encrypt failed" : enc_err;
      return false;
    }
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  mi::server::proto::WriteString(peer_username, plain);
  mi::server::proto::WriteBytes(payload, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kGroupSenderKeySend, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group sender key send failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "group sender key response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ =
        server_err.empty() ? "group sender key send failed" : server_err;
    return false;
  }
  return true;
}

std::vector<ClientCore::PendingGroupCipher> MessagingService::PullGroupCipherMessages(ClientCore& core) const {
  std::vector<ClientCore::PendingGroupCipher> out;
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return out;
  }

  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kGroupCipherPull, {}, resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "group pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "group pull failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    core.last_error_ = "group pull response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    ClientCore::PendingGroupCipher m;
    if (!mi::server::proto::ReadString(resp_payload, off, m.group_id) ||
        !mi::server::proto::ReadString(resp_payload, off, m.sender_username) ||
        !mi::server::proto::ReadBytes(resp_payload, off, m.payload)) {
      out.clear();
      core.last_error_ = "group pull response invalid";
      return out;
    }
    out.push_back(std::move(m));
  }
  if (off != resp_payload.size()) {
    out.clear();
    core.last_error_ = "group pull response invalid";
    return out;
  }
  return out;
}

std::vector<ClientCore::PendingGroupNotice> MessagingService::PullGroupNoticeMessages(ClientCore& core) const {
  std::vector<ClientCore::PendingGroupNotice> out;
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return out;
  }

  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kGroupNoticePull, {}, resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "group notice pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "group notice pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ =
        server_err.empty() ? "group notice pull failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    core.last_error_ = "group notice pull response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    ClientCore::PendingGroupNotice m;
    if (!mi::server::proto::ReadString(resp_payload, off, m.group_id) ||
        !mi::server::proto::ReadString(resp_payload, off, m.sender_username) ||
        !mi::server::proto::ReadBytes(resp_payload, off, m.payload)) {
      out.clear();
      core.last_error_ = "group notice pull response invalid";
      return out;
    }
    out.push_back(std::move(m));
  }
  if (off != resp_payload.size()) {
    out.clear();
    core.last_error_ = "group notice pull response invalid";
    return out;
  }
  return out;
}

bool MessagingService::SendChatText(ClientCore& core, const std::string& peer_username,
                              const std::string& text_utf8,
                              std::string& out_message_id_hex) const {
  out_message_id_hex.clear();
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    if (peer_username.empty()) {
      core.last_error_ = "peer empty";
      return false;
    }
    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      core.last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

    std::vector<std::uint8_t> envelope;
    if (!EncodeChatText(msg_id, text_utf8, envelope)) {
      core.last_error_ = "encode chat text failed";
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = core.PushDeviceSyncCiphertext(event_cipher);
    core.BestEffortPersistHistoryEnvelope(false, true, peer_username, core.username_, envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }
  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatText(msg_id, text_utf8, envelope)) {
    core.last_error_ = "encode chat text failed";
    return false;
  }
  const bool ok = core.SendPrivateE2ee(peer_username, envelope);
  core.BestEffortPersistHistoryEnvelope(false, true, peer_username, core.username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }
  core.BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, core.username_,
                                      envelope);
  return true;
}

bool MessagingService::ResendChatText(ClientCore& core, const std::string& peer_username,
                                const std::string& message_id_hex,
                                const std::string& text_utf8) const {
  core.last_error_.clear();
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }
  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    std::array<std::uint8_t, 16> msg_id{};
    if (!HexToFixedBytes16(message_id_hex, msg_id)) {
      core.last_error_ = "invalid message id";
      return false;
    }
    std::vector<std::uint8_t> envelope;
    if (!EncodeChatText(msg_id, text_utf8, envelope)) {
      core.last_error_ = "encode chat text failed";
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = core.PushDeviceSyncCiphertext(event_cipher);
    core.BestEffortPersistHistoryEnvelope(false, true, peer_username, core.username_, envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    core.last_error_ = "invalid message id";
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatText(msg_id, text_utf8, envelope)) {
    core.last_error_ = "encode chat text failed";
    return false;
  }
  const bool ok = core.SendPrivateE2ee(peer_username, envelope);
  core.BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                ok ? HistoryStatus::kSent
                                   : HistoryStatus::kFailed,
                                NowUnixSeconds());
  if (!ok) {
    return false;
  }
  core.BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, core.username_,
                                      envelope);
  return true;
}

bool MessagingService::SendChatTextWithReply(ClientCore& core, const std::string& peer_username,
                                      const std::string& text_utf8,
                                      const std::string& reply_to_message_id_hex,
                                      const std::string& reply_preview_utf8,
                                      std::string& out_message_id_hex) const {
  out_message_id_hex.clear();
  core.last_error_.clear();
  if (reply_to_message_id_hex.empty()) {
    return core.SendChatText(peer_username, text_utf8, out_message_id_hex);
  }
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }
  std::array<std::uint8_t, 16> reply_to{};
  if (!HexToFixedBytes16(reply_to_message_id_hex, reply_to)) {
    core.last_error_ = "invalid reply message id";
    return false;
  }
  std::string preview = reply_preview_utf8;
  if (preview.size() > 512) {
    preview.resize(512);
  }

  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      core.last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
    std::vector<std::uint8_t> envelope;
    if (!EncodeChatRichText(msg_id, text_utf8, true, reply_to, preview, envelope)) {
      core.last_error_ = "encode chat rich failed";
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = core.PushDeviceSyncCiphertext(event_cipher);
    core.BestEffortPersistHistoryEnvelope(false, true, peer_username, core.username_, envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }

  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatRichText(msg_id, text_utf8, true, reply_to, preview, envelope)) {
    core.last_error_ = "encode chat rich failed";
    return false;
  }
  const bool ok = core.SendPrivateE2ee(peer_username, envelope);
  core.BestEffortPersistHistoryEnvelope(false, true, peer_username, core.username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }
  core.BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, core.username_,
                                      envelope);
  return true;
}

bool MessagingService::ResendChatTextWithReply(ClientCore& core, const std::string& peer_username,
                                        const std::string& message_id_hex,
                                        const std::string& text_utf8,
                                        const std::string& reply_to_message_id_hex,
                                        const std::string& reply_preview_utf8) const {
  core.last_error_.clear();
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }
  if (reply_to_message_id_hex.empty()) {
    return core.ResendChatText(peer_username, message_id_hex, text_utf8);
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    core.last_error_ = "invalid message id";
    return false;
  }
  std::array<std::uint8_t, 16> reply_to{};
  if (!HexToFixedBytes16(reply_to_message_id_hex, reply_to)) {
    core.last_error_ = "invalid reply message id";
    return false;
  }
  std::string preview = reply_preview_utf8;
  if (preview.size() > 512) {
    preview.resize(512);
  }

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatRichText(msg_id, text_utf8, true, reply_to, preview, envelope)) {
    core.last_error_ = "encode chat rich failed";
    return false;
  }

  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = core.PushDeviceSyncCiphertext(event_cipher);
    core.BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
    return ok;
  }

  const bool ok = core.SendPrivateE2ee(peer_username, envelope);
  core.BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                ok ? HistoryStatus::kSent
                                   : HistoryStatus::kFailed,
                                NowUnixSeconds());
  if (!ok) {
    return false;
  }
  core.BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, core.username_,
                                      envelope);
  return true;
}

bool MessagingService::SendChatLocation(ClientCore& core, const std::string& peer_username,
                                  std::int32_t lat_e7, std::int32_t lon_e7,
                                  const std::string& label_utf8,
                                  std::string& out_message_id_hex) const {
  out_message_id_hex.clear();
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }
  if (lat_e7 < -900000000 || lat_e7 > 900000000) {
    core.last_error_ = "latitude out of range";
    return false;
  }
  if (lon_e7 < -1800000000 || lon_e7 > 1800000000) {
    core.last_error_ = "longitude out of range";
    return false;
  }

  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      core.last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
    std::vector<std::uint8_t> envelope;
    if (!EncodeChatRichLocation(msg_id, lat_e7, lon_e7, label_utf8, envelope)) {
      core.last_error_ = "encode chat rich failed";
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    return core.PushDeviceSyncCiphertext(event_cipher);
  }

  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatRichLocation(msg_id, lat_e7, lon_e7, label_utf8, envelope)) {
    core.last_error_ = "encode chat rich failed";
    return false;
  }
  const bool ok = core.SendPrivateE2ee(peer_username, envelope);
  core.BestEffortPersistHistoryEnvelope(false, true, peer_username, core.username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }
  core.BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, core.username_,
                                      envelope);
  return true;
}

bool MessagingService::ResendChatLocation(ClientCore& core, const std::string& peer_username,
                                    const std::string& message_id_hex,
                                    std::int32_t lat_e7, std::int32_t lon_e7,
                                    const std::string& label_utf8) const {
  core.last_error_.clear();
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }
  if (lat_e7 < -900000000 || lat_e7 > 900000000) {
    core.last_error_ = "latitude out of range";
    return false;
  }
  if (lon_e7 < -1800000000 || lon_e7 > 1800000000) {
    core.last_error_ = "longitude out of range";
    return false;
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    core.last_error_ = "invalid message id";
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatRichLocation(msg_id, lat_e7, lon_e7, label_utf8, envelope)) {
    core.last_error_ = "encode chat rich failed";
    return false;
  }

  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = core.PushDeviceSyncCiphertext(event_cipher);
    core.BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
    return ok;
  }

  const bool ok = core.SendPrivateE2ee(peer_username, envelope);
  core.BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                ok ? HistoryStatus::kSent
                                   : HistoryStatus::kFailed,
                                NowUnixSeconds());
  if (!ok) {
    return false;
  }
  core.BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, core.username_,
                                      envelope);
  return true;
}

bool MessagingService::SendChatContactCard(ClientCore& core, const std::string& peer_username,
                                     const std::string& card_username,
                                     const std::string& card_display,
                                     std::string& out_message_id_hex) const {
  out_message_id_hex.clear();
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }
  if (card_username.empty()) {
    core.last_error_ = "card username empty";
    return false;
  }

  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      core.last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
    std::vector<std::uint8_t> envelope;
    if (!EncodeChatRichContactCard(msg_id, card_username, card_display, envelope)) {
      core.last_error_ = "encode chat rich failed";
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    return core.PushDeviceSyncCiphertext(event_cipher);
  }

  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatRichContactCard(msg_id, card_username, card_display, envelope)) {
    core.last_error_ = "encode chat rich failed";
    return false;
  }
  const bool ok = core.SendPrivateE2ee(peer_username, envelope);
  core.BestEffortPersistHistoryEnvelope(false, true, peer_username, core.username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }
  core.BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, core.username_,
                                      envelope);
  return true;
}

bool MessagingService::ResendChatContactCard(ClientCore& core, const std::string& peer_username,
                                       const std::string& message_id_hex,
                                       const std::string& card_username,
                                       const std::string& card_display) const {
  core.last_error_.clear();
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }
  if (card_username.empty()) {
    core.last_error_ = "card username empty";
    return false;
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    core.last_error_ = "invalid message id";
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatRichContactCard(msg_id, card_username, card_display, envelope)) {
    core.last_error_ = "encode chat rich failed";
    return false;
  }

  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = core.PushDeviceSyncCiphertext(event_cipher);
    core.BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
    return ok;
  }

  const bool ok = core.SendPrivateE2ee(peer_username, envelope);
  core.BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                ok ? HistoryStatus::kSent
                                   : HistoryStatus::kFailed,
                                NowUnixSeconds());
  if (!ok) {
    return false;
  }
  core.BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, core.username_,
                                      envelope);
  return true;
}

bool MessagingService::SendChatSticker(ClientCore& core, const std::string& peer_username,
                                 const std::string& sticker_id,
                                 std::string& out_message_id_hex) const {
  out_message_id_hex.clear();
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }
  if (sticker_id.empty()) {
    core.last_error_ = "sticker id empty";
    return false;
  }
  if (sticker_id.size() > 128) {
    core.last_error_ = "sticker id too long";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatSticker(msg_id, sticker_id, envelope)) {
    core.last_error_ = "encode chat sticker failed";
    return false;
  }

  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    return core.PushDeviceSyncCiphertext(event_cipher);
  }

  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }
  const bool ok = core.SendPrivateE2ee(peer_username, envelope);
  core.BestEffortPersistHistoryEnvelope(false, true, peer_username, core.username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }
  core.BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, core.username_,
                                      envelope);
  return true;
}

bool MessagingService::ResendChatSticker(ClientCore& core, const std::string& peer_username,
                                   const std::string& message_id_hex,
                                   const std::string& sticker_id) const {
  core.last_error_.clear();
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }
  if (sticker_id.empty()) {
    core.last_error_ = "sticker id empty";
    return false;
  }
  if (sticker_id.size() > 128) {
    core.last_error_ = "sticker id too long";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    core.last_error_ = "invalid message id";
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatSticker(msg_id, sticker_id, envelope)) {
    core.last_error_ = "encode chat sticker failed";
    return false;
  }

  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = core.PushDeviceSyncCiphertext(event_cipher);
    core.BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
    return ok;
  }

  const bool ok = core.SendPrivateE2ee(peer_username, envelope);
  core.BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                ok ? HistoryStatus::kSent
                                   : HistoryStatus::kFailed,
                                NowUnixSeconds());
  if (!ok) {
    return false;
  }
  core.BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, core.username_,
                                      envelope);
  return true;
}

bool MessagingService::SendChatReadReceipt(ClientCore& core, const std::string& peer_username,
                                     const std::string& message_id_hex) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    core.last_error_ = "invalid message id";
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatReadReceipt(msg_id, envelope)) {
    core.last_error_ = "encode read receipt failed";
    return false;
  }

  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    return core.PushDeviceSyncCiphertext(event_cipher);
  }

  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }
  return core.SendPrivateE2ee(peer_username, envelope);
}

bool MessagingService::SendChatTyping(ClientCore& core, const std::string& peer_username, bool typing) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatTyping(msg_id, typing, envelope)) {
    core.last_error_ = "encode typing failed";
    return false;
  }

  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    return core.PushDeviceSyncCiphertext(event_cipher);
  }

  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }
  return core.SendPrivateE2ee(peer_username, envelope);
}

bool MessagingService::SendChatPresence(ClientCore& core, const std::string& peer_username, bool online) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatPresence(msg_id, online, envelope)) {
    core.last_error_ = "encode presence failed";
    return false;
  }

  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    return core.PushDeviceSyncCiphertext(event_cipher);
  }

  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }
  return core.SendPrivateE2ee(peer_username, envelope);
}

bool MessagingService::SendChatFile(ClientCore& core, const std::string& peer_username,
                              const std::filesystem::path& file_path,
                              std::string& out_message_id_hex) const {
  out_message_id_hex.clear();
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    if (peer_username.empty()) {
      core.last_error_ = "peer empty";
      return false;
    }

    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      core.last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

    std::error_code ec;
    if (file_path.empty() || !pfs::Exists(file_path, ec) || ec) {
      core.last_error_ = "file not found";
      return false;
    }
    if (pfs::IsDirectory(file_path, ec) || ec) {
      core.last_error_ = "path is directory";
      return false;
    }
    const std::uint64_t size64 = pfs::FileSize(file_path, ec);
    if (ec || size64 == 0) {
      core.last_error_ = "file empty";
      return false;
    }
    if (size64 > kMaxChatFileBytes) {
      core.last_error_ = "file too large";
      return false;
    }

    const std::string file_name =
        file_path.has_filename() ? file_path.filename().u8string() : "file";

    std::array<std::uint8_t, 32> file_key{};
    std::string file_id;
    if (!core.UploadChatFileFromPath(file_path, size64, file_name, file_key,
                                file_id)) {
      return false;
    }

    std::vector<std::uint8_t> envelope;
    if (!EncodeChatFile(msg_id, size64, file_name, file_id, file_key, envelope)) {
      core.last_error_ = "encode chat file failed";
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    return core.PushDeviceSyncCiphertext(event_cipher);
  }
  if (!core.EnsureE2ee()) {
    return false;
  }
  if (!core.EnsurePreKeyPublished()) {
    return false;
  }
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

  std::error_code ec;
  if (file_path.empty() || !pfs::Exists(file_path, ec) || ec) {
    core.last_error_ = "file not found";
    return false;
  }
  if (pfs::IsDirectory(file_path, ec) || ec) {
    core.last_error_ = "path is directory";
    return false;
  }
  const std::uint64_t size64 = pfs::FileSize(file_path, ec);
  if (ec || size64 == 0) {
    core.last_error_ = "file empty";
    return false;
  }
  if (size64 > kMaxChatFileBytes) {
    core.last_error_ = "file too large";
    return false;
  }

  const std::string file_name =
      file_path.has_filename() ? file_path.filename().u8string() : "file";

  std::array<std::uint8_t, 32> file_key{};
  std::string file_id;
  if (!core.UploadChatFileFromPath(file_path, size64, file_name, file_key, file_id)) {
    return false;
  }

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatFile(msg_id, size64, file_name, file_id, file_key, envelope)) {
    core.last_error_ = "encode chat file failed";
    return false;
  }
  const bool ok = core.SendPrivateE2ee(peer_username, envelope);
  core.BestEffortPersistHistoryEnvelope(false, true, peer_username, core.username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }
  core.BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, core.username_,
                                      envelope);
  return true;
}

bool MessagingService::ResendChatFile(ClientCore& core, const std::string& peer_username,
                                const std::string& message_id_hex,
                                const std::filesystem::path& file_path) const {
  core.last_error_.clear();
  if (peer_username.empty()) {
    core.last_error_ = "peer empty";
    return false;
  }
  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return false;
    }
    std::array<std::uint8_t, 16> msg_id{};
    if (!HexToFixedBytes16(message_id_hex, msg_id)) {
      core.last_error_ = "invalid message id";
      return false;
    }

    std::error_code ec;
    if (file_path.empty() || !pfs::Exists(file_path, ec) || ec) {
      core.last_error_ = "file not found";
      return false;
    }
    const std::uint64_t size64 = pfs::FileSize(file_path, ec);
    if (ec || size64 == 0) {
      core.last_error_ = "file empty";
      return false;
    }
    if (size64 > kMaxChatFileBytes) {
      core.last_error_ = "file too large";
      return false;
    }

    const std::string file_name =
        file_path.has_filename() ? file_path.filename().u8string() : "file";

    std::array<std::uint8_t, 32> file_key{};
    std::string file_id;
    if (!core.UploadChatFileFromPath(file_path, size64, file_name, file_key,
                                file_id)) {
      return false;
    }

    std::vector<std::uint8_t> envelope;
    if (!EncodeChatFile(msg_id, size64, file_name, file_id, file_key, envelope)) {
      core.last_error_ = "encode chat file failed";
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      core.last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = core.PushDeviceSyncCiphertext(event_cipher);
    core.BestEffortPersistHistoryEnvelope(false, true, peer_username, core.username_, envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    core.last_error_ = "invalid message id";
    return false;
  }

  std::error_code ec;
  if (file_path.empty() || !pfs::Exists(file_path, ec) || ec) {
    core.last_error_ = "file not found";
    return false;
  }
  const std::uint64_t size64 = pfs::FileSize(file_path, ec);
  if (ec || size64 == 0) {
    core.last_error_ = "file empty";
    return false;
  }
  if (size64 > kMaxChatFileBytes) {
    core.last_error_ = "file too large";
    return false;
  }

  const std::string file_name =
      file_path.has_filename() ? file_path.filename().u8string() : "file";

  std::array<std::uint8_t, 32> file_key{};
  std::string file_id;
  if (!core.UploadChatFileFromPath(file_path, size64, file_name, file_key, file_id)) {
    return false;
  }

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatFile(msg_id, size64, file_name, file_id, file_key, envelope)) {
    core.last_error_ = "encode chat file failed";
    return false;
  }
  const bool ok = core.SendPrivateE2ee(peer_username, envelope);
  core.BestEffortPersistHistoryEnvelope(false, true, peer_username, core.username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }
  core.BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, core.username_,
                                      envelope);
  return true;
}

ClientCore::ChatPollResult MessagingService::PollChat(ClientCore& core) const {
  ChatPollResult result;
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return result;
  }
  {
    const std::string saved_err = core.last_error_;
    (void)core.MaybeSendCoverTraffic();
    core.last_error_ = saved_err;
  }
  {
    const std::string saved_err = core.last_error_;
    core.ResendPendingSenderKeyDistributions();
    core.last_error_ = saved_err;
  }
  {
    const std::string saved_err = core.last_error_;
    MaybeRotateDeviceSyncKey(core);
    core.last_error_ = saved_err;
  }

  if (core.device_sync_enabled_ && !core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      return result;
    }

    std::string sync_err;
    const auto pulled = core.PullDeviceSyncCiphertexts();
    if (!core.last_error_.empty()) {
      sync_err = core.last_error_;
    }
    core.last_error_.clear();

    for (const auto& cipher : pulled) {
      std::vector<std::uint8_t> plain;
      if (!core.DecryptDeviceSync(cipher, plain)) {
        if (sync_err.empty() && !core.last_error_.empty()) {
          sync_err = core.last_error_;
        }
        core.last_error_.clear();
        continue;
      }

      DeviceSyncEvent ev;
      if (!DecodeDeviceSyncEvent(plain, ev)) {
        continue;
      }

      if (ev.type == kDeviceSyncEventRotateKey) {
        if (!core.StoreDeviceSyncKey(ev.new_key)) {
          if (sync_err.empty() && !core.last_error_.empty()) {
            sync_err = core.last_error_;
          }
          core.last_error_.clear();
        }
        continue;
      }

      if (ev.type == kDeviceSyncEventHistorySnapshot) {
        if (ev.target_device_id.empty() || ev.target_device_id != core.device_id_) {
          continue;
        }
        const std::string saved_err = core.last_error_;
        if (core.history_store_) {
          for (const auto& m : ev.history) {
            std::string hist_err;
            if (m.is_system) {
              (void)core.history_store_->AppendSystem(m.is_group, m.conv_id,
                                                 m.system_text_utf8,
                                                 m.timestamp_sec, hist_err);
            } else {
              (void)core.history_store_->AppendEnvelope(
                  m.is_group, m.outgoing, m.conv_id, m.sender, m.envelope,
                  m.status, m.timestamp_sec, hist_err);
            }
          }
        }
        core.last_error_ = saved_err;
        continue;
      }

      if (ev.type == kDeviceSyncEventMessage) {
        std::uint8_t type = 0;
        std::array<std::uint8_t, 16> msg_id{};
        std::size_t off = 0;
        if (!DecodeChatHeader(ev.envelope, type, msg_id, off)) {
          continue;
        }
        const std::string id_hex =
            BytesToHexLower(msg_id.data(), msg_id.size());

        if (type == kChatTypeTyping) {
          if (off >= ev.envelope.size()) {
            continue;
          }
          const std::uint8_t state = ev.envelope[off++];
          if (off != ev.envelope.size()) {
            continue;
          }
          ChatTypingEvent te;
          te.from_username = ev.sender;
          te.typing = state != 0;
          result.typing_events.push_back(std::move(te));
          continue;
        }

        if (type == kChatTypePresence) {
          if (off >= ev.envelope.size()) {
            continue;
          }
          const std::uint8_t state = ev.envelope[off++];
          if (off != ev.envelope.size()) {
            continue;
          }
          ChatPresenceEvent pe;
          pe.from_username = ev.sender;
          pe.online = state != 0;
          result.presence_events.push_back(std::move(pe));
          continue;
        }

        if (type == kChatTypeRich) {
          RichDecoded rich;
          if (!DecodeChatRich(ev.envelope, off, rich) || off != ev.envelope.size()) {
            continue;
          }
          std::string text = FormatRichAsText(rich);
          if (ev.outgoing) {
            OutgoingChatTextMessage t;
            t.peer_username = ev.conv_id;
            t.message_id_hex = id_hex;
            t.text_utf8 = std::move(text);
            result.outgoing_texts.push_back(std::move(t));
          } else {
            ChatTextMessage t;
            t.from_username = ev.sender;
            t.message_id_hex = id_hex;
            t.text_utf8 = std::move(text);
            result.texts.push_back(std::move(t));
          }
          core.BestEffortPersistHistoryEnvelope(ev.is_group, ev.outgoing, ev.conv_id,
                                          ev.sender, ev.envelope,
                                          HistoryStatus::kSent, NowUnixSeconds());
          continue;
        }

        if (type == kChatTypeText) {
          std::string text;
          if (!mi::server::proto::ReadString(ev.envelope, off, text) ||
              off != ev.envelope.size()) {
            continue;
          }
          if (ev.outgoing) {
            OutgoingChatTextMessage t;
            t.peer_username = ev.conv_id;
            t.message_id_hex = id_hex;
            t.text_utf8 = std::move(text);
            result.outgoing_texts.push_back(std::move(t));
          } else {
            ChatTextMessage t;
            t.from_username = ev.sender;
            t.message_id_hex = id_hex;
            t.text_utf8 = std::move(text);
            result.texts.push_back(std::move(t));
          }
          core.BestEffortPersistHistoryEnvelope(ev.is_group, ev.outgoing, ev.conv_id,
                                          ev.sender, ev.envelope,
                                          HistoryStatus::kSent, NowUnixSeconds());
          continue;
        }

        if (type == kChatTypeFile) {
          std::uint64_t file_size = 0;
          std::string file_name;
          std::string file_id;
          std::array<std::uint8_t, 32> file_key{};
          if (!DecodeChatFile(ev.envelope, off, file_size, file_name, file_id,
                              file_key) ||
              off != ev.envelope.size()) {
            continue;
          }
          if (ev.outgoing) {
            OutgoingChatFileMessage f;
            f.peer_username = ev.conv_id;
            f.message_id_hex = id_hex;
            f.file_id = std::move(file_id);
            f.file_key = file_key;
            f.file_name = std::move(file_name);
            f.file_size = file_size;
            result.outgoing_files.push_back(std::move(f));
          } else {
            ChatFileMessage f;
            f.from_username = ev.sender;
            f.message_id_hex = id_hex;
            f.file_id = std::move(file_id);
            f.file_key = file_key;
            f.file_name = std::move(file_name);
            f.file_size = file_size;
            result.files.push_back(std::move(f));
          }
          core.BestEffortPersistHistoryEnvelope(ev.is_group, ev.outgoing, ev.conv_id,
                                          ev.sender, ev.envelope,
                                          HistoryStatus::kSent, NowUnixSeconds());
          continue;
        }

        if (type == kChatTypeSticker) {
          std::string sticker_id;
          if (!mi::server::proto::ReadString(ev.envelope, off, sticker_id) ||
              off != ev.envelope.size()) {
            continue;
          }
          if (ev.outgoing) {
            OutgoingChatStickerMessage s;
            s.peer_username = ev.conv_id;
            s.message_id_hex = id_hex;
            s.sticker_id = std::move(sticker_id);
            result.outgoing_stickers.push_back(std::move(s));
          } else {
            ChatStickerMessage s;
            s.from_username = ev.sender;
            s.message_id_hex = id_hex;
            s.sticker_id = std::move(sticker_id);
            result.stickers.push_back(std::move(s));
          }
          core.BestEffortPersistHistoryEnvelope(ev.is_group, ev.outgoing, ev.conv_id,
                                          ev.sender, ev.envelope,
                                          HistoryStatus::kSent, NowUnixSeconds());
          continue;
        }

        if (type == kChatTypeGroupText) {
          std::string group_id;
          std::string text;
          if (!mi::server::proto::ReadString(ev.envelope, off, group_id) ||
              !mi::server::proto::ReadString(ev.envelope, off, text) ||
              off != ev.envelope.size() || group_id != ev.conv_id) {
            continue;
          }
          if (ev.outgoing) {
            OutgoingGroupChatTextMessage t;
            t.group_id = group_id;
            t.message_id_hex = id_hex;
            t.text_utf8 = std::move(text);
            result.outgoing_group_texts.push_back(std::move(t));
          } else {
            GroupChatTextMessage t;
            t.group_id = group_id;
            t.from_username = ev.sender;
            t.message_id_hex = id_hex;
            t.text_utf8 = std::move(text);
            result.group_texts.push_back(std::move(t));
          }
          core.BestEffortPersistHistoryEnvelope(ev.is_group, ev.outgoing, ev.conv_id,
                                          ev.sender, ev.envelope,
                                          HistoryStatus::kSent, NowUnixSeconds());
          continue;
        }

        if (type == kChatTypeGroupFile) {
          std::string group_id;
          std::uint64_t file_size = 0;
          std::string file_name;
          std::string file_id;
          std::array<std::uint8_t, 32> file_key{};
          if (!DecodeChatGroupFile(ev.envelope, off, group_id, file_size,
                                   file_name, file_id, file_key) ||
              off != ev.envelope.size() || group_id != ev.conv_id) {
            continue;
          }
          if (ev.outgoing) {
            OutgoingGroupChatFileMessage f;
            f.group_id = group_id;
            f.message_id_hex = id_hex;
            f.file_id = std::move(file_id);
            f.file_key = file_key;
            f.file_name = std::move(file_name);
            f.file_size = file_size;
            result.outgoing_group_files.push_back(std::move(f));
          } else {
            GroupChatFileMessage f;
            f.group_id = group_id;
            f.from_username = ev.sender;
            f.message_id_hex = id_hex;
            f.file_id = std::move(file_id);
            f.file_key = file_key;
            f.file_name = std::move(file_name);
            f.file_size = file_size;
            result.group_files.push_back(std::move(f));
          }
          core.BestEffortPersistHistoryEnvelope(ev.is_group, ev.outgoing, ev.conv_id,
                                          ev.sender, ev.envelope,
                                          HistoryStatus::kSent, NowUnixSeconds());
          continue;
        }

        if (type == kChatTypeGroupInvite && !ev.outgoing) {
          std::string group_id;
          if (!mi::server::proto::ReadString(ev.envelope, off, group_id) ||
              off != ev.envelope.size()) {
            continue;
          }
          GroupInviteMessage inv;
          inv.group_id = std::move(group_id);
          inv.from_username = ev.sender;
          inv.message_id_hex = id_hex;
          result.group_invites.push_back(std::move(inv));
          continue;
        }

        continue;
      }

      if (ev.type == kDeviceSyncEventGroupNotice) {
        if (ev.conv_id.empty() || ev.sender.empty() || ev.envelope.empty()) {
          continue;
        }
        std::uint8_t kind = 0;
        std::string target;
        std::optional<std::uint8_t> role;
        if (!DecodeGroupNoticePayload(ev.envelope, kind, target, role)) {
          continue;
        }
        GroupNotice n;
        n.group_id = ev.conv_id;
        n.kind = kind;
        n.actor_username = ev.sender;
        n.target_username = std::move(target);
        if (role.has_value()) {
          const std::uint8_t rb = role.value();
          if (rb <= static_cast<std::uint8_t>(GroupMemberRole::kMember)) {
            n.role = static_cast<GroupMemberRole>(rb);
          }
        }
        result.group_notices.push_back(std::move(n));
        continue;
      }

      if (ev.type == kDeviceSyncEventDelivery) {
        if (ev.conv_id.empty()) {
          continue;
        }
        const std::string id_hex =
            BytesToHexLower(ev.msg_id.data(), ev.msg_id.size());
        if (id_hex.empty()) {
          continue;
        }
        if (ev.is_read) {
          ChatReadReceipt r;
          r.from_username = ev.conv_id;
          r.message_id_hex = id_hex;
          result.read_receipts.push_back(std::move(r));
        } else {
          ChatDelivery d;
          d.from_username = ev.conv_id;
          d.message_id_hex = id_hex;
          result.deliveries.push_back(std::move(d));
        }
        core.BestEffortPersistHistoryStatus(ev.is_group, ev.conv_id, ev.msg_id,
                                      ev.is_read ? HistoryStatus::kRead
                                                 : HistoryStatus::kDelivered,
                                      NowUnixSeconds());
        continue;
      }
    }

    core.last_error_ = sync_err;
    return result;
  }

  if (!core.EnsureE2ee()) {
    return result;
  }
  if (!core.EnsurePreKeyPublished()) {
    return result;
  }

  std::string sync_err;
  if (core.device_sync_enabled_ && core.device_sync_is_primary_) {
    if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
      sync_err = core.last_error_;
      core.last_error_.clear();
    }
  }
  if (core.device_sync_enabled_ && core.device_sync_is_primary_ && core.device_sync_key_loaded_) {
    const auto pulled = core.PullDeviceSyncCiphertexts();
    if (sync_err.empty() && !core.last_error_.empty()) {
      sync_err = core.last_error_;
    }
    core.last_error_.clear();

    const auto send_group_envelope =
        [&](const std::string& group_id, const std::vector<std::uint8_t>& envelope,
            std::string& out_warn) -> bool {
      out_warn.clear();
      if (group_id.empty()) {
        core.last_error_ = "group id empty";
        return false;
      }

      std::uint8_t type = 0;
      std::array<std::uint8_t, 16> msg_id{};
      std::size_t off = 0;
      if (!DecodeChatHeader(envelope, type, msg_id, off)) {
        core.last_error_ = "group envelope invalid";
        return false;
      }
      if (type != kChatTypeGroupText && type != kChatTypeGroupFile) {
        core.last_error_ = "group envelope invalid";
        return false;
      }
      std::size_t tmp_off = off;
      std::string inner_group_id;
      if (!mi::server::proto::ReadString(envelope, tmp_off, inner_group_id) ||
          inner_group_id != group_id) {
        core.last_error_ = "group envelope invalid";
        return false;
      }

      const auto members = core.ListGroupMembers(group_id);
      if (members.empty()) {
        if (core.last_error_.empty()) {
          core.last_error_ = "group member list empty";
        }
        return false;
      }

      ClientCore::GroupSenderKeyState* sender_key = nullptr;
      std::string warn;
      if (!core.EnsureGroupSenderKeyForSend(group_id, members, sender_key, warn)) {
        return false;
      }
      out_warn = warn;
      if (!sender_key) {
        core.last_error_ = "sender key unavailable";
        return false;
      }

      std::array<std::uint8_t, 32> next_ck{};
      std::array<std::uint8_t, 32> mk{};
      if (!KdfGroupCk(sender_key->ck, next_ck, mk)) {
        core.last_error_ = "kdf failed";
        return false;
      }
      const std::uint32_t iter = sender_key->next_iteration;

      std::array<std::uint8_t, 24> nonce{};
      if (!RandomBytes(nonce.data(), nonce.size())) {
        core.last_error_ = "rng failed";
        return false;
      }
      std::vector<std::uint8_t> ad;
      BuildGroupCipherAd(group_id, core.username_, sender_key->version, iter, ad);

      std::vector<std::uint8_t> padded_envelope;
      std::string pad_err;
      if (!PadPayload(envelope, padded_envelope, pad_err)) {
        core.last_error_ = pad_err.empty() ? "pad group message failed" : pad_err;
        return false;
      }

      std::vector<std::uint8_t> cipher;
      cipher.resize(padded_envelope.size());
      std::array<std::uint8_t, 16> mac{};
      crypto_aead_lock(cipher.data(), mac.data(), mk.data(), nonce.data(),
                       ad.data(), ad.size(), padded_envelope.data(),
                       padded_envelope.size());

      std::vector<std::uint8_t> wire_no_sig;
      if (!EncodeGroupCipherNoSig(group_id, core.username_, sender_key->version, iter,
                                  nonce, mac, cipher, wire_no_sig)) {
        core.last_error_ = "encode group cipher failed";
        return false;
      }

      std::vector<std::uint8_t> msg_sig;
      std::string msg_sig_err;
      if (!core.e2ee_.SignDetached(wire_no_sig, msg_sig, msg_sig_err)) {
        core.last_error_ =
            msg_sig_err.empty() ? "sign group message failed" : msg_sig_err;
        return false;
      }

      std::vector<std::uint8_t> wire = std::move(wire_no_sig);
      mi::server::proto::WriteBytes(msg_sig, wire);

      if (!core.SendGroupCipherMessage(group_id, wire)) {
        return false;
      }

      sender_key->ck = next_ck;
      sender_key->next_iteration++;
      sender_key->sent_count++;
      return true;
    };

    for (const auto& cipher : pulled) {
      std::vector<std::uint8_t> plain;
      if (!core.DecryptDeviceSync(cipher, plain)) {
        if (sync_err.empty() && !core.last_error_.empty()) {
          sync_err = core.last_error_;
        }
        core.last_error_.clear();
        continue;
      }

      DeviceSyncEvent ev;
      if (!DecodeDeviceSyncEvent(plain, ev)) {
        continue;
      }

      if (ev.type == kDeviceSyncEventRotateKey) {
        if (!core.StoreDeviceSyncKey(ev.new_key)) {
          if (sync_err.empty() && !core.last_error_.empty()) {
            sync_err = core.last_error_;
          }
          core.last_error_.clear();
        }
        continue;
      }

      if (ev.type == kDeviceSyncEventSendPrivate) {
        if (ev.conv_id.empty() || ev.envelope.empty()) {
          continue;
        }
        std::uint8_t type = 0;
        std::array<std::uint8_t, 16> msg_id{};
        std::size_t off = 0;
        if (!DecodeChatHeader(ev.envelope, type, msg_id, off)) {
          continue;
        }
        const std::string id_hex =
            BytesToHexLower(msg_id.data(), msg_id.size());

        const bool can_sync_out =
            (type == kChatTypeText || type == kChatTypeFile || type == kChatTypeRich ||
             type == kChatTypeSticker);

        const std::string saved_err = core.last_error_;
        const bool sent = core.SendPrivateE2ee(ev.conv_id, ev.envelope);
        core.last_error_ = saved_err;
        if (!sent) {
          continue;
        }
        core.BestEffortPersistHistoryEnvelope(false, true, ev.conv_id, core.username_,
                                        ev.envelope, HistoryStatus::kSent,
                                        NowUnixSeconds());

        if (type == kChatTypeText) {
          std::string text;
          if (!mi::server::proto::ReadString(ev.envelope, off, text) ||
              off != ev.envelope.size()) {
            continue;
          }
          OutgoingChatTextMessage t;
          t.peer_username = ev.conv_id;
          t.message_id_hex = id_hex;
          t.text_utf8 = std::move(text);
          result.outgoing_texts.push_back(std::move(t));
        } else if (type == kChatTypeFile) {
          std::uint64_t file_size = 0;
          std::string file_name;
          std::string file_id;
          std::array<std::uint8_t, 32> file_key{};
          if (!DecodeChatFile(ev.envelope, off, file_size, file_name, file_id,
                              file_key) ||
              off != ev.envelope.size()) {
            continue;
          }
          OutgoingChatFileMessage f;
          f.peer_username = ev.conv_id;
          f.message_id_hex = id_hex;
          f.file_id = std::move(file_id);
          f.file_key = file_key;
          f.file_name = std::move(file_name);
          f.file_size = file_size;
          result.outgoing_files.push_back(std::move(f));
        } else if (type == kChatTypeRich) {
          RichDecoded rich;
          if (!DecodeChatRich(ev.envelope, off, rich) || off != ev.envelope.size()) {
            continue;
          }
          OutgoingChatTextMessage t;
          t.peer_username = ev.conv_id;
          t.message_id_hex = id_hex;
          t.text_utf8 = FormatRichAsText(rich);
          result.outgoing_texts.push_back(std::move(t));
        } else if (type == kChatTypeSticker) {
          std::string sticker_id;
          if (!mi::server::proto::ReadString(ev.envelope, off, sticker_id) ||
              off != ev.envelope.size()) {
            continue;
          }
          OutgoingChatStickerMessage s;
          s.peer_username = ev.conv_id;
          s.message_id_hex = id_hex;
          s.sticker_id = std::move(sticker_id);
          result.outgoing_stickers.push_back(std::move(s));
        }

        if (can_sync_out) {
          core.BestEffortBroadcastDeviceSyncMessage(false, true, ev.conv_id, core.username_,
                                              ev.envelope);
        }
        continue;
      }

      if (ev.type == kDeviceSyncEventSendGroup) {
        if (ev.conv_id.empty() || ev.envelope.empty()) {
          continue;
        }
        std::uint8_t type = 0;
        std::array<std::uint8_t, 16> msg_id{};
        std::size_t off = 0;
        if (!DecodeChatHeader(ev.envelope, type, msg_id, off)) {
          continue;
        }
        const std::string id_hex =
            BytesToHexLower(msg_id.data(), msg_id.size());
        const bool can_sync_out =
            (type == kChatTypeGroupText || type == kChatTypeGroupFile);
        if (!can_sync_out) {
          continue;
        }

        std::string warn;
        const std::string saved_err = core.last_error_;
        const bool sent = send_group_envelope(ev.conv_id, ev.envelope, warn);
        core.last_error_ = saved_err;
        if (!sent) {
          continue;
        }
        core.BestEffortPersistHistoryEnvelope(true, true, ev.conv_id, core.username_,
                                        ev.envelope, HistoryStatus::kSent,
                                        NowUnixSeconds());

        if (!id_hex.empty()) {
          const auto map_it = core.group_delivery_map_.find(id_hex);
          if (map_it == core.group_delivery_map_.end()) {
            core.group_delivery_map_[id_hex] = ev.conv_id;
            core.group_delivery_order_.push_back(id_hex);
            while (core.group_delivery_order_.size() > kChatSeenLimit) {
              core.group_delivery_map_.erase(core.group_delivery_order_.front());
              core.group_delivery_order_.pop_front();
            }
          } else {
            map_it->second = ev.conv_id;
          }
        }

        if (type == kChatTypeGroupText) {
          std::string group_id;
          std::string text;
          if (!mi::server::proto::ReadString(ev.envelope, off, group_id) ||
              !mi::server::proto::ReadString(ev.envelope, off, text) ||
              off != ev.envelope.size() || group_id != ev.conv_id) {
            continue;
          }
          OutgoingGroupChatTextMessage t;
          t.group_id = group_id;
          t.message_id_hex = id_hex;
          t.text_utf8 = std::move(text);
          result.outgoing_group_texts.push_back(std::move(t));
        } else if (type == kChatTypeGroupFile) {
          std::string group_id;
          std::uint64_t file_size = 0;
          std::string file_name;
          std::string file_id;
          std::array<std::uint8_t, 32> file_key{};
          if (!DecodeChatGroupFile(ev.envelope, off, group_id, file_size,
                                   file_name, file_id, file_key) ||
              off != ev.envelope.size() || group_id != ev.conv_id) {
            continue;
          }
          OutgoingGroupChatFileMessage f;
          f.group_id = group_id;
          f.message_id_hex = id_hex;
          f.file_id = std::move(file_id);
          f.file_key = file_key;
          f.file_name = std::move(file_name);
          f.file_size = file_size;
          result.outgoing_group_files.push_back(std::move(f));
        }

        core.BestEffortBroadcastDeviceSyncMessage(true, true, ev.conv_id, core.username_,
                                            ev.envelope);
        continue;
      }
    }
  }

  std::string group_notice_err;
  const std::string saved_poll_err = core.last_error_;
  const auto group_notice_msgs = core.PullGroupNoticeMessages();
  if (!core.last_error_.empty()) {
    group_notice_err = core.last_error_;
  }
  core.last_error_ = saved_poll_err;
  if (sync_err.empty() && saved_poll_err.empty() && !group_notice_err.empty()) {
    sync_err = group_notice_err;
  }

  if (!group_notice_msgs.empty()) {
    const auto broadcast_notice = [&](const std::string& group_id,
                                      const std::string& actor_username,
                                      const std::vector<std::uint8_t>& payload) {
      if (!core.device_sync_enabled_ || !core.device_sync_is_primary_) {
        return;
      }
      const std::string saved_err = core.last_error_;
      if (!core.device_sync_key_loaded_ && !core.LoadDeviceSyncKey()) {
        core.last_error_ = saved_err;
        return;
      }
      MaybeRotateDeviceSyncKey(core);
      std::vector<std::uint8_t> event_plain;
      if (!EncodeDeviceSyncGroupNotice(group_id, actor_username, payload,
                                       event_plain)) {
        core.last_error_ = saved_err;
        return;
      }
      std::vector<std::uint8_t> event_cipher;
      if (!core.EncryptDeviceSync(event_plain, event_cipher)) {
        core.last_error_ = saved_err;
        return;
      }
      if (core.PushDeviceSyncCiphertext(event_cipher)) {
        core.device_sync_send_count_++;
      }
      core.last_error_ = saved_err;
    };

    for (const auto& m : group_notice_msgs) {
      if (m.group_id.empty() || m.sender_username.empty() || m.payload.empty()) {
        continue;
      }
      std::uint8_t kind = 0;
      std::string target;
      std::optional<std::uint8_t> role;
      if (!DecodeGroupNoticePayload(m.payload, kind, target, role)) {
        continue;
      }

      GroupNotice n;
      n.group_id = m.group_id;
      n.kind = kind;
      n.actor_username = m.sender_username;
      n.target_username = std::move(target);
      if (role.has_value()) {
        const std::uint8_t rb = role.value();
        if (rb <= static_cast<std::uint8_t>(GroupMemberRole::kMember)) {
          n.role = static_cast<GroupMemberRole>(rb);
        }
      }
      result.group_notices.push_back(std::move(n));

      broadcast_notice(m.group_id, m.sender_username, m.payload);

      if (kind == kGroupNoticeJoin || kind == kGroupNoticeLeave ||
          kind == kGroupNoticeKick) {
        core.group_membership_dirty_.insert(m.group_id);
      }
    }
  }

  if (!core.group_membership_dirty_.empty()) {
    std::vector<std::string> pending;
    pending.reserve(core.group_membership_dirty_.size());
    for (const auto& gid : core.group_membership_dirty_) {
      pending.push_back(gid);
    }

    std::size_t attempt = 0;
    for (const auto& gid : pending) {
      if (++attempt > 16) {
        break;
      }
      const std::string saved_err = core.last_error_;
      const auto members = core.ListGroupMembers(gid);
      const std::string list_err = core.last_error_;
      if (members.empty()) {
        if (list_err == "not in group") {
          core.group_membership_dirty_.erase(gid);
        }
        core.last_error_ = saved_err;
        continue;
      }
      ClientCore::GroupSenderKeyState* sender_key = nullptr;
      std::string warn;
      const bool ok = core.EnsureGroupSenderKeyForSend(gid, members, sender_key, warn);
      if (ok && sender_key) {
        core.group_membership_dirty_.erase(gid);
      }
      core.last_error_ = saved_err;
    }
  }

  const auto pulled = core.PullPrivateE2ee();
  const std::string pull_err = core.last_error_;
  const auto ready = core.DrainReadyPrivateE2ee();
  const std::string ready_err = core.last_error_;
  core.last_error_ = !ready_err.empty() ? ready_err : pull_err;

  const auto handle = [&](const mi::client::e2ee::PrivateMessage& msg) {
    if (msg.from_username.empty()) {
      return;
    }
    std::uint8_t type = 0;
    std::array<std::uint8_t, 16> msg_id{};
    std::size_t off = 0;
    if (!DecodeChatHeader(msg.plaintext, type, msg_id, off)) {
      // Legacy plaintext: forward as best-effort utf8 text.
      ChatTextMessage t;
      t.from_username = msg.from_username;
      t.text_utf8.assign(reinterpret_cast<const char*>(msg.plaintext.data()),
                         msg.plaintext.size());
      result.texts.push_back(std::move(t));
      return;
    }

    const std::string id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
    if (type == kChatTypeAck) {
      if (off != msg.plaintext.size()) {
        return;
      }
      const auto pending_it = core.pending_sender_key_dists_.find(id_hex);
      if (pending_it != core.pending_sender_key_dists_.end()) {
        pending_it->second.pending_members.erase(msg.from_username);
        if (pending_it->second.pending_members.empty()) {
          core.pending_sender_key_dists_.erase(pending_it);
        }
        return;
      }
      ChatDelivery d;
      d.from_username = msg.from_username;
      d.message_id_hex = id_hex;
      result.deliveries.push_back(std::move(d));
      bool delivery_is_group = false;
      std::string delivery_conv = msg.from_username;
      const auto g_it = core.group_delivery_map_.find(id_hex);
      if (g_it != core.group_delivery_map_.end()) {
        delivery_is_group = true;
        delivery_conv = g_it->second;
      }
      core.BestEffortBroadcastDeviceSyncDelivery(delivery_is_group, delivery_conv,
                                           msg_id, false);
      return;
    }

    if (type == kChatTypeReadReceipt) {
      if (off != msg.plaintext.size()) {
        return;
      }
      ChatReadReceipt r;
      r.from_username = msg.from_username;
      r.message_id_hex = id_hex;
      result.read_receipts.push_back(std::move(r));
      core.BestEffortBroadcastDeviceSyncDelivery(false, msg.from_username, msg_id, true);
      return;
    }

    if (type == kChatTypeTyping) {
      if (off >= msg.plaintext.size()) {
        return;
      }
      const std::uint8_t state = msg.plaintext[off++];
      if (off != msg.plaintext.size()) {
        return;
      }
      ChatTypingEvent te;
      te.from_username = msg.from_username;
      te.typing = state != 0;
      result.typing_events.push_back(std::move(te));
      core.BestEffortBroadcastDeviceSyncMessage(false, false, msg.from_username,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypePresence) {
      if (off >= msg.plaintext.size()) {
        return;
      }
      const std::uint8_t state = msg.plaintext[off++];
      if (off != msg.plaintext.size()) {
        return;
      }
      ChatPresenceEvent pe;
      pe.from_username = msg.from_username;
      pe.online = state != 0;
      result.presence_events.push_back(std::move(pe));
      core.BestEffortBroadcastDeviceSyncMessage(false, false, msg.from_username,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypeGroupSenderKeyDist) {
      std::string group_id;
      std::uint32_t version = 0;
      std::uint32_t iteration = 0;
      std::array<std::uint8_t, 32> ck{};
      std::vector<std::uint8_t> sig;
      if (!DecodeChatGroupSenderKeyDist(msg.plaintext, off, group_id, version,
                                        iteration, ck, sig) ||
          off != msg.plaintext.size()) {
        return;
      }
      if (group_id.empty() || version == 0 || sig.empty()) {
        return;
      }

      ClientCore::CachedPeerIdentity peer;
      if (!core.GetPeerIdentityCached(msg.from_username, peer, true)) {
        return;
      }
      const auto sig_msg =
          BuildGroupSenderKeyDistSigMessage(group_id, version, iteration, ck);
      std::string ver_err;
      if (!mi::client::e2ee::Engine::VerifyDetached(sig_msg, sig, peer.id_sig_pk,
                                                    ver_err)) {
        return;
      }

      const std::string key =
          MakeGroupSenderKeyMapKey(group_id, msg.from_username);
      auto& state = core.group_sender_keys_[key];
      const bool have_key =
          (state.version != 0 && !IsAllZero(state.ck.data(), state.ck.size()));
      const bool accept =
          (!have_key || version > state.version ||
           (version == state.version && iteration >= state.next_iteration));
      if (accept) {
        state.group_id = group_id;
        state.sender_username = msg.from_username;
        state.version = version;
        state.next_iteration = iteration;
        state.ck = ck;
        state.members_hash.clear();
        state.rotated_at = NowUnixSeconds();
        state.sent_count = 0;
        state.skipped_mks.clear();
        state.skipped_order.clear();
      }

      std::vector<std::uint8_t> ack;
      if (EncodeChatAck(msg_id, ack)) {
        const std::string saved_err = core.last_error_;
        core.SendPrivateE2ee(msg.from_username, ack);
        core.last_error_ = saved_err;
      }
      return;
    }

    if (type == kChatTypeGroupSenderKeyReq) {
      std::string group_id;
      std::uint32_t want_version = 0;
      if (!DecodeChatGroupSenderKeyReq(msg.plaintext, off, group_id,
                                       want_version) ||
          off != msg.plaintext.size()) {
        return;
      }
      if (group_id.empty()) {
        return;
      }

      const std::string map_key = MakeGroupSenderKeyMapKey(group_id, core.username_);
      const auto it = core.group_sender_keys_.find(map_key);
      if (it == core.group_sender_keys_.end() || it->second.version == 0 ||
          IsAllZero(it->second.ck.data(), it->second.ck.size())) {
        return;
      }
      if (want_version != 0 && it->second.version < want_version) {
        return;
      }

      {
        const std::string saved_err = core.last_error_;
        const auto members = core.ListGroupMembers(group_id);
        core.last_error_ = saved_err;
        bool allowed = false;
        for (const auto& m : members) {
          if (m == msg.from_username) {
            allowed = true;
            break;
          }
        }
        if (!allowed) {
          return;
        }
      }

      std::array<std::uint8_t, 16> dist_id{};
      if (!RandomBytes(dist_id.data(), dist_id.size())) {
        return;
      }
      const std::string dist_id_hex =
          BytesToHexLower(dist_id.data(), dist_id.size());

      const auto sig_msg = BuildGroupSenderKeyDistSigMessage(
          group_id, it->second.version, it->second.next_iteration, it->second.ck);
      std::vector<std::uint8_t> sig;
      std::string sig_err;
      if (!core.e2ee_.SignDetached(sig_msg, sig, sig_err)) {
        return;
      }

      std::vector<std::uint8_t> dist_envelope;
      if (!EncodeChatGroupSenderKeyDist(dist_id, group_id, it->second.version,
                                        it->second.next_iteration, it->second.ck,
                                        sig, dist_envelope)) {
        return;
      }

      ClientCore::PendingSenderKeyDistribution pending;
      pending.group_id = group_id;
      pending.version = it->second.version;
      pending.envelope = dist_envelope;
      pending.last_sent_ms = mi::platform::NowSteadyMs();
      pending.pending_members.insert(msg.from_username);
      core.pending_sender_key_dists_[dist_id_hex] = std::move(pending);

      const std::string saved_err = core.last_error_;
      core.SendPrivateE2ee(msg.from_username, dist_envelope);
      core.last_error_ = saved_err;
      return;
    }

    if (type == kChatTypeGroupCallKeyDist) {
      std::string group_id;
      std::array<std::uint8_t, 16> call_id{};
      std::uint32_t key_id = 0;
      std::array<std::uint8_t, 32> call_key{};
      std::vector<std::uint8_t> sig;
      if (!DecodeChatGroupCallKeyDist(msg.plaintext, off, group_id, call_id,
                                      key_id, call_key, sig) ||
          off != msg.plaintext.size()) {
        return;
      }
      if (group_id.empty() || key_id == 0 || sig.empty()) {
        return;
      }

      ClientCore::CachedPeerIdentity peer;
      if (!core.GetPeerIdentityCached(msg.from_username, peer, true)) {
        return;
      }
      const auto sig_msg =
          core.BuildGroupCallKeyDistSigMessage(group_id, call_id, key_id, call_key);
      std::string ver_err;
      if (!mi::client::e2ee::Engine::VerifyDetached(sig_msg, sig,
                                                    peer.id_sig_pk, ver_err)) {
        return;
      }

      const std::string map_key = MakeGroupCallKeyMapKey(group_id, call_id);
      const auto it = core.group_call_keys_.find(map_key);
      const bool accept = (it == core.group_call_keys_.end() ||
                           it->second.key_id == 0 ||
                           key_id >= it->second.key_id);
      if (accept) {
        core.StoreGroupCallKey(group_id, call_id, key_id, call_key);
      }

      std::vector<std::uint8_t> ack;
      if (EncodeChatAck(msg_id, ack)) {
        const std::string saved_err = core.last_error_;
        core.SendPrivateE2ee(msg.from_username, ack);
        core.last_error_ = saved_err;
      }
      return;
    }

    if (type == kChatTypeGroupCallKeyReq) {
      std::string group_id;
      std::array<std::uint8_t, 16> call_id{};
      std::uint32_t want_key_id = 0;
      if (!DecodeChatGroupCallKeyReq(msg.plaintext, off, group_id, call_id,
                                     want_key_id) ||
          off != msg.plaintext.size()) {
        return;
      }
      if (group_id.empty() || want_key_id == 0) {
        return;
      }
      std::array<std::uint8_t, 32> call_key{};
      if (!core.LookupGroupCallKey(group_id, call_id, want_key_id, call_key)) {
        return;
      }

      {
        const std::string saved_err = core.last_error_;
        const auto members = core.ListGroupMembers(group_id);
        core.last_error_ = saved_err;
        bool allowed = false;
        for (const auto& m : members) {
          if (m == msg.from_username) {
            allowed = true;
            break;
          }
        }
        if (!allowed) {
          return;
        }
      }

      std::array<std::uint8_t, 16> dist_id{};
      if (!RandomBytes(dist_id.data(), dist_id.size())) {
        return;
      }
      const auto sig_msg =
          core.BuildGroupCallKeyDistSigMessage(group_id, call_id, want_key_id, call_key);
      std::vector<std::uint8_t> sig;
      std::string sig_err;
      if (!core.e2ee_.SignDetached(sig_msg, sig, sig_err)) {
        return;
      }
      std::vector<std::uint8_t> envelope;
      if (!EncodeChatGroupCallKeyDist(dist_id, group_id, call_id, want_key_id,
                                      call_key, sig, envelope)) {
        return;
      }
      const std::string saved_err = core.last_error_;
      core.SendGroupSenderKeyEnvelope(group_id, msg.from_username, envelope);
      core.last_error_ = saved_err;
      return;
    }

    const bool known_type =
        (type == kChatTypeText || type == kChatTypeFile || type == kChatTypeRich ||
         type == kChatTypeSticker || type == kChatTypeGroupText ||
         type == kChatTypeGroupInvite || type == kChatTypeGroupFile);
    if (!known_type) {
      return;
    }

    // Send delivery ack (best effort).
    std::vector<std::uint8_t> ack;
    if (EncodeChatAck(msg_id, ack)) {
      const std::string saved_err = core.last_error_;
      core.SendPrivateE2ee(msg.from_username, ack);
      core.last_error_ = saved_err;
    }

    const std::string seen_key = msg.from_username + "|" + id_hex;
    if (core.chat_seen_ids_.find(seen_key) != core.chat_seen_ids_.end()) {
      return;
    }
    core.chat_seen_ids_.insert(seen_key);
    core.chat_seen_order_.push_back(seen_key);
    while (core.chat_seen_order_.size() > kChatSeenLimit) {
      core.chat_seen_ids_.erase(core.chat_seen_order_.front());
      core.chat_seen_order_.pop_front();
    }

    if (type == kChatTypeText) {
      std::string text;
      if (!mi::server::proto::ReadString(msg.plaintext, off, text) ||
          off != msg.plaintext.size()) {
        return;
      }
      ChatTextMessage t;
      t.from_username = msg.from_username;
      t.message_id_hex = id_hex;
      t.text_utf8 = std::move(text);
      result.texts.push_back(std::move(t));
      core.BestEffortPersistHistoryEnvelope(false, false, msg.from_username,
                                      msg.from_username, msg.plaintext,
                                      HistoryStatus::kSent, NowUnixSeconds());
      core.BestEffortBroadcastDeviceSyncMessage(false, false, msg.from_username,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypeRich) {
      RichDecoded rich;
      if (!DecodeChatRich(msg.plaintext, off, rich) || off != msg.plaintext.size()) {
        return;
      }
      ChatTextMessage t;
      t.from_username = msg.from_username;
      t.message_id_hex = id_hex;
      t.text_utf8 = FormatRichAsText(rich);
      result.texts.push_back(std::move(t));
      core.BestEffortPersistHistoryEnvelope(false, false, msg.from_username,
                                      msg.from_username, msg.plaintext,
                                      HistoryStatus::kSent, NowUnixSeconds());
      core.BestEffortBroadcastDeviceSyncMessage(false, false, msg.from_username,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypeFile) {
      std::uint64_t file_size = 0;
      std::string file_name;
      std::string file_id;
      std::array<std::uint8_t, 32> file_key{};
      if (!DecodeChatFile(msg.plaintext, off, file_size, file_name, file_id,
                          file_key) ||
          off != msg.plaintext.size()) {
        return;
      }
      ChatFileMessage f;
      f.from_username = msg.from_username;
      f.message_id_hex = id_hex;
      f.file_id = std::move(file_id);
      f.file_key = file_key;
      f.file_name = std::move(file_name);
      f.file_size = file_size;
      result.files.push_back(std::move(f));
      core.BestEffortPersistHistoryEnvelope(false, false, msg.from_username,
                                      msg.from_username, msg.plaintext,
                                      HistoryStatus::kSent, NowUnixSeconds());
      core.BestEffortBroadcastDeviceSyncMessage(false, false, msg.from_username,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypeSticker) {
      std::string sticker_id;
      if (!mi::server::proto::ReadString(msg.plaintext, off, sticker_id) ||
          off != msg.plaintext.size()) {
        return;
      }
      ChatStickerMessage s;
      s.from_username = msg.from_username;
      s.message_id_hex = id_hex;
      s.sticker_id = std::move(sticker_id);
      result.stickers.push_back(std::move(s));
      core.BestEffortPersistHistoryEnvelope(false, false, msg.from_username,
                                      msg.from_username, msg.plaintext,
                                      HistoryStatus::kSent, NowUnixSeconds());
      core.BestEffortBroadcastDeviceSyncMessage(false, false, msg.from_username,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypeGroupText) {
      std::string group_id;
      std::string text;
      if (!mi::server::proto::ReadString(msg.plaintext, off, group_id) ||
          !mi::server::proto::ReadString(msg.plaintext, off, text) ||
          off != msg.plaintext.size()) {
        return;
      }
      GroupChatTextMessage t;
      t.group_id = group_id;
      t.from_username = msg.from_username;
      t.message_id_hex = id_hex;
      t.text_utf8 = std::move(text);
      result.group_texts.push_back(std::move(t));
      core.BestEffortPersistHistoryEnvelope(true, false, group_id, msg.from_username,
                                      msg.plaintext, HistoryStatus::kSent,
                                      NowUnixSeconds());
      core.BestEffortBroadcastDeviceSyncMessage(true, false, group_id,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypeGroupFile) {
      std::string group_id;
      std::uint64_t file_size = 0;
      std::string file_name;
      std::string file_id;
      std::array<std::uint8_t, 32> file_key{};
      if (!DecodeChatGroupFile(msg.plaintext, off, group_id, file_size,
                               file_name, file_id, file_key) ||
          off != msg.plaintext.size()) {
        return;
      }
      GroupChatFileMessage f;
      f.group_id = group_id;
      f.from_username = msg.from_username;
      f.message_id_hex = id_hex;
      f.file_id = std::move(file_id);
      f.file_key = file_key;
      f.file_name = std::move(file_name);
      f.file_size = file_size;
      result.group_files.push_back(std::move(f));
      core.BestEffortPersistHistoryEnvelope(true, false, group_id, msg.from_username,
                                      msg.plaintext, HistoryStatus::kSent,
                                      NowUnixSeconds());
      core.BestEffortBroadcastDeviceSyncMessage(true, false, group_id,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypeGroupInvite) {
      std::string group_id;
      if (!mi::server::proto::ReadString(msg.plaintext, off, group_id) ||
          off != msg.plaintext.size()) {
        return;
      }
      GroupInviteMessage inv;
      inv.group_id = std::move(group_id);
      inv.from_username = msg.from_username;
      inv.message_id_hex = id_hex;
      result.group_invites.push_back(std::move(inv));
      core.BestEffortBroadcastDeviceSyncMessage(true, false, inv.group_id,
                                          msg.from_username, msg.plaintext);
      return;
    }
  };

  for (const auto& m : pulled) {
    handle(m);
  }
  for (const auto& m : ready) {
    handle(m);
  }

  const std::string poll_err = core.last_error_;
  auto group_msgs = core.PullGroupCipherMessages();
  const std::string group_err = core.last_error_;
  core.last_error_ = !poll_err.empty() ? poll_err : group_err;

  std::deque<ClientCore::PendingGroupCipher> work;
  work.swap(core.pending_group_cipher_);
  for (auto& m : group_msgs) {
    work.push_back(std::move(m));
  }

  const std::uint64_t now_ms = mi::platform::NowSteadyMs();
  const auto send_key_req = [&](const std::string& group_id,
                                const std::string& sender_username,
                                std::uint32_t want_version) {
    const std::string req_key =
        group_id + "|" + sender_username + "|" + std::to_string(want_version);
    const auto it = core.sender_key_req_last_sent_.find(req_key);
    if (it != core.sender_key_req_last_sent_.end() &&
        now_ms - it->second < 3 * 1000) {
      return;
    }
    core.sender_key_req_last_sent_[req_key] = now_ms;
    if (core.sender_key_req_last_sent_.size() > 4096) {
      core.sender_key_req_last_sent_.clear();
    }

    std::array<std::uint8_t, 16> req_id{};
    if (!RandomBytes(req_id.data(), req_id.size())) {
      return;
    }
    std::vector<std::uint8_t> req;
    if (!EncodeChatGroupSenderKeyReq(req_id, group_id, want_version, req)) {
      return;
    }
    const std::string saved_err = core.last_error_;
    core.SendPrivateE2ee(sender_username, req);
    core.last_error_ = saved_err;
  };

  for (auto& m : work) {
    std::uint32_t sender_key_version = 0;
    std::uint32_t sender_key_iteration = 0;
    std::string group_id;
    std::string sender_username;
    std::array<std::uint8_t, 24> nonce{};
    std::array<std::uint8_t, 16> mac{};
    std::vector<std::uint8_t> cipher;
    std::vector<std::uint8_t> sig;
    std::size_t sig_offset = 0;
    if (!DecodeGroupCipher(m.payload, sender_key_version, sender_key_iteration,
                           group_id, sender_username, nonce, mac, cipher, sig,
                           sig_offset)) {
      continue;
    }
    if ((!m.group_id.empty() && group_id != m.group_id) ||
        (!m.sender_username.empty() && sender_username != m.sender_username)) {
      continue;
    }
    if (group_id.empty() || sender_username.empty() || sig.empty() ||
        sig_offset == 0 || sig_offset > m.payload.size()) {
      continue;
    }

    ClientCore::CachedPeerIdentity peer;
    if (!core.GetPeerIdentityCached(sender_username, peer, true)) {
      core.pending_group_cipher_.push_back(std::move(m));
      continue;
    }

    std::vector<std::uint8_t> signed_part;
    signed_part.assign(m.payload.begin(), m.payload.begin() + sig_offset);
    std::string sig_err;
    if (!mi::client::e2ee::Engine::VerifyDetached(signed_part, sig, peer.id_sig_pk,
                                                  sig_err)) {
      continue;
    }

    const std::string key = MakeGroupSenderKeyMapKey(group_id, sender_username);
    auto it = core.group_sender_keys_.find(key);
    if (it == core.group_sender_keys_.end() || it->second.version == 0 ||
        IsAllZero(it->second.ck.data(), it->second.ck.size()) ||
        it->second.version < sender_key_version) {
      send_key_req(group_id, sender_username, sender_key_version);
      core.pending_group_cipher_.push_back(std::move(m));
      continue;
    }
    if (it->second.version > sender_key_version) {
      continue;
    }

    ClientCore::GroupSenderKeyState tmp = it->second;
    std::array<std::uint8_t, 32> mk{};
    if (!DeriveGroupMessageKey(tmp, sender_key_iteration, mk)) {
      send_key_req(group_id, sender_username, sender_key_version);
      continue;
    }

    std::vector<std::uint8_t> ad;
    BuildGroupCipherAd(group_id, sender_username, sender_key_version,
                       sender_key_iteration, ad);

    std::vector<std::uint8_t> plain;
    plain.resize(cipher.size());
    const int ok = crypto_aead_unlock(plain.data(), mac.data(), mk.data(),
                                      nonce.data(), ad.data(), ad.size(),
                                      cipher.data(), cipher.size());
    if (ok != 0) {
      crypto_wipe(plain.data(), plain.size());
      send_key_req(group_id, sender_username, sender_key_version);
      continue;
    }
    std::vector<std::uint8_t> unpadded;
    std::string pad_err;
    if (!UnpadPayload(plain, unpadded, pad_err)) {
      crypto_wipe(plain.data(), plain.size());
      continue;
    }
    crypto_wipe(plain.data(), plain.size());
    plain = std::move(unpadded);
    it->second = std::move(tmp);

    std::uint8_t type = 0;
    std::array<std::uint8_t, 16> msg_id{};
    std::size_t off = 0;
    if (!DecodeChatHeader(plain, type, msg_id, off)) {
      crypto_wipe(plain.data(), plain.size());
      continue;
    }

    std::vector<std::uint8_t> ack;
    if (EncodeChatAck(msg_id, ack)) {
      const std::string saved_err = core.last_error_;
      core.SendPrivateE2ee(sender_username, ack);
      core.last_error_ = saved_err;
    }

    const std::string id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
    const std::string seen_key = group_id + "|" + sender_username + "|" + id_hex;
    if (core.chat_seen_ids_.find(seen_key) != core.chat_seen_ids_.end()) {
      crypto_wipe(plain.data(), plain.size());
      continue;
    }
    core.chat_seen_ids_.insert(seen_key);
    core.chat_seen_order_.push_back(seen_key);
    while (core.chat_seen_order_.size() > kChatSeenLimit) {
      core.chat_seen_ids_.erase(core.chat_seen_order_.front());
      core.chat_seen_order_.pop_front();
    }

    if (type == kChatTypeGroupText) {
      std::string inner_group_id;
      std::string text;
      if (!mi::server::proto::ReadString(plain, off, inner_group_id) ||
          !mi::server::proto::ReadString(plain, off, text) ||
          off != plain.size() || inner_group_id != group_id) {
        crypto_wipe(plain.data(), plain.size());
        continue;
      }
      GroupChatTextMessage t;
      t.group_id = group_id;
      t.from_username = sender_username;
      t.message_id_hex = id_hex;
      t.text_utf8 = std::move(text);
      result.group_texts.push_back(std::move(t));
      core.BestEffortPersistHistoryEnvelope(true, false, group_id, sender_username,
                                      plain, HistoryStatus::kSent,
                                      NowUnixSeconds());
      core.BestEffortBroadcastDeviceSyncMessage(true, false, group_id, sender_username,
                                          plain);
    } else if (type == kChatTypeGroupFile) {
      std::string inner_group_id;
      std::uint64_t file_size = 0;
      std::string file_name;
      std::string file_id;
      std::array<std::uint8_t, 32> file_key{};
      if (!DecodeChatGroupFile(plain, off, inner_group_id, file_size, file_name,
                               file_id, file_key) ||
          off != plain.size() || inner_group_id != group_id) {
        crypto_wipe(plain.data(), plain.size());
        continue;
      }
      GroupChatFileMessage f;
      f.group_id = group_id;
      f.from_username = sender_username;
      f.message_id_hex = id_hex;
      f.file_id = std::move(file_id);
      f.file_key = file_key;
      f.file_name = std::move(file_name);
      f.file_size = file_size;
      result.group_files.push_back(std::move(f));
      core.BestEffortPersistHistoryEnvelope(true, false, group_id, sender_username,
                                      plain, HistoryStatus::kSent,
                                      NowUnixSeconds());
      core.BestEffortBroadcastDeviceSyncMessage(true, false, group_id, sender_username,
                                          plain);
    }

    crypto_wipe(plain.data(), plain.size());
  }

  while (core.pending_group_cipher_.size() > kPendingGroupCipherLimit) {
    core.pending_group_cipher_.pop_front();
  }

  if (core.last_error_.empty() && !sync_err.empty()) {
    core.last_error_ = sync_err;
  }
  return result;
}

}  // namespace mi::client
