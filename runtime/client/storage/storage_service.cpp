#include "storage_service.h"

#include "client_core.h"
#include "file_blob.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

#include "chat_history_store.h"
#include "crypto.h"
#include "buffer_pool.h"
#include "miniz.h"
#include "monocypher.h"
#include "platform_fs.h"
#include "platform_random.h"
#include "platform_time.h"
#include "protocol.h"

namespace mi::client {

using ChatFileMessage = ClientCore::ChatFileMessage;
using HistoryEntry = ClientCore::HistoryEntry;
using HistoryKind = ClientCore::HistoryKind;
using HistoryStatus = ClientCore::HistoryStatus;

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

constexpr std::size_t kChatHeaderSize = sizeof(kChatMagic) + 1 + 1 + 16;

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

bool ReadFixed16(const std::vector<std::uint8_t>& data, std::size_t& offset,
                 std::array<std::uint8_t, 16>& out) {
  if (offset + out.size() > data.size()) {
    return false;
  }
  std::memcpy(out.data(), data.data() + offset, out.size());
  offset += out.size();
  return true;
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

bool RandomUint32(std::uint32_t& out) {
  return mi::platform::RandomUint32(out);
}

bool RandomBytes(std::uint8_t* out, std::size_t len) {
  return mi::platform::RandomBytes(out, len);
}

std::uint64_t NowUnixSeconds() {
  return mi::platform::NowUnixSeconds();
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

mi::common::ByteBufferPool& FileBlobChunkPool() {
  static mi::common::ByteBufferPool pool(8, kE2eeBlobChunkBytes);
  return pool;
}

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
  if (version == kFileBlobVersionV1) {
    header_len = kFileBlobV1PrefixSize;
    header_size = kFileBlobV1HeaderSize;
  } else if (version == kFileBlobVersionV2) {
    header_len = kFileBlobV2PrefixSize;
    header_size = kFileBlobV2HeaderSize;
  } else if (version == kFileBlobVersionV3) {
    header_len = kFileBlobV3PrefixSize;
    header_size = kFileBlobV3HeaderSize;
  } else if (version == kFileBlobVersionV4) {
    header_len = kFileBlobV4BaseHeaderSize;
    header_size = kFileBlobV4BaseHeaderSize;
  } else {
    return false;
  }
  if (blob.size() < header_size) {
    return false;
  }

  std::size_t off = sizeof(kFileBlobMagic) + 1;
  std::uint8_t flags = 0;
  std::uint8_t algo = 0;
  std::uint64_t original_size = 0;
  std::uint64_t stage1_size = 0;
  std::uint64_t stage2_size = 0;
  std::uint64_t chunk_count = 0;
  std::uint32_t chunk_size = 0;
  if (version == kFileBlobVersionV2) {
    flags = blob[off++];
    algo = blob[off++];
    off++;
    if (!mi::server::proto::ReadUint64(blob, off, original_size) ||
        !mi::server::proto::ReadUint64(blob, off, stage1_size) ||
        !mi::server::proto::ReadUint64(blob, off, stage2_size) ||
        off != kFileBlobV2PrefixSize) {
      return false;
    }
    if (original_size == 0 ||
        original_size > static_cast<std::uint64_t>(kMaxChatFileBytes) ||
        stage2_size == 0 ||
        stage2_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
      return false;
    }
  } else if (version == kFileBlobVersionV3) {
    flags = blob[off++];
    algo = blob[off++];
    off++;
    if (!mi::server::proto::ReadUint32(blob, off, chunk_size) ||
        !mi::server::proto::ReadUint64(blob, off, original_size) ||
        off + 24 != kFileBlobV3PrefixSize) {
      return false;
    }
    if (algo != kFileBlobAlgoRaw) {
      return false;
    }
    if (chunk_size == 0 ||
        chunk_size > (kE2eeBlobChunkBytes - 16u) ||
        original_size == 0 ||
        original_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
      return false;
    }
    const std::uint64_t expect =
        static_cast<std::uint64_t>(kFileBlobV3PrefixSize) +
        (static_cast<std::uint64_t>(original_size + chunk_size - 1) /
         chunk_size) * 16u + original_size;
    if (expect > static_cast<std::uint64_t>(kMaxChatFileBlobBytes) ||
        expect != static_cast<std::uint64_t>(blob.size())) {
      return false;
    }
  } else if (version == kFileBlobVersionV4) {
    flags = blob[off++];
    algo = blob[off++];
    off++;
    std::uint32_t chunk_u32 = 0;
    if (!mi::server::proto::ReadUint32(blob, off, chunk_u32) ||
        !mi::server::proto::ReadUint64(blob, off, original_size) ||
        off + 24 != kFileBlobV4BaseHeaderSize) {
      return false;
    }
    if (algo != kFileBlobAlgoRaw) {
      return false;
    }
    if (chunk_u32 == 0 || original_size == 0 ||
        original_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
      return false;
    }
    chunk_count = chunk_u32;
  }

  if (version == kFileBlobVersionV2 &&
      blob.size() < kFileBlobV2HeaderSize) {
    return false;
  }
  if (version == kFileBlobVersionV1) {
    flags = blob[off++];
    algo = blob[off++];
    off++;
    if (!mi::server::proto::ReadUint64(blob, off, original_size)) {
      return false;
    }
  }

  if (version == kFileBlobVersionV1 || version == kFileBlobVersionV2) {
    if ((flags & kFileBlobFlagDoubleCompression) == 0) {
      stage1_size = 0;
    }
    if (algo != kFileBlobAlgoRaw && algo != kFileBlobAlgoDeflate) {
      return false;
    }
    if (original_size == 0 ||
        original_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
      return false;
    }
    if (stage1_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
      return false;
    }

    if (stage2_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
      return false;
    }
    if (header_size + stage2_size != blob.size()) {
      return false;
    }

    const std::uint8_t* header = blob.data();
    const std::uint8_t* nonce = blob.data() + header_len;
    const std::uint8_t* mac = blob.data() + header_len + 24;
    const std::uint8_t* cipher = blob.data() + header_size;
    std::vector<std::uint8_t> stage2;
    stage2.resize(stage2_size);
    const int ok = crypto_aead_unlock(stage2.data(), mac, key.data(), nonce,
                                      header, header_len, cipher,
                                      static_cast<std::size_t>(stage2_size));
    if (ok != 0) {
      crypto_wipe(stage2.data(), stage2.size());
      return false;
    }

    if (algo == kFileBlobAlgoRaw) {
      out_plaintext = std::move(stage2);
      return true;
    }

    if ((flags & kFileBlobFlagDoubleCompression) == 0) {
      std::vector<std::uint8_t> plain;
      if (!DeflateDecompress(stage2.data(), stage2.size(),
                             static_cast<std::size_t>(original_size),
                             plain)) {
        crypto_wipe(stage2.data(), stage2.size());
        return false;
      }
      crypto_wipe(stage2.data(), stage2.size());
      out_plaintext = std::move(plain);
      return true;
    }

    if (algo != kFileBlobAlgoDeflate) {
      crypto_wipe(stage2.data(), stage2.size());
      return false;
    }
    if (stage1_size == 0 ||
        stage1_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
      crypto_wipe(stage2.data(), stage2.size());
      return false;
    }
    std::vector<std::uint8_t> stage1;
    if (!DeflateDecompress(stage2.data(), stage2.size(),
                           static_cast<std::size_t>(stage1_size),
                           stage1)) {
      crypto_wipe(stage2.data(), stage2.size());
      return false;
    }
    crypto_wipe(stage2.data(), stage2.size());
    std::vector<std::uint8_t> plain;
    if (!DeflateDecompress(stage1.data(), stage1.size(),
                           static_cast<std::size_t>(original_size),
                           plain)) {
      crypto_wipe(stage1.data(), stage1.size());
      return false;
    }
    crypto_wipe(stage1.data(), stage1.size());
    out_plaintext = std::move(plain);
    return true;
  }

  if (version == kFileBlobVersionV3) {
    std::size_t blob_off = kFileBlobV3PrefixSize;
    const std::uint64_t chunks =
        (original_size + chunk_size - 1) / chunk_size;
    std::array<std::uint8_t, 24> base_nonce{};
    std::memcpy(base_nonce.data(), blob.data() + off, base_nonce.size());

    std::vector<std::uint8_t> plain;
    plain.reserve(static_cast<std::size_t>(original_size));
    for (std::uint64_t idx = 0; idx < chunks; ++idx) {
      const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(
          chunk_size, original_size - plain.size()));
      if (blob_off + 16 + want > blob.size()) {
        crypto_wipe(plain.data(), plain.size());
        return false;
      }
      const std::uint8_t* mac = blob.data() + blob_off;
      const std::uint8_t* cipher = blob.data() + blob_off + 16;
      std::vector<std::uint8_t> piece;
      piece.resize(want);
      std::array<std::uint8_t, 24> nonce = base_nonce;
      for (int i = 0; i < 8; ++i) {
        nonce[16 + i] = static_cast<std::uint8_t>((idx >> (8 * i)) & 0xFF);
      }
      const int ok = crypto_aead_unlock(piece.data(), mac, key.data(),
                                        nonce.data(), blob.data(), header_len,
                                        cipher, want);
      if (ok != 0) {
        crypto_wipe(piece.data(), piece.size());
        crypto_wipe(plain.data(), plain.size());
        return false;
      }
      plain.insert(plain.end(), piece.begin(), piece.end());
      crypto_wipe(piece.data(), piece.size());
      blob_off += 16 + want;
    }
    out_plaintext = std::move(plain);
    return true;
  }

  if (version == kFileBlobVersionV4) {
    const std::size_t header_size =
        kFileBlobV4BaseHeaderSize + static_cast<std::size_t>(chunk_count) * 4u;
    if (header_size < kFileBlobV4BaseHeaderSize ||
        header_size > blob.size()) {
      return false;
    }
    std::array<std::uint8_t, 24> base_nonce{};
    std::memcpy(base_nonce.data(), blob.data() + off, base_nonce.size());

    std::vector<std::uint32_t> chunk_sizes;
    chunk_sizes.reserve(static_cast<std::size_t>(chunk_count));
    std::size_t table_off = kFileBlobV4BaseHeaderSize;
    for (std::uint32_t i = 0; i < chunk_count; ++i) {
      std::uint32_t size = 0;
      if (!mi::server::proto::ReadUint32(blob, table_off, size)) {
        return false;
      }
      if (size == 0 || size > (kE2eeBlobChunkBytes - 16u)) {
        return false;
      }
      chunk_sizes.push_back(size);
    }

    std::vector<std::uint8_t> plain;
    plain.reserve(static_cast<std::size_t>(original_size));
    std::uint64_t blob_off = header_size;
    for (std::uint32_t idx = 0; idx < chunk_count; ++idx) {
      const std::size_t record_len =
          16u + static_cast<std::size_t>(chunk_sizes[idx]);
      if (blob_off + record_len > blob.size()) {
        crypto_wipe(plain.data(), plain.size());
        return false;
      }
      const std::uint8_t* mac = blob.data() + blob_off;
      const std::uint8_t* cipher = blob.data() + blob_off + 16;
      std::vector<std::uint8_t> record;
      record.resize(chunk_sizes[idx]);
      std::array<std::uint8_t, 24> nonce = base_nonce;
      for (int i = 0; i < 8; ++i) {
        nonce[16 + i] = static_cast<std::uint8_t>((idx >> (8 * i)) & 0xFF);
      }
      const int ok = crypto_aead_unlock(record.data(), mac, key.data(),
                                        nonce.data(), blob.data(), header_size,
                                        cipher, record.size());
      if (ok != 0) {
        crypto_wipe(record.data(), record.size());
        crypto_wipe(plain.data(), plain.size());
        return false;
      }
      if (record.size() < 4) {
        crypto_wipe(record.data(), record.size());
        crypto_wipe(plain.data(), plain.size());
        return false;
      }
      std::uint32_t piece_len = static_cast<std::uint32_t>(record[0]) |
                                (static_cast<std::uint32_t>(record[1]) << 8) |
                                (static_cast<std::uint32_t>(record[2]) << 16) |
                                (static_cast<std::uint32_t>(record[3]) << 24);
      if (piece_len > record.size() - 4) {
        crypto_wipe(record.data(), record.size());
        crypto_wipe(plain.data(), plain.size());
        return false;
      }
      plain.insert(plain.end(), record.begin() + 4,
                   record.begin() + 4 + piece_len);
      crypto_wipe(record.data(), record.size());
      blob_off += record_len;
      if (plain.size() > static_cast<std::size_t>(original_size)) {
        crypto_wipe(plain.data(), plain.size());
        return false;
      }
    }
    if (plain.size() != static_cast<std::size_t>(original_size)) {
      crypto_wipe(plain.data(), plain.size());
      return false;
    }
    out_plaintext = std::move(plain);
    return true;
  }

  return false;
}

}  // namespace

bool DecryptFileBlobForTooling(const std::vector<std::uint8_t>& blob,
                               const std::array<std::uint8_t, 32>& key,
                               std::vector<std::uint8_t>& out_plaintext) {
  return DecryptFileBlob(blob, key, out_plaintext);
}
void StorageService::BestEffortPersistHistoryEnvelope(ClientCore& core, 
    bool is_group,
    bool outgoing,
    const std::string& conv_id,
    const std::string& sender,
    const std::vector<std::uint8_t>& envelope,
    HistoryStatus status,
    std::uint64_t timestamp_sec) const {
  if (!core.history_store_) {
    return;
  }
  const std::string saved_err = core.last_error_;
  std::string hist_err;
  (void)core.history_store_->AppendEnvelope(
      is_group, outgoing, conv_id, sender, envelope,
      static_cast<ChatHistoryStatus>(status), timestamp_sec, hist_err);
  core.last_error_ = saved_err;
}

void StorageService::BestEffortPersistHistoryStatus(ClientCore& core, 
    bool is_group,
    const std::string& conv_id,
    const std::array<std::uint8_t, 16>& msg_id,
    HistoryStatus status,
    std::uint64_t timestamp_sec) const {
  if (!core.history_store_) {
    return;
  }
  const std::string saved_err = core.last_error_;
  std::string hist_err;
  (void)core.history_store_->AppendStatusUpdate(
      is_group, conv_id, msg_id, static_cast<ChatHistoryStatus>(status),
      timestamp_sec, hist_err);
  core.last_error_ = saved_err;
}

void StorageService::BestEffortStoreAttachmentPreviewBytes(ClientCore& core, 
    const std::string& file_id,
    const std::string& file_name,
    std::uint64_t file_size,
    const std::vector<std::uint8_t>& bytes) const {
  if (!core.history_store_ || file_id.empty() || bytes.empty()) {
    return;
  }
  const std::string saved_err = core.last_error_;
  const std::size_t max_bytes = 256u * 1024u;
  const std::size_t take = std::min(bytes.size(), max_bytes);
  if (take == 0) {
    return;
  }
  std::vector<std::uint8_t> preview(bytes.begin(), bytes.begin() + take);
  std::string hist_err;
  (void)core.history_store_->StoreAttachmentPreview(file_id, file_name, file_size,
                                               preview, hist_err);
  crypto_wipe(preview.data(), preview.size());
  core.last_error_ = saved_err;
}

void StorageService::BestEffortStoreAttachmentPreviewFromPath(ClientCore& core, 
    const std::string& file_id,
    const std::string& file_name,
    std::uint64_t file_size,
    const std::filesystem::path& path) const {
  if (!core.history_store_ || file_id.empty() || path.empty()) {
    return;
  }
  const std::string saved_err = core.last_error_;
  const std::size_t max_bytes = 256u * 1024u;
  std::size_t want = max_bytes;
  if (file_size > 0 &&
      file_size <= static_cast<std::uint64_t>(
                       (std::numeric_limits<std::size_t>::max)())) {
    want = std::min<std::size_t>(max_bytes, static_cast<std::size_t>(file_size));
  }
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    core.last_error_ = saved_err;
    return;
  }
  std::vector<std::uint8_t> preview;
  preview.resize(want);
  ifs.read(reinterpret_cast<char*>(preview.data()),
           static_cast<std::streamsize>(preview.size()));
  const std::streamsize got = ifs.gcount();
  if (got <= 0) {
    crypto_wipe(preview.data(), preview.size());
    core.last_error_ = saved_err;
    return;
  }
  preview.resize(static_cast<std::size_t>(got));
  std::string hist_err;
  (void)core.history_store_->StoreAttachmentPreview(file_id, file_name, file_size,
                                               preview, hist_err);
  crypto_wipe(preview.data(), preview.size());
  core.last_error_ = saved_err;
}

void StorageService::WarmupHistoryOnStartup(ClientCore& core) const {
  if (!core.history_store_) {
    return;
  }
  const std::string saved_err = core.last_error_;
  std::vector<ChatHistoryMessage> msgs;
  std::string hist_err;
  (void)core.history_store_->ExportRecentSnapshot(20, 50, msgs, hist_err);
  core.last_error_ = saved_err;
}

void StorageService::FlushHistoryOnShutdown(ClientCore& core) const {
  if (!core.history_store_) {
    return;
  }
  const std::string saved_err = core.last_error_;
  std::string hist_err;
  (void)core.history_store_->Flush(hist_err);
  core.last_error_ = saved_err;
}

bool StorageService::DeleteChatHistory(ClientCore& core, const std::string& conv_id,
                                   bool is_group,
                                   bool delete_attachments,
                                   bool secure_wipe) const {
  core.last_error_.clear();
  if (!core.history_store_) {
    return true;
  }
  if (conv_id.empty()) {
    core.last_error_ = "conv id empty";
    return false;
  }
  std::string err;
  if (!core.history_store_->DeleteConversation(is_group, conv_id, delete_attachments,
                                          secure_wipe, err)) {
    core.last_error_ = err.empty() ? "history delete failed" : err;
    return false;
  }
  core.last_error_.clear();
  return true;
}

bool StorageService::DownloadChatFileToPath(ClientCore& core, 
    const ChatFileMessage& file,
    const std::filesystem::path& out_path,
    bool wipe_after_read,
    const std::function<void(std::uint64_t, std::uint64_t)>& on_progress) const {
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (file.file_id.empty()) {
    core.last_error_ = "file id empty";
    return false;
  }
  if (out_path.empty()) {
    core.last_error_ = "output path empty";
    return false;
  }

  if (file.file_size > (8u * 1024u * 1024u)) {
    const bool ok =
        core.DownloadE2eeFileBlobV3ToPath(file.file_id, file.file_key, out_path,
                                     wipe_after_read, on_progress);
    if (ok) {
      core.BestEffortStoreAttachmentPreviewFromPath(file.file_id, file.file_name,
                                               file.file_size, out_path);
    }
    return ok;
  }

  const std::size_t file_size_bytes = static_cast<std::size_t>(file.file_size);
  auto& pool = FileBlobChunkPool();
  mi::common::ScopedBuffer blob_buf(pool, file_size_bytes, false);
  auto& blob = blob_buf.get();
  if (!core.DownloadE2eeFileBlob(file.file_id, blob, wipe_after_read, on_progress)) {
    return false;
  }

  mi::common::ScopedBuffer plain_buf(pool, file_size_bytes, false);
  auto& plaintext = plain_buf.get();
  if (!DecryptFileBlob(blob, file.file_key, plaintext)) {
    core.last_error_ = "file decrypt failed";
    crypto_wipe(plaintext.data(), plaintext.size());
    return false;
  }
  core.BestEffortStoreAttachmentPreviewBytes(file.file_id, file.file_name,
                                        file.file_size, plaintext);

  std::error_code ec;
  const auto parent =
      out_path.has_parent_path() ? out_path.parent_path() : std::filesystem::path{};
  if (!parent.empty()) {
    pfs::CreateDirectories(parent, ec);
  }
  std::ofstream ofs(out_path, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    core.last_error_ = "open output file failed";
    crypto_wipe(plaintext.data(), plaintext.size());
    return false;
  }
  ofs.write(reinterpret_cast<const char*>(plaintext.data()),
            static_cast<std::streamsize>(plaintext.size()));
  if (!ofs) {
    core.last_error_ = "write output file failed";
    crypto_wipe(plaintext.data(), plaintext.size());
    return false;
  }
  ofs.close();
  crypto_wipe(plaintext.data(), plaintext.size());
  return true;
}

bool StorageService::DownloadChatFileToBytes(ClientCore& core, const ChatFileMessage& file,
                                        std::vector<std::uint8_t>& out_bytes,
                                        bool wipe_after_read) const {
  out_bytes.clear();
  core.last_error_.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (file.file_id.empty()) {
    core.last_error_ = "file id empty";
    return false;
  }

  const std::size_t blob_reserve =
      static_cast<std::size_t>(std::min<std::uint64_t>(
          file.file_size, static_cast<std::uint64_t>(kE2eeBlobChunkBytes)));
  auto& pool = FileBlobChunkPool();
  mi::common::ScopedBuffer blob_buf(pool, blob_reserve, false);
  auto& blob = blob_buf.get();
  if (!core.DownloadE2eeFileBlob(file.file_id, blob, wipe_after_read, nullptr)) {
    return false;
  }

  std::vector<std::uint8_t> plaintext;
  if (!DecryptFileBlob(blob, file.file_key, plaintext)) {
    core.last_error_ = "file decrypt failed";
    return false;
  }

  out_bytes = std::move(plaintext);
  core.BestEffortStoreAttachmentPreviewBytes(file.file_id, file.file_name,
                                        file.file_size, out_bytes);
  return true;
}

std::vector<ClientCore::HistoryEntry> StorageService::LoadChatHistory(ClientCore& core, 
    const std::string& conv_id, bool is_group, std::size_t limit) const {
  std::vector<HistoryEntry> out;
  core.last_error_.clear();
  if (!core.history_store_) {
    return out;
  }
  if (conv_id.empty()) {
    core.last_error_ = "conv id empty";
    return out;
  }

  std::vector<ChatHistoryMessage> msgs;
  std::string err;
  if (!core.history_store_->LoadConversation(is_group, conv_id, limit, msgs, err)) {
    core.last_error_ = err.empty() ? "history load failed" : err;
    return out;
  }

  out.reserve(msgs.size());
  for (auto& m : msgs) {
    HistoryEntry e;
    e.is_group = is_group;
    e.outgoing = m.outgoing;
    e.timestamp_sec = m.timestamp_sec;
    e.conv_id = conv_id;
    e.sender = m.sender;
    e.status = static_cast<HistoryStatus>(m.status);

    if (m.is_system) {
      e.kind = HistoryKind::kSystem;
      e.text_utf8 = std::move(m.system_text_utf8);
      out.push_back(std::move(e));
      continue;
    }

    const auto trySummary = [&]() -> bool {
      if (ApplyHistorySummary(m.summary, e)) {
        out.push_back(std::move(e));
        return true;
      }
      return false;
    };

    std::uint8_t type = 0;
    std::array<std::uint8_t, 16> msg_id{};
    std::size_t off = 0;
    if (!DecodeChatHeader(m.envelope, type, msg_id, off)) {
      (void)trySummary();
      continue;
    }
    e.message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

    if (type == kChatTypeText) {
      std::string text;
      if (!mi::server::proto::ReadString(m.envelope, off, text) ||
          off != m.envelope.size()) {
        (void)trySummary();
        continue;
      }
      e.kind = HistoryKind::kText;
      e.text_utf8 = std::move(text);
      out.push_back(std::move(e));
      continue;
    }

    if (type == kChatTypeRich) {
      RichDecoded rich;
      if (!DecodeChatRich(m.envelope, off, rich) || off != m.envelope.size()) {
        (void)trySummary();
        continue;
      }
      e.kind = HistoryKind::kText;
      e.text_utf8 = FormatRichAsText(rich);
      out.push_back(std::move(e));
      continue;
    }

    if (type == kChatTypeFile) {
      std::uint64_t file_size = 0;
      std::string file_name;
      std::string file_id;
      std::array<std::uint8_t, 32> file_key{};
      if (!DecodeChatFile(m.envelope, off, file_size, file_name, file_id,
                          file_key) ||
          off != m.envelope.size()) {
        (void)trySummary();
        continue;
      }
      e.kind = HistoryKind::kFile;
      e.file_id = std::move(file_id);
      e.file_key = file_key;
      e.file_name = std::move(file_name);
      e.file_size = file_size;
      out.push_back(std::move(e));
      continue;
    }

    if (type == kChatTypeSticker) {
      std::string sticker_id;
      if (!mi::server::proto::ReadString(m.envelope, off, sticker_id) ||
          off != m.envelope.size()) {
        (void)trySummary();
        continue;
      }
      e.kind = HistoryKind::kSticker;
      e.sticker_id = std::move(sticker_id);
      out.push_back(std::move(e));
      continue;
    }

    if (type == kChatTypeGroupText) {
      std::string group_id;
      std::string text;
      if (!mi::server::proto::ReadString(m.envelope, off, group_id) ||
          !mi::server::proto::ReadString(m.envelope, off, text) ||
          off != m.envelope.size()) {
        (void)trySummary();
        continue;
      }
      e.kind = HistoryKind::kText;
      e.text_utf8 = std::move(text);
      out.push_back(std::move(e));
      continue;
    }

    if (type == kChatTypeGroupFile) {
      std::string group_id;
      std::uint64_t file_size = 0;
      std::string file_name;
      std::string file_id;
      std::array<std::uint8_t, 32> file_key{};
      if (!DecodeChatGroupFile(m.envelope, off, group_id, file_size, file_name,
                               file_id, file_key) ||
          off != m.envelope.size()) {
        (void)trySummary();
        continue;
      }
      e.kind = HistoryKind::kFile;
      e.file_id = std::move(file_id);
      e.file_key = file_key;
      e.file_name = std::move(file_name);
      e.file_size = file_size;
      out.push_back(std::move(e));
      continue;
    }

    (void)trySummary();
  }
  return out;
}

bool StorageService::AddHistorySystemMessage(ClientCore& core, const std::string& conv_id,
                                        bool is_group,
                                        const std::string& text_utf8) const {
  core.last_error_.clear();
  if (!core.history_store_) {
    return true;
  }
  if (conv_id.empty()) {
    core.last_error_ = "conv id empty";
    return false;
  }
  if (text_utf8.empty()) {
    core.last_error_ = "system text empty";
    return false;
  }
  std::string err;
  if (!core.history_store_->AppendSystem(is_group, conv_id, text_utf8, NowUnixSeconds(),
                                    err)) {
    core.last_error_ = err.empty() ? "history write failed" : err;
    return false;
  }
  return true;
}

void StorageService::SetHistoryEnabled(ClientCore& core, bool enabled) const {
  core.history_enabled_ = enabled;
  if (!core.history_enabled_) {
    core.history_store_.reset();
    return;
  }
  if (core.history_store_ || core.username_.empty() || core.e2ee_state_dir_.empty()) {
    return;
  }
  auto store = std::make_unique<ChatHistoryStore>();
  std::string hist_err;
  if (store->Init(core.e2ee_state_dir_, core.username_, hist_err)) {
    core.history_store_ = std::move(store);
    core.WarmupHistoryOnStartup();
  } else {
    core.history_store_.reset();
  }
}

bool StorageService::ClearAllHistory(ClientCore& core, bool delete_attachments,
                                 bool secure_wipe,
                                 std::string& error) const {
  error.clear();
  core.last_error_.clear();
  if (core.username_.empty() || core.e2ee_state_dir_.empty()) {
    error = "history user empty";
    core.last_error_ = error;
    return false;
  }
  if (core.history_store_) {
    if (!core.history_store_->ClearAll(delete_attachments, secure_wipe, error)) {
      core.last_error_ = error.empty() ? "history clear failed" : error;
      return false;
    }
    core.history_store_.reset();
    return true;
  }
  auto store = std::make_unique<ChatHistoryStore>();
  if (!store->Init(core.e2ee_state_dir_, core.username_, error)) {
    core.last_error_ = error.empty() ? "history init failed" : error;
    return false;
  }
  if (!store->ClearAll(delete_attachments, secure_wipe, error)) {
    core.last_error_ = error.empty() ? "history clear failed" : error;
    return false;
  }
  core.last_error_.clear();
  return true;
}

bool StorageService::UploadE2eeFileBlob(ClientCore& core, const std::vector<std::uint8_t>& blob,
                                    std::string& out_file_id) const {
  out_file_id.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (blob.empty()) {
    core.last_error_ = "empty payload";
    return false;
  }
  if (blob.size() > kMaxChatFileBlobBytes) {
    core.last_error_ = "payload too large";
    return false;
  }

  if (blob.size() > (8u * 1024u * 1024u)) {
    std::string file_id;
    std::string upload_id;
    if (!core.StartE2eeFileBlobUpload(static_cast<std::uint64_t>(blob.size()), file_id,
                                 upload_id)) {
      if (core.last_error_.empty()) {
        core.last_error_ = "file upload start failed";
      }
      return false;
    }

    std::uint64_t off = 0;
    auto& pool = FileBlobChunkPool();
    mi::common::ScopedBuffer chunk_buf(pool, kE2eeBlobChunkBytes, false);
    auto& chunk = chunk_buf.get();
    while (off < static_cast<std::uint64_t>(blob.size())) {
      const std::size_t remaining =
          static_cast<std::size_t>(blob.size() - off);
      const std::size_t chunk_len =
          std::min<std::size_t>(remaining, kE2eeBlobChunkBytes);
      chunk.assign(blob.data() + off, blob.data() + off + chunk_len);

      std::uint64_t received = 0;
      if (!core.UploadE2eeFileBlobChunk(file_id, upload_id, off, chunk, received)) {
        if (core.last_error_.empty()) {
          core.last_error_ = "file upload chunk failed";
        }
        return false;
      }
      if (received != off + chunk_len) {
        core.last_error_ = "file upload chunk response invalid";
        return false;
      }
      off = received;
    }

    if (!core.FinishE2eeFileBlobUpload(file_id, upload_id,
                                  static_cast<std::uint64_t>(blob.size()))) {
      if (core.last_error_.empty()) {
        core.last_error_ = "file upload finish failed";
      }
      return false;
    }
    out_file_id = std::move(file_id);
    return true;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteBytes(blob, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kE2eeFileUpload, plain,
                        resp_payload)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "file upload failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "file upload response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ = server_err.empty() ? "file upload failed" : server_err;
    return false;
  }
  std::size_t off = 1;
  std::string file_id;
  std::uint64_t size = 0;
  if (!mi::server::proto::ReadString(resp_payload, off, file_id) ||
      !mi::server::proto::ReadUint64(resp_payload, off, size) ||
      off != resp_payload.size() || file_id.empty()) {
    core.last_error_ = "file upload response invalid";
    return false;
  }
  out_file_id = std::move(file_id);
  return true;
}

bool StorageService::DownloadE2eeFileBlob(ClientCore& core, 
    const std::string& file_id,
    std::vector<std::uint8_t>& out_blob,
    bool wipe_after_read,
    const std::function<void(std::uint64_t, std::uint64_t)>& on_progress) const {
  out_blob.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (file_id.empty()) {
    core.last_error_ = "file id empty";
    return false;
  }

  std::string download_id;
  std::uint64_t size = 0;
  if (!core.StartE2eeFileBlobDownload(file_id, wipe_after_read, download_id, size)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "file download start failed";
    }
    return false;
  }

  if (size == 0 || size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
    core.last_error_ = "file download response invalid";
    return false;
  }

  std::vector<std::uint8_t> blob = std::move(out_blob);
  blob.clear();
  blob.resize(static_cast<std::size_t>(size));
  if (on_progress) {
    on_progress(0, size);
  }

  std::uint64_t off = 0;
  bool eof = false;
  auto& pool = FileBlobChunkPool();
  mi::common::ScopedBuffer chunk_buf(pool, kE2eeBlobChunkBytes, false);
  auto& chunk = chunk_buf.get();
  while (off < size) {
    const std::uint64_t remaining = size - off;
    const std::uint32_t max_len =
        static_cast<std::uint32_t>(
            std::min<std::uint64_t>(remaining, kE2eeBlobChunkBytes));
    bool chunk_eof = false;
    if (!core.DownloadE2eeFileBlobChunk(file_id, download_id, off, max_len, chunk,
                                   chunk_eof)) {
      if (core.last_error_.empty()) {
        core.last_error_ = "file download chunk failed";
      }
      return false;
    }
    if (chunk.empty()) {
      core.last_error_ = "file download chunk empty";
      return false;
    }
    const std::size_t chunk_size = chunk.size();
    if (off + static_cast<std::uint64_t>(chunk_size) >
        static_cast<std::uint64_t>(blob.size())) {
      core.last_error_ = "file download chunk invalid";
      return false;
    }
    std::memcpy(blob.data() + off, chunk.data(), chunk_size);
    off += static_cast<std::uint64_t>(chunk.size());
    eof = chunk_eof;
    if (on_progress) {
      on_progress(off, size);
    }
    if (eof) {
      break;
    }
  }

  if (off != size || !eof || blob.size() != static_cast<std::size_t>(size)) {
    core.last_error_ = "file download incomplete";
    return false;
  }

  out_blob = std::move(blob);
  return true;
}

bool StorageService::StartE2eeFileBlobUpload(ClientCore& core, std::uint64_t expected_size,
                                        std::string& out_file_id,
                                        std::string& out_upload_id) const {
  out_file_id.clear();
  out_upload_id.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (expected_size == 0 ||
      expected_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
    core.last_error_ = "payload too large";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteUint64(expected_size, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kE2eeFileUploadStart, plain,
                        resp_payload)) {
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "file upload start response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ =
        server_err.empty() ? "file upload start failed" : server_err;
    return false;
  }

  std::size_t off = 1;
  std::string file_id;
  std::string upload_id;
  if (!mi::server::proto::ReadString(resp_payload, off, file_id) ||
      !mi::server::proto::ReadString(resp_payload, off, upload_id) ||
      off != resp_payload.size() || file_id.empty() || upload_id.empty()) {
    core.last_error_ = "file upload start response invalid";
    return false;
  }
  out_file_id = std::move(file_id);
  out_upload_id = std::move(upload_id);
  return true;
}

bool StorageService::UploadE2eeFileBlobChunk(ClientCore& core, const std::string& file_id,
                                        const std::string& upload_id,
                                        std::uint64_t offset,
                                        const std::vector<std::uint8_t>& chunk,
                                        std::uint64_t& out_bytes_received) const {
  out_bytes_received = 0;
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (file_id.empty() || upload_id.empty()) {
    core.last_error_ = "invalid session";
    return false;
  }
  if (chunk.empty()) {
    core.last_error_ = "empty payload";
    return false;
  }
  if (chunk.size() > kE2eeBlobChunkBytes) {
    core.last_error_ = "chunk too large";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(file_id, plain);
  mi::server::proto::WriteString(upload_id, plain);
  mi::server::proto::WriteUint64(offset, plain);
  mi::server::proto::WriteBytes(chunk, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kE2eeFileUploadChunk, plain,
                        resp_payload)) {
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "file upload chunk response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ =
        server_err.empty() ? "file upload chunk failed" : server_err;
    return false;
  }

  std::size_t off = 1;
  std::uint64_t received = 0;
  if (!mi::server::proto::ReadUint64(resp_payload, off, received) ||
      off != resp_payload.size()) {
    core.last_error_ = "file upload chunk response invalid";
    return false;
  }
  out_bytes_received = received;
  return true;
}

bool StorageService::FinishE2eeFileBlobUpload(ClientCore& core, const std::string& file_id,
                                         const std::string& upload_id,
                                         std::uint64_t total_size) const {
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (file_id.empty() || upload_id.empty()) {
    core.last_error_ = "invalid session";
    return false;
  }
  if (total_size == 0 ||
      total_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
    core.last_error_ = "payload too large";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(file_id, plain);
  mi::server::proto::WriteString(upload_id, plain);
  mi::server::proto::WriteUint64(total_size, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kE2eeFileUploadFinish, plain,
                        resp_payload)) {
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "file upload finish response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ =
        server_err.empty() ? "file upload finish failed" : server_err;
    return false;
  }
  std::size_t off = 1;
  std::uint64_t size = 0;
  if (!mi::server::proto::ReadUint64(resp_payload, off, size) ||
      off != resp_payload.size() || size != total_size) {
    core.last_error_ = "file upload finish response invalid";
    return false;
  }
  return true;
}

bool StorageService::StartE2eeFileBlobDownload(ClientCore& core, const std::string& file_id,
                                          bool wipe_after_read,
                                          std::string& out_download_id,
                                          std::uint64_t& out_size) const {
  out_download_id.clear();
  out_size = 0;
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (file_id.empty()) {
    core.last_error_ = "file id empty";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(file_id, plain);
  plain.push_back(wipe_after_read ? 1 : 0);

  std::vector<std::uint8_t> resp_payload;
  if (!core.ProcessEncrypted(mi::server::FrameType::kE2eeFileDownloadStart, plain,
                        resp_payload)) {
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "file download start response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ =
        server_err.empty() ? "file download start failed" : server_err;
    return false;
  }
  std::size_t off = 1;
  std::string download_id;
  std::uint64_t size = 0;
  if (!mi::server::proto::ReadString(resp_payload, off, download_id) ||
      !mi::server::proto::ReadUint64(resp_payload, off, size) ||
      off != resp_payload.size() || download_id.empty()) {
    core.last_error_ = "file download start response invalid";
    return false;
  }

  out_download_id = std::move(download_id);
  out_size = size;
  return true;
}

bool StorageService::DownloadE2eeFileBlobChunk(ClientCore& core, const std::string& file_id,
                                          const std::string& download_id,
                                          std::uint64_t offset,
                                          std::uint32_t max_len,
                                          std::vector<std::uint8_t>& out_chunk,
                                          bool& out_eof) const {
  out_chunk.clear();
  out_eof = false;
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (file_id.empty() || download_id.empty()) {
    core.last_error_ = "invalid session";
    return false;
  }
  if (max_len == 0 || max_len > kE2eeBlobChunkBytes) {
    max_len = kE2eeBlobChunkBytes;
  }
  out_chunk.reserve(max_len);

  auto& pool = FileBlobChunkPool();
  const std::size_t plain_hint =
      file_id.size() + download_id.size() + 32u;
  mi::common::ScopedBuffer plain_buf(pool, plain_hint, false);
  auto& plain = plain_buf.get();
  plain.clear();
  mi::server::proto::WriteString(file_id, plain);
  mi::server::proto::WriteString(download_id, plain);
  mi::server::proto::WriteUint64(offset, plain);
  mi::server::proto::WriteUint32(max_len, plain);

  std::size_t resp_hint = static_cast<std::size_t>(max_len);
  if (resp_hint <= (kE2eeBlobChunkBytes - 64u)) {
    resp_hint += 64u;
  }
  mi::common::ScopedBuffer resp_buf(pool, resp_hint, false);
  auto& resp_payload = resp_buf.get();
  resp_payload.clear();
  if (!core.ProcessEncrypted(mi::server::FrameType::kE2eeFileDownloadChunk, plain,
                        resp_payload)) {
    return false;
  }
  if (resp_payload.empty()) {
    core.last_error_ = "file download chunk response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    core.last_error_ =
        server_err.empty() ? "file download chunk failed" : server_err;
    return false;
  }

  std::size_t off = 1;
  std::uint64_t resp_off = 0;
  if (!mi::server::proto::ReadUint64(resp_payload, off, resp_off) ||
      off >= resp_payload.size()) {
    core.last_error_ = "file download chunk response invalid";
    return false;
  }
  const bool eof = resp_payload[off++] != 0;
  if (!mi::server::proto::ReadBytes(resp_payload, off, out_chunk) ||
      off != resp_payload.size()) {
    core.last_error_ = "file download chunk response invalid";
    return false;
  }
  if (resp_off != offset) {
    core.last_error_ = "file download chunk response invalid";
    return false;
  }
  if (out_chunk.size() > max_len) {
    core.last_error_ = "file download chunk response invalid";
    return false;
  }

  out_eof = eof;
  return true;
}

bool StorageService::UploadE2eeFileBlobV3FromPath(ClientCore& core, 
    const std::filesystem::path& file_path, std::uint64_t plaintext_size,
    const std::array<std::uint8_t, 32>& file_key, std::string& out_file_id) const {
  out_file_id.clear();
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (file_path.empty()) {
    core.last_error_ = "file path empty";
    return false;
  }
  if (plaintext_size == 0 ||
      plaintext_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
    core.last_error_ = "file too large";
    return false;
  }

  const std::uint64_t chunks =
      (plaintext_size + kFileBlobV4PlainChunkBytes - 1) / kFileBlobV4PlainChunkBytes;
  if (chunks == 0 || chunks > (1ull << 31) ||
      chunks > static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)())) {
    core.last_error_ = "file size invalid";
    return false;
  }
  std::vector<std::uint32_t> chunk_sizes;
  chunk_sizes.reserve(static_cast<std::size_t>(chunks));
  std::uint32_t max_chunk_size = 0;
  std::uint64_t remaining = plaintext_size;
  std::uint64_t payload_bytes = 0;
  for (std::uint64_t idx = 0; idx < chunks; ++idx) {
    const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(
        remaining, kFileBlobV4PlainChunkBytes));
    const std::size_t min_len = want + 4;
    const std::size_t target_len = SelectFileChunkTarget(min_len);
    if (target_len == 0) {
      core.last_error_ = "file chunk size invalid";
      return false;
    }
    chunk_sizes.push_back(static_cast<std::uint32_t>(target_len));
    if (target_len > max_chunk_size) {
      max_chunk_size = static_cast<std::uint32_t>(target_len);
    }
    payload_bytes += 16u + static_cast<std::uint64_t>(target_len);
    remaining -= want;
  }
  const std::size_t header_size =
      kFileBlobV4BaseHeaderSize + chunk_sizes.size() * 4;
  const std::uint64_t blob_size =
      static_cast<std::uint64_t>(header_size) + payload_bytes;
  if (blob_size == 0 ||
      blob_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
    core.last_error_ = "payload too large";
    return false;
  }

  std::vector<std::uint8_t> header;
  header.reserve(header_size);
  header.insert(header.end(), kFileBlobMagic,
                kFileBlobMagic + sizeof(kFileBlobMagic));
  header.push_back(kFileBlobVersionV4);
  header.push_back(0);  // flags
  header.push_back(kFileBlobAlgoRaw);
  header.push_back(0);  // reserved
  mi::server::proto::WriteUint32(static_cast<std::uint32_t>(chunks), header);
  mi::server::proto::WriteUint64(plaintext_size, header);
  std::array<std::uint8_t, 24> base_nonce{};
  if (!RandomBytes(base_nonce.data(), base_nonce.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  header.insert(header.end(), base_nonce.begin(), base_nonce.end());
  for (const auto chunk_len : chunk_sizes) {
    mi::server::proto::WriteUint32(chunk_len, header);
  }
  if (header.size() != header_size) {
    core.last_error_ = "blob header invalid";
    return false;
  }

  std::string file_id;
  std::string upload_id;
  if (!core.StartE2eeFileBlobUpload(blob_size, file_id, upload_id)) {
    return false;
  }

  std::uint64_t off = 0;
  {
    std::uint64_t received = 0;
    if (!core.UploadE2eeFileBlobChunk(file_id, upload_id, off, header, received)) {
      return false;
    }
    if (received != header.size()) {
      core.last_error_ = "file upload chunk response invalid";
      return false;
    }
    off = received;
  }

  std::ifstream ifs(file_path, std::ios::binary);
  if (!ifs) {
    core.last_error_ = "open file failed";
    return false;
  }

  auto& pool = FileBlobChunkPool();
  mi::common::ScopedBuffer plain_buf(pool, kFileBlobV4PlainChunkBytes, false);
  auto& plain = plain_buf.get();
  plain.resize(kFileBlobV4PlainChunkBytes);
  mi::common::ScopedBuffer padded_buf(
      pool, static_cast<std::size_t>(max_chunk_size), false);
  auto& padded = padded_buf.get();
  padded.reserve(max_chunk_size);
  mi::common::ScopedBuffer record_buf(
      pool, static_cast<std::size_t>(16u + max_chunk_size), false);
  auto& record = record_buf.get();
  record.reserve(static_cast<std::size_t>(16u + max_chunk_size));
  remaining = plaintext_size;
  for (std::uint64_t idx = 0; idx < chunks; ++idx) {
    const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(
        remaining, kFileBlobV4PlainChunkBytes));
    ifs.read(reinterpret_cast<char*>(plain.data()),
             static_cast<std::streamsize>(want));
    if (!ifs) {
      core.last_error_ = "read file failed";
      crypto_wipe(plain.data(), plain.size());
      return false;
    }

    const std::uint32_t target_len =
        chunk_sizes[static_cast<std::size_t>(idx)];
    if (target_len < 4u + want) {
      core.last_error_ = "file chunk size invalid";
      crypto_wipe(plain.data(), plain.size());
      return false;
    }
    padded.resize(target_len);
    padded[0] = static_cast<std::uint8_t>(want & 0xFF);
    padded[1] = static_cast<std::uint8_t>((want >> 8) & 0xFF);
    padded[2] = static_cast<std::uint8_t>((want >> 16) & 0xFF);
    padded[3] = static_cast<std::uint8_t>((want >> 24) & 0xFF);
    if (want > 0) {
      std::memcpy(padded.data() + 4, plain.data(), want);
    }
    const std::size_t pad_len = padded.size() - 4 - want;
    if (pad_len > 0 &&
        !RandomBytes(padded.data() + 4 + want, pad_len)) {
      core.last_error_ = "rng failed";
      crypto_wipe(plain.data(), plain.size());
      crypto_wipe(padded.data(), padded.size());
      return false;
    }

    record.resize(16u + padded.size());
    std::array<std::uint8_t, 24> nonce = base_nonce;
    for (int i = 0; i < 8; ++i) {
      nonce[16 + i] = static_cast<std::uint8_t>((idx >> (8 * i)) & 0xFF);
    }
    crypto_aead_lock(record.data() + 16, record.data(), file_key.data(),
                     nonce.data(), header.data(), header.size(), padded.data(),
                     padded.size());
    crypto_wipe(plain.data(), want);
    crypto_wipe(padded.data(), padded.size());

    std::uint64_t received = 0;
    if (!core.UploadE2eeFileBlobChunk(file_id, upload_id, off, record, received)) {
      return false;
    }
    if (received != off + record.size()) {
      core.last_error_ = "file upload chunk response invalid";
      return false;
    }
    off = received;

    remaining -= want;
  }
  crypto_wipe(plain.data(), plain.size());

  if (!core.FinishE2eeFileBlobUpload(file_id, upload_id, blob_size)) {
    return false;
  }

  out_file_id = std::move(file_id);
  return true;
}

bool StorageService::DownloadE2eeFileBlobV3ToPath(ClientCore& core, 
    const std::string& file_id, const std::array<std::uint8_t, 32>& file_key,
    const std::filesystem::path& out_path, bool wipe_after_read,
    const std::function<void(std::uint64_t, std::uint64_t)>& on_progress) const {
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (file_id.empty()) {
    core.last_error_ = "file id empty";
    return false;
  }
  if (out_path.empty()) {
    core.last_error_ = "output path empty";
    return false;
  }

  std::string download_id;
  std::uint64_t blob_size = 0;
  if (!core.StartE2eeFileBlobDownload(file_id, wipe_after_read, download_id,
                                 blob_size)) {
    return false;
  }
  if (blob_size < kFileBlobV3PrefixSize + 16 + 1 ||
      blob_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
    core.last_error_ = "file download response invalid";
    return false;
  }

  auto& pool = FileBlobChunkPool();
  mi::common::ScopedBuffer header_buf(pool, kFileBlobV3PrefixSize, false);
  auto& header = header_buf.get();
  bool eof = false;
  if (!core.DownloadE2eeFileBlobChunk(
          file_id, download_id, 0,
          static_cast<std::uint32_t>(kFileBlobV3PrefixSize), header, eof)) {
    return false;
  }
  if (header.size() != kFileBlobV3PrefixSize) {
    core.last_error_ = "file blob header invalid";
    return false;
  }
  if (std::memcmp(header.data(), kFileBlobMagic, sizeof(kFileBlobMagic)) != 0) {
    core.last_error_ = "file blob header invalid";
    return false;
  }
  const std::uint8_t version = header[sizeof(kFileBlobMagic)];
  if (version != kFileBlobVersionV3 && version != kFileBlobVersionV4) {
    core.last_error_ = "file blob version mismatch";
    return false;
  }

  std::size_t h = sizeof(kFileBlobMagic) + 1;
  const std::uint8_t flags = header[h++];
  const std::uint8_t algo = header[h++];
  h++;  // reserved

  if (version == kFileBlobVersionV3) {
    std::uint32_t chunk_size = 0;
    std::uint64_t original_size = 0;
    if (!mi::server::proto::ReadUint32(header, h, chunk_size) ||
        !mi::server::proto::ReadUint64(header, h, original_size) ||
        h + 24 != header.size()) {
      core.last_error_ = "file blob header invalid";
      return false;
    }
    (void)flags;
    if (algo != kFileBlobAlgoRaw || chunk_size == 0 || original_size == 0 ||
        chunk_size > (kE2eeBlobChunkBytes - 16u) ||
        original_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
      core.last_error_ = "file blob header invalid";
      return false;
    }

    std::array<std::uint8_t, 24> base_nonce{};
    std::memcpy(base_nonce.data(), header.data() + h, base_nonce.size());

    const std::uint64_t chunks = (original_size + chunk_size - 1) / chunk_size;
    const std::uint64_t overhead = chunks * 16u;
    if (chunks != 0 && overhead / chunks != 16u) {
      core.last_error_ = "file blob size mismatch";
      return false;
    }
    if (static_cast<std::uint64_t>(kFileBlobV3PrefixSize) >
        (std::numeric_limits<std::uint64_t>::max)() - overhead - original_size) {
      core.last_error_ = "file blob size mismatch";
      return false;
    }
    const std::uint64_t expect =
        static_cast<std::uint64_t>(kFileBlobV3PrefixSize) + overhead +
        original_size;
    if (expect != blob_size) {
      core.last_error_ = "file blob size mismatch";
      return false;
    }

    std::error_code ec;
    const auto parent = out_path.has_parent_path()
                            ? out_path.parent_path()
                            : std::filesystem::path{};
    if (!parent.empty()) {
      pfs::CreateDirectories(parent, ec);
    }
    const auto temp_path = out_path.string() + ".part";
    std::ofstream ofs(temp_path, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      core.last_error_ = "open output file failed";
      return false;
    }

    std::uint64_t blob_off = kFileBlobV3PrefixSize;
    std::uint64_t written = 0;
    mi::common::ScopedBuffer record_buf(
        pool, static_cast<std::size_t>(16u + chunk_size), false);
    auto& record = record_buf.get();
    mi::common::ScopedBuffer plain_buf(
        pool, static_cast<std::size_t>(chunk_size), false);
    auto& plain = plain_buf.get();
    std::array<std::uint8_t, 24> nonce = base_nonce;
    if (on_progress) {
      on_progress(0, original_size);
    }
    for (std::uint64_t idx = 0; idx < chunks; ++idx) {
      const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(
          chunk_size, original_size - written));
      const std::uint32_t record_len =
          static_cast<std::uint32_t>(16 + want);
      if (blob_off > blob_size ||
          static_cast<std::uint64_t>(record_len) > (blob_size - blob_off)) {
        core.last_error_ = "file download chunk invalid";
        return false;
      }
      bool record_eof = false;
      if (!core.DownloadE2eeFileBlobChunk(file_id, download_id, blob_off, record_len,
                                     record, record_eof)) {
        crypto_wipe(record.data(), record.size());
        return false;
      }
      if (record.size() != record_len) {
        crypto_wipe(record.data(), record.size());
        core.last_error_ = "file download chunk invalid";
        return false;
      }

      const bool is_last = (idx + 1 == chunks);
      if (record_eof && !is_last) {
        crypto_wipe(record.data(), record.size());
        core.last_error_ = "file download chunk invalid";
        return false;
      }

      for (int i = 0; i < 8; ++i) {
        nonce[16 + i] = static_cast<std::uint8_t>((idx >> (8 * i)) & 0xFF);
      }

      plain.resize(want);
      const std::uint8_t* mac = record.data();
      const std::uint8_t* cipher = record.data() + 16;
      const int ok = crypto_aead_unlock(plain.data(), mac, file_key.data(),
                                        nonce.data(), header.data(), header.size(),
                                        cipher, want);
      crypto_wipe(record.data(), record.size());
      if (ok != 0) {
        crypto_wipe(plain.data(), plain.size());
        core.last_error_ = "file decrypt failed";
        return false;
      }

      ofs.write(reinterpret_cast<const char*>(plain.data()),
                static_cast<std::streamsize>(plain.size()));
      crypto_wipe(plain.data(), plain.size());
      if (!ofs) {
        core.last_error_ = "write output file failed";
        return false;
      }

      blob_off += record_len;
      written += want;
      eof = record_eof;
      if (on_progress) {
        on_progress(written, original_size);
      }
    }
    ofs.close();
    if (written != original_size || blob_off != blob_size || !eof) {
      core.last_error_ = "file download incomplete";
      pfs::Remove(temp_path, ec);
      return false;
    }

    pfs::Remove(out_path, ec);
    pfs::Rename(temp_path, out_path, ec);
    if (ec) {
      pfs::Remove(temp_path, ec);
      core.last_error_ = "finalize output failed";
      return false;
    }
    return true;
  }

  std::uint32_t chunk_count = 0;
  std::uint64_t original_size = 0;
  if (!mi::server::proto::ReadUint32(header, h, chunk_count) ||
      !mi::server::proto::ReadUint64(header, h, original_size) ||
      h + 24 != header.size()) {
    core.last_error_ = "file blob header invalid";
    return false;
  }
  (void)flags;
  if (algo != kFileBlobAlgoRaw || chunk_count == 0 || original_size == 0 ||
      original_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
    core.last_error_ = "file blob header invalid";
    return false;
  }
  const std::uint64_t expected_chunks =
      (original_size + kFileBlobV4PlainChunkBytes - 1) / kFileBlobV4PlainChunkBytes;
  if (expected_chunks == 0 ||
      expected_chunks >
          static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)()) ||
      chunk_count != static_cast<std::uint32_t>(expected_chunks)) {
    core.last_error_ = "file blob header invalid";
    return false;
  }

  std::array<std::uint8_t, 24> base_nonce{};
  std::memcpy(base_nonce.data(), header.data() + h, base_nonce.size());

  if (static_cast<std::size_t>(chunk_count) >
      ((std::numeric_limits<std::size_t>::max)() - kFileBlobV4BaseHeaderSize) /
          4u) {
    core.last_error_ = "file blob header invalid";
    return false;
  }
  const std::size_t header_size =
      kFileBlobV4BaseHeaderSize + static_cast<std::size_t>(chunk_count) * 4u;
  if (header_size < kFileBlobV4BaseHeaderSize ||
      header_size > static_cast<std::size_t>(blob_size)) {
    core.last_error_ = "file blob header invalid";
    return false;
  }
  if (header_size > header.size()) {
    if (header.capacity() < header_size) {
      mi::common::ScopedBuffer header_expand(pool, header_size, false);
      auto& header_copy = header_expand.get();
      header_copy.resize(header.size());
      if (!header.empty()) {
        std::memcpy(header_copy.data(), header.data(), header.size());
      }
      header.swap(header_copy);
    }
    const std::size_t need = header_size - header.size();
    if (need > kE2eeBlobChunkBytes) {
      core.last_error_ = "file blob header invalid";
      return false;
    }
    mi::common::ScopedBuffer tail_buf(pool, need, false);
    auto& tail = tail_buf.get();
    bool tail_eof = false;
    if (!core.DownloadE2eeFileBlobChunk(
            file_id, download_id, header.size(),
            static_cast<std::uint32_t>(need), tail, tail_eof)) {
      return false;
    }
    if (tail.size() != need) {
      core.last_error_ = "file blob header invalid";
      return false;
    }
    if (tail_eof) {
      core.last_error_ = "file blob header invalid";
      return false;
    }
    const std::size_t have = header.size();
    header.resize(header_size);
    std::memcpy(header.data() + have, tail.data(), tail.size());
  }
  if (header.size() != header_size) {
    core.last_error_ = "file blob header invalid";
    return false;
  }

  std::vector<std::uint32_t> chunk_sizes;
  chunk_sizes.reserve(chunk_count);
  std::uint64_t payload_expect = 0;
  std::uint32_t max_chunk_len = 0;
  std::size_t table_off = kFileBlobV4BaseHeaderSize;
  for (std::uint32_t i = 0; i < chunk_count; ++i) {
    std::uint32_t chunk_len = 0;
    if (!mi::server::proto::ReadUint32(header, table_off, chunk_len)) {
      core.last_error_ = "file blob header invalid";
      return false;
    }
    if (chunk_len < 4u || chunk_len > (kE2eeBlobChunkBytes - 16u)) {
      core.last_error_ = "file blob header invalid";
      return false;
    }
    chunk_sizes.push_back(chunk_len);
    if (chunk_len > max_chunk_len) {
      max_chunk_len = chunk_len;
    }
    const std::uint64_t add = 16u + static_cast<std::uint64_t>(chunk_len);
    if (payload_expect > (std::numeric_limits<std::uint64_t>::max)() - add) {
      core.last_error_ = "file blob header invalid";
      return false;
    }
    payload_expect += add;
  }
  if (table_off != header.size()) {
    core.last_error_ = "file blob header invalid";
    return false;
  }
  if (payload_expect >
      (std::numeric_limits<std::uint64_t>::max)() -
          static_cast<std::uint64_t>(header_size)) {
    core.last_error_ = "file blob size mismatch";
    return false;
  }
  const std::uint64_t expect =
      static_cast<std::uint64_t>(header_size) + payload_expect;
  if (expect != blob_size) {
    core.last_error_ = "file blob size mismatch";
    return false;
  }

  std::error_code ec;
  const auto parent = out_path.has_parent_path()
                          ? out_path.parent_path()
                          : std::filesystem::path{};
  if (!parent.empty()) {
    pfs::CreateDirectories(parent, ec);
  }
  const auto temp_path = out_path.string() + ".part";
  std::ofstream ofs(temp_path, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    core.last_error_ = "open output file failed";
    return false;
  }

  std::uint64_t blob_off = header_size;
  std::uint64_t written = 0;
  mi::common::ScopedBuffer record_buf(
      pool, static_cast<std::size_t>(16u + max_chunk_len), false);
  auto& record = record_buf.get();
  mi::common::ScopedBuffer plain_buf(
      pool, static_cast<std::size_t>(max_chunk_len), false);
  auto& plain = plain_buf.get();
  std::array<std::uint8_t, 24> nonce = base_nonce;
  if (on_progress) {
    on_progress(0, original_size);
  }
  const std::uint64_t total_chunks =
      static_cast<std::uint64_t>(chunk_sizes.size());
  for (std::uint64_t idx = 0; idx < total_chunks; ++idx) {
    const std::uint32_t chunk_len = chunk_sizes[static_cast<std::size_t>(idx)];
    const std::uint32_t record_len = static_cast<std::uint32_t>(16u + chunk_len);
    if (blob_off > blob_size ||
        static_cast<std::uint64_t>(record_len) > (blob_size - blob_off)) {
      core.last_error_ = "file download chunk invalid";
      return false;
    }
    bool record_eof = false;
    if (!core.DownloadE2eeFileBlobChunk(file_id, download_id, blob_off, record_len,
                                   record, record_eof)) {
      crypto_wipe(record.data(), record.size());
      return false;
    }
    if (record.size() != record_len) {
      crypto_wipe(record.data(), record.size());
      core.last_error_ = "file download chunk invalid";
      return false;
    }

    const bool is_last = (idx + 1 == total_chunks);
    if (record_eof && !is_last) {
      crypto_wipe(record.data(), record.size());
      core.last_error_ = "file download chunk invalid";
      return false;
    }

    for (int i = 0; i < 8; ++i) {
      nonce[16 + i] = static_cast<std::uint8_t>((idx >> (8 * i)) & 0xFF);
    }

    plain.resize(chunk_len);
    const std::uint8_t* mac = record.data();
    const std::uint8_t* cipher = record.data() + 16;
    const int ok = crypto_aead_unlock(plain.data(), mac, file_key.data(),
                                      nonce.data(), header.data(), header.size(),
                                      cipher, chunk_len);
    crypto_wipe(record.data(), record.size());
    if (ok != 0) {
      crypto_wipe(plain.data(), plain.size());
      core.last_error_ = "file decrypt failed";
      return false;
    }
    if (plain.size() < 4) {
      crypto_wipe(plain.data(), plain.size());
      core.last_error_ = "file blob chunk invalid";
      return false;
    }
    const std::uint32_t actual_len =
        static_cast<std::uint32_t>(plain[0]) |
        (static_cast<std::uint32_t>(plain[1]) << 8) |
        (static_cast<std::uint32_t>(plain[2]) << 16) |
        (static_cast<std::uint32_t>(plain[3]) << 24);
    if (actual_len > (plain.size() - 4) ||
        actual_len > kFileBlobV4PlainChunkBytes ||
        written + actual_len > original_size) {
      crypto_wipe(plain.data(), plain.size());
      core.last_error_ = "file blob chunk invalid";
      return false;
    }

    ofs.write(reinterpret_cast<const char*>(plain.data() + 4),
              static_cast<std::streamsize>(actual_len));
    crypto_wipe(plain.data(), plain.size());
    if (!ofs) {
      core.last_error_ = "write output file failed";
      return false;
    }

    blob_off += record_len;
    written += actual_len;
    eof = record_eof;
    if (on_progress) {
      on_progress(written, original_size);
    }
  }
  ofs.close();
  if (written != original_size || blob_off != blob_size || !eof) {
    core.last_error_ = "file download incomplete";
    pfs::Remove(temp_path, ec);
    return false;
  }

  pfs::Remove(out_path, ec);
  pfs::Rename(temp_path, out_path, ec);
  if (ec) {
    pfs::Remove(temp_path, ec);
    core.last_error_ = "finalize output failed";
    return false;
  }

  return true;
}

bool StorageService::UploadChatFileFromPath(ClientCore& core, const std::filesystem::path& file_path,
                                       std::uint64_t file_size,
                                       const std::string& file_name,
                                       std::array<std::uint8_t, 32>& out_file_key,
                                       std::string& out_file_id) const {
  out_file_id.clear();
  out_file_key.fill(0);
  if (!core.EnsureChannel()) {
    core.last_error_ = "not logged in";
    return false;
  }
  if (file_path.empty()) {
    core.last_error_ = "file not found";
    return false;
  }
  if (file_size == 0 ||
      file_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
    core.last_error_ = "file too large";
    return false;
  }

  if (!RandomBytes(out_file_key.data(), out_file_key.size())) {
    core.last_error_ = "rng failed";
    return false;
  }

  if (file_size > (8u * 1024u * 1024u)) {
    const bool ok = core.UploadE2eeFileBlobV3FromPath(file_path, file_size,
                                                 out_file_key, out_file_id);
    if (ok) {
      core.BestEffortStoreAttachmentPreviewFromPath(out_file_id, file_name,
                                               file_size, file_path);
    }
    return ok;
  }

  if (file_size > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
    core.last_error_ = "file too large";
    return false;
  }

  const std::size_t file_size_bytes = static_cast<std::size_t>(file_size);
  auto& pool = FileBlobChunkPool();

  std::ifstream ifs(file_path, std::ios::binary);
  if (!ifs) {
    core.last_error_ = "open file failed";
    return false;
  }

  mi::common::ScopedBuffer plain_buf(pool, file_size_bytes, false);
  auto& plaintext = plain_buf.get();
  plaintext.resize(file_size_bytes);
  ifs.read(reinterpret_cast<char*>(plaintext.data()),
           static_cast<std::streamsize>(plaintext.size()));
  if (!ifs) {
    crypto_wipe(plaintext.data(), plaintext.size());
    core.last_error_ = "read file failed";
    return false;
  }

  std::vector<std::uint8_t> preview;
  const std::size_t max_preview = 256u * 1024u;
  const std::size_t take = std::min(plaintext.size(), max_preview);
  if (take > 0) {
    preview.assign(plaintext.begin(), plaintext.begin() + take);
  }

  mi::common::ScopedBuffer blob_buf(pool, file_size_bytes, false);
  auto& blob = blob_buf.get();
  const bool encrypted_ok =
      EncryptFileBlobAdaptive(plaintext, out_file_key, file_name, blob);
  crypto_wipe(plaintext.data(), plaintext.size());
  plaintext.clear();
  if (!encrypted_ok) {
    core.last_error_ = "file encrypt failed";
    return false;
  }

  if (!core.UploadE2eeFileBlob(blob, out_file_id)) {
    return false;
  }
  if (!preview.empty()) {
    core.BestEffortStoreAttachmentPreviewBytes(out_file_id, file_name, file_size,
                                          preview);
    crypto_wipe(preview.data(), preview.size());
  }
  return true;
}

}  // namespace mi::client

