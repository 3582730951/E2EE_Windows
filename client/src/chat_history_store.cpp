#include "chat_history_store.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iterator>
#include <limits>
#include <thread>
#include <unordered_map>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

#include "dpapi_util.h"
#include "miniz.h"
#include "monocypher.h"

#include "../server/include/crypto.h"
#include "../server/include/protocol.h"

namespace mi::client {

struct ProfileLockState {
#ifdef _WIN32
  HANDLE handle{INVALID_HANDLE_VALUE};
#else
  int fd{-1};
#endif
  std::filesystem::path path;
};

namespace {

constexpr std::uint8_t kContainerMagic[8] = {'M', 'I', 'H', 'D',
                                             'B', '0', '1', 0};
[[maybe_unused]] constexpr std::uint8_t kContainerVersionV1 = 1;
constexpr std::uint8_t kContainerVersionV2 = 2;
[[maybe_unused]] constexpr std::size_t kPeStubSize = 512;
constexpr bool kEnableLegacyHistoryCompat = false;
constexpr std::size_t kMaxConversationsPerFile = 3;
constexpr std::uint64_t kMaxRecordsPerFile = 200000;
constexpr std::size_t kSeqWidth = 6;
constexpr std::uint8_t kLegacyMagic[8] = {'M', 'I', 'H', 'L',
                                          'O', 'G', '0', '1'};
constexpr std::uint8_t kLegacyVersion = 1;

constexpr std::uint8_t kChatMagic[4] = {'M', 'I', 'C', 'H'};
constexpr std::uint8_t kChatVersion = 1;
constexpr std::uint8_t kChatTypeText = 1;
constexpr std::uint8_t kChatTypeFile = 3;
constexpr std::uint8_t kChatTypeGroupText = 4;
constexpr std::uint8_t kChatTypeGroupInvite = 5;
constexpr std::uint8_t kChatTypeGroupFile = 6;
constexpr std::uint8_t kChatTypeRich = 9;
constexpr std::uint8_t kChatTypeSticker = 12;
constexpr std::size_t kChatHeaderSize = sizeof(kChatMagic) + 1 + 1 + 16;
constexpr std::uint8_t kRichKindText = 1;
constexpr std::uint8_t kRichKindLocation = 2;
constexpr std::uint8_t kRichKindContactCard = 3;
constexpr std::uint8_t kRichFlagHasReply = 0x01;

constexpr std::uint8_t kRecordMeta = 1;

std::string ToLowerAscii(std::string value) {
  for (char& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return value;
}

void CopyHistoryFilesIfMissing(const std::filesystem::path& from,
                               const std::filesystem::path& to) {
  if (from.empty() || to.empty()) {
    return;
  }
  std::error_code ec;
  if (!std::filesystem::exists(from, ec) || ec) {
    return;
  }
  std::filesystem::create_directories(to, ec);
  for (const auto& entry : std::filesystem::directory_iterator(from, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file(ec) || ec) {
      continue;
    }
    const auto name = entry.path().filename();
    if (name.empty()) {
      continue;
    }
    const auto target = to / name;
    if (std::filesystem::exists(target, ec)) {
      ec.clear();
      continue;
    }
    std::filesystem::copy_file(entry.path(), target,
                               std::filesystem::copy_options::skip_existing, ec);
    ec.clear();
  }
}
constexpr std::uint8_t kRecordMessage = 2;
constexpr std::uint8_t kRecordStatus = 3;

constexpr std::uint8_t kMetaKindFlush = 1;
constexpr std::uint8_t kMetaKindFileInfo = 2;
constexpr std::uint8_t kMetaKindFileSummary = 3;
constexpr std::uint8_t kMetaFileInfoVersion = 1;
constexpr std::uint8_t kMetaFileSummaryVersionV1 = 1;
constexpr std::uint8_t kMetaFileSummaryVersion = 2;
constexpr char kFileMetaConvId[] = "MI_E2EE_FILE_META_V1";
constexpr char kFileChainPrefix[] = "MI_E2EE_FILE_CHAIN_V1";

constexpr std::uint8_t kMessageKindEnvelope = 1;
constexpr std::uint8_t kMessageKindSystem = 2;

constexpr std::uint8_t kPadMagic[4] = {'M', 'I', 'P', 'D'};
constexpr std::size_t kPadHeaderBytes = 8;
constexpr std::size_t kPadBuckets[] = {256, 512, 1024, 2048,
                                       4096, 8192, 16384};
constexpr std::uint8_t kCompressMagic[4] = {'M', 'I', 'C', 'M'};
constexpr std::uint8_t kCompressVersion = 1;
constexpr std::uint8_t kCompressMethodDeflate = 1;
constexpr int kCompressLevel = 1;
constexpr std::size_t kCompressHeaderBytes =
    sizeof(kCompressMagic) + 1 + 1 + 2 + 4;
constexpr std::uint8_t kAesLayerMagic[8] = {'M', 'I', 'A', 'E',
                                            'S', '0', '1', 0};
constexpr std::uint8_t kAesLayerVersion = 1;
constexpr std::size_t kAesNonceBytes = 12;
constexpr std::size_t kAesTagBytes = 16;
constexpr std::size_t kAesLayerHeaderBytes =
    sizeof(kAesLayerMagic) + 1 + kAesNonceBytes + kAesTagBytes + 4;
constexpr std::uint8_t kWrapMagic[4] = {'M', 'I', 'H', '2'};
constexpr std::uint8_t kWrapVersion = 1;
constexpr std::size_t kWrapKeyBytes = 32;
constexpr std::size_t kWrapSlotCount = 3;
constexpr std::size_t kWrapSlotNonceBytes = 24;
constexpr std::size_t kWrapSlotCipherBytes = kWrapKeyBytes;
constexpr std::size_t kWrapSlotMacBytes = 16;
constexpr std::size_t kWrapHeaderBytes = 8;
constexpr std::size_t kWrapNonceBytes = 24;
constexpr std::size_t kWrapMacBytes = 16;

constexpr std::size_t kMaxRecordCipherLen = 2u * 1024u * 1024u;
constexpr std::size_t kMaxWrapRecordBytes = kMaxRecordCipherLen + 4096;
constexpr std::size_t kMaxHistoryKeyFileBytes = 64u * 1024u;
constexpr std::size_t kTagKeyBytes = 32;
constexpr std::size_t kUserTagBytes = 16;
constexpr std::uint8_t kIndexFileMagic[8] = {'M', 'I', 'H', 'I',
                                             'D', 'X', '0', '1'};
constexpr std::uint8_t kIndexPlainMagic[8] = {'M', 'I', 'H', 'I',
                                              'P', 'L', '0', '1'};
constexpr std::uint8_t kIndexVersionV2 = 2;
constexpr std::uint8_t kIndexVersion = 3;
constexpr std::size_t kIndexNonceBytes = 24;
constexpr std::size_t kIndexMacBytes = 16;
constexpr std::uint8_t kProfilesFileMagic[8] = {'M', 'I', 'H', 'P',
                                                'D', 'X', '0', '1'};
constexpr std::uint8_t kProfilesPlainMagic[8] = {'M', 'I', 'H', 'P',
                                                 'P', 'L', '0', '1'};
constexpr std::uint8_t kProfilesVersion = 1;
constexpr std::size_t kProfilesNonceBytes = 24;
constexpr std::size_t kProfilesMacBytes = 16;
constexpr std::size_t kMaxProfiles = 4096;
constexpr std::uint8_t kAttachmentIndexMagic[8] = {'M', 'I', 'H', 'A',
                                                   'D', 'X', '0', '1'};
constexpr std::uint8_t kAttachmentIndexPlainMagic[8] = {'M', 'I', 'H', 'A',
                                                        'P', 'L', '0', '1'};
constexpr std::uint8_t kAttachmentIndexVersion = 1;
constexpr std::size_t kAttachmentIndexNonceBytes = 24;
constexpr std::size_t kAttachmentIndexMacBytes = 16;
constexpr std::size_t kMaxAttachmentEntries = 200000;
constexpr std::uint8_t kAttachmentPreviewMagic[8] = {'M', 'I', 'H', 'A',
                                                     'T', '0', '1', 0};
constexpr std::uint8_t kAttachmentPreviewVersion = 1;
constexpr std::size_t kAttachmentPreviewNonceBytes = 24;
constexpr std::size_t kAttachmentPreviewMacBytes = 16;
constexpr std::size_t kAttachmentPreviewMaxBytes = 256u * 1024u;
constexpr std::uint8_t kJournalMagic[8] = {'M', 'I', 'H', 'J',
                                           'D', 'X', '0', '1'};
constexpr std::uint8_t kJournalVersion = 1;
constexpr std::uint8_t kJournalEntryFileCreate = 1;
constexpr std::uint8_t kJournalEntryConvAdd = 2;
constexpr std::uint8_t kJournalEntryFileStats = 3;
constexpr std::uint8_t kJournalEntryConvStats = 4;

constexpr std::size_t kContainerHeaderBytes =
    sizeof(kContainerMagic) + 1 + 3;
constexpr std::uint8_t kMih3Magic[4] = {'M', 'I', 'H', '3'};
constexpr std::uint8_t kMih3Version = 1;
constexpr std::size_t kMih3PlainBytes = 96;
constexpr std::size_t kMih3NonceBytes = 24;
constexpr std::size_t kMih3MacBytes = 16;
constexpr std::size_t kMih3HeaderBytes =
    sizeof(kMih3Magic) + 1 + 1 + 2 + kMih3NonceBytes + 4 + kMih3MacBytes +
    kMih3PlainBytes;
constexpr std::uint8_t kMih3FlagTrailer = 0x01;

bool ReadExact(std::ifstream& in, void* dst, std::size_t len);
bool LocateContainerOffset(std::ifstream& in,
                           std::uint32_t& out_offset,
                           std::string& error);
bool DecodeAesLayer(const std::array<std::uint8_t, 32>& conv_key,
                    bool is_group,
                    const std::string& conv_id,
                    const std::vector<std::uint8_t>& input,
                    std::vector<std::uint8_t>& out_plain,
                    bool& out_used_aes,
                    std::string& error);

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

bool AcquireFileLock(const std::filesystem::path& path,
                     std::unique_ptr<ProfileLockState>& out,
                     std::string& error) {
  error.clear();
  out.reset();
  if (path.empty()) {
    error = "history lock path empty";
    return false;
  }
  std::error_code ec;
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
  }
#ifdef _WIN32
  const std::wstring wpath = path.wstring();
  HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                         nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    error = "history lock failed";
    return false;
  }
  auto state = std::make_unique<ProfileLockState>();
  state->handle = h;
  state->path = path;
  out = std::move(state);
  return true;
#else
  int fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0600);
  if (fd < 0) {
    error = "history lock failed";
    return false;
  }
  if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
    ::close(fd);
    error = "history lock busy";
    return false;
  }
  auto state = std::make_unique<ProfileLockState>();
  state->fd = fd;
  state->path = path;
  out = std::move(state);
  return true;
#endif
}

void ReleaseFileLock(std::unique_ptr<ProfileLockState>& lock) {
  if (!lock) {
    return;
  }
#ifdef _WIN32
  if (lock->handle != INVALID_HANDLE_VALUE) {
    CloseHandle(lock->handle);
  }
#else
  if (lock->fd >= 0) {
    flock(lock->fd, LOCK_UN);
    ::close(lock->fd);
  }
#endif
  lock.reset();
}

std::string Sha256HexLower(const std::vector<std::uint8_t>& in) {
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(in.data(), in.size(), d);
  return BytesToHexLower(d.bytes.data(), d.bytes.size());
}

enum class AttachmentKind : std::uint8_t {
  kGeneric = 0,
  kImage = 1,
  kAudio = 2,
  kVideo = 3,
};

char LowerAscii(char c) {
  if (c >= 'A' && c <= 'Z') {
    return static_cast<char>(c + ('a' - 'A'));
  }
  return c;
}

std::string LowerAsciiCopy(const std::string& in) {
  std::string out;
  out.resize(in.size());
  for (std::size_t i = 0; i < in.size(); ++i) {
    out[i] = LowerAscii(in[i]);
  }
  return out;
}

bool EndsWith(const std::string& value, const std::string& suffix) {
  if (suffix.empty() || value.size() < suffix.size()) {
    return false;
  }
  return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

AttachmentKind GuessAttachmentKind(const std::string& file_name) {
  const std::string name = LowerAsciiCopy(file_name);
  if (EndsWith(name, ".png") || EndsWith(name, ".jpg") ||
      EndsWith(name, ".jpeg") || EndsWith(name, ".bmp") ||
      EndsWith(name, ".gif") || EndsWith(name, ".webp") ||
      EndsWith(name, ".heic") || EndsWith(name, ".heif")) {
    return AttachmentKind::kImage;
  }
  if (EndsWith(name, ".mp3") || EndsWith(name, ".wav") ||
      EndsWith(name, ".aac") || EndsWith(name, ".flac") ||
      EndsWith(name, ".ogg") || EndsWith(name, ".m4a") ||
      EndsWith(name, ".opus")) {
    return AttachmentKind::kAudio;
  }
  if (EndsWith(name, ".mp4") || EndsWith(name, ".mkv") ||
      EndsWith(name, ".webm") || EndsWith(name, ".avi") ||
      EndsWith(name, ".mov") || EndsWith(name, ".wmv") ||
      EndsWith(name, ".m4v")) {
    return AttachmentKind::kVideo;
  }
  return AttachmentKind::kGeneric;
}

std::string AttachmentPreviewName(const std::string& file_id) {
  static constexpr char kPrefix[] = "MI_E2EE_ATTACH_PREVIEW_V1";
  std::vector<std::uint8_t> buf;
  buf.reserve(sizeof(kPrefix) - 1 + file_id.size());
  buf.insert(buf.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  buf.insert(buf.end(), file_id.begin(), file_id.end());
  const std::string hex = Sha256HexLower(buf);
  return "att_" + hex.substr(0, 32) + ".bin";
}

std::string DeriveUserTagHmac(const std::array<std::uint8_t, 32>& key,
                              const std::string& username) {
  static constexpr char kPrefix[] = "MI_E2EE_HISTORY_TAG_V1";
  std::vector<std::uint8_t> buf;
  buf.reserve(sizeof(kPrefix) + 1 + username.size());
  buf.insert(buf.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  buf.push_back(0);
  buf.insert(buf.end(), username.begin(), username.end());

  mi::server::crypto::Sha256Digest out;
  mi::server::crypto::HmacSha256(key.data(), key.size(), buf.data(), buf.size(),
                                 out);
  return BytesToHexLower(out.bytes.data(), kUserTagBytes);
}

std::array<std::uint8_t, 16> DeriveConvHash(
    const std::array<std::uint8_t, 32>& key,
    const std::string& conv_key) {
  mi::server::crypto::Sha256Digest out;
  mi::server::crypto::HmacSha256(key.data(), key.size(),
                                 reinterpret_cast<const std::uint8_t*>(
                                     conv_key.data()),
                                 conv_key.size(), out);
  std::array<std::uint8_t, 16> short_hash{};
  std::memcpy(short_hash.data(), out.bytes.data(), short_hash.size());
  return short_hash;
}

bool DeriveAttachmentIndexKey(const std::array<std::uint8_t, 32>& master_key,
                              std::array<std::uint8_t, 32>& out_key,
                              std::string& error) {
  error.clear();
  out_key.fill(0);
  if (IsAllZero(master_key.data(), master_key.size())) {
    error = "history key missing";
    return false;
  }
  static constexpr char kPrefix[] = "MI_E2EE_HISTORY_ATTACH_INDEX_KEY_V1";
  static constexpr char kSalt[] = "MI_E2EE_HISTORY_ATTACH_INDEX_SALT_V1";
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(reinterpret_cast<const std::uint8_t*>(kSalt),
                             sizeof(kSalt) - 1, d);
  const auto& salt = d.bytes;
  if (!mi::server::crypto::HkdfSha256(master_key.data(), master_key.size(),
                                      salt.data(), salt.size(),
                                      reinterpret_cast<const std::uint8_t*>(
                                          kPrefix),
                                      sizeof(kPrefix) - 1, out_key.data(),
                                      out_key.size())) {
    error = "history hkdf failed";
    return false;
  }
  return true;
}

bool DeriveAttachmentPreviewKey(const std::array<std::uint8_t, 32>& master_key,
                                const std::string& file_id,
                                std::array<std::uint8_t, 32>& out_key,
                                std::string& error) {
  error.clear();
  out_key.fill(0);
  if (IsAllZero(master_key.data(), master_key.size())) {
    error = "history key missing";
    return false;
  }
  if (file_id.empty()) {
    error = "file id empty";
    return false;
  }
  static constexpr char kPrefix[] = "MI_E2EE_HISTORY_ATTACH_PREVIEW_KEY_V1";
  static constexpr char kSalt[] = "MI_E2EE_HISTORY_ATTACH_PREVIEW_SALT_V1";
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(reinterpret_cast<const std::uint8_t*>(kSalt),
                             sizeof(kSalt) - 1, d);
  const auto& salt = d.bytes;
  std::vector<std::uint8_t> info;
  info.reserve(sizeof(kPrefix) - 1 + 1 + file_id.size());
  info.insert(info.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  info.push_back(0);
  info.insert(info.end(), file_id.begin(), file_id.end());
  if (!mi::server::crypto::HkdfSha256(master_key.data(), master_key.size(),
                                      salt.data(), salt.size(), info.data(),
                                      info.size(), out_key.data(),
                                      out_key.size())) {
    error = "history hkdf failed";
    return false;
  }
  return true;
}

std::string ConvHashKey(const std::array<std::uint8_t, 16>& hash) {
  return BytesToHexLower(hash.data(), hash.size());
}

std::array<std::uint8_t, 32> ComputeFileChainHash(
    const std::array<std::uint8_t, 16>& file_uuid,
    std::uint32_t seq,
    const std::array<std::uint8_t, 32>& prev_hash) {
  std::vector<std::uint8_t> buf;
  buf.reserve(sizeof(kFileChainPrefix) - 1 + file_uuid.size() + 4 +
              prev_hash.size());
  buf.insert(buf.end(), kFileChainPrefix,
             kFileChainPrefix + sizeof(kFileChainPrefix) - 1);
  buf.insert(buf.end(), file_uuid.begin(), file_uuid.end());
  buf.push_back(static_cast<std::uint8_t>(seq & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((seq >> 8) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((seq >> 16) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((seq >> 24) & 0xFF));
  buf.insert(buf.end(), prev_hash.begin(), prev_hash.end());
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(buf.data(), buf.size(), d);
  return d.bytes;
}

struct Mih3Summary {
  std::uint32_t file_seq{0};
  std::array<std::uint8_t, 16> file_uuid{};
  std::array<std::uint8_t, 32> prev_hash{};
  std::uint64_t min_ts{0};
  std::uint64_t max_ts{0};
  std::uint64_t record_count{0};
  std::uint64_t message_count{0};
  std::uint32_t conv_count{0};
  std::uint32_t flags{0};
  std::uint32_t reserved{0};
};

bool DeriveMih3Key(const std::array<std::uint8_t, 32>& master_key,
                   std::array<std::uint8_t, 32>& out_key,
                   std::string& error) {
  error.clear();
  out_key.fill(0);
  if (IsAllZero(master_key.data(), master_key.size())) {
    error = "history key missing";
    return false;
  }
  static constexpr char kPrefix[] = "MI_E2EE_HISTORY_MIH3_KEY_V1";
  static constexpr char kSalt[] = "MI_E2EE_HISTORY_MIH3_SALT_V1";
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(reinterpret_cast<const std::uint8_t*>(kSalt),
                             sizeof(kSalt) - 1, d);
  const auto& salt = d.bytes;
  if (!mi::server::crypto::HkdfSha256(master_key.data(), master_key.size(),
                                      salt.data(), salt.size(),
                                      reinterpret_cast<const std::uint8_t*>(
                                          kPrefix),
                                      sizeof(kPrefix) - 1, out_key.data(),
                                      out_key.size())) {
    error = "history hkdf failed";
    return false;
  }
  return true;
}

bool EncodeMih3Plain(const Mih3Summary& summary,
                     std::vector<std::uint8_t>& out) {
  out.clear();
  out.reserve(kMih3PlainBytes);
  if (!mi::server::proto::WriteUint32(summary.file_seq, out)) {
    return false;
  }
  out.insert(out.end(), summary.file_uuid.begin(), summary.file_uuid.end());
  out.insert(out.end(), summary.prev_hash.begin(), summary.prev_hash.end());
  mi::server::proto::WriteUint64(summary.min_ts, out);
  mi::server::proto::WriteUint64(summary.max_ts, out);
  mi::server::proto::WriteUint64(summary.record_count, out);
  mi::server::proto::WriteUint64(summary.message_count, out);
  mi::server::proto::WriteUint32(summary.conv_count, out);
  mi::server::proto::WriteUint32(summary.flags, out);
  mi::server::proto::WriteUint32(summary.reserved, out);
  if (out.size() < kMih3PlainBytes) {
    out.resize(kMih3PlainBytes, 0);
  }
  return out.size() == kMih3PlainBytes;
}

bool DecodeMih3Plain(const std::vector<std::uint8_t>& in,
                     Mih3Summary& out) {
  if (in.size() != kMih3PlainBytes) {
    return false;
  }
  std::size_t off = 0;
  if (!mi::server::proto::ReadUint32(in, off, out.file_seq)) {
    return false;
  }
  if (off + out.file_uuid.size() + out.prev_hash.size() > in.size()) {
    return false;
  }
  std::memcpy(out.file_uuid.data(), in.data() + off, out.file_uuid.size());
  off += out.file_uuid.size();
  std::memcpy(out.prev_hash.data(), in.data() + off, out.prev_hash.size());
  off += out.prev_hash.size();
  if (!mi::server::proto::ReadUint64(in, off, out.min_ts) ||
      !mi::server::proto::ReadUint64(in, off, out.max_ts) ||
      !mi::server::proto::ReadUint64(in, off, out.record_count) ||
      !mi::server::proto::ReadUint64(in, off, out.message_count) ||
      !mi::server::proto::ReadUint32(in, off, out.conv_count) ||
      !mi::server::proto::ReadUint32(in, off, out.flags) ||
      !mi::server::proto::ReadUint32(in, off, out.reserved)) {
    return false;
  }
  return off <= in.size();
}

bool BuildMih3BlockBytes(const std::array<std::uint8_t, 32>& master_key,
                         const Mih3Summary& summary,
                         std::uint8_t header_flags,
                         std::vector<std::uint8_t>& out,
                         std::string& error) {
  error.clear();
  out.clear();
  std::array<std::uint8_t, 32> key{};
  std::string key_err;
  if (!DeriveMih3Key(master_key, key, key_err)) {
    error = key_err.empty() ? "history write failed" : key_err;
    return false;
  }
  std::vector<std::uint8_t> plain;
  if (!EncodeMih3Plain(summary, plain)) {
    crypto_wipe(key.data(), key.size());
    error = "history write failed";
    return false;
  }
  std::array<std::uint8_t, kMih3NonceBytes> nonce{};
  if (!mi::server::crypto::RandomBytes(nonce.data(), nonce.size())) {
    crypto_wipe(key.data(), key.size());
    error = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> cipher(plain.size());
  std::array<std::uint8_t, kMih3MacBytes> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), key.data(), nonce.data(),
                   nullptr, 0, plain.data(), plain.size());
  crypto_wipe(key.data(), key.size());

  out.reserve(kMih3HeaderBytes);
  out.insert(out.end(), kMih3Magic, kMih3Magic + sizeof(kMih3Magic));
  out.push_back(kMih3Version);
  out.push_back(header_flags);
  out.push_back(0);
  out.push_back(0);
  out.insert(out.end(), nonce.begin(), nonce.end());
  const std::uint32_t cipher_len =
      static_cast<std::uint32_t>(cipher.size());
  out.push_back(static_cast<std::uint8_t>(cipher_len & 0xFF));
  out.push_back(static_cast<std::uint8_t>((cipher_len >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((cipher_len >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((cipher_len >> 24) & 0xFF));
  out.insert(out.end(), mac.begin(), mac.end());
  out.insert(out.end(), cipher.begin(), cipher.end());
  return out.size() == kMih3HeaderBytes;
}

bool WriteMih3Block(std::ofstream& out,
                    const std::array<std::uint8_t, 32>& master_key,
                    const Mih3Summary& summary,
                    std::uint8_t header_flags,
                    std::string& error) {
  error.clear();
  if (!out) {
    error = "history write failed";
    return false;
  }
  std::vector<std::uint8_t> bytes;
  if (!BuildMih3BlockBytes(master_key, summary, header_flags, bytes, error)) {
    return false;
  }
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  if (!out.good()) {
    error = "history write failed";
    return false;
  }
  return true;
}

bool ReadMih3Block(std::ifstream& in,
                   const std::array<std::uint8_t, 32>& master_key,
                   Mih3Summary& summary,
                   std::uint8_t& out_flags,
                   bool& out_valid) {
  summary = Mih3Summary{};
  out_flags = 0;
  out_valid = false;
  std::uint8_t magic[sizeof(kMih3Magic)]{};
  if (!ReadExact(in, magic, sizeof(magic))) {
    return false;
  }
  if (std::memcmp(magic, kMih3Magic, sizeof(kMih3Magic)) != 0) {
    return false;
  }
  std::uint8_t version = 0;
  if (!ReadExact(in, &version, sizeof(version))) {
    return false;
  }
  std::uint8_t flags = 0;
  if (!ReadExact(in, &flags, sizeof(flags))) {
    return false;
  }
  std::uint8_t reserved[2]{};
  if (!ReadExact(in, reserved, sizeof(reserved))) {
    return false;
  }
  out_flags = flags;
  if (version != kMih3Version) {
    return true;
  }
  std::array<std::uint8_t, kMih3NonceBytes> nonce{};
  if (!ReadExact(in, nonce.data(), nonce.size())) {
    return false;
  }
  std::uint8_t len_bytes[4]{};
  if (!ReadExact(in, len_bytes, sizeof(len_bytes))) {
    return false;
  }
  const std::uint32_t cipher_len =
      static_cast<std::uint32_t>(len_bytes[0]) |
      (static_cast<std::uint32_t>(len_bytes[1]) << 8) |
      (static_cast<std::uint32_t>(len_bytes[2]) << 16) |
      (static_cast<std::uint32_t>(len_bytes[3]) << 24);
  if (cipher_len != kMih3PlainBytes) {
    if (cipher_len > (8u * 1024u)) {
      return false;
    }
  }
  std::array<std::uint8_t, kMih3MacBytes> mac{};
  if (!ReadExact(in, mac.data(), mac.size())) {
    return false;
  }
  std::vector<std::uint8_t> cipher(cipher_len);
  if (!ReadExact(in, cipher.data(), cipher.size())) {
    return false;
  }
  std::array<std::uint8_t, 32> key{};
  std::string key_err;
  if (!DeriveMih3Key(master_key, key, key_err)) {
    return true;
  }
  std::vector<std::uint8_t> plain(cipher_len);
  const int ok = crypto_aead_unlock(plain.data(), mac.data(), key.data(),
                                    nonce.data(), nullptr, 0, cipher.data(),
                                    cipher.size());
  crypto_wipe(key.data(), key.size());
  if (ok != 0) {
    return true;
  }
  Mih3Summary parsed;
  if (!DecodeMih3Plain(plain, parsed)) {
    return true;
  }
  summary = parsed;
  out_valid = true;
  return true;
}

bool ConsumeMih3Header(std::ifstream& in,
                       const std::array<std::uint8_t, 32>& master_key,
                       Mih3Summary* out_summary) {
  const auto start = in.tellg();
  if (start == std::streampos(-1)) {
    return false;
  }
  std::uint8_t magic[sizeof(kMih3Magic)]{};
  if (!ReadExact(in, magic, sizeof(magic))) {
    in.clear();
    in.seekg(start, std::ios::beg);
    return false;
  }
  if (std::memcmp(magic, kMih3Magic, sizeof(kMih3Magic)) != 0) {
    in.clear();
    in.seekg(start, std::ios::beg);
    return false;
  }
  in.clear();
  in.seekg(start, std::ios::beg);
  Mih3Summary summary;
  std::uint8_t flags = 0;
  bool valid = false;
  if (!ReadMih3Block(in, master_key, summary, flags, valid)) {
    return false;
  }
  if (out_summary && valid && (flags & kMih3FlagTrailer) == 0) {
    *out_summary = summary;
  }
  return true;
}

bool UpdateMih3HeaderOnDisk(const std::filesystem::path& path,
                            const std::array<std::uint8_t, 32>& master_key,
                            const Mih3Summary& summary,
                            std::string& error) {
  error.clear();
  if (path.empty()) {
    error = "history write failed";
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    error = "history write failed";
    return false;
  }
  std::uint32_t container_offset = 0;
  if (!LocateContainerOffset(in, container_offset, error)) {
    return false;
  }
  in.close();
  std::vector<std::uint8_t> header;
  if (!BuildMih3BlockBytes(master_key, summary, 0, header, error)) {
    return false;
  }
  std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
  if (!io) {
    error = "history write failed";
    return false;
  }
  io.seekp(static_cast<std::streamoff>(container_offset + kContainerHeaderBytes),
           std::ios::beg);
  io.write(reinterpret_cast<const char*>(header.data()),
           static_cast<std::streamsize>(header.size()));
  io.flush();
  if (!io) {
    error = "history write failed";
    return false;
  }
  return true;
}
void BestEffortWipeFile(const std::filesystem::path& path) {
  if (path.empty()) {
    return;
  }
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    return;
  }
  const std::uintmax_t size = std::filesystem::file_size(path, ec);
  if (!ec && size > 0) {
    std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
    if (io) {
      const std::size_t wipe_len =
          size < 16 ? static_cast<std::size_t>(size) : 16u;
      const std::vector<std::uint8_t> ff(wipe_len, 0xFF);
      io.seekp(0, std::ios::beg);
      io.write(reinterpret_cast<const char*>(ff.data()),
               static_cast<std::streamsize>(ff.size()));
      if (size > wipe_len) {
        const std::uintmax_t mid =
            size > wipe_len * 2 ? (size / 2) : wipe_len;
        io.seekp(static_cast<std::streamoff>(mid), std::ios::beg);
        io.write(reinterpret_cast<const char*>(ff.data()),
                 static_cast<std::streamsize>(ff.size()));
      }
      if (size > wipe_len * 2) {
        io.seekp(static_cast<std::streamoff>(size - wipe_len), std::ios::beg);
        io.write(reinterpret_cast<const char*>(ff.data()),
                 static_cast<std::streamsize>(ff.size()));
      }
      io.flush();
    }
  }
  std::filesystem::resize_file(path, 0, ec);
  std::filesystem::remove(path, ec);
}

bool ReadExact(std::ifstream& in, void* dst, std::size_t len) {
  if (len == 0) {
    return true;
  }
  in.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(len));
  return in.good();
}

bool WriteUint32(std::ofstream& out, std::uint32_t v) {
  const std::uint8_t b[4] = {
      static_cast<std::uint8_t>(v & 0xFF),
      static_cast<std::uint8_t>((v >> 8) & 0xFF),
      static_cast<std::uint8_t>((v >> 16) & 0xFF),
      static_cast<std::uint8_t>((v >> 24) & 0xFF),
  };
  out.write(reinterpret_cast<const char*>(b), sizeof(b));
  return out.good();
}

bool ReadUint32(std::ifstream& in, std::uint32_t& v) {
  std::uint8_t b[4];
  if (!ReadExact(in, b, sizeof(b))) {
    return false;
  }
  v = static_cast<std::uint32_t>(b[0]) |
      (static_cast<std::uint32_t>(b[1]) << 8) |
      (static_cast<std::uint32_t>(b[2]) << 16) |
      (static_cast<std::uint32_t>(b[3]) << 24);
  return true;
}

bool RandomUint32(std::uint32_t& out) {
  return mi::server::crypto::RandomBytes(reinterpret_cast<std::uint8_t*>(&out),
                                         sizeof(out));
}

std::uint64_t NowUnixSeconds() {
  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::seconds>(
          now.time_since_epoch())
          .count());
}

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

bool PadPlain(const std::vector<std::uint8_t>& plain,
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
    if (!mi::server::crypto::RandomBytes(out.data() + offset, pad_len)) {
      error = "pad rng failed";
      return false;
    }
  }
  return true;
}

bool UnpadPlain(const std::vector<std::uint8_t>& plain,
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

bool EncodeCompressionLayer(const std::vector<std::uint8_t>& plain,
                            std::vector<std::uint8_t>& out,
                            std::string& error) {
  error.clear();
  out.clear();
  if (plain.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "history record too large";
    return false;
  }

  mz_ulong bound = mz_compressBound(static_cast<mz_ulong>(plain.size()));
  if (bound == 0 || bound > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "history compress failed";
    return false;
  }
  std::vector<std::uint8_t> comp;
  comp.resize(static_cast<std::size_t>(bound));
  mz_ulong comp_len = bound;
  const int rc = mz_compress2(comp.data(), &comp_len, plain.data(),
                              static_cast<mz_ulong>(plain.size()),
                              kCompressLevel);
  if (rc != MZ_OK) {
    error = "history compress failed";
    return false;
  }
  comp.resize(static_cast<std::size_t>(comp_len));

  out.reserve(kCompressHeaderBytes + 4 + comp.size());
  out.insert(out.end(), kCompressMagic,
             kCompressMagic + sizeof(kCompressMagic));
  out.push_back(kCompressVersion);
  out.push_back(kCompressMethodDeflate);
  out.push_back(0);
  out.push_back(0);
  if (!mi::server::proto::WriteUint32(
          static_cast<std::uint32_t>(plain.size()), out)) {
    error = "history record too large";
    return false;
  }
  if (!mi::server::proto::WriteBytes(comp, out)) {
    error = "history record too large";
    return false;
  }
  return true;
}

bool DecodeCompressionLayer(const std::vector<std::uint8_t>& input,
                            std::vector<std::uint8_t>& out_plain,
                            bool& out_used_compress,
                            std::string& error) {
  error.clear();
  out_plain.clear();
  out_used_compress = false;
  if (input.size() < kCompressHeaderBytes) {
    out_plain = input;
    return true;
  }
  if (std::memcmp(input.data(), kCompressMagic,
                  sizeof(kCompressMagic)) != 0) {
    out_plain = input;
    return true;
  }
  std::size_t off = sizeof(kCompressMagic);
  const std::uint8_t version = input[off++];
  const std::uint8_t method = input[off++];
  off += 2;
  if (version != kCompressVersion || method != kCompressMethodDeflate) {
    error = "history version mismatch";
    return false;
  }
  std::uint32_t plain_len = 0;
  if (!mi::server::proto::ReadUint32(input, off, plain_len)) {
    error = "history read failed";
    return false;
  }
  if (plain_len > kMaxRecordCipherLen) {
    error = "history record size invalid";
    return false;
  }
  std::vector<std::uint8_t> comp;
  if (!mi::server::proto::ReadBytes(input, off, comp) || off != input.size()) {
    error = "history read failed";
    return false;
  }
  std::vector<std::uint8_t> plain;
  plain.resize(plain_len);
  mz_ulong dest_len = static_cast<mz_ulong>(plain_len);
  const int rc = mz_uncompress(plain.data(), &dest_len, comp.data(),
                               static_cast<mz_ulong>(comp.size()));
  if (rc != MZ_OK || dest_len != plain_len) {
    error = "history compress failed";
    return false;
  }
  out_plain = std::move(plain);
  out_used_compress = true;
  return true;
}

bool DecodeInnerRecordPlain(const std::array<std::uint8_t, 32>& conv_key,
                            bool is_group,
                            const std::string& conv_id,
                            const std::array<std::uint8_t, 24>& inner_nonce,
                            const std::vector<std::uint8_t>& inner_cipher,
                            const std::array<std::uint8_t, 16>& inner_mac,
                            std::vector<std::uint8_t>& out_plain,
                            std::string& error) {
  error.clear();
  out_plain.clear();
  if (inner_cipher.empty()) {
    return true;
  }
  std::vector<std::uint8_t> plain(inner_cipher.size());
  const int ok = crypto_aead_unlock(plain.data(), inner_mac.data(),
                                    conv_key.data(), inner_nonce.data(),
                                    nullptr, 0, inner_cipher.data(),
                                    inner_cipher.size());
  if (ok != 0) {
    error = "history auth failed";
    return false;
  }
  std::vector<std::uint8_t> padded;
  bool used_aes = false;
  std::string aes_err;
  if (!DecodeAesLayer(conv_key, is_group, conv_id, plain, padded, used_aes,
                      aes_err)) {
    error = aes_err.empty() ? "history read failed" : aes_err;
    return false;
  }
  (void)used_aes;
  std::vector<std::uint8_t> unpadded;
  std::string pad_err;
  if (!UnpadPlain(padded, unpadded, pad_err)) {
    error = pad_err.empty() ? "history read failed" : pad_err;
    return false;
  }
  std::vector<std::uint8_t> record_plain;
  bool used_compress = false;
  std::string comp_err;
  if (!DecodeCompressionLayer(unpadded, record_plain, used_compress,
                              comp_err)) {
    error = comp_err.empty() ? "history read failed" : comp_err;
    return false;
  }
  (void)used_compress;
  out_plain = std::move(record_plain);
  return true;
}

bool ParseFileMetaRecord(const std::vector<std::uint8_t>& record_plain,
                         std::uint32_t& out_seq,
                         std::array<std::uint8_t, 16>& out_uuid,
                         std::uint64_t& out_ts) {
  out_seq = 0;
  out_uuid.fill(0);
  out_ts = 0;
  if (record_plain.size() < 2 || record_plain[0] != kRecordMeta) {
    return false;
  }
  if (record_plain.size() == 1 + 8) {
    return false;
  }
  std::size_t off = 1;
  const std::uint8_t kind = record_plain[off++];
  if (kind != kMetaKindFileInfo) {
    return false;
  }
  if (off >= record_plain.size()) {
    return false;
  }
  const std::uint8_t version = record_plain[off++];
  if (version != kMetaFileInfoVersion) {
    return false;
  }
  std::uint32_t seq = 0;
  if (!mi::server::proto::ReadUint32(record_plain, off, seq)) {
    return false;
  }
  if (off + out_uuid.size() + 8 > record_plain.size()) {
    return false;
  }
  std::memcpy(out_uuid.data(), record_plain.data() + off, out_uuid.size());
  off += out_uuid.size();
  std::uint64_t ts = 0;
  if (!mi::server::proto::ReadUint64(record_plain, off, ts)) {
    return false;
  }
  if (off != record_plain.size()) {
    return false;
  }
  out_seq = seq;
  out_ts = ts;
  return true;
}

bool ParseFileSummaryRecord(const std::vector<std::uint8_t>& record_plain,
                            std::uint32_t& out_seq,
                            std::array<std::uint8_t, 16>& out_uuid,
                            std::array<std::uint8_t, 32>& out_prev_hash,
                            std::uint64_t& out_min_ts,
                            std::uint64_t& out_max_ts,
                            std::uint64_t& out_record_count,
                            std::uint64_t& out_message_count,
                            std::vector<std::array<std::uint8_t, 16>>& out_conv_hashes,
                            std::vector<ChatHistoryConvStats>* out_conv_stats) {
  out_seq = 0;
  out_uuid.fill(0);
  out_prev_hash.fill(0);
  out_min_ts = 0;
  out_max_ts = 0;
  out_record_count = 0;
  out_message_count = 0;
  out_conv_hashes.clear();
  if (out_conv_stats) {
    out_conv_stats->clear();
  }
  if (record_plain.size() < 2 || record_plain[0] != kRecordMeta) {
    return false;
  }
  std::size_t off = 1;
  const std::uint8_t kind = record_plain[off++];
  if (kind != kMetaKindFileSummary) {
    return false;
  }
  if (off >= record_plain.size()) {
    return false;
  }
  const std::uint8_t version = record_plain[off++];
  if (version != kMetaFileSummaryVersion &&
      version != kMetaFileSummaryVersionV1) {
    return false;
  }
  std::uint32_t seq = 0;
  if (!mi::server::proto::ReadUint32(record_plain, off, seq)) {
    return false;
  }
  if (off + out_uuid.size() + out_prev_hash.size() + 8 * 4 > record_plain.size()) {
    return false;
  }
  std::memcpy(out_uuid.data(), record_plain.data() + off, out_uuid.size());
  off += out_uuid.size();
  std::memcpy(out_prev_hash.data(), record_plain.data() + off,
              out_prev_hash.size());
  off += out_prev_hash.size();
  std::uint64_t min_ts = 0;
  std::uint64_t max_ts = 0;
  std::uint64_t record_count = 0;
  std::uint64_t message_count = 0;
  if (!mi::server::proto::ReadUint64(record_plain, off, min_ts) ||
      !mi::server::proto::ReadUint64(record_plain, off, max_ts) ||
      !mi::server::proto::ReadUint64(record_plain, off, record_count) ||
      !mi::server::proto::ReadUint64(record_plain, off, message_count)) {
    return false;
  }
  std::uint32_t conv_count = 0;
  if (!mi::server::proto::ReadUint32(record_plain, off, conv_count)) {
    return false;
  }
  if (conv_count > 64) {
    return false;
  }
  if (off + static_cast<std::size_t>(conv_count) * 16 > record_plain.size()) {
    return false;
  }
  out_conv_hashes.resize(conv_count);
  for (std::uint32_t i = 0; i < conv_count; ++i) {
    std::memcpy(out_conv_hashes[i].data(), record_plain.data() + off,
                out_conv_hashes[i].size());
    off += out_conv_hashes[i].size();
  }
  if (version >= kMetaFileSummaryVersion && conv_count > 0) {
    if (off + static_cast<std::size_t>(conv_count) * 8 * 4 >
        record_plain.size()) {
      return false;
    }
    std::vector<ChatHistoryConvStats> parsed;
    parsed.resize(conv_count);
    for (std::uint32_t i = 0; i < conv_count; ++i) {
      if (!mi::server::proto::ReadUint64(record_plain, off,
                                         parsed[i].min_ts) ||
          !mi::server::proto::ReadUint64(record_plain, off,
                                         parsed[i].max_ts) ||
          !mi::server::proto::ReadUint64(record_plain, off,
                                         parsed[i].record_count) ||
          !mi::server::proto::ReadUint64(record_plain, off,
                                         parsed[i].message_count)) {
        return false;
      }
    }
    if (out_conv_stats) {
      *out_conv_stats = std::move(parsed);
    }
  }
  if (off != record_plain.size()) {
    return false;
  }
  out_seq = seq;
  out_min_ts = min_ts;
  out_max_ts = max_ts;
  out_record_count = record_count;
  out_message_count = message_count;
  return true;
}

std::string MakeConvKey(bool is_group, const std::string& conv_id) {
  std::string out;
  out.reserve(conv_id.size() + 2);
  out.push_back(is_group ? 'g' : 'p');
  out.push_back(':');
  out.append(conv_id);
  return out;
}

bool ParseConvKey(const std::string& key,
                  bool& out_is_group,
                  std::string& out_conv_id) {
  out_is_group = false;
  out_conv_id.clear();
  if (key.size() < 3 || key[1] != ':') {
    return false;
  }
  const char prefix = key[0];
  if (prefix != 'g' && prefix != 'p') {
    return false;
  }
  out_is_group = (prefix == 'g');
  out_conv_id = key.substr(2);
  return !out_conv_id.empty();
}

std::string PadSeq(std::uint32_t seq) {
  std::string s = std::to_string(seq);
  if (s.size() < kSeqWidth) {
    s.insert(0, kSeqWidth - s.size(), '0');
  }
  return s;
}

std::string BuildHistoryFileName(const std::string& user_tag,
                                 std::uint32_t seq) {
  return "main_" + user_tag + "_" + PadSeq(seq) + ".dll";
}

bool ParseHistoryFileName(const std::string& name,
                          const std::string& user_tag,
                          std::uint32_t& out_seq) {
  out_seq = 0;
  if (user_tag.empty()) {
    return false;
  }
  const std::string prefix = "main_" + user_tag + "_";
  if (name.size() <= prefix.size() + 4 || name.rfind(prefix, 0) != 0 ||
      name.substr(name.size() - 4) != ".dll") {
    return false;
  }
  const std::size_t num_len = name.size() - prefix.size() - 4;
  if (num_len == 0) {
    return false;
  }
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < num_len; ++i) {
    const char c = name[prefix.size() + i];
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return false;
    }
    value = value * 10u + static_cast<std::uint64_t>(c - '0');
    if (value > (std::numeric_limits<std::uint32_t>::max)()) {
      return false;
    }
  }
  out_seq = static_cast<std::uint32_t>(value);
  return out_seq != 0;
}

void WriteLe16(std::vector<std::uint8_t>& buf, std::size_t off,
               std::uint16_t v) {
  if (off + 2 > buf.size()) {
    return;
  }
  buf[off] = static_cast<std::uint8_t>(v & 0xFF);
  buf[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}

void WriteLe32(std::vector<std::uint8_t>& buf, std::size_t off,
               std::uint32_t v) {
  if (off + 4 > buf.size()) {
    return;
  }
  buf[off] = static_cast<std::uint8_t>(v & 0xFF);
  buf[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
  buf[off + 2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
  buf[off + 3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
}

void FillRandomBytes(std::vector<std::uint8_t>& buf,
                     std::size_t off,
                     std::size_t len) {
  if (off + len > buf.size() || len == 0) {
    return;
  }
  mi::server::crypto::RandomBytes(buf.data() + off, len);
}

void FillVmText(std::vector<std::uint8_t>& buf,
                std::size_t off,
                std::size_t len) {
  if (off + len > buf.size() || len == 0) {
    return;
  }
  std::size_t pos = off;
  const std::size_t end = off + len;
  if (pos + 3 <= end) {
    buf[pos++] = 0x31;
    buf[pos++] = 0xC0;
    buf[pos++] = 0xC3;
  }
  while (pos < end) {
    std::uint32_t r = 0;
    RandomUint32(r);
    const std::uint8_t kind = static_cast<std::uint8_t>(r % 5u);
    if (kind == 0 && pos + 2 <= end) {
      buf[pos++] = 0xEB;
      buf[pos++] = static_cast<std::uint8_t>(r & 0xFF);
      continue;
    }
    if (kind == 1 && pos + 5 <= end) {
      buf[pos++] = 0xE9;
      buf[pos++] = static_cast<std::uint8_t>(r & 0xFF);
      buf[pos++] = static_cast<std::uint8_t>((r >> 8) & 0xFF);
      buf[pos++] = static_cast<std::uint8_t>((r >> 16) & 0xFF);
      buf[pos++] = static_cast<std::uint8_t>((r >> 24) & 0xFF);
      continue;
    }
    if (kind == 2 && pos + 1 <= end) {
      buf[pos++] = 0x90;
      continue;
    }
    if (kind == 3 && pos + 1 <= end) {
      buf[pos++] = 0xCC;
      continue;
    }
    if (pos + 3 <= end) {
      buf[pos++] = 0x31;
      buf[pos++] = 0xC0;
      buf[pos++] = 0xC3;
      continue;
    }
    buf[pos++] = 0x90;
  }
}

void WriteSectionHeader(std::vector<std::uint8_t>& buf,
                        std::size_t off,
                        const char name[8],
                        std::uint32_t virt_size,
                        std::uint32_t virt_addr,
                        std::uint32_t raw_size,
                        std::uint32_t raw_ptr,
                        std::uint32_t characteristics) {
  if (off + 40 > buf.size()) {
    return;
  }
  std::memcpy(buf.data() + off, name, 8);
  WriteLe32(buf, off + 8, virt_size);
  WriteLe32(buf, off + 12, virt_addr);
  WriteLe32(buf, off + 16, raw_size);
  WriteLe32(buf, off + 20, raw_ptr);
  WriteLe32(buf, off + 36, characteristics);
}

[[maybe_unused]] void ShuffleSections(std::vector<std::array<char, 8>>& names,
                     std::vector<std::uint32_t>& vsize,
                     std::vector<std::uint32_t>& vaddr,
                     std::vector<std::uint32_t>& raw_size,
                     std::vector<std::uint32_t>& raw_ptr,
                     std::vector<std::uint32_t>& characteristics) {
  if (names.size() < 2) {
    return;
  }
  for (std::size_t i = names.size() - 1; i > 0; --i) {
    std::uint32_t r = 0;
    if (!RandomUint32(r)) {
      continue;
    }
    const std::size_t j = static_cast<std::size_t>(r % (i + 1));
    if (i == j) {
      continue;
    }
    std::swap(names[i], names[j]);
    std::swap(vsize[i], vsize[j]);
    std::swap(vaddr[i], vaddr[j]);
    std::swap(raw_size[i], raw_size[j]);
    std::swap(raw_ptr[i], raw_ptr[j]);
    std::swap(characteristics[i], characteristics[j]);
  }
}

std::array<char, 8> PickSectionName(
    const std::vector<std::array<char, 8>>& options) {
  if (options.empty()) {
    return {0, 0, 0, 0, 0, 0, 0, 0};
  }
  std::uint32_t r = 0;
  if (!RandomUint32(r)) {
    return options.front();
  }
  return options[static_cast<std::size_t>(r % options.size())];
}

std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment) {
  if (alignment == 0) {
    return value;
  }
  const std::uint32_t mask = alignment - 1;
  return (value + mask) & ~mask;
}

bool IsPowerOfTwo(std::uint32_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

std::vector<std::uint8_t> BuildPeContainer(std::uint32_t& out_hist_offset) {
  struct PeSection {
    std::array<char, 8> name{};
    std::uint32_t vsize{0};
    std::uint32_t vaddr{0};
    std::uint32_t raw_size{0};
    std::uint32_t raw_ptr{0};
    std::uint32_t characteristics{0};
    bool is_text{false};
    bool is_data{false};
  };

  constexpr std::uint32_t kFileAlignment = 0x200;
  constexpr std::uint32_t kSectionAlignment = 0x1000;
  constexpr std::uint32_t kPeOffset = 0x80;
  constexpr std::uint16_t kSectionCount = 6;
  constexpr std::uint16_t kOptSize = 0xE0;
  constexpr std::uint32_t kRawSize = 0x200;

  const std::uint32_t header_size =
      AlignUp(kPeOffset + 4 + 20 + kOptSize + kSectionCount * 40,
              kFileAlignment);

  const std::vector<std::array<char, 8>> text_names = {
      {'.', 't', 'e', 'x', 't', 0, 0, 0},
      {'.', 'c', 'o', 'd', 'e', 0, 0, 0},
      {'.', 'v', 'm', 't', 'x', 't', 0, 0},
  };
  const std::vector<std::array<char, 8>> rdata_names = {
      {'.', 'r', 'd', 'a', 't', 'a', 0, 0},
      {'.', 'i', 'd', 'a', 't', 'a', 0, 0},
  };
  const std::vector<std::array<char, 8>> data_names = {
      {'.', 'd', 'a', 't', 'a', 0, 0, 0},
      {'.', 'b', 's', 's', 0, 0, 0, 0},
  };
  const std::vector<std::array<char, 8>> rsrc_names = {
      {'.', 'r', 's', 'r', 'c', 0, 0, 0},
      {'.', 'r', 's', 'r', '1', 0, 0, 0},
  };
  const std::vector<std::array<char, 8>> reloc_names = {
      {'.', 'r', 'e', 'l', 'o', 'c', 0, 0},
      {'.', 'r', 'e', 'l', '1', 0, 0, 0},
  };

  std::vector<PeSection> sections;
  sections.reserve(5);
  std::uint32_t raw_ptr = header_size;
  sections.push_back(
      {PickSectionName(text_names), kRawSize, 0x1000, kRawSize, raw_ptr,
       0x60000020, true, false});
  raw_ptr += kRawSize;
  sections.push_back(
      {PickSectionName(rdata_names), kRawSize, 0x2000, kRawSize, raw_ptr,
       0x40000040, false, false});
  raw_ptr += kRawSize;
  sections.push_back(
      {PickSectionName(data_names), kRawSize, 0x3000, kRawSize, raw_ptr,
       0xC0000040, false, true});
  raw_ptr += kRawSize;
  sections.push_back(
      {PickSectionName(rsrc_names), kRawSize, 0x4000, kRawSize, raw_ptr,
       0x40000040, false, false});
  raw_ptr += kRawSize;
  sections.push_back(
      {PickSectionName(reloc_names), kRawSize, 0x5000, kRawSize, raw_ptr,
       0x42000040, false, false});
  raw_ptr += kRawSize;

  PeSection hist_section{
      {'.', 'h', 'i', 's', 't', 0, 0, 0},
      kRawSize,
      0x6000,
      kRawSize,
      raw_ptr,
      0x40000040,
      false,
      false,
  };

  out_hist_offset = hist_section.raw_ptr;
  std::vector<std::uint8_t> buf(out_hist_offset, 0);

  buf[0] = 'M';
  buf[1] = 'Z';
  WriteLe32(buf, 0x3C, kPeOffset);
  buf[kPeOffset] = 'P';
  buf[kPeOffset + 1] = 'E';
  buf[kPeOffset + 2] = 0;
  buf[kPeOffset + 3] = 0;

  const std::size_t coff_off = kPeOffset + 4;
  WriteLe16(buf, coff_off + 0, 0x14c);
  WriteLe16(buf, coff_off + 2, kSectionCount);
  std::uint32_t ts = 0;
  RandomUint32(ts);
  WriteLe32(buf, coff_off + 4, ts);
  WriteLe32(buf, coff_off + 8, 0);
  WriteLe32(buf, coff_off + 12, 0);
  WriteLe16(buf, coff_off + 16, kOptSize);
  WriteLe16(buf, coff_off + 18, 0x2102);

  std::uint32_t image_base = 0x400000;
  std::uint32_t base_rand = 0;
  if (RandomUint32(base_rand)) {
    image_base += (base_rand & 0xFFu) * 0x10000u;
  }

  std::uint32_t base_of_code = 0x1000;
  std::uint32_t base_of_data = 0x3000;
  std::uint32_t size_of_code = 0;
  std::uint32_t size_of_init_data = 0;
  std::uint32_t size_of_image = 0;
  for (const auto& sec : sections) {
    if (sec.is_text) {
      base_of_code = sec.vaddr;
      size_of_code += sec.raw_size;
    } else {
      size_of_init_data += sec.raw_size;
    }
    if (sec.is_data) {
      base_of_data = sec.vaddr;
    }
    size_of_image = std::max(
        size_of_image,
        AlignUp(sec.vaddr + sec.vsize, kSectionAlignment));
  }
  size_of_init_data += hist_section.raw_size;
  size_of_image =
      std::max(size_of_image,
               AlignUp(hist_section.vaddr + hist_section.vsize,
                       kSectionAlignment));

  const std::size_t opt_off = coff_off + 20;
  WriteLe16(buf, opt_off + 0, 0x10B);
  buf[opt_off + 2] = 0;
  buf[opt_off + 3] = 0;
  WriteLe32(buf, opt_off + 4, size_of_code);
  WriteLe32(buf, opt_off + 8, size_of_init_data);
  WriteLe32(buf, opt_off + 12, 0);
  WriteLe32(buf, opt_off + 16, base_of_code);
  WriteLe32(buf, opt_off + 20, base_of_code);
  WriteLe32(buf, opt_off + 24, base_of_data);
  WriteLe32(buf, opt_off + 28, image_base);
  WriteLe32(buf, opt_off + 32, kSectionAlignment);
  WriteLe32(buf, opt_off + 36, kFileAlignment);
  WriteLe16(buf, opt_off + 40, 6);
  WriteLe16(buf, opt_off + 42, 0);
  WriteLe16(buf, opt_off + 44, 0);
  WriteLe16(buf, opt_off + 46, 0);
  WriteLe16(buf, opt_off + 48, 6);
  WriteLe16(buf, opt_off + 50, 0);
  WriteLe32(buf, opt_off + 52, 0);
  WriteLe32(buf, opt_off + 56, size_of_image);
  WriteLe32(buf, opt_off + 60, header_size);
  WriteLe32(buf, opt_off + 64, 0);
  WriteLe16(buf, opt_off + 68, 2);
  WriteLe16(buf, opt_off + 70, 0x0140);
  WriteLe32(buf, opt_off + 72, 0x100000);
  WriteLe32(buf, opt_off + 76, 0x1000);
  WriteLe32(buf, opt_off + 80, 0x100000);
  WriteLe32(buf, opt_off + 84, 0x1000);
  WriteLe32(buf, opt_off + 88, 0);
  WriteLe32(buf, opt_off + 92, 16);

  std::size_t sec_off = opt_off + kOptSize;
  for (const auto& sec : sections) {
    WriteSectionHeader(buf, sec_off, sec.name.data(), sec.vsize, sec.vaddr,
                       sec.raw_size, sec.raw_ptr, sec.characteristics);
    sec_off += 40;
  }
  WriteSectionHeader(buf, sec_off, hist_section.name.data(),
                     hist_section.vsize, hist_section.vaddr,
                     hist_section.raw_size, hist_section.raw_ptr,
                     hist_section.characteristics);

  for (const auto& sec : sections) {
    if (sec.is_text) {
      FillVmText(buf, sec.raw_ptr, sec.raw_size);
    } else {
      FillRandomBytes(buf, sec.raw_ptr, sec.raw_size);
    }
  }
  return buf;
}

bool WriteContainerHeader(std::ofstream& out,
                          std::uint8_t version,
                          std::string& error) {
  error.clear();
  if (!out) {
    error = "history write failed";
    return false;
  }
  out.write(reinterpret_cast<const char*>(kContainerMagic),
            sizeof(kContainerMagic));
  out.put(static_cast<char>(version));
  const char zero[3] = {0, 0, 0};
  out.write(zero, sizeof(zero));
  if (!out.good()) {
    error = "history write failed";
    return false;
  }
  return true;
}

bool ReadContainerHeader(std::ifstream& in,
                         std::uint8_t& out_version,
                         std::string& error) {
  error.clear();
  out_version = 0;
  std::uint8_t magic[sizeof(kContainerMagic)]{};
  if (!ReadExact(in, magic, sizeof(magic))) {
    error = "history read failed";
    return false;
  }
  if (std::memcmp(magic, kContainerMagic, sizeof(kContainerMagic)) != 0) {
    error = "history magic mismatch";
    return false;
  }
  std::uint8_t version = 0;
  if (!ReadExact(in, &version, sizeof(version))) {
    error = "history read failed";
    return false;
  }
  std::uint8_t reserved[3]{};
  if (!ReadExact(in, reserved, sizeof(reserved))) {
    error = "history read failed";
    return false;
  }
  out_version = version;
  return true;
}

bool LocateContainerOffset(std::ifstream& in,
                           std::uint32_t& out_offset,
                           std::string& error) {
  error.clear();
  out_offset = 0;
  if (!in) {
    error = "history read failed";
    return false;
  }
  in.clear();
  in.seekg(0, std::ios::end);
  const std::streampos end_pos = in.tellg();
  if (end_pos <= 0) {
    error = "history pe invalid";
    return false;
  }
  const std::size_t file_size = static_cast<std::size_t>(end_pos);
  if (file_size < 0x100) {
    error = "history pe invalid";
    return false;
  }

  in.seekg(0, std::ios::beg);
  std::array<std::uint8_t, 64> dos{};
  if (!ReadExact(in, dos.data(), dos.size())) {
    error = "history read failed";
    return false;
  }
  if (dos[0] != 'M' || dos[1] != 'Z') {
    error = "history pe invalid";
    return false;
  }
  const std::uint32_t pe_off =
      static_cast<std::uint32_t>(dos[0x3C]) |
      (static_cast<std::uint32_t>(dos[0x3D]) << 8) |
      (static_cast<std::uint32_t>(dos[0x3E]) << 16) |
      (static_cast<std::uint32_t>(dos[0x3F]) << 24);
  if (pe_off < 0x40 || pe_off > file_size - (4 + 20)) {
    error = "history pe invalid";
    return false;
  }
  in.seekg(pe_off, std::ios::beg);
  std::uint8_t sig[4]{};
  if (!ReadExact(in, sig, sizeof(sig)) ||
      sig[0] != 'P' || sig[1] != 'E' || sig[2] != 0 || sig[3] != 0) {
    error = "history pe invalid";
    return false;
  }
  std::uint8_t coff[20]{};
  if (!ReadExact(in, coff, sizeof(coff))) {
    error = "history pe invalid";
    return false;
  }
  const std::uint16_t section_count =
      static_cast<std::uint16_t>(coff[2] | (coff[3] << 8));
  const std::uint16_t opt_size =
      static_cast<std::uint16_t>(coff[16] | (coff[17] << 8));
  if (section_count == 0 || section_count > 96) {
    error = "history pe invalid";
    return false;
  }
  if (opt_size < 0xE0 || opt_size > 0x1000) {
    error = "history pe invalid";
    return false;
  }
  const std::size_t sections_end =
      static_cast<std::size_t>(pe_off) + 4 + 20 + opt_size +
      static_cast<std::size_t>(section_count) * 40;
  if (sections_end > file_size) {
    error = "history pe invalid";
    return false;
  }
  std::vector<std::uint8_t> opt(opt_size);
  if (!ReadExact(in, opt.data(), opt.size())) {
    error = "history pe invalid";
    return false;
  }
  const auto read16 = [&](std::size_t off) -> std::uint16_t {
    if (off + 1 >= opt.size()) {
      return 0;
    }
    return static_cast<std::uint16_t>(opt[off] | (opt[off + 1] << 8));
  };
  const auto read32 = [&](std::size_t off) -> std::uint32_t {
    if (off + 3 >= opt.size()) {
      return 0;
    }
    return static_cast<std::uint32_t>(opt[off]) |
           (static_cast<std::uint32_t>(opt[off + 1]) << 8) |
           (static_cast<std::uint32_t>(opt[off + 2]) << 16) |
           (static_cast<std::uint32_t>(opt[off + 3]) << 24);
  };
  const std::uint16_t magic = read16(0);
  if (magic != 0x10B) {
    error = "history pe invalid";
    return false;
  }
  const std::uint32_t section_align = read32(0x20);
  const std::uint32_t file_align = read32(0x24);
  const std::uint32_t size_of_image = read32(0x38);
  const std::uint32_t size_of_headers = read32(0x3C);
  if (!IsPowerOfTwo(section_align) || !IsPowerOfTwo(file_align) ||
      file_align < 0x200 || section_align < file_align) {
    error = "history pe invalid";
    return false;
  }
  if (size_of_image == 0 || (size_of_image % section_align) != 0) {
    error = "history pe invalid";
    return false;
  }
  if (size_of_headers == 0 ||
      (size_of_headers % file_align) != 0 ||
      size_of_headers > file_size) {
    error = "history pe invalid";
    return false;
  }
  const std::uint32_t min_headers =
      AlignUp(static_cast<std::uint32_t>(sections_end), file_align);
  if (size_of_headers < min_headers) {
    error = "history pe invalid";
    return false;
  }

  bool found_hist = false;
  std::uint32_t hist_ptr = 0;
  std::uint32_t hist_size = 0;
  std::uint32_t max_end = 0;
  for (std::uint16_t i = 0; i < section_count; ++i) {
    std::uint8_t sec[40]{};
    if (!ReadExact(in, sec, sizeof(sec))) {
      error = "history pe invalid";
      return false;
    }
    const std::uint32_t vsize =
        static_cast<std::uint32_t>(sec[8]) |
        (static_cast<std::uint32_t>(sec[9]) << 8) |
        (static_cast<std::uint32_t>(sec[10]) << 16) |
        (static_cast<std::uint32_t>(sec[11]) << 24);
    const std::uint32_t vaddr =
        static_cast<std::uint32_t>(sec[12]) |
        (static_cast<std::uint32_t>(sec[13]) << 8) |
        (static_cast<std::uint32_t>(sec[14]) << 16) |
        (static_cast<std::uint32_t>(sec[15]) << 24);
    const std::uint32_t raw_size =
        static_cast<std::uint32_t>(sec[16]) |
        (static_cast<std::uint32_t>(sec[17]) << 8) |
        (static_cast<std::uint32_t>(sec[18]) << 16) |
        (static_cast<std::uint32_t>(sec[19]) << 24);
    const std::uint32_t raw_ptr =
        static_cast<std::uint32_t>(sec[20]) |
        (static_cast<std::uint32_t>(sec[21]) << 8) |
        (static_cast<std::uint32_t>(sec[22]) << 16) |
        (static_cast<std::uint32_t>(sec[23]) << 24);
    if (raw_size == 0) {
      if (raw_ptr != 0) {
        error = "history pe invalid";
        return false;
      }
    } else {
      if ((raw_ptr % file_align) != 0 ||
          (raw_size % file_align) != 0 ||
          raw_ptr < size_of_headers ||
          raw_ptr > file_size ||
          raw_ptr + raw_size > file_size) {
        error = "history pe invalid";
        return false;
      }
    }
    if ((vaddr % section_align) != 0) {
      error = "history pe invalid";
      return false;
    }
    const std::uint32_t end =
        vaddr + std::max<std::uint32_t>(vsize, raw_size);
    if (end > max_end) {
      max_end = end;
    }
    const bool is_hist =
        sec[0] == '.' && sec[1] == 'h' && sec[2] == 'i' && sec[3] == 's' &&
        sec[4] == 't';
    if (is_hist) {
      found_hist = true;
      hist_ptr = raw_ptr;
      hist_size = raw_size;
    }
  }
  if (AlignUp(max_end, section_align) > size_of_image) {
    error = "history pe invalid";
    return false;
  }
  if (!found_hist || hist_ptr == 0 || hist_size == 0) {
    error = "history pe missing hist";
    return false;
  }
  out_offset = hist_ptr;
  return true;
}

bool ParseOuterPlain(const std::vector<std::uint8_t>& outer_plain,
                     bool& out_is_group,
                     std::string& out_conv_id,
                     std::array<std::uint8_t, 24>& out_inner_nonce,
                     std::vector<std::uint8_t>& out_inner_cipher,
                     std::array<std::uint8_t, 16>& out_inner_mac,
                     std::string& error) {
  error.clear();
  out_is_group = false;
  out_conv_id.clear();
  out_inner_nonce.fill(0);
  out_inner_cipher.clear();
  out_inner_mac.fill(0);

  if (outer_plain.empty()) {
    error = "history record empty";
    return false;
  }
  std::size_t off = 0;
  out_is_group = outer_plain[off++] != 0;
  if (!mi::server::proto::ReadString(outer_plain, off, out_conv_id) ||
      out_conv_id.empty()) {
    error = "history read failed";
    return false;
  }
  if (off + out_inner_nonce.size() > outer_plain.size()) {
    error = "history read failed";
    return false;
  }
  std::memcpy(out_inner_nonce.data(), outer_plain.data() + off,
              out_inner_nonce.size());
  off += out_inner_nonce.size();
  if (!mi::server::proto::ReadBytes(outer_plain, off, out_inner_cipher)) {
    error = "history read failed";
    return false;
  }
  if (off + out_inner_mac.size() != outer_plain.size()) {
    error = "history read failed";
    return false;
  }
  std::memcpy(out_inner_mac.data(), outer_plain.data() + off,
              out_inner_mac.size());
  return true;
}

bool DecryptOuterBlob(const std::array<std::uint8_t, 32>& master_key,
                      const std::vector<std::uint8_t>& blob,
                      bool& out_is_group,
                      std::string& out_conv_id,
                      std::array<std::uint8_t, 24>& out_inner_nonce,
                      std::vector<std::uint8_t>& out_inner_cipher,
                      std::array<std::uint8_t, 16>& out_inner_mac,
                      std::string& error) {
  error.clear();
  out_is_group = false;
  out_conv_id.clear();
  out_inner_nonce.fill(0);
  out_inner_cipher.clear();
  out_inner_mac.fill(0);

  if (IsAllZero(master_key.data(), master_key.size())) {
    error = "history key invalid";
    return false;
  }
  if (blob.size() < (kWrapNonceBytes + kWrapMacBytes)) {
    error = "history record size invalid";
    return false;
  }
  const std::size_t cipher_len =
      blob.size() - kWrapNonceBytes - kWrapMacBytes;
  if (cipher_len == 0 || cipher_len > kMaxRecordCipherLen) {
    error = "history record size invalid";
    return false;
  }
  std::array<std::uint8_t, 24> nonce{};
  std::memcpy(nonce.data(), blob.data(), nonce.size());
  std::vector<std::uint8_t> cipher(cipher_len);
  std::memcpy(cipher.data(), blob.data() + nonce.size(), cipher.size());
  std::array<std::uint8_t, 16> mac{};
  std::memcpy(mac.data(), blob.data() + nonce.size() + cipher.size(),
              mac.size());

  std::vector<std::uint8_t> outer_plain(cipher.size());
  const int ok = crypto_aead_unlock(outer_plain.data(), mac.data(),
                                    master_key.data(), nonce.data(), nullptr,
                                    0, cipher.data(), cipher.size());
  if (ok != 0) {
    error = "history auth failed";
    return false;
  }
  return ParseOuterPlain(outer_plain, out_is_group, out_conv_id,
                         out_inner_nonce, out_inner_cipher, out_inner_mac,
                         error);
}

std::array<std::uint8_t, 32> DeriveMaskFromLabel(const char* label) {
  mi::server::crypto::Sha256Digest d;
  const std::size_t len = label ? std::strlen(label) : 0u;
  mi::server::crypto::Sha256(reinterpret_cast<const std::uint8_t*>(label), len,
                             d);
  return d.bytes;
}

const std::array<std::uint8_t, 32>& WhiteboxMask1() {
  static const std::array<std::uint8_t, 32> mask =
      DeriveMaskFromLabel("MI_E2EE_WB_MASK1_V1");
  return mask;
}

const std::array<std::uint8_t, 32>& WhiteboxMask2() {
  static const std::array<std::uint8_t, 32> mask =
      DeriveMaskFromLabel("MI_E2EE_WB_MASK2_V1");
  return mask;
}

const std::array<std::uint8_t, 32>& WhiteboxMask3() {
  static const std::array<std::uint8_t, 32> mask =
      DeriveMaskFromLabel("MI_E2EE_WB_MASK3_V1");
  return mask;
}

void WhiteboxMixKey(std::array<std::uint8_t, 32>& key) {
  const auto& m1 = WhiteboxMask1();
  const auto& m2 = WhiteboxMask2();
  const auto& m3 = WhiteboxMask3();
  for (std::size_t i = 0; i < key.size(); ++i) {
    key[i] = static_cast<std::uint8_t>(key[i] ^ m1[i]);
  }
  for (std::size_t i = 0; i < key.size(); ++i) {
    key[i] = static_cast<std::uint8_t>(key[i] + m2[i]);
  }
  for (std::size_t i = 0; i < key.size(); ++i) {
    const std::uint8_t src = m3[i];
    const std::uint8_t shift = static_cast<std::uint8_t>(i & 7u);
    std::uint8_t rot = src;
    if (shift != 0) {
      rot = static_cast<std::uint8_t>(
          static_cast<std::uint8_t>(src << shift) |
          static_cast<std::uint8_t>(src >> (8u - shift)));
    }
    key[i] = static_cast<std::uint8_t>(key[i] ^ rot ^
                                       key[(i + 13u) % key.size()]);
  }
}

bool DeriveWhiteboxAesKey(const std::array<std::uint8_t, 32>& conv_key,
                          bool is_group,
                          const std::string& conv_id,
                          std::array<std::uint8_t, 32>& out_key,
                          std::string& error) {
  error.clear();
  out_key.fill(0);
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }
  if (IsAllZero(conv_key.data(), conv_key.size())) {
    error = "history key invalid";
    return false;
  }
  std::vector<std::uint8_t> info;
  static constexpr char kPrefix[] = "MI_E2EE_HISTORY_AESGCM_WB_V1";
  info.insert(info.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  info.push_back(0);
  info.push_back(is_group ? 1 : 0);
  info.push_back(0);
  info.insert(info.end(), conv_id.begin(), conv_id.end());

  std::array<std::uint8_t, 32> salt{};
  static constexpr char kSalt[] = "MI_E2EE_HISTORY_AESGCM_WB_SALT_V1";
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(reinterpret_cast<const std::uint8_t*>(kSalt),
                             sizeof(kSalt) - 1, d);
  salt = d.bytes;

  if (!mi::server::crypto::HkdfSha256(conv_key.data(), conv_key.size(),
                                      salt.data(), salt.size(), info.data(),
                                      info.size(), out_key.data(),
                                      out_key.size())) {
    error = "history hkdf failed";
    return false;
  }
  WhiteboxMixKey(out_key);
  return true;
}

constexpr std::uint8_t kAesSbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
    0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
    0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
    0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
    0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
    0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
    0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
    0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
    0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
    0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
    0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
    0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
    0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
    0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
    0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

constexpr std::uint8_t kAesRcon[15] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,
    0x80, 0x1B, 0x36, 0x6C, 0xD8, 0xAB, 0x4D,
};

struct Aes256KeySchedule {
  std::array<std::uint8_t, 240> bytes;
};

void RotWord(std::uint8_t w[4]) {
  const std::uint8_t tmp = w[0];
  w[0] = w[1];
  w[1] = w[2];
  w[2] = w[3];
  w[3] = tmp;
}

void SubWord(std::uint8_t w[4]) {
  w[0] = kAesSbox[w[0]];
  w[1] = kAesSbox[w[1]];
  w[2] = kAesSbox[w[2]];
  w[3] = kAesSbox[w[3]];
}

void Aes256KeyExpand(const std::array<std::uint8_t, 32>& key,
                     Aes256KeySchedule& ks) {
  ks.bytes.fill(0);
  std::memcpy(ks.bytes.data(), key.data(), key.size());
  std::size_t bytes_generated = key.size();
  std::size_t rcon_iter = 1;
  std::uint8_t temp[4];
  while (bytes_generated < ks.bytes.size()) {
    std::memcpy(temp, ks.bytes.data() + bytes_generated - 4, sizeof(temp));
    if (bytes_generated % 32 == 0) {
      RotWord(temp);
      SubWord(temp);
      temp[0] = static_cast<std::uint8_t>(temp[0] ^ kAesRcon[rcon_iter]);
      ++rcon_iter;
    } else if (bytes_generated % 32 == 16) {
      SubWord(temp);
    }
    for (std::size_t i = 0; i < 4; ++i) {
      ks.bytes[bytes_generated] =
          static_cast<std::uint8_t>(ks.bytes[bytes_generated - 32] ^ temp[i]);
      ++bytes_generated;
    }
  }
}

std::uint8_t Xtime(std::uint8_t v) {
  return static_cast<std::uint8_t>((v << 1) ^ ((v & 0x80u) ? 0x1Bu : 0));
}

using ByteBijection = std::array<std::uint8_t, 256>;
using RoundBijections = std::array<std::array<ByteBijection, 16>, 15>;

struct WhiteboxAesTables {
  std::array<std::array<std::array<std::array<std::uint32_t, 256>, 4>, 4>, 13>
      rounds{};
  std::array<std::array<std::array<std::uint32_t, 256>, 4>, 4> final{};
  RoundBijections enc_a{};
  RoundBijections dec_a{};
  RoundBijections enc_b{};
  RoundBijections dec_b{};
};

constexpr std::array<std::array<int, 4>, 4> kAesTboxInputIndex = {{
    {{0, 5, 10, 15}},
    {{4, 9, 14, 3}},
    {{8, 13, 2, 7}},
    {{12, 1, 6, 11}},
}};

void WordsToBytes(const std::array<std::uint32_t, 4>& words,
                  std::array<std::uint8_t, 16>& out) {
  for (std::size_t i = 0; i < words.size(); ++i) {
    const std::uint32_t w = words[i];
    out[i * 4 + 0] = static_cast<std::uint8_t>((w >> 24) & 0xFF);
    out[i * 4 + 1] = static_cast<std::uint8_t>((w >> 16) & 0xFF);
    out[i * 4 + 2] = static_cast<std::uint8_t>((w >> 8) & 0xFF);
    out[i * 4 + 3] = static_cast<std::uint8_t>(w & 0xFF);
  }
}

std::uint32_t LoadBe32(const std::uint8_t* ptr) {
  return (static_cast<std::uint32_t>(ptr[0]) << 24) |
         (static_cast<std::uint32_t>(ptr[1]) << 16) |
         (static_cast<std::uint32_t>(ptr[2]) << 8) |
         static_cast<std::uint32_t>(ptr[3]);
}

void LoadRoundKeys(const Aes256KeySchedule& ks,
                   std::array<std::uint32_t, 60>& out) {
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = LoadBe32(ks.bytes.data() + i * 4);
  }
}

void BuildBaseTables(
    std::array<std::array<std::uint32_t, 256>, 4>& te,
    std::array<std::array<std::uint32_t, 256>, 4>& fe) {
  for (std::size_t i = 0; i < 256; ++i) {
    const std::uint8_t s = kAesSbox[i];
    const std::uint8_t s2 = Xtime(s);
    const std::uint8_t s3 = static_cast<std::uint8_t>(s2 ^ s);
    te[0][i] =
        (static_cast<std::uint32_t>(s2) << 24) |
        (static_cast<std::uint32_t>(s) << 16) |
        (static_cast<std::uint32_t>(s) << 8) |
        static_cast<std::uint32_t>(s3);
    te[1][i] =
        (static_cast<std::uint32_t>(s3) << 24) |
        (static_cast<std::uint32_t>(s2) << 16) |
        (static_cast<std::uint32_t>(s) << 8) |
        static_cast<std::uint32_t>(s);
    te[2][i] =
        (static_cast<std::uint32_t>(s) << 24) |
        (static_cast<std::uint32_t>(s3) << 16) |
        (static_cast<std::uint32_t>(s2) << 8) |
        static_cast<std::uint32_t>(s);
    te[3][i] =
        (static_cast<std::uint32_t>(s) << 24) |
        (static_cast<std::uint32_t>(s) << 16) |
        (static_cast<std::uint32_t>(s3) << 8) |
        static_cast<std::uint32_t>(s2);
    fe[0][i] = static_cast<std::uint32_t>(s) << 24;
    fe[1][i] = static_cast<std::uint32_t>(s) << 16;
    fe[2][i] = static_cast<std::uint32_t>(s) << 8;
    fe[3][i] = static_cast<std::uint32_t>(s);
  }
}

std::uint8_t EncodeByte(const WhiteboxAesTables& tables,
                        std::size_t round,
                        std::size_t pos,
                        std::uint8_t value) {
  return tables.enc_b[round][pos][tables.enc_a[round][pos][value]];
}

std::uint8_t DecodeByte(const WhiteboxAesTables& tables,
                        std::size_t round,
                        std::size_t pos,
                        std::uint8_t value) {
  return tables.dec_a[round][pos][tables.dec_b[round][pos][value]];
}

std::uint32_t EncodeWord(const WhiteboxAesTables& tables,
                         std::size_t round,
                         std::uint32_t word,
                         std::size_t word_index) {
  const std::uint8_t b0 = static_cast<std::uint8_t>((word >> 24) & 0xFF);
  const std::uint8_t b1 = static_cast<std::uint8_t>((word >> 16) & 0xFF);
  const std::uint8_t b2 = static_cast<std::uint8_t>((word >> 8) & 0xFF);
  const std::uint8_t b3 = static_cast<std::uint8_t>(word & 0xFF);
  const std::size_t base = word_index * 4;
  const std::uint8_t e0 = EncodeByte(tables, round, base + 0, b0);
  const std::uint8_t e1 = EncodeByte(tables, round, base + 1, b1);
  const std::uint8_t e2 = EncodeByte(tables, round, base + 2, b2);
  const std::uint8_t e3 = EncodeByte(tables, round, base + 3, b3);
  return (static_cast<std::uint32_t>(e0) << 24) |
         (static_cast<std::uint32_t>(e1) << 16) |
         (static_cast<std::uint32_t>(e2) << 8) |
         static_cast<std::uint32_t>(e3);
}

std::array<std::uint8_t, 32> Sha256Bytes(const std::vector<std::uint8_t>& in) {
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(in.data(), in.size(), d);
  return d.bytes;
}

std::uint8_t Parity8(std::uint8_t v) {
  v ^= static_cast<std::uint8_t>(v >> 4);
  v ^= static_cast<std::uint8_t>(v >> 2);
  v ^= static_cast<std::uint8_t>(v >> 1);
  return static_cast<std::uint8_t>(v & 1u);
}

std::uint8_t ApplyMatrix(const std::array<std::uint8_t, 8>& mat,
                         std::uint8_t value) {
  std::uint8_t out = 0;
  for (std::size_t row = 0; row < mat.size(); ++row) {
    const std::uint8_t bits = static_cast<std::uint8_t>(mat[row] & value);
    const std::uint8_t bit = Parity8(bits);
    if (bit) {
      out = static_cast<std::uint8_t>(out | (1u << (7u - row)));
    }
  }
  return out;
}

bool InvertMatrix(const std::array<std::uint8_t, 8>& mat,
                  std::array<std::uint8_t, 8>& inv) {
  std::array<std::uint16_t, 8> rows{};
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const std::uint16_t left = static_cast<std::uint16_t>(mat[i]) << 8;
    const std::uint16_t right =
        static_cast<std::uint16_t>(1u << (7u - i));
    rows[i] = static_cast<std::uint16_t>(left | right);
  }

  for (std::size_t col = 0; col < 8; ++col) {
    const std::uint16_t mask =
        static_cast<std::uint16_t>(1u << (15u - col));
    std::size_t pivot = col;
    while (pivot < 8 && (rows[pivot] & mask) == 0) {
      ++pivot;
    }
    if (pivot == 8) {
      return false;
    }
    if (pivot != col) {
      std::swap(rows[pivot], rows[col]);
    }
    for (std::size_t r = 0; r < 8; ++r) {
      if (r != col && (rows[r] & mask)) {
        rows[r] = static_cast<std::uint16_t>(rows[r] ^ rows[col]);
      }
    }
  }

  for (std::size_t i = 0; i < 8; ++i) {
    inv[i] = static_cast<std::uint8_t>(rows[i] & 0xFFu);
  }
  return true;
}

bool BuildLinearBijection(const std::array<std::uint8_t, 32>& key,
                          std::uint32_t round,
                          std::uint32_t pos,
                          const char* label,
                          ByteBijection& enc,
                          ByteBijection& dec,
                          std::string& error) {
  error.clear();
  std::array<std::uint8_t, 8> mat{};
  std::array<std::uint8_t, 8> inv{};
  bool ok = false;
  for (std::uint32_t attempt = 0; attempt < 1024; ++attempt) {
    std::vector<std::uint8_t> seed;
    if (label && *label) {
      const std::size_t len = std::strlen(label);
      seed.insert(seed.end(), label, label + len);
    }
    seed.insert(seed.end(), key.begin(), key.end());
    seed.push_back(static_cast<std::uint8_t>(round & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((round >> 8) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((round >> 16) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((round >> 24) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>(pos & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((pos >> 8) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((pos >> 16) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((pos >> 24) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>(attempt & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((attempt >> 8) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((attempt >> 16) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((attempt >> 24) & 0xFF));
    const auto hash = Sha256Bytes(seed);
    for (std::size_t i = 0; i < mat.size(); ++i) {
      mat[i] = hash[i];
    }
    if (InvertMatrix(mat, inv)) {
      ok = true;
      break;
    }
  }
  if (!ok) {
    error = "history whitebox linear map failed";
    return false;
  }

  for (std::size_t v = 0; v < 256; ++v) {
    const auto byte = static_cast<std::uint8_t>(v);
    enc[v] = ApplyMatrix(mat, byte);
    dec[v] = ApplyMatrix(inv, byte);
  }
  return true;
}

bool BuildRoundBijections(const std::array<std::uint8_t, 32>& key,
                          const char* label,
                          RoundBijections& enc,
                          RoundBijections& dec,
                          std::string& error) {
  error.clear();
  for (std::uint32_t round = 0; round < enc.size(); ++round) {
    for (std::uint32_t pos = 0; pos < 16; ++pos) {
      if (!BuildLinearBijection(key, round, pos, label,
                                enc[round][pos], dec[round][pos], error)) {
        return false;
      }
    }
  }
  return true;
}

std::array<std::uint32_t, 4> DeriveRoundMask(
    const std::array<std::uint8_t, 32>& key,
    std::uint32_t round) {
  std::vector<std::uint8_t> buf;
  static constexpr char kLabel[] = "MI_E2EE_WB_AES_OUTMASK_V1";
  buf.insert(buf.end(), kLabel, kLabel + sizeof(kLabel) - 1);
  buf.insert(buf.end(), key.begin(), key.end());
  buf.push_back(static_cast<std::uint8_t>(round & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((round >> 8) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((round >> 16) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((round >> 24) & 0xFF));
  const auto hash = Sha256Bytes(buf);
  std::array<std::uint32_t, 4> out{};
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = LoadBe32(hash.data() + i * 4);
  }
  return out;
}

std::array<std::uint32_t, 3> DeriveShareMask(
    const std::array<std::uint8_t, 32>& key,
    std::uint32_t round,
    std::uint32_t word,
    bool final_round) {
  std::vector<std::uint8_t> buf;
  static constexpr char kLabel[] = "MI_E2EE_WB_AES_SHARE_V1";
  static constexpr char kFinalLabel[] = "MI_E2EE_WB_AES_FSHARE_V1";
  const char* label = final_round ? kFinalLabel : kLabel;
  const std::size_t label_len =
      final_round ? (sizeof(kFinalLabel) - 1) : (sizeof(kLabel) - 1);
  buf.insert(buf.end(), label, label + label_len);
  buf.insert(buf.end(), key.begin(), key.end());
  buf.push_back(static_cast<std::uint8_t>(round & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((round >> 8) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((round >> 16) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((round >> 24) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>(word & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((word >> 8) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((word >> 16) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((word >> 24) & 0xFF));
  const auto hash = Sha256Bytes(buf);
  std::array<std::uint32_t, 3> out{};
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = LoadBe32(hash.data() + i * 4);
  }
  return out;
}

bool BuildWhiteboxTables(const std::array<std::uint8_t, 32>& key,
                         WhiteboxAesTables& out,
                         std::string& error) {
  error.clear();
  if (IsAllZero(key.data(), key.size())) {
    error = "history key invalid";
    return false;
  }
  if (!BuildRoundBijections(key, "MI_E2EE_WB_LIN_A_V1",
                            out.enc_a, out.dec_a, error)) {
    return false;
  }
  if (!BuildRoundBijections(key, "MI_E2EE_WB_LIN_B_V1",
                            out.enc_b, out.dec_b, error)) {
    return false;
  }

  Aes256KeySchedule ks;
  Aes256KeyExpand(key, ks);
  std::array<std::uint32_t, 60> round_keys{};
  LoadRoundKeys(ks, round_keys);

  std::array<std::array<std::uint32_t, 256>, 4> te{};
  std::array<std::array<std::uint32_t, 256>, 4> fe{};
  BuildBaseTables(te, fe);

  std::array<std::uint8_t, 16> in_mask{};
  {
    std::array<std::uint32_t, 4> rk0{};
    for (std::size_t w = 0; w < 4; ++w) {
      rk0[w] = round_keys[w];
    }
    WordsToBytes(rk0, in_mask);
  }

  for (std::uint32_t round = 0; round < 13; ++round) {
    const auto out_mask_words = DeriveRoundMask(key, round);
    std::array<std::uint8_t, 16> next_mask{};
    WordsToBytes(out_mask_words, next_mask);

    for (std::uint32_t word = 0; word < 4; ++word) {
      const auto shares = DeriveShareMask(key, round, word, false);
      const std::uint32_t rk = round_keys[(round + 1) * 4 + word];
      const std::uint32_t const_word = rk ^ out_mask_words[word];
      const std::uint32_t enc_const =
          EncodeWord(out, round + 1, const_word, word);
      const std::uint32_t share0 = shares[0];
      const std::uint32_t share1 = shares[1];
      const std::uint32_t share2 = shares[2];
      const std::uint32_t share3 =
          enc_const ^ shares[0] ^ shares[1] ^ shares[2];
      const std::uint32_t share[4] = {share0, share1, share2, share3};

      for (std::size_t table = 0; table < 4; ++table) {
        const int idx = kAesTboxInputIndex[word][table];
        const std::uint8_t mask = in_mask[static_cast<std::size_t>(idx)];
        for (std::size_t b = 0; b < 256; ++b) {
          const std::uint8_t unmasked =
              static_cast<std::uint8_t>(
                  DecodeByte(out, round, static_cast<std::size_t>(idx),
                             static_cast<std::uint8_t>(b)) ^
                  mask);
          out.rounds[round][word][table][b] =
              EncodeWord(out, round + 1, te[table][unmasked], word) ^
              share[table];
        }
      }
    }
    in_mask = next_mask;
  }

  const std::uint32_t final_round = 13;
  for (std::uint32_t word = 0; word < 4; ++word) {
    const auto shares = DeriveShareMask(key, final_round, word, true);
    const std::uint32_t rk = round_keys[14 * 4 + word];
    const std::uint32_t enc_const =
        EncodeWord(out, 14, rk, word);
    const std::uint32_t share0 = shares[0];
    const std::uint32_t share1 = shares[1];
    const std::uint32_t share2 = shares[2];
    const std::uint32_t share3 =
        enc_const ^ shares[0] ^ shares[1] ^ shares[2];
    const std::uint32_t share[4] = {share0, share1, share2, share3};

    for (std::size_t table = 0; table < 4; ++table) {
      const int idx = kAesTboxInputIndex[word][table];
      const std::uint8_t mask = in_mask[static_cast<std::size_t>(idx)];
      for (std::size_t b = 0; b < 256; ++b) {
        const std::uint8_t unmasked =
            static_cast<std::uint8_t>(
                DecodeByte(out, final_round,
                           static_cast<std::size_t>(idx),
                           static_cast<std::uint8_t>(b)) ^
                mask);
        out.final[word][table][b] =
            EncodeWord(out, 14, fe[table][unmasked], word) ^
            share[table];
      }
    }
  }

  crypto_wipe(ks.bytes.data(), ks.bytes.size());
  crypto_wipe(round_keys.data(), round_keys.size() * sizeof(round_keys[0]));
  return true;
}

void WipeWhiteboxTables(WhiteboxAesTables& tables) {
  crypto_wipe(&tables, sizeof(tables));
}

void WhiteboxAesEncryptBlock(const WhiteboxAesTables& tables,
                             const std::uint8_t in[16],
                             std::uint8_t out[16]) {
  std::array<std::uint8_t, 16> encoded{};
  for (std::size_t i = 0; i < encoded.size(); ++i) {
    encoded[i] = EncodeByte(tables, 0, i, in[i]);
  }
  std::uint32_t s0 = LoadBe32(encoded.data() + 0);
  std::uint32_t s1 = LoadBe32(encoded.data() + 4);
  std::uint32_t s2 = LoadBe32(encoded.data() + 8);
  std::uint32_t s3 = LoadBe32(encoded.data() + 12);

  for (std::size_t round = 0; round < 13; ++round) {
    const auto& r = tables.rounds[round];
    const std::uint32_t t0 =
        r[0][0][s0 >> 24] ^
        r[0][1][(s1 >> 16) & 0xFF] ^
        r[0][2][(s2 >> 8) & 0xFF] ^
        r[0][3][s3 & 0xFF];
    const std::uint32_t t1 =
        r[1][0][s1 >> 24] ^
        r[1][1][(s2 >> 16) & 0xFF] ^
        r[1][2][(s3 >> 8) & 0xFF] ^
        r[1][3][s0 & 0xFF];
    const std::uint32_t t2 =
        r[2][0][s2 >> 24] ^
        r[2][1][(s3 >> 16) & 0xFF] ^
        r[2][2][(s0 >> 8) & 0xFF] ^
        r[2][3][s1 & 0xFF];
    const std::uint32_t t3 =
        r[3][0][s3 >> 24] ^
        r[3][1][(s0 >> 16) & 0xFF] ^
        r[3][2][(s1 >> 8) & 0xFF] ^
        r[3][3][s2 & 0xFF];
    s0 = t0;
    s1 = t1;
    s2 = t2;
    s3 = t3;
  }

  const auto& f = tables.final;
  const std::uint32_t t0 =
      f[0][0][s0 >> 24] ^
      f[0][1][(s1 >> 16) & 0xFF] ^
      f[0][2][(s2 >> 8) & 0xFF] ^
      f[0][3][s3 & 0xFF];
  const std::uint32_t t1 =
      f[1][0][s1 >> 24] ^
      f[1][1][(s2 >> 16) & 0xFF] ^
      f[1][2][(s3 >> 8) & 0xFF] ^
      f[1][3][s0 & 0xFF];
  const std::uint32_t t2 =
      f[2][0][s2 >> 24] ^
      f[2][1][(s3 >> 16) & 0xFF] ^
      f[2][2][(s0 >> 8) & 0xFF] ^
      f[2][3][s1 & 0xFF];
  const std::uint32_t t3 =
      f[3][0][s3 >> 24] ^
      f[3][1][(s0 >> 16) & 0xFF] ^
      f[3][2][(s1 >> 8) & 0xFF] ^
      f[3][3][s2 & 0xFF];

  const std::uint8_t raw[16] = {
      static_cast<std::uint8_t>(t0 >> 24),
      static_cast<std::uint8_t>(t0 >> 16),
      static_cast<std::uint8_t>(t0 >> 8),
      static_cast<std::uint8_t>(t0),
      static_cast<std::uint8_t>(t1 >> 24),
      static_cast<std::uint8_t>(t1 >> 16),
      static_cast<std::uint8_t>(t1 >> 8),
      static_cast<std::uint8_t>(t1),
      static_cast<std::uint8_t>(t2 >> 24),
      static_cast<std::uint8_t>(t2 >> 16),
      static_cast<std::uint8_t>(t2 >> 8),
      static_cast<std::uint8_t>(t2),
      static_cast<std::uint8_t>(t3 >> 24),
      static_cast<std::uint8_t>(t3 >> 16),
      static_cast<std::uint8_t>(t3 >> 8),
      static_cast<std::uint8_t>(t3),
  };
  for (std::size_t i = 0; i < 16; ++i) {
    out[i] = DecodeByte(tables, 14, i, raw[i]);
  }
}

void StoreUint64Be(std::uint8_t* out, std::uint64_t v) {
  out[0] = static_cast<std::uint8_t>(v >> 56);
  out[1] = static_cast<std::uint8_t>(v >> 48);
  out[2] = static_cast<std::uint8_t>(v >> 40);
  out[3] = static_cast<std::uint8_t>(v >> 32);
  out[4] = static_cast<std::uint8_t>(v >> 24);
  out[5] = static_cast<std::uint8_t>(v >> 16);
  out[6] = static_cast<std::uint8_t>(v >> 8);
  out[7] = static_cast<std::uint8_t>(v);
}

void GcmXorBlock(std::uint8_t out[16],
                 const std::uint8_t a[16],
                 const std::uint8_t b[16]) {
  for (std::size_t i = 0; i < 16; ++i) {
    out[i] = static_cast<std::uint8_t>(a[i] ^ b[i]);
  }
}

void GcmShiftRightOne(std::uint8_t v[16]) {
  const bool lsb = (v[15] & 1u) != 0;
  for (int i = 15; i > 0; --i) {
    v[i] = static_cast<std::uint8_t>((v[i] >> 1) | ((v[i - 1] & 1u) << 7));
  }
  v[0] = static_cast<std::uint8_t>(v[0] >> 1);
  if (lsb) {
    v[0] = static_cast<std::uint8_t>(v[0] ^ 0xE1u);
  }
}

void GcmMul(const std::uint8_t x[16],
            const std::uint8_t h[16],
            std::uint8_t out[16]) {
  std::uint8_t z[16]{};
  std::uint8_t v[16];
  std::memcpy(v, h, 16);
  for (int i = 0; i < 128; ++i) {
    const std::uint8_t bit =
        static_cast<std::uint8_t>((x[i / 8] >> (7 - (i % 8))) & 1u);
    if (bit) {
      for (std::size_t j = 0; j < 16; ++j) {
        z[j] = static_cast<std::uint8_t>(z[j] ^ v[j]);
      }
    }
    GcmShiftRightOne(v);
  }
  std::memcpy(out, z, 16);
}

void GcmGhash(const std::uint8_t h[16],
              const std::uint8_t* aad,
              std::size_t aad_len,
              const std::uint8_t* cipher,
              std::size_t cipher_len,
              std::uint8_t out[16]) {
  std::uint8_t y[16]{};
  std::uint8_t block[16]{};
  std::size_t offset = 0;
  while (offset < aad_len) {
    const std::size_t take =
        std::min<std::size_t>(16, aad_len - offset);
    std::memset(block, 0, sizeof(block));
    if (aad) {
      std::memcpy(block, aad + offset, take);
    }
    std::uint8_t tmp[16];
    GcmXorBlock(tmp, y, block);
    GcmMul(tmp, h, y);
    offset += take;
  }
  offset = 0;
  while (offset < cipher_len) {
    const std::size_t take =
        std::min<std::size_t>(16, cipher_len - offset);
    std::memset(block, 0, sizeof(block));
    if (cipher) {
      std::memcpy(block, cipher + offset, take);
    }
    std::uint8_t tmp[16];
    GcmXorBlock(tmp, y, block);
    GcmMul(tmp, h, y);
    offset += take;
  }
  std::uint8_t len_block[16]{};
  StoreUint64Be(len_block, static_cast<std::uint64_t>(aad_len) * 8u);
  StoreUint64Be(len_block + 8, static_cast<std::uint64_t>(cipher_len) * 8u);
  std::uint8_t tmp[16];
  GcmXorBlock(tmp, y, len_block);
  GcmMul(tmp, h, y);
  std::memcpy(out, y, 16);
}

void Increment32(std::uint8_t counter[16]) {
  for (int i = 15; i >= 12; --i) {
    counter[i] = static_cast<std::uint8_t>(counter[i] + 1u);
    if (counter[i] != 0) {
      break;
    }
  }
}

bool Aes256GcmEncrypt(const std::array<std::uint8_t, 32>& key,
                      const std::array<std::uint8_t, kAesNonceBytes>& nonce,
                      const std::vector<std::uint8_t>& plain,
                      std::vector<std::uint8_t>& out_cipher,
                      std::array<std::uint8_t, kAesTagBytes>& out_tag,
                      std::string& error) {
  error.clear();
  out_cipher.clear();
  out_tag.fill(0);
  if (IsAllZero(key.data(), key.size())) {
    error = "history key invalid";
    return false;
  }

  WhiteboxAesTables tables;
  if (!BuildWhiteboxTables(key, tables, error)) {
    return false;
  }
  auto wipe = [&]() { WipeWhiteboxTables(tables); };

  std::uint8_t h[16]{};
  std::uint8_t zero[16]{};
  WhiteboxAesEncryptBlock(tables, zero, h);

  std::uint8_t j0[16]{};
  std::memcpy(j0, nonce.data(), nonce.size());
  j0[15] = 0x01;

  out_cipher.resize(plain.size());
  std::uint8_t counter[16];
  std::memcpy(counter, j0, sizeof(counter));
  std::size_t offset = 0;
  while (offset < plain.size()) {
    Increment32(counter);
    std::uint8_t stream[16];
    WhiteboxAesEncryptBlock(tables, counter, stream);
    const std::size_t take =
        std::min<std::size_t>(16, plain.size() - offset);
    for (std::size_t i = 0; i < take; ++i) {
      out_cipher[offset + i] =
          static_cast<std::uint8_t>(plain[offset + i] ^ stream[i]);
    }
    offset += take;
  }

  std::uint8_t ghash[16]{};
  GcmGhash(h, nullptr, 0, out_cipher.data(), out_cipher.size(), ghash);

  std::uint8_t s[16];
  WhiteboxAesEncryptBlock(tables, j0, s);
  for (std::size_t i = 0; i < out_tag.size(); ++i) {
    out_tag[i] = static_cast<std::uint8_t>(s[i] ^ ghash[i]);
  }
  wipe();
  return true;
}

bool Aes256GcmDecrypt(const std::array<std::uint8_t, 32>& key,
                      const std::array<std::uint8_t, kAesNonceBytes>& nonce,
                      const std::vector<std::uint8_t>& cipher,
                      const std::array<std::uint8_t, kAesTagBytes>& tag,
                      std::vector<std::uint8_t>& out_plain,
                      std::string& error) {
  error.clear();
  out_plain.clear();
  if (IsAllZero(key.data(), key.size())) {
    error = "history key invalid";
    return false;
  }

  WhiteboxAesTables tables;
  if (!BuildWhiteboxTables(key, tables, error)) {
    return false;
  }
  auto wipe = [&]() { WipeWhiteboxTables(tables); };

  std::uint8_t h[16]{};
  std::uint8_t zero[16]{};
  WhiteboxAesEncryptBlock(tables, zero, h);

  std::uint8_t j0[16]{};
  std::memcpy(j0, nonce.data(), nonce.size());
  j0[15] = 0x01;

  std::uint8_t ghash[16]{};
  GcmGhash(h, nullptr, 0, cipher.data(), cipher.size(), ghash);

  std::uint8_t s[16];
  WhiteboxAesEncryptBlock(tables, j0, s);
  std::uint8_t expected[16];
  for (std::size_t i = 0; i < sizeof(expected); ++i) {
    expected[i] = static_cast<std::uint8_t>(s[i] ^ ghash[i]);
  }

  if (crypto_verify16(expected, tag.data()) != 0) {
    wipe();
    error = "history auth failed";
    return false;
  }

  out_plain.resize(cipher.size());
  std::uint8_t counter[16];
  std::memcpy(counter, j0, sizeof(counter));
  std::size_t offset = 0;
  while (offset < cipher.size()) {
    Increment32(counter);
    std::uint8_t stream[16];
    WhiteboxAesEncryptBlock(tables, counter, stream);
    const std::size_t take =
        std::min<std::size_t>(16, cipher.size() - offset);
    for (std::size_t i = 0; i < take; ++i) {
      out_plain[offset + i] =
          static_cast<std::uint8_t>(cipher[offset + i] ^ stream[i]);
    }
    offset += take;
  }
  wipe();
  return true;
}

bool EncodeAesLayer(const std::array<std::uint8_t, 32>& conv_key,
                    bool is_group,
                    const std::string& conv_id,
                    const std::vector<std::uint8_t>& plain,
                    std::vector<std::uint8_t>& out,
                    std::string& error) {
  error.clear();
  out.clear();
  std::array<std::uint8_t, 32> aes_key{};
  if (!DeriveWhiteboxAesKey(conv_key, is_group, conv_id, aes_key, error)) {
    return false;
  }
  std::array<std::uint8_t, kAesNonceBytes> nonce{};
  if (!mi::server::crypto::RandomBytes(nonce.data(), nonce.size())) {
    error = "rng failed";
    crypto_wipe(aes_key.data(), aes_key.size());
    return false;
  }
  std::vector<std::uint8_t> cipher;
  std::array<std::uint8_t, kAesTagBytes> tag{};
  if (!Aes256GcmEncrypt(aes_key, nonce, plain, cipher, tag, error)) {
    crypto_wipe(aes_key.data(), aes_key.size());
    return false;
  }
  if (cipher.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "history record too large";
    crypto_wipe(aes_key.data(), aes_key.size());
    return false;
  }

  out.reserve(kAesLayerHeaderBytes + cipher.size());
  out.insert(out.end(), kAesLayerMagic,
             kAesLayerMagic + sizeof(kAesLayerMagic));
  out.push_back(kAesLayerVersion);
  out.insert(out.end(), nonce.begin(), nonce.end());
  out.insert(out.end(), tag.begin(), tag.end());
  if (!mi::server::proto::WriteBytes(cipher, out)) {
    error = "history record too large";
    crypto_wipe(aes_key.data(), aes_key.size());
    return false;
  }
  crypto_wipe(aes_key.data(), aes_key.size());
  return true;
}

bool DecodeAesLayer(const std::array<std::uint8_t, 32>& conv_key,
                    bool is_group,
                    const std::string& conv_id,
                    const std::vector<std::uint8_t>& input,
                    std::vector<std::uint8_t>& out_plain,
                    bool& out_used_aes,
                    std::string& error) {
  error.clear();
  out_plain.clear();
  out_used_aes = false;
  if (input.size() < kAesLayerHeaderBytes) {
    out_plain = input;
    return true;
  }
  if (std::memcmp(input.data(), kAesLayerMagic,
                  sizeof(kAesLayerMagic)) != 0) {
    out_plain = input;
    return true;
  }
  std::size_t off = sizeof(kAesLayerMagic);
  const std::uint8_t version = input[off++];
  if (version != kAesLayerVersion) {
    error = "history version mismatch";
    return false;
  }
  if (off + kAesNonceBytes + kAesTagBytes > input.size()) {
    error = "history read failed";
    return false;
  }
  std::array<std::uint8_t, kAesNonceBytes> nonce{};
  std::memcpy(nonce.data(), input.data() + off, nonce.size());
  off += nonce.size();
  std::array<std::uint8_t, kAesTagBytes> tag{};
  std::memcpy(tag.data(), input.data() + off, tag.size());
  off += tag.size();
  std::vector<std::uint8_t> cipher;
  if (!mi::server::proto::ReadBytes(input, off, cipher) ||
      off != input.size()) {
    error = "history read failed";
    return false;
  }
  std::array<std::uint8_t, 32> aes_key{};
  if (!DeriveWhiteboxAesKey(conv_key, is_group, conv_id, aes_key, error)) {
    return false;
  }
  std::vector<std::uint8_t> plain;
  if (!Aes256GcmDecrypt(aes_key, nonce, cipher, tag, plain, error)) {
    crypto_wipe(aes_key.data(), aes_key.size());
    return false;
  }
  crypto_wipe(aes_key.data(), aes_key.size());
  out_plain = std::move(plain);
  out_used_aes = true;
  return true;
}

bool WriteMultiWrappedRecord(std::ofstream& out,
                             const std::array<std::uint8_t, 32>& master_key,
                             const std::vector<std::uint8_t>& payload,
                             std::string& error);

bool WriteEncryptedRecord(std::ofstream& out,
                          const std::array<std::uint8_t, 32>& master_key,
                          const std::array<std::uint8_t, 32>& conv_key,
                          bool is_group,
                          const std::string& conv_id,
                          const std::vector<std::uint8_t>& inner_plain,
                          std::uint8_t format_version,
                          std::string& error) {
  error.clear();
  if (!out) {
    error = "history write failed";
    return false;
  }
  if (IsAllZero(master_key.data(), master_key.size()) ||
      IsAllZero(conv_key.data(), conv_key.size())) {
    error = "history key invalid";
    return false;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }

  std::vector<std::uint8_t> compressed;
  if (!EncodeCompressionLayer(inner_plain, compressed, error)) {
    return false;
  }

  std::vector<std::uint8_t> padded;
  if (!PadPlain(compressed, padded, error)) {
    return false;
  }

  std::vector<std::uint8_t> aes_layer;
  if (!EncodeAesLayer(conv_key, is_group, conv_id, padded, aes_layer, error)) {
    return false;
  }

  std::array<std::uint8_t, 24> inner_nonce{};
  if (!mi::server::crypto::RandomBytes(inner_nonce.data(),
                                       inner_nonce.size())) {
    error = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> inner_cipher(aes_layer.size());
  std::array<std::uint8_t, 16> inner_mac{};
  crypto_aead_lock(inner_cipher.data(), inner_mac.data(), conv_key.data(),
                   inner_nonce.data(), nullptr, 0, aes_layer.data(),
                   aes_layer.size());

  std::vector<std::uint8_t> outer_plain;
  outer_plain.reserve(1 + 2 + conv_id.size() + inner_nonce.size() + 4 +
                      inner_cipher.size() + inner_mac.size());
  outer_plain.push_back(is_group ? 1 : 0);
  if (!mi::server::proto::WriteString(conv_id, outer_plain)) {
    error = "conv id too long";
    return false;
  }
  outer_plain.insert(outer_plain.end(), inner_nonce.begin(),
                     inner_nonce.end());
  if (!mi::server::proto::WriteBytes(inner_cipher, outer_plain)) {
    error = "history record too large";
    return false;
  }
  outer_plain.insert(outer_plain.end(), inner_mac.begin(), inner_mac.end());

  std::array<std::uint8_t, 24> outer_nonce{};
  if (!mi::server::crypto::RandomBytes(outer_nonce.data(),
                                       outer_nonce.size())) {
    error = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> outer_cipher(outer_plain.size());
  std::array<std::uint8_t, 16> outer_mac{};
  crypto_aead_lock(outer_cipher.data(), outer_mac.data(), master_key.data(),
                   outer_nonce.data(), nullptr, 0, outer_plain.data(),
                   outer_plain.size());

  std::vector<std::uint8_t> outer_blob;
  outer_blob.reserve(outer_nonce.size() + outer_cipher.size() + outer_mac.size());
  outer_blob.insert(outer_blob.end(), outer_nonce.begin(), outer_nonce.end());
  outer_blob.insert(outer_blob.end(), outer_cipher.begin(), outer_cipher.end());
  outer_blob.insert(outer_blob.end(), outer_mac.begin(), outer_mac.end());

  if (format_version >= kContainerVersionV2) {
    return WriteMultiWrappedRecord(out, master_key, outer_blob, error);
  }

  if (outer_cipher.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "history record too large";
    return false;
  }
  const std::uint32_t cipher_len =
      static_cast<std::uint32_t>(outer_cipher.size());
  if (!WriteUint32(out, cipher_len)) {
    error = "history write failed";
    return false;
  }
  out.write(reinterpret_cast<const char*>(outer_nonce.data()),
            static_cast<std::streamsize>(outer_nonce.size()));
  out.write(reinterpret_cast<const char*>(outer_cipher.data()),
            static_cast<std::streamsize>(outer_cipher.size()));
  out.write(reinterpret_cast<const char*>(outer_mac.data()),
            static_cast<std::streamsize>(outer_mac.size()));
  if (!out.good()) {
    error = "history write failed";
    return false;
  }
  return true;
}

bool ReadOuterRecord(std::ifstream& in,
                     const std::array<std::uint8_t, 32>& master_key,
                     bool& out_has_record,
                     bool& out_is_group,
                     std::string& out_conv_id,
                     std::array<std::uint8_t, 24>& out_inner_nonce,
                     std::vector<std::uint8_t>& out_inner_cipher,
                     std::array<std::uint8_t, 16>& out_inner_mac,
                     std::string& error) {
  error.clear();
  out_has_record = false;
  out_is_group = false;
  out_conv_id.clear();
  out_inner_nonce.fill(0);
  out_inner_cipher.clear();
  out_inner_mac.fill(0);

  if (IsAllZero(master_key.data(), master_key.size())) {
    error = "history key invalid";
    return false;
  }
  if (!in) {
    error = "history read failed";
    return false;
  }

  std::uint32_t cipher_len = 0;
  if (!ReadUint32(in, cipher_len)) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }
  if (cipher_len == 0 || cipher_len > kMaxRecordCipherLen) {
    error = "history record size invalid";
    return false;
  }

  std::array<std::uint8_t, 24> nonce{};
  if (!ReadExact(in, nonce.data(), nonce.size())) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }

  std::vector<std::uint8_t> cipher;
  cipher.resize(cipher_len);
  if (!ReadExact(in, cipher.data(), cipher.size())) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }

  std::array<std::uint8_t, 16> mac{};
  if (!ReadExact(in, mac.data(), mac.size())) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }

  std::vector<std::uint8_t> outer_plain(cipher.size());
  const int ok = crypto_aead_unlock(outer_plain.data(), mac.data(),
                                    master_key.data(), nonce.data(), nullptr,
                                    0, cipher.data(), cipher.size());
  if (ok != 0) {
    error = "history auth failed";
    return false;
  }

  std::string parse_err;
  if (!ParseOuterPlain(outer_plain, out_is_group, out_conv_id,
                       out_inner_nonce, out_inner_cipher, out_inner_mac,
                       parse_err)) {
    error = parse_err.empty() ? "history read failed" : parse_err;
    return false;
  }
  out_has_record = true;
  return true;
}

bool DeriveWrapSlotKey(const std::array<std::uint8_t, 32>& master_key,
                       std::uint32_t slot,
                       std::array<std::uint8_t, 32>& out_key,
                       std::string& error) {
  error.clear();
  out_key.fill(0);
  if (IsAllZero(master_key.data(), master_key.size())) {
    error = "history key invalid";
    return false;
  }
  std::vector<std::uint8_t> info;
  static constexpr char kPrefix[] = "MI_E2EE_HISTORY_WRAP_SLOT_V1";
  info.insert(info.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  info.push_back(0);
  info.push_back(static_cast<std::uint8_t>(slot & 0xFF));
  info.push_back(static_cast<std::uint8_t>((slot >> 8) & 0xFF));
  info.push_back(static_cast<std::uint8_t>((slot >> 16) & 0xFF));
  info.push_back(static_cast<std::uint8_t>((slot >> 24) & 0xFF));

  std::array<std::uint8_t, 32> salt{};
  static constexpr char kSalt[] = "MI_E2EE_HISTORY_WRAP_SALT_V1";
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(reinterpret_cast<const std::uint8_t*>(kSalt),
                             sizeof(kSalt) - 1, d);
  salt = d.bytes;

  if (!mi::server::crypto::HkdfSha256(master_key.data(), master_key.size(),
                                      salt.data(), salt.size(), info.data(),
                                      info.size(), out_key.data(),
                                      out_key.size())) {
    error = "history hkdf failed";
    return false;
  }
  return true;
}

bool WriteMultiWrappedRecord(std::ofstream& out,
                             const std::array<std::uint8_t, 32>& master_key,
                             const std::vector<std::uint8_t>& payload,
                             std::string& error) {
  error.clear();
  if (!out) {
    error = "history write failed";
    return false;
  }
  if (IsAllZero(master_key.data(), master_key.size())) {
    error = "history key invalid";
    return false;
  }
  if (payload.empty()) {
    error = "history record empty";
    return false;
  }
  if (payload.size() > (kMaxRecordCipherLen + 64u)) {
    error = "history record too large";
    return false;
  }

  std::array<std::uint8_t, kWrapKeyBytes> wrap_key{};
  if (!mi::server::crypto::RandomBytes(wrap_key.data(), wrap_key.size())) {
    error = "rng failed";
    return false;
  }

  struct WrapSlot {
    std::array<std::uint8_t, kWrapSlotNonceBytes> nonce{};
    std::array<std::uint8_t, kWrapSlotCipherBytes> cipher{};
    std::array<std::uint8_t, kWrapSlotMacBytes> mac{};
  };

  std::array<WrapSlot, kWrapSlotCount> slots{};
  for (std::size_t i = 0; i < slots.size(); ++i) {
    std::array<std::uint8_t, 32> slot_key{};
    std::string slot_err;
    if (!DeriveWrapSlotKey(master_key, static_cast<std::uint32_t>(i),
                           slot_key, slot_err)) {
      error = slot_err.empty() ? "history hkdf failed" : slot_err;
      return false;
    }
    if (!mi::server::crypto::RandomBytes(slots[i].nonce.data(),
                                         slots[i].nonce.size())) {
      error = "rng failed";
      return false;
    }
    crypto_aead_lock(slots[i].cipher.data(), slots[i].mac.data(),
                     slot_key.data(), slots[i].nonce.data(), nullptr, 0,
                     wrap_key.data(), wrap_key.size());
    crypto_wipe(slot_key.data(), slot_key.size());
  }

  std::array<std::uint8_t, kWrapNonceBytes> wrap_nonce{};
  if (!mi::server::crypto::RandomBytes(wrap_nonce.data(),
                                       wrap_nonce.size())) {
    error = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> wrap_cipher(payload.size());
  std::array<std::uint8_t, kWrapMacBytes> wrap_mac{};
  crypto_aead_lock(wrap_cipher.data(), wrap_mac.data(), wrap_key.data(),
                   wrap_nonce.data(), nullptr, 0, payload.data(),
                   payload.size());

  std::vector<std::uint8_t> record;
  record.reserve(kWrapHeaderBytes +
                 slots.size() * (kWrapSlotNonceBytes + kWrapSlotCipherBytes +
                                 kWrapSlotMacBytes) +
                 kWrapNonceBytes + 4 + wrap_cipher.size() + kWrapMacBytes);
  record.insert(record.end(), kWrapMagic, kWrapMagic + sizeof(kWrapMagic));
  record.push_back(kWrapVersion);
  record.push_back(static_cast<std::uint8_t>(slots.size()));
  record.push_back(0);
  record.push_back(0);
  for (const auto& slot : slots) {
    record.insert(record.end(), slot.nonce.begin(), slot.nonce.end());
    record.insert(record.end(), slot.cipher.begin(), slot.cipher.end());
    record.insert(record.end(), slot.mac.begin(), slot.mac.end());
  }
  record.insert(record.end(), wrap_nonce.begin(), wrap_nonce.end());
  if (!mi::server::proto::WriteBytes(wrap_cipher, record)) {
    error = "history record too large";
    return false;
  }
  record.insert(record.end(), wrap_mac.begin(), wrap_mac.end());

  if (record.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "history record too large";
    return false;
  }
  const std::uint32_t record_len =
      static_cast<std::uint32_t>(record.size());
  if (!WriteUint32(out, record_len)) {
    error = "history write failed";
    return false;
  }
  out.write(reinterpret_cast<const char*>(record.data()),
            static_cast<std::streamsize>(record.size()));
  if (!out.good()) {
    error = "history write failed";
    return false;
  }
  return true;
}

bool ReadOuterRecordV2(std::ifstream& in,
                       const std::array<std::uint8_t, 32>& master_key,
                       bool& out_has_record,
                       bool& out_is_group,
                       std::string& out_conv_id,
                       std::array<std::uint8_t, 24>& out_inner_nonce,
                       std::vector<std::uint8_t>& out_inner_cipher,
                       std::array<std::uint8_t, 16>& out_inner_mac,
                       std::string& error) {
  error.clear();
  out_has_record = false;
  out_is_group = false;
  out_conv_id.clear();
  out_inner_nonce.fill(0);
  out_inner_cipher.clear();
  out_inner_mac.fill(0);

  if (IsAllZero(master_key.data(), master_key.size())) {
    error = "history key invalid";
    return false;
  }
  if (!in) {
    error = "history read failed";
    return false;
  }

  std::uint32_t record_len = 0;
  if (!ReadUint32(in, record_len)) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }
  if (record_len == 0 || record_len > kMaxWrapRecordBytes) {
    error = "history record size invalid";
    return false;
  }

  std::vector<std::uint8_t> record;
  record.resize(record_len);
  if (!ReadExact(in, record.data(), record.size())) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }

  std::size_t off = 0;
  if (record.size() < kWrapHeaderBytes ||
      std::memcmp(record.data(), kWrapMagic, sizeof(kWrapMagic)) != 0) {
    error = "history magic mismatch";
    return false;
  }
  off += sizeof(kWrapMagic);
  const std::uint8_t version = record[off++];
  const std::uint8_t slot_count = record[off++];
  off += 2;
  if (version != kWrapVersion || slot_count == 0 ||
      slot_count > static_cast<std::uint8_t>(kWrapSlotCount)) {
    error = "history version mismatch";
    return false;
  }
  const std::size_t slot_bytes =
      kWrapSlotNonceBytes + kWrapSlotCipherBytes + kWrapSlotMacBytes;
  const std::size_t slot_block = static_cast<std::size_t>(slot_count) * slot_bytes;
  if (off + slot_block + kWrapNonceBytes + 4 + kWrapMacBytes > record.size()) {
    error = "history record size invalid";
    return false;
  }

  struct SlotView {
    std::array<std::uint8_t, kWrapSlotNonceBytes> nonce{};
    std::array<std::uint8_t, kWrapSlotCipherBytes> cipher{};
    std::array<std::uint8_t, kWrapSlotMacBytes> mac{};
  };
  std::vector<SlotView> slots;
  slots.resize(slot_count);
  for (std::size_t i = 0; i < slots.size(); ++i) {
    std::memcpy(slots[i].nonce.data(), record.data() + off,
                slots[i].nonce.size());
    off += slots[i].nonce.size();
    std::memcpy(slots[i].cipher.data(), record.data() + off,
                slots[i].cipher.size());
    off += slots[i].cipher.size();
    std::memcpy(slots[i].mac.data(), record.data() + off,
                slots[i].mac.size());
    off += slots[i].mac.size();
  }

  std::array<std::uint8_t, kWrapNonceBytes> wrap_nonce{};
  std::memcpy(wrap_nonce.data(), record.data() + off, wrap_nonce.size());
  off += wrap_nonce.size();
  std::vector<std::uint8_t> wrap_cipher;
  if (!mi::server::proto::ReadBytes(record, off, wrap_cipher)) {
    error = "history read failed";
    return false;
  }
  if (wrap_cipher.size() > (kMaxRecordCipherLen + 64u)) {
    error = "history record size invalid";
    return false;
  }
  if (off + kWrapMacBytes > record.size()) {
    error = "history read failed";
    return false;
  }
  std::array<std::uint8_t, kWrapMacBytes> wrap_mac{};
  std::memcpy(wrap_mac.data(), record.data() + off, wrap_mac.size());
  off += wrap_mac.size();
  if (off != record.size()) {
    error = "history read failed";
    return false;
  }

  std::array<std::uint8_t, kWrapKeyBytes> wrap_key{};
  bool slot_ok = false;
  for (std::size_t i = 0; i < slots.size(); ++i) {
    std::array<std::uint8_t, 32> slot_key{};
    std::string slot_err;
    if (!DeriveWrapSlotKey(master_key, static_cast<std::uint32_t>(i),
                           slot_key, slot_err)) {
      error = slot_err.empty() ? "history hkdf failed" : slot_err;
      return false;
    }
    std::array<std::uint8_t, kWrapKeyBytes> candidate{};
    const int ok = crypto_aead_unlock(
        candidate.data(), slots[i].mac.data(), slot_key.data(),
        slots[i].nonce.data(), nullptr, 0, slots[i].cipher.data(),
        slots[i].cipher.size());
    crypto_wipe(slot_key.data(), slot_key.size());
    if (ok == 0) {
      wrap_key = candidate;
      slot_ok = true;
      break;
    }
  }
  if (!slot_ok) {
    error = "history auth failed";
    return false;
  }

  std::vector<std::uint8_t> outer_blob(wrap_cipher.size());
  const int ok = crypto_aead_unlock(outer_blob.data(), wrap_mac.data(),
                                    wrap_key.data(), wrap_nonce.data(), nullptr,
                                    0, wrap_cipher.data(), wrap_cipher.size());
  if (ok != 0) {
    error = "history auth failed";
    return false;
  }

  std::string parse_err;
  if (!DecryptOuterBlob(master_key, outer_blob, out_is_group, out_conv_id,
                        out_inner_nonce, out_inner_cipher, out_inner_mac,
                        parse_err)) {
    error = parse_err.empty() ? "history read failed" : parse_err;
    return false;
  }
  out_has_record = true;
  return true;
}

bool ReadFixed16(const std::vector<std::uint8_t>& payload,
                 std::size_t& offset,
                 std::array<std::uint8_t, 16>& out) {
  if (offset + out.size() > payload.size()) {
    return false;
  }
  std::memcpy(out.data(), payload.data() + offset, out.size());
  offset += out.size();
  return true;
}

bool LooksLikeChatEnvelopeId(const std::vector<std::uint8_t>& envelope,
                             std::array<std::uint8_t, 16>& out_msg_id) {
  out_msg_id.fill(0);
  if (envelope.size() < kChatHeaderSize) {
    return false;
  }
  if (std::memcmp(envelope.data(), kChatMagic, sizeof(kChatMagic)) != 0) {
    return false;
  }
  std::size_t off = sizeof(kChatMagic);
  const std::uint8_t version = envelope[off++];
  if (version != kChatVersion) {
    return false;
  }
  off += 1;
  std::memcpy(out_msg_id.data(), envelope.data() + off, out_msg_id.size());
  return true;
}

bool DecodeChatHeaderBrief(const std::vector<std::uint8_t>& payload,
                           std::uint8_t& out_type,
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
  offset += 16;
  return true;
}

void WriteHistorySummaryHeader(ChatHistorySummaryKind kind,
                               std::vector<std::uint8_t>& out) {
  out.clear();
  out.insert(out.end(), kHistorySummaryMagic.begin(),
             kHistorySummaryMagic.end());
  out.push_back(kHistorySummaryVersion);
  out.push_back(static_cast<std::uint8_t>(kind));
}

bool BuildHistorySummaryText(const std::string& text,
                             std::vector<std::uint8_t>& out) {
  WriteHistorySummaryHeader(ChatHistorySummaryKind::kText, out);
  return mi::server::proto::WriteString(text, out);
}

bool BuildHistorySummaryFile(std::uint64_t size,
                             const std::string& file_name,
                             const std::string& file_id,
                             std::vector<std::uint8_t>& out) {
  WriteHistorySummaryHeader(ChatHistorySummaryKind::kFile, out);
  return mi::server::proto::WriteUint64(size, out) &&
         mi::server::proto::WriteString(file_name, out) &&
         mi::server::proto::WriteString(file_id, out);
}

bool BuildHistorySummarySticker(const std::string& sticker_id,
                                std::vector<std::uint8_t>& out) {
  WriteHistorySummaryHeader(ChatHistorySummaryKind::kSticker, out);
  return mi::server::proto::WriteString(sticker_id, out);
}

bool BuildHistorySummaryLocation(std::int32_t lat_e7,
                                 std::int32_t lon_e7,
                                 const std::string& label,
                                 std::vector<std::uint8_t>& out) {
  WriteHistorySummaryHeader(ChatHistorySummaryKind::kLocation, out);
  return mi::server::proto::WriteUint32(
             static_cast<std::uint32_t>(lat_e7), out) &&
         mi::server::proto::WriteUint32(
             static_cast<std::uint32_t>(lon_e7), out) &&
         mi::server::proto::WriteString(label, out);
}

bool BuildHistorySummaryContact(const std::string& username,
                                const std::string& display,
                                std::vector<std::uint8_t>& out) {
  WriteHistorySummaryHeader(ChatHistorySummaryKind::kContactCard, out);
  return mi::server::proto::WriteString(username, out) &&
         mi::server::proto::WriteString(display, out);
}

bool BuildHistorySummaryGroupInvite(const std::string& group_id,
                                    std::vector<std::uint8_t>& out) {
  WriteHistorySummaryHeader(ChatHistorySummaryKind::kGroupInvite, out);
  return mi::server::proto::WriteString(group_id, out);
}

bool BuildEnvelopeSummary(const std::vector<std::uint8_t>& envelope,
                          std::vector<std::uint8_t>& out) {
  out.clear();
  std::uint8_t type = 0;
  std::size_t off = 0;
  if (!DecodeChatHeaderBrief(envelope, type, off)) {
    return false;
  }
  if (type == kChatTypeText) {
    std::string text;
    if (!mi::server::proto::ReadString(envelope, off, text) ||
        off != envelope.size()) {
      return false;
    }
    return BuildHistorySummaryText(text, out);
  }
  if (type == kChatTypeSticker) {
    std::string sticker_id;
    if (!mi::server::proto::ReadString(envelope, off, sticker_id) ||
        off != envelope.size()) {
      return false;
    }
    return BuildHistorySummarySticker(sticker_id, out);
  }
  if (type == kChatTypeFile) {
    std::uint64_t file_size = 0;
    std::string file_name;
    std::string file_id;
    if (!mi::server::proto::ReadUint64(envelope, off, file_size) ||
        !mi::server::proto::ReadString(envelope, off, file_name) ||
        !mi::server::proto::ReadString(envelope, off, file_id)) {
      return false;
    }
    if (off + 32 != envelope.size()) {
      return false;
    }
    return BuildHistorySummaryFile(file_size, file_name, file_id, out);
  }
  if (type == kChatTypeGroupText) {
    std::string group_id;
    std::string text;
    if (!mi::server::proto::ReadString(envelope, off, group_id) ||
        !mi::server::proto::ReadString(envelope, off, text) ||
        off != envelope.size()) {
      return false;
    }
    return BuildHistorySummaryText(text, out);
  }
  if (type == kChatTypeGroupFile) {
    std::string group_id;
    std::uint64_t file_size = 0;
    std::string file_name;
    std::string file_id;
    if (!mi::server::proto::ReadString(envelope, off, group_id) ||
        !mi::server::proto::ReadUint64(envelope, off, file_size) ||
        !mi::server::proto::ReadString(envelope, off, file_name) ||
        !mi::server::proto::ReadString(envelope, off, file_id)) {
      return false;
    }
    if (off + 32 != envelope.size()) {
      return false;
    }
    return BuildHistorySummaryFile(file_size, file_name, file_id, out);
  }
  if (type == kChatTypeGroupInvite) {
    std::string group_id;
    if (!mi::server::proto::ReadString(envelope, off, group_id) ||
        off != envelope.size()) {
      return false;
    }
    return BuildHistorySummaryGroupInvite(group_id, out);
  }
  if (type == kChatTypeRich) {
    if (off + 2 > envelope.size()) {
      return false;
    }
    const std::uint8_t rich_kind = envelope[off++];
    const std::uint8_t flags = envelope[off++];
    const bool has_reply = (flags & kRichFlagHasReply) != 0;
    if (has_reply) {
      std::array<std::uint8_t, 16> reply_to{};
      std::string reply_preview;
      if (!ReadFixed16(envelope, off, reply_to) ||
          !mi::server::proto::ReadString(envelope, off, reply_preview)) {
        return false;
      }
    }
    if (rich_kind == kRichKindText) {
      std::string text;
      if (!mi::server::proto::ReadString(envelope, off, text) ||
          off != envelope.size()) {
        return false;
      }
      return BuildHistorySummaryText(text, out);
    }
    if (rich_kind == kRichKindLocation) {
      std::uint32_t lat_u = 0;
      std::uint32_t lon_u = 0;
      std::string label;
      if (!mi::server::proto::ReadUint32(envelope, off, lat_u) ||
          !mi::server::proto::ReadUint32(envelope, off, lon_u) ||
          !mi::server::proto::ReadString(envelope, off, label) ||
          off != envelope.size()) {
        return false;
      }
      return BuildHistorySummaryLocation(static_cast<std::int32_t>(lat_u),
                                         static_cast<std::int32_t>(lon_u),
                                         label, out);
    }
    if (rich_kind == kRichKindContactCard) {
      std::string username;
      std::string display;
      if (!mi::server::proto::ReadString(envelope, off, username) ||
          !mi::server::proto::ReadString(envelope, off, display) ||
          off != envelope.size()) {
        return false;
      }
      return BuildHistorySummaryContact(username, display, out);
    }
  }
  return false;
}

std::filesystem::path LegacyConversationPath(const std::filesystem::path& conv_dir,
                                             bool is_group,
                                             const std::string& conv_id) {
  std::vector<std::uint8_t> buf;
  buf.reserve(3 + conv_id.size());
  buf.push_back('m');
  buf.push_back(is_group ? 'g' : 'p');
  buf.push_back(0);
  buf.insert(buf.end(), conv_id.begin(), conv_id.end());
  const std::string hex = Sha256HexLower(buf);
  const std::string name =
      std::string(is_group ? "g_" : "p_") + hex.substr(0, 32) + ".bin";
  return conv_dir / name;
}

bool ReadLegacyRecord(std::ifstream& in,
                      const std::array<std::uint8_t, 32>& conv_key,
                      const std::array<std::uint8_t, 32>& master_key,
                      std::vector<std::uint8_t>& out_plain,
                      std::string& error) {
  error.clear();
  out_plain.clear();
  if (!in) {
    error = "history read failed";
    return false;
  }
  if (IsAllZero(conv_key.data(), conv_key.size()) &&
      IsAllZero(master_key.data(), master_key.size())) {
    error = "history key invalid";
    return false;
  }

  std::uint32_t cipher_len = 0;
  if (!ReadUint32(in, cipher_len)) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }
  if (cipher_len == 0 || cipher_len > kMaxRecordCipherLen) {
    error = "history record size invalid";
    return false;
  }

  std::array<std::uint8_t, 24> nonce{};
  if (!ReadExact(in, nonce.data(), nonce.size())) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }

  std::vector<std::uint8_t> cipher;
  cipher.resize(cipher_len);
  if (!ReadExact(in, cipher.data(), cipher.size())) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }

  std::array<std::uint8_t, 16> mac{};
  if (!ReadExact(in, mac.data(), mac.size())) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }

  const auto try_dec = [&](const std::array<std::uint8_t, 32>& key) -> bool {
    if (IsAllZero(key.data(), key.size())) {
      return false;
    }
    std::vector<std::uint8_t> plain(cipher.size());
    const int ok = crypto_aead_unlock(plain.data(), mac.data(), key.data(),
                                      nonce.data(), nullptr, 0, cipher.data(),
                                      cipher.size());
    if (ok != 0) {
      return false;
    }
    out_plain = std::move(plain);
    return true;
  };

  if (try_dec(conv_key)) {
    return true;
  }
  if (try_dec(master_key)) {
    return true;
  }
  error = "history auth failed";
  out_plain.clear();
  return false;
}

}  // namespace

std::uint32_t ChatHistoryStore::EffectiveSeq(const HistoryFileEntry& entry) {
  return entry.has_internal_seq ? entry.internal_seq : entry.seq;
}

void ChatHistoryStore::UpdateEntryStats(HistoryFileEntry& entry,
                                        std::uint64_t ts,
                                        bool is_message) {
  if (ts != 0) {
    if (entry.min_ts == 0 || ts < entry.min_ts) {
      entry.min_ts = ts;
    }
    if (ts > entry.max_ts) {
      entry.max_ts = ts;
    }
  }
  entry.record_count++;
  if (is_message) {
    entry.message_count++;
  }
}

void ChatHistoryStore::UpdateConvStats(HistoryFileEntry& entry,
                                       const std::string& conv_key,
                                       std::uint64_t ts,
                                       bool is_message) {
  if (conv_key.empty()) {
    return;
  }
  auto& stats = entry.conv_stats[conv_key];
  if (ts != 0) {
    if (stats.min_ts == 0 || ts < stats.min_ts) {
      stats.min_ts = ts;
    }
    if (ts > stats.max_ts) {
      stats.max_ts = ts;
    }
  }
  stats.record_count++;
  if (is_message) {
    stats.message_count++;
  }
  if (!entry.conv_keys.empty() &&
      entry.conv_stats.size() >= entry.conv_keys.size()) {
    entry.conv_stats_complete = true;
  }
}

void ChatHistoryStore::ValidateFileChain(std::vector<HistoryFileEntry>& files) {
  std::array<std::uint8_t, 32> prev_hash{};
  std::array<std::uint8_t, 16> prev_uuid{};
  std::uint32_t prev_seq = 0;
  bool have_prev = false;
  for (auto& entry : files) {
    entry.chain_valid = true;
    const bool have_uuid =
        !IsAllZero(entry.file_uuid.data(), entry.file_uuid.size());
    if (!have_uuid) {
      entry.chain_valid = false;
    }
    if (!have_prev) {
      if (entry.has_prev_hash &&
          !IsAllZero(entry.prev_hash.data(), entry.prev_hash.size())) {
        entry.chain_valid = false;
      }
    } else {
      if (!entry.has_prev_hash) {
        entry.chain_valid = false;
      } else {
        const auto expected =
            ComputeFileChainHash(prev_uuid, prev_seq, prev_hash);
        if (expected != entry.prev_hash) {
          entry.chain_valid = false;
        }
      }
    }
    prev_uuid = entry.file_uuid;
    prev_seq = EffectiveSeq(entry);
    prev_hash = entry.has_prev_hash ? entry.prev_hash
                                    : std::array<std::uint8_t, 32>{};
    have_prev = true;
  }
}

ChatHistoryStore::ChatHistoryStore() = default;

ChatHistoryStore::~ChatHistoryStore() {
  ReleaseProfileLock();
  if (key_loaded_) {
    crypto_wipe(master_key_.data(), master_key_.size());
  }
  if (tag_key_loaded_) {
    crypto_wipe(tag_key_.data(), tag_key_.size());
  }
}

bool ChatHistoryStore::Init(const std::filesystem::path& e2ee_state_dir,
                            const std::string& username,
                            std::string& error) {
  error.clear();
  e2ee_state_dir_ = e2ee_state_dir;
  user_dir_.clear();
  key_path_.clear();
  legacy_conv_dir_.clear();
  history_dir_.clear();
  user_tag_.clear();
  legacy_tag_.clear();
  legacy_tag_alt_.clear();
  profiles_path_.clear();
  profiles_lock_path_.clear();
  profile_lock_path_.clear();
  profile_lock_.reset();
  index_path_.clear();
  journal_path_.clear();
  attachments_dir_.clear();
  attachments_index_path_.clear();
  history_files_.clear();
  conv_to_file_.clear();
  attachments_.clear();
  next_seq_ = 1;
  key_loaded_ = false;
  tag_key_loaded_ = false;
  index_dirty_ = false;
  read_only_ = false;
  attachments_loaded_ = false;
  attachments_dirty_ = false;
  master_key_.fill(0);
  tag_key_.fill(0);
  profile_id_.fill(0);

  if (e2ee_state_dir_.empty()) {
    error = "state dir empty";
    return false;
  }
  if (username.empty()) {
    error = "username empty";
    return false;
  }

  std::vector<std::uint8_t> user_bytes(username.begin(), username.end());
  const std::string user_hash = Sha256HexLower(user_bytes);
  if (user_hash.empty()) {
    error = "username hash failed";
    return false;
  }

  const std::filesystem::path history_root = e2ee_state_dir_ / "history";
  profiles_path_ = history_root / "profiles.idx";
  profiles_lock_path_ = history_root / "profiles.lock";
  tag_key_path_ = history_root / "tag_key.bin";
  const std::filesystem::path legacy_user_dir =
      history_root / user_hash.substr(0, 32);
  legacy_conv_dir_ = legacy_user_dir / "conversations";
  key_path_ = legacy_user_dir / "history_key.bin";
  legacy_tag_alt_ =
      user_hash.substr(0, std::min<std::size_t>(16, user_hash.size()));
  std::string tag_err;
  if (!EnsureTagKeyLoaded(tag_err)) {
    error = tag_err.empty() ? "history tag key load failed" : tag_err;
    return false;
  }
  if (!EnsureProfileLoaded(username, tag_err)) {
    error = tag_err.empty() ? "history profile load failed" : tag_err;
    return false;
  }
  user_tag_ = BytesToHexLower(profile_id_.data(), profile_id_.size());
  std::string derived_tag;
  if (DeriveUserTag(username, derived_tag, tag_err)) {
    legacy_tag_ = derived_tag;
  } else {
    legacy_tag_ = legacy_tag_alt_;
  }
  if (legacy_tag_ == user_tag_) {
    legacy_tag_.clear();
  }
  if (legacy_tag_alt_ == user_tag_ || legacy_tag_alt_ == legacy_tag_) {
    legacy_tag_alt_.clear();
  }
  if (!kEnableLegacyHistoryCompat) {
    legacy_tag_.clear();
    legacy_tag_alt_.clear();
  }

  user_dir_ = history_root / ("profile_" + user_tag_);
  key_path_ = user_dir_ / "history_key.bin";
  index_path_ = user_dir_ / "history_index.bin";
  journal_path_ = user_dir_ / "history_journal.bin";
  profile_lock_path_ = user_dir_ / "profile.lock";
  attachments_dir_ = user_dir_ / "attachments";
  attachments_index_path_ = user_dir_ / "attachments_index.bin";
  std::filesystem::path base_dir = e2ee_state_dir_.parent_path();
  if (base_dir.empty()) {
    base_dir = e2ee_state_dir_;
  }
  std::filesystem::path legacy_history_dir;
  if (!base_dir.empty() &&
      ToLowerAscii(base_dir.filename().string()) == "database") {
    history_dir_ = base_dir;
    legacy_history_dir = base_dir / "database";
  } else {
    history_dir_ = base_dir / "database";
  }

  std::error_code ec;
  std::filesystem::create_directories(legacy_conv_dir_, ec);
  std::filesystem::create_directories(history_root, ec);
  std::filesystem::create_directories(history_dir_, ec);
  std::filesystem::create_directories(user_dir_, ec);
  std::filesystem::create_directories(attachments_dir_, ec);
  if (!legacy_history_dir.empty() && legacy_history_dir != history_dir_) {
    CopyHistoryFilesIfMissing(legacy_history_dir, history_dir_);
  }
  if (!legacy_tag_.empty()) {
    std::string migrate_err;
    (void)MigrateLegacyHistoryFiles(legacy_tag_, user_tag_, migrate_err);
  }
  if (!legacy_tag_alt_.empty()) {
    std::string migrate_err;
    (void)MigrateLegacyHistoryFiles(legacy_tag_alt_, user_tag_, migrate_err);
  }
  std::error_code key_ec;
  if (!std::filesystem::exists(key_path_, key_ec)) {
    const auto legacy_key = legacy_user_dir / "history_key.bin";
    if (std::filesystem::exists(legacy_key, key_ec) && !key_ec) {
      std::filesystem::create_directories(user_dir_, key_ec);
      std::filesystem::copy_file(legacy_key, key_path_,
                                 std::filesystem::copy_options::overwrite_existing,
                                 key_ec);
    }
  }
  if (!EnsureKeyLoaded(error)) {
    return false;
  }
  std::string lock_err;
  (void)AcquireProfileLock(lock_err);
  std::string scan_err;
  (void)LoadHistoryFiles(scan_err);
  return true;
}

bool ChatHistoryStore::EnsureKeyLoaded(std::string& error) {
  error.clear();
  if (key_loaded_) {
    return true;
  }
  if (key_path_.empty()) {
    error = "history key path empty";
    return false;
  }

  std::error_code ec;
  if (std::filesystem::exists(key_path_, ec)) {
    if (ec) {
      error = "history key path error";
      return false;
    }
    const auto size = std::filesystem::file_size(key_path_, ec);
    if (ec) {
      error = "history key size stat failed";
      return false;
    }
    if (size > kMaxHistoryKeyFileBytes) {
      error = "history key too large";
      return false;
    }
  } else if (ec) {
    error = "history key path error";
    return false;
  }

  std::vector<std::uint8_t> bytes;
  {
    std::ifstream f(key_path_, std::ios::binary);
    if (f.is_open()) {
      bytes.assign(std::istreambuf_iterator<char>(f),
                   std::istreambuf_iterator<char>());
    }
  }

  if (!bytes.empty()) {
    std::vector<std::uint8_t> plain;
    bool was_dpapi = false;
    static constexpr char kMagic[] = "MI_E2EE_HISTORY_KEY_DPAPI1";
    static constexpr char kEntropy[] = "MI_E2EE_HISTORY_KEY_ENTROPY_V1";
    std::string dpapi_err;
    if (!MaybeUnprotectDpapi(bytes, kMagic, kEntropy, plain, was_dpapi,
                             dpapi_err)) {
      error = dpapi_err.empty() ? "history key unprotect failed" : dpapi_err;
      return false;
    }

    if (plain.size() != master_key_.size()) {
      error = "history key size invalid";
      return false;
    }
    std::memcpy(master_key_.data(), plain.data(), master_key_.size());
    key_loaded_ = true;

#ifdef _WIN32
    if (!was_dpapi) {
      std::vector<std::uint8_t> wrapped;
      std::string wrap_err;
      if (ProtectDpapi(plain, kMagic, kEntropy, wrapped, wrap_err)) {
        std::error_code ec;
        const auto tmp = key_path_.string() + ".tmp";
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (out) {
          out.write(reinterpret_cast<const char*>(wrapped.data()),
                    static_cast<std::streamsize>(wrapped.size()));
          out.close();
          if (out) {
            std::filesystem::rename(tmp, key_path_, ec);
            if (ec) {
              std::filesystem::remove(tmp, ec);
            }
          } else {
            std::filesystem::remove(tmp, ec);
          }
        }
      }
    }
#endif
    return true;
  }

  std::array<std::uint8_t, 32> k{};
  if (!mi::server::crypto::RandomBytes(k.data(), k.size())) {
    error = "rng failed";
    return false;
  }

  std::vector<std::uint8_t> plain(k.begin(), k.end());
  std::vector<std::uint8_t> out_bytes = plain;
#ifdef _WIN32
  static constexpr char kMagic[] = "MI_E2EE_HISTORY_KEY_DPAPI1";
  static constexpr char kEntropy[] = "MI_E2EE_HISTORY_KEY_ENTROPY_V1";
  std::string wrap_err;
  std::vector<std::uint8_t> wrapped;
  if (!ProtectDpapi(plain, kMagic, kEntropy, wrapped, wrap_err)) {
    error = wrap_err.empty() ? "history key protect failed" : wrap_err;
    return false;
  }
  out_bytes = std::move(wrapped);
#endif

  std::error_code ec2;
  std::filesystem::create_directories(user_dir_, ec2);
  const auto tmp = key_path_.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      error = "history key write failed";
      return false;
    }
    out.write(reinterpret_cast<const char*>(out_bytes.data()),
              static_cast<std::streamsize>(out_bytes.size()));
    out.close();
    if (!out) {
      error = "history key write failed";
      std::filesystem::remove(tmp, ec2);
      return false;
    }
  }
  std::filesystem::rename(tmp, key_path_, ec2);
  if (ec2) {
    std::filesystem::remove(tmp, ec2);
    error = "history key write failed";
    return false;
  }

  master_key_ = k;
  key_loaded_ = true;
  return true;
}

bool ChatHistoryStore::EnsureTagKeyLoaded(std::string& error) {
  error.clear();
  if (tag_key_loaded_) {
    return true;
  }
  if (tag_key_path_.empty()) {
    error = "history tag key path empty";
    return false;
  }

  std::error_code ec;
  const auto parent = tag_key_path_.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
  }

  if (std::filesystem::exists(tag_key_path_, ec)) {
    if (ec) {
      error = "history tag key path error";
      return false;
    }
    const auto size = std::filesystem::file_size(tag_key_path_, ec);
    if (ec) {
      error = "history tag key size stat failed";
      return false;
    }
    if (size > kMaxHistoryKeyFileBytes) {
      error = "history tag key too large";
      return false;
    }
  } else if (ec) {
    error = "history tag key path error";
    return false;
  }

  std::vector<std::uint8_t> bytes;
  {
    std::ifstream f(tag_key_path_, std::ios::binary);
    if (f.is_open()) {
      bytes.assign(std::istreambuf_iterator<char>(f),
                   std::istreambuf_iterator<char>());
    }
  }

  std::vector<std::uint8_t> plain;
  bool was_dpapi = false;
#ifdef _WIN32
  static constexpr char kMagic[] = "MI_E2EE_HISTORY_TAG_KEY_DPAPI1";
  static constexpr char kEntropy[] = "MI_E2EE_HISTORY_TAG_KEY_ENTROPY_V1";
  if (!bytes.empty()) {
    std::string unwrap_err;
    if (!MaybeUnprotectDpapi(bytes, kMagic, kEntropy, plain, was_dpapi,
                             unwrap_err)) {
      error = unwrap_err.empty() ? "history tag key read failed" : unwrap_err;
      return false;
    }
  }
#else
  plain = std::move(bytes);
#endif

  if (plain.empty()) {
    plain.resize(kTagKeyBytes);
    if (!mi::server::crypto::RandomBytes(plain.data(), plain.size())) {
      error = "rng failed";
      return false;
    }
  }
  if (plain.size() != kTagKeyBytes) {
    error = "history tag key invalid";
    return false;
  }

#ifdef _WIN32
  std::vector<std::uint8_t> out_bytes;
  if (!was_dpapi) {
    std::string wrap_err;
    if (ProtectDpapi(plain, kMagic, kEntropy, out_bytes, wrap_err)) {
      bytes = std::move(out_bytes);
    }
  }
#endif
  if (bytes.empty()) {
    bytes = plain;
  }

  const auto tmp = tag_key_path_.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      error = "history tag key write failed";
      return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    if (!out) {
      std::filesystem::remove(tmp, ec);
      error = "history tag key write failed";
      return false;
    }
  }
  std::filesystem::rename(tmp, tag_key_path_, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    error = "history tag key write failed";
    return false;
  }

  std::copy(plain.begin(), plain.end(), tag_key_.begin());
  tag_key_loaded_ = true;
  return true;
}

bool ChatHistoryStore::EnsureProfileLoaded(const std::string& username,
                                           std::string& error) {
  error.clear();
  if (!IsAllZero(profile_id_.data(), profile_id_.size())) {
    return true;
  }
  if (username.empty()) {
    error = "username empty";
    return false;
  }
  if (profiles_path_.empty()) {
    error = "history profiles path empty";
    return false;
  }
  if (!tag_key_loaded_ || IsAllZero(tag_key_.data(), tag_key_.size())) {
    error = "history tag key missing";
    return false;
  }

  std::unique_ptr<ProfileLockState> lock;
  std::string lock_err;
  bool locked = false;
  for (int attempt = 0; attempt < 40; ++attempt) {
    if (AcquireFileLock(profiles_lock_path_, lock, lock_err)) {
      locked = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  if (!locked) {
    error = lock_err.empty() ? "history lock failed" : lock_err;
    return false;
  }

  bool has_file = false;
  bool updated = false;
  std::vector<std::pair<std::string, std::array<std::uint8_t, 16>>> entries;
  std::error_code ec;
  if (std::filesystem::exists(profiles_path_, ec)) {
    has_file = !ec;
  }
  if (has_file) {
    std::vector<std::uint8_t> bytes;
    {
      std::ifstream in(profiles_path_, std::ios::binary);
      if (!in.is_open()) {
        error = "history profiles read failed";
        ReleaseFileLock(lock);
        return false;
      }
      bytes.assign(std::istreambuf_iterator<char>(in),
                   std::istreambuf_iterator<char>());
    }
    if (bytes.size() < sizeof(kProfilesFileMagic) + 1 + 3 +
                          kProfilesNonceBytes + 4 + kProfilesMacBytes) {
      error = "history profiles read failed";
      ReleaseFileLock(lock);
      return false;
    }
    std::size_t off = 0;
    if (std::memcmp(bytes.data(), kProfilesFileMagic,
                    sizeof(kProfilesFileMagic)) != 0) {
      error = "history profiles read failed";
      ReleaseFileLock(lock);
      return false;
    }
    off += sizeof(kProfilesFileMagic);
    const std::uint8_t version = bytes[off++];
    if (version != kProfilesVersion) {
      error = "history profiles read failed";
      ReleaseFileLock(lock);
      return false;
    }
    off += 3;
    if (off + kProfilesNonceBytes + 4 > bytes.size()) {
      error = "history profiles read failed";
      ReleaseFileLock(lock);
      return false;
    }
    std::array<std::uint8_t, kProfilesNonceBytes> nonce{};
    std::memcpy(nonce.data(), bytes.data() + off, nonce.size());
    off += nonce.size();
    const std::uint32_t cipher_len =
        static_cast<std::uint32_t>(bytes[off]) |
        (static_cast<std::uint32_t>(bytes[off + 1]) << 8) |
        (static_cast<std::uint32_t>(bytes[off + 2]) << 16) |
        (static_cast<std::uint32_t>(bytes[off + 3]) << 24);
    off += 4;
    if (cipher_len == 0 ||
        off + cipher_len + kProfilesMacBytes != bytes.size()) {
      error = "history profiles read failed";
      ReleaseFileLock(lock);
      return false;
    }
    const std::uint8_t* cipher = bytes.data() + off;
    const std::uint8_t* mac = bytes.data() + off + cipher_len;

    std::array<std::uint8_t, 32> profile_key{};
    std::string key_err;
    if (!DeriveProfilesKey(profile_key, key_err)) {
      error = key_err.empty() ? "history profiles read failed" : key_err;
      ReleaseFileLock(lock);
      return false;
    }
    std::vector<std::uint8_t> plain;
    plain.resize(cipher_len);
    const int ok = crypto_aead_unlock(plain.data(), mac, profile_key.data(),
                                      nonce.data(), nullptr, 0, cipher,
                                      cipher_len);
    crypto_wipe(profile_key.data(), profile_key.size());
    if (ok != 0) {
      error = "history profiles auth failed";
      ReleaseFileLock(lock);
      return false;
    }
    if (plain.size() < sizeof(kProfilesPlainMagic) + 1) {
      error = "history profiles read failed";
      ReleaseFileLock(lock);
      return false;
    }
    std::size_t poff = 0;
    if (std::memcmp(plain.data(), kProfilesPlainMagic,
                    sizeof(kProfilesPlainMagic)) != 0) {
      error = "history profiles read failed";
      ReleaseFileLock(lock);
      return false;
    }
    poff += sizeof(kProfilesPlainMagic);
    const std::uint8_t plain_ver = plain[poff++];
    if (plain_ver != kProfilesVersion) {
      error = "history profiles read failed";
      ReleaseFileLock(lock);
      return false;
    }
    std::uint32_t count = 0;
    if (!mi::server::proto::ReadUint32(plain, poff, count)) {
      error = "history profiles read failed";
      ReleaseFileLock(lock);
      return false;
    }
    if (count > kMaxProfiles) {
      error = "history profiles read failed";
      ReleaseFileLock(lock);
      return false;
    }
    entries.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
      std::string entry_user;
      if (!mi::server::proto::ReadString(plain, poff, entry_user) ||
          entry_user.empty()) {
        error = "history profiles read failed";
        ReleaseFileLock(lock);
        return false;
      }
      if (poff + 16 > plain.size()) {
        error = "history profiles read failed";
        ReleaseFileLock(lock);
        return false;
      }
      std::array<std::uint8_t, 16> pid{};
      std::memcpy(pid.data(), plain.data() + poff, pid.size());
      poff += pid.size();
      entries.emplace_back(std::move(entry_user), pid);
    }
    if (poff != plain.size()) {
      error = "history profiles read failed";
      ReleaseFileLock(lock);
      return false;
    }
  }

  for (const auto& entry : entries) {
    if (entry.first == username) {
      profile_id_ = entry.second;
      ReleaseFileLock(lock);
      return true;
    }
  }

  if (!mi::server::crypto::RandomBytes(profile_id_.data(),
                                       profile_id_.size())) {
    error = "rng failed";
    ReleaseFileLock(lock);
    return false;
  }
  entries.emplace_back(username, profile_id_);
  updated = true;

  if (updated || !has_file) {
    std::vector<std::pair<std::string, std::array<std::uint8_t, 16>>> sorted =
        entries;
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    std::vector<std::uint8_t> plain;
    plain.reserve(64);
    plain.insert(plain.end(), kProfilesPlainMagic,
                 kProfilesPlainMagic + sizeof(kProfilesPlainMagic));
    plain.push_back(kProfilesVersion);
    if (!mi::server::proto::WriteUint32(
            static_cast<std::uint32_t>(sorted.size()), plain)) {
      error = "history profiles write failed";
      ReleaseFileLock(lock);
      return false;
    }
    for (const auto& entry : sorted) {
      if (!mi::server::proto::WriteString(entry.first, plain)) {
        error = "history profiles write failed";
        ReleaseFileLock(lock);
        return false;
      }
      plain.insert(plain.end(), entry.second.begin(), entry.second.end());
    }

    std::array<std::uint8_t, 32> profile_key{};
    std::string key_err;
    if (!DeriveProfilesKey(profile_key, key_err)) {
      error = key_err.empty() ? "history profiles write failed" : key_err;
      ReleaseFileLock(lock);
      return false;
    }
    std::array<std::uint8_t, kProfilesNonceBytes> nonce{};
    if (!mi::server::crypto::RandomBytes(nonce.data(), nonce.size())) {
      crypto_wipe(profile_key.data(), profile_key.size());
      error = "rng failed";
      ReleaseFileLock(lock);
      return false;
    }
    std::vector<std::uint8_t> cipher(plain.size());
    std::array<std::uint8_t, kProfilesMacBytes> mac{};
    crypto_aead_lock(cipher.data(), mac.data(), profile_key.data(),
                     nonce.data(), nullptr, 0, plain.data(), plain.size());
    crypto_wipe(profile_key.data(), profile_key.size());

    std::vector<std::uint8_t> out;
    out.reserve(sizeof(kProfilesFileMagic) + 1 + 3 + nonce.size() + 4 +
                cipher.size() + mac.size());
    out.insert(out.end(), kProfilesFileMagic,
               kProfilesFileMagic + sizeof(kProfilesFileMagic));
    out.push_back(kProfilesVersion);
    out.push_back(0);
    out.push_back(0);
    out.push_back(0);
    out.insert(out.end(), nonce.begin(), nonce.end());
    const std::uint32_t cipher_len =
        static_cast<std::uint32_t>(cipher.size());
    out.push_back(static_cast<std::uint8_t>(cipher_len & 0xFF));
    out.push_back(static_cast<std::uint8_t>((cipher_len >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((cipher_len >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((cipher_len >> 24) & 0xFF));
    out.insert(out.end(), cipher.begin(), cipher.end());
    out.insert(out.end(), mac.begin(), mac.end());

    const auto tmp = profiles_path_.string() + ".tmp";
    {
      std::ofstream fout(tmp, std::ios::binary | std::ios::trunc);
      if (!fout) {
        error = "history profiles write failed";
        ReleaseFileLock(lock);
        return false;
      }
      fout.write(reinterpret_cast<const char*>(out.data()),
                 static_cast<std::streamsize>(out.size()));
      fout.close();
      if (!fout) {
        std::filesystem::remove(tmp, ec);
        error = "history profiles write failed";
        ReleaseFileLock(lock);
        return false;
      }
    }
    std::filesystem::rename(tmp, profiles_path_, ec);
    if (ec) {
      std::filesystem::remove(tmp, ec);
      error = "history profiles write failed";
      ReleaseFileLock(lock);
      return false;
    }
  }

  ReleaseFileLock(lock);
  return true;
}

bool ChatHistoryStore::AcquireProfileLock(std::string& error) {
  error.clear();
  if (read_only_) {
    return true;
  }
  if (profile_lock_) {
    return true;
  }
  if (profile_lock_path_.empty()) {
    error = "history lock path empty";
    return false;
  }
  std::string lock_err;
  if (AcquireFileLock(profile_lock_path_, profile_lock_, lock_err)) {
    return true;
  }
  read_only_ = true;
  error = lock_err.empty() ? "history lock failed" : lock_err;
  return true;
}

void ChatHistoryStore::ReleaseProfileLock() {
  ReleaseFileLock(profile_lock_);
}

bool ChatHistoryStore::DeriveIndexKey(std::array<std::uint8_t, 32>& out_key,
                                      std::string& error) const {
  error.clear();
  out_key.fill(0);
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    error = "history key missing";
    return false;
  }

  static constexpr char kPrefix[] = "MI_E2EE_HISTORY_INDEX_KEY_V1";
  static constexpr char kSalt[] = "MI_E2EE_HISTORY_INDEX_SALT_V1";
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(reinterpret_cast<const std::uint8_t*>(kSalt),
                             sizeof(kSalt) - 1, d);
  const auto& salt = d.bytes;

  if (!mi::server::crypto::HkdfSha256(master_key_.data(), master_key_.size(),
                                      salt.data(), salt.size(),
                                      reinterpret_cast<const std::uint8_t*>(
                                          kPrefix),
                                      sizeof(kPrefix) - 1, out_key.data(),
                                      out_key.size())) {
    error = "history hkdf failed";
    return false;
  }
  return true;
}

bool ChatHistoryStore::DeriveProfilesKey(std::array<std::uint8_t, 32>& out_key,
                                         std::string& error) const {
  error.clear();
  out_key.fill(0);
  if (!tag_key_loaded_ || IsAllZero(tag_key_.data(), tag_key_.size())) {
    error = "history tag key missing";
    return false;
  }

  static constexpr char kPrefix[] = "MI_E2EE_HISTORY_PROFILE_KEY_V1";
  static constexpr char kSalt[] = "MI_E2EE_HISTORY_PROFILE_SALT_V1";
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(reinterpret_cast<const std::uint8_t*>(kSalt),
                             sizeof(kSalt) - 1, d);
  const auto& salt = d.bytes;

  if (!mi::server::crypto::HkdfSha256(tag_key_.data(), tag_key_.size(),
                                      salt.data(), salt.size(),
                                      reinterpret_cast<const std::uint8_t*>(
                                          kPrefix),
                                      sizeof(kPrefix) - 1, out_key.data(),
                                      out_key.size())) {
    error = "history hkdf failed";
    return false;
  }
  return true;
}

bool ChatHistoryStore::DeriveUserTag(const std::string& username,
                                     std::string& out_tag,
                                     std::string& error) const {
  error.clear();
  out_tag.clear();
  if (!tag_key_loaded_ || IsAllZero(tag_key_.data(), tag_key_.size())) {
    error = "history tag key missing";
    return false;
  }
  if (username.empty()) {
    error = "username empty";
    return false;
  }
  out_tag = DeriveUserTagHmac(tag_key_, username);
  if (out_tag.empty()) {
    error = "history tag derivation failed";
    return false;
  }
  return true;
}

bool ChatHistoryStore::LoadHistoryIndex(std::string& error) {
  error.clear();
  history_files_.clear();
  conv_to_file_.clear();
  next_seq_ = 1;
  if (index_path_.empty() || history_dir_.empty() || user_tag_.empty()) {
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(index_path_, ec)) {
    return false;
  }
  if (ec) {
    return false;
  }

  std::vector<std::uint8_t> bytes;
  {
    std::ifstream in(index_path_, std::ios::binary);
    if (!in.is_open()) {
      return false;
    }
    bytes.assign(std::istreambuf_iterator<char>(in),
                 std::istreambuf_iterator<char>());
  }
  if (bytes.size() < sizeof(kIndexFileMagic) + 1 + 3 + kIndexNonceBytes + 4 +
                         kIndexMacBytes) {
    return false;
  }
  std::size_t off = 0;
  if (std::memcmp(bytes.data(), kIndexFileMagic, sizeof(kIndexFileMagic)) !=
      0) {
    return false;
  }
  off += sizeof(kIndexFileMagic);
  const std::uint8_t version = bytes[off++];
  if (version != kIndexVersion && version != kIndexVersionV2) {
    return false;
  }
  off += 3;
  if (off + kIndexNonceBytes + 4 > bytes.size()) {
    return false;
  }
  std::array<std::uint8_t, kIndexNonceBytes> nonce{};
  std::memcpy(nonce.data(), bytes.data() + off, nonce.size());
  off += nonce.size();
  const std::uint32_t cipher_len =
      static_cast<std::uint32_t>(bytes[off]) |
      (static_cast<std::uint32_t>(bytes[off + 1]) << 8) |
      (static_cast<std::uint32_t>(bytes[off + 2]) << 16) |
      (static_cast<std::uint32_t>(bytes[off + 3]) << 24);
  off += 4;
  if (cipher_len == 0 ||
      off + cipher_len + kIndexMacBytes != bytes.size()) {
    return false;
  }
  const std::uint8_t* cipher = bytes.data() + off;
  const std::uint8_t* mac = bytes.data() + off + cipher_len;

  std::array<std::uint8_t, 32> index_key{};
  std::string key_err;
  if (!DeriveIndexKey(index_key, key_err)) {
    return false;
  }
  std::vector<std::uint8_t> plain;
  plain.resize(cipher_len);
  const int ok = crypto_aead_unlock(plain.data(), mac, index_key.data(),
                                    nonce.data(), nullptr, 0, cipher,
                                    cipher_len);
  crypto_wipe(index_key.data(), index_key.size());
  if (ok != 0) {
    return false;
  }
  if (plain.size() < sizeof(kIndexPlainMagic) + 1) {
    return false;
  }
  std::size_t poff = 0;
  if (std::memcmp(plain.data(), kIndexPlainMagic,
                  sizeof(kIndexPlainMagic)) != 0) {
    return false;
  }
  poff += sizeof(kIndexPlainMagic);
  const std::uint8_t plain_ver = plain[poff++];
  if (plain_ver != version) {
    return false;
  }
  std::string idx_tag;
  if (!mi::server::proto::ReadString(plain, poff, idx_tag)) {
    return false;
  }
  if (idx_tag != user_tag_) {
    return false;
  }
  std::uint32_t file_count = 0;
  if (!mi::server::proto::ReadUint32(plain, poff, file_count)) {
    return false;
  }
  if (file_count > 200000) {
    return false;
  }

  std::vector<HistoryFileEntry> files;
  files.reserve(file_count);
  for (std::uint32_t i = 0; i < file_count; ++i) {
    std::string name;
    if (!mi::server::proto::ReadString(plain, poff, name) || name.empty()) {
      return false;
    }
    std::uint32_t seq = 0;
    std::uint32_t internal_seq = 0;
    if (!mi::server::proto::ReadUint32(plain, poff, seq) ||
        !mi::server::proto::ReadUint32(plain, poff, internal_seq)) {
      return false;
    }
    if (poff + 2 > plain.size()) {
      return false;
    }
    const bool has_internal_seq = plain[poff++] != 0;
    const std::uint8_t file_ver = plain[poff++];
    std::string tag;
    if (!mi::server::proto::ReadString(plain, poff, tag)) {
      return false;
    }
    std::uint32_t conv_count = 0;
    if (!mi::server::proto::ReadUint32(plain, poff, conv_count)) {
      return false;
    }
    if (conv_count > 64) {
      return false;
    }
    if (poff + 16 > plain.size()) {
      return false;
    }
    HistoryFileEntry entry;
    entry.path = history_dir_ / name;
    entry.seq = seq;
    entry.internal_seq = internal_seq;
    entry.has_internal_seq = has_internal_seq;
    entry.version = file_ver;
    entry.tag = std::move(tag);
    std::memcpy(entry.file_uuid.data(), plain.data() + poff,
                entry.file_uuid.size());
    poff += entry.file_uuid.size();
    if (poff + 1 + entry.prev_hash.size() + 8 * 4 > plain.size()) {
      return false;
    }
    entry.has_prev_hash = plain[poff++] != 0;
    std::memcpy(entry.prev_hash.data(), plain.data() + poff,
                entry.prev_hash.size());
    poff += entry.prev_hash.size();
    if (!mi::server::proto::ReadUint64(plain, poff, entry.min_ts) ||
        !mi::server::proto::ReadUint64(plain, poff, entry.max_ts) ||
        !mi::server::proto::ReadUint64(plain, poff, entry.record_count) ||
        !mi::server::proto::ReadUint64(plain, poff, entry.message_count)) {
      return false;
    }
    std::uint32_t conv_hash_count = 0;
    if (!mi::server::proto::ReadUint32(plain, poff, conv_hash_count)) {
      return false;
    }
    if (conv_hash_count > 64) {
      return false;
    }
    if (poff + static_cast<std::size_t>(conv_hash_count) * 16 > plain.size()) {
      return false;
    }
    if (conv_hash_count > 0) {
      entry.conv_hashes.resize(conv_hash_count);
      for (std::uint32_t i = 0; i < conv_hash_count; ++i) {
        std::memcpy(entry.conv_hashes[i].data(), plain.data() + poff,
                    entry.conv_hashes[i].size());
        poff += entry.conv_hashes[i].size();
      }
      entry.has_conv_hashes = true;
    } else {
      entry.conv_hashes.clear();
      entry.has_conv_hashes = true;
    }
    for (std::uint32_t c = 0; c < conv_count; ++c) {
      std::string conv_key;
      if (!mi::server::proto::ReadString(plain, poff, conv_key) ||
          conv_key.empty()) {
        return false;
      }
      if (plain_ver >= kIndexVersion) {
        ChatHistoryConvStats stats;
        if (!mi::server::proto::ReadUint64(plain, poff, stats.min_ts) ||
            !mi::server::proto::ReadUint64(plain, poff, stats.max_ts) ||
            !mi::server::proto::ReadUint64(plain, poff, stats.record_count) ||
            !mi::server::proto::ReadUint64(plain, poff, stats.message_count)) {
          return false;
        }
        entry.conv_stats.emplace(conv_key, stats);
      }
      entry.conv_keys.insert(std::move(conv_key));
    }
    entry.conv_keys_complete = true;
    if (plain_ver >= kIndexVersion && !entry.conv_keys.empty() &&
        entry.conv_stats.size() >= entry.conv_keys.size()) {
      entry.conv_stats_complete = true;
    }

    if (!std::filesystem::exists(entry.path, ec) || ec) {
      return false;
    }
    std::ifstream fin(entry.path, std::ios::binary);
    if (!fin.is_open()) {
      return false;
    }
    std::uint32_t container_offset = 0;
    std::uint8_t real_ver = 0;
    std::string hdr_err;
    if (!LocateContainerOffset(fin, container_offset, hdr_err)) {
      return false;
    }
    fin.clear();
    fin.seekg(container_offset, std::ios::beg);
    if (!ReadContainerHeader(fin, real_ver, hdr_err)) {
      return false;
    }
    if (real_ver != kContainerVersionV2) {
      return false;
    }
    entry.version = real_ver;
    files.push_back(std::move(entry));
  }
  if (poff != plain.size()) {
    return false;
  }

  const auto effectiveSeq = [](const HistoryFileEntry& entry) {
    return entry.has_internal_seq ? entry.internal_seq : entry.seq;
  };
  std::uint32_t max_seq = 0;
  for (const auto& f : files) {
    max_seq = std::max(max_seq, effectiveSeq(f));
  }
  next_seq_ = max_seq + 1;
  std::sort(files.begin(), files.end(),
            [&](const HistoryFileEntry& a, const HistoryFileEntry& b) {
              return effectiveSeq(a) < effectiveSeq(b);
            });
  ValidateFileChain(files);
  history_files_ = std::move(files);
  for (std::size_t i = 0; i < history_files_.size(); ++i) {
    for (const auto& key : history_files_[i].conv_keys) {
      conv_to_file_[key] = i;
    }
  }
  RebuildConvHashIndex();
  const bool chain_ok = std::all_of(
      history_files_.begin(), history_files_.end(),
      [](const HistoryFileEntry& entry) { return entry.chain_valid; });
  index_dirty_ = !chain_ok;
  return true;
}

bool ChatHistoryStore::SaveHistoryIndex(std::string& error) {
  error.clear();
  if (!index_dirty_) {
    return true;
  }
  if (read_only_) {
    return true;
  }
  if (index_path_.empty() || history_dir_.empty() || user_tag_.empty()) {
    return true;
  }
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    return true;
  }

  std::vector<std::uint8_t> plain;
  plain.reserve(128);
  plain.insert(plain.end(), kIndexPlainMagic,
               kIndexPlainMagic + sizeof(kIndexPlainMagic));
  plain.push_back(kIndexVersion);
  if (!mi::server::proto::WriteString(user_tag_, plain)) {
    error = "history index write failed";
    return false;
  }
  if (!mi::server::proto::WriteUint32(
          static_cast<std::uint32_t>(history_files_.size()), plain)) {
    error = "history index write failed";
    return false;
  }

  for (auto& entry : history_files_) {
    const std::string name = entry.path.filename().string();
    if (!mi::server::proto::WriteString(name, plain)) {
      error = "history index write failed";
      return false;
    }
    if (!mi::server::proto::WriteUint32(entry.seq, plain) ||
        !mi::server::proto::WriteUint32(entry.internal_seq, plain)) {
      error = "history index write failed";
      return false;
    }
    plain.push_back(entry.has_internal_seq ? 1 : 0);
    plain.push_back(entry.version);
    if (!mi::server::proto::WriteString(entry.tag, plain)) {
      error = "history index write failed";
      return false;
    }
    const std::uint32_t conv_count =
        static_cast<std::uint32_t>(entry.conv_keys.size());
    if (!mi::server::proto::WriteUint32(conv_count, plain)) {
      error = "history index write failed";
      return false;
    }
    plain.insert(plain.end(), entry.file_uuid.begin(), entry.file_uuid.end());
    plain.push_back(entry.has_prev_hash ? 1 : 0);
    plain.insert(plain.end(), entry.prev_hash.begin(), entry.prev_hash.end());
    mi::server::proto::WriteUint64(entry.min_ts, plain);
    mi::server::proto::WriteUint64(entry.max_ts, plain);
    mi::server::proto::WriteUint64(entry.record_count, plain);
    mi::server::proto::WriteUint64(entry.message_count, plain);
    std::vector<std::array<std::uint8_t, 16>> conv_hashes = entry.conv_hashes;
    if (!entry.has_conv_hashes && tag_key_loaded_ &&
        !IsAllZero(tag_key_.data(), tag_key_.size()) &&
        !entry.conv_keys.empty()) {
      conv_hashes.reserve(entry.conv_keys.size());
      for (const auto& key : entry.conv_keys) {
        conv_hashes.push_back(DeriveConvHash(tag_key_, key));
      }
      std::sort(conv_hashes.begin(), conv_hashes.end(),
                [](const auto& a, const auto& b) { return a < b; });
      entry.conv_hashes = conv_hashes;
      entry.has_conv_hashes = true;
    }
    if (!mi::server::proto::WriteUint32(
            static_cast<std::uint32_t>(conv_hashes.size()), plain)) {
      error = "history index write failed";
      return false;
    }
    for (const auto& h : conv_hashes) {
      plain.insert(plain.end(), h.begin(), h.end());
    }
    std::vector<std::string> conv_keys;
    conv_keys.reserve(entry.conv_keys.size());
    for (const auto& key : entry.conv_keys) {
      conv_keys.push_back(key);
    }
    std::sort(conv_keys.begin(), conv_keys.end());
    for (const auto& key : conv_keys) {
      if (!mi::server::proto::WriteString(key, plain)) {
        error = "history index write failed";
        return false;
      }
      if (kIndexVersion >= 3) {
        auto it = entry.conv_stats.find(key);
        ChatHistoryConvStats stats;
        if (it != entry.conv_stats.end()) {
          stats = it->second;
        }
        mi::server::proto::WriteUint64(stats.min_ts, plain);
        mi::server::proto::WriteUint64(stats.max_ts, plain);
        mi::server::proto::WriteUint64(stats.record_count, plain);
        mi::server::proto::WriteUint64(stats.message_count, plain);
      }
    }
  }

  if (plain.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "history index write failed";
    return false;
  }
  std::array<std::uint8_t, 32> index_key{};
  std::string key_err;
  if (!DeriveIndexKey(index_key, key_err)) {
    error = key_err.empty() ? "history index write failed" : key_err;
    return false;
  }
  std::array<std::uint8_t, kIndexNonceBytes> nonce{};
  if (!mi::server::crypto::RandomBytes(nonce.data(), nonce.size())) {
    crypto_wipe(index_key.data(), index_key.size());
    error = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> cipher(plain.size());
  std::array<std::uint8_t, kIndexMacBytes> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), index_key.data(), nonce.data(),
                   nullptr, 0, plain.data(), plain.size());
  crypto_wipe(index_key.data(), index_key.size());

  std::vector<std::uint8_t> out;
  out.reserve(sizeof(kIndexFileMagic) + 1 + 3 + nonce.size() + 4 +
              cipher.size() + mac.size());
  out.insert(out.end(), kIndexFileMagic,
             kIndexFileMagic + sizeof(kIndexFileMagic));
  out.push_back(kIndexVersion);
  out.push_back(0);
  out.push_back(0);
  out.push_back(0);
  out.insert(out.end(), nonce.begin(), nonce.end());
  const std::uint32_t cipher_len =
      static_cast<std::uint32_t>(cipher.size());
  out.push_back(static_cast<std::uint8_t>(cipher_len & 0xFF));
  out.push_back(static_cast<std::uint8_t>((cipher_len >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((cipher_len >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((cipher_len >> 24) & 0xFF));
  out.insert(out.end(), cipher.begin(), cipher.end());
  out.insert(out.end(), mac.begin(), mac.end());

  std::error_code ec;
  const auto tmp = index_path_.string() + ".tmp";
  {
    std::ofstream fout(tmp, std::ios::binary | std::ios::trunc);
    if (!fout) {
      error = "history index write failed";
      return false;
    }
    fout.write(reinterpret_cast<const char*>(out.data()),
               static_cast<std::streamsize>(out.size()));
    fout.close();
    if (!fout) {
      std::filesystem::remove(tmp, ec);
      error = "history index write failed";
      return false;
    }
  }
  std::filesystem::rename(tmp, index_path_, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    error = "history index write failed";
    return false;
  }
  index_dirty_ = false;
  ClearHistoryJournal();
  return true;
}

void ChatHistoryStore::RebuildConvHashIndex() {
  conv_hash_to_files_.clear();
  if (!tag_key_loaded_ || IsAllZero(tag_key_.data(), tag_key_.size())) {
    return;
  }
  for (std::size_t i = 0; i < history_files_.size(); ++i) {
    auto& entry = history_files_[i];
    if (!entry.has_conv_hashes || entry.conv_hashes.empty()) {
      if (!entry.conv_keys.empty()) {
        entry.conv_hashes.clear();
        entry.conv_hashes.reserve(entry.conv_keys.size());
        for (const auto& key : entry.conv_keys) {
          entry.conv_hashes.push_back(DeriveConvHash(tag_key_, key));
        }
        std::sort(entry.conv_hashes.begin(), entry.conv_hashes.end(),
                  [](const auto& a, const auto& b) { return a < b; });
        entry.has_conv_hashes = true;
      }
    }
    if (!entry.conv_stats_complete && !entry.conv_keys.empty()) {
      for (const auto& key : entry.conv_keys) {
        if (entry.conv_stats.find(key) != entry.conv_stats.end()) {
          continue;
        }
        const auto h = DeriveConvHash(tag_key_, key);
        const std::string hk = ConvHashKey(h);
        auto hit = entry.conv_stats.find(hk);
        if (hit != entry.conv_stats.end()) {
          entry.conv_stats.emplace(key, hit->second);
        }
      }
      if (entry.conv_stats.size() >= entry.conv_keys.size()) {
        entry.conv_stats_complete = true;
      }
    }
    if (entry.has_conv_hashes) {
      for (const auto& h : entry.conv_hashes) {
        const std::string k = ConvHashKey(h);
        auto& list = conv_hash_to_files_[k];
        if (std::find(list.begin(), list.end(), i) == list.end()) {
          list.push_back(i);
        }
      }
    }
  }
}

bool ChatHistoryStore::ScanFileForConversations(HistoryFileEntry& entry,
                                                std::string& error) {
  error.clear();
  if (entry.conv_keys_complete) {
    return true;
  }
  if (entry.path.empty()) {
    return false;
  }
  std::ifstream in(entry.path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }
  std::uint32_t container_offset = 0;
  std::string hdr_err;
  if (!LocateContainerOffset(in, container_offset, hdr_err)) {
    return false;
  }
  in.clear();
  in.seekg(container_offset, std::ios::beg);
  std::uint8_t version = 0;
  if (!ReadContainerHeader(in, version, hdr_err)) {
    return false;
  }
  if (version != kContainerVersionV2) {
    return false;
  }
  entry.version = version;
  (void)ConsumeMih3Header(in, master_key_, nullptr);

  while (true) {
    bool has_record = false;
    bool rec_group = false;
    std::string rec_conv;
    std::array<std::uint8_t, 24> inner_nonce{};
    std::vector<std::uint8_t> inner_cipher;
    std::array<std::uint8_t, 16> inner_mac{};
    std::string rec_err;
    const bool record_ok =
        (version >= kContainerVersionV2)
            ? ReadOuterRecordV2(in, master_key_, has_record, rec_group,
                                rec_conv, inner_nonce, inner_cipher, inner_mac,
                                rec_err)
            : ReadOuterRecord(in, master_key_, has_record, rec_group, rec_conv,
                              inner_nonce, inner_cipher, inner_mac, rec_err);
    if (!record_ok || !has_record) {
      break;
    }
    if (rec_conv.empty() || rec_conv == kFileMetaConvId) {
      continue;
    }
    entry.conv_keys.insert(MakeConvKey(rec_group, rec_conv));
  }
  entry.conv_keys_complete = true;
  if (!entry.conv_keys.empty()) {
    index_dirty_ = true;
  }
  return true;
}

bool ChatHistoryStore::ScanFileForConvStats(HistoryFileEntry& entry,
                                            std::string& error) {
  error.clear();
  if (entry.path.empty()) {
    return false;
  }
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    return false;
  }
  std::ifstream in(entry.path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }
  std::uint32_t container_offset = 0;
  std::string hdr_err;
  if (!LocateContainerOffset(in, container_offset, hdr_err)) {
    return false;
  }
  in.clear();
  in.seekg(container_offset, std::ios::beg);
  std::uint8_t version = 0;
  if (!ReadContainerHeader(in, version, hdr_err)) {
    return false;
  }
  if (version != kContainerVersionV2) {
    return false;
  }
  entry.version = version;
  (void)ConsumeMih3Header(in, master_key_, nullptr);

  std::uint64_t min_ts = 0;
  std::uint64_t max_ts = 0;
  std::uint64_t record_count = 0;
  std::uint64_t message_count = 0;

  while (true) {
    bool has_record = false;
    bool rec_group = false;
    std::string rec_conv;
    std::array<std::uint8_t, 24> inner_nonce{};
    std::vector<std::uint8_t> inner_cipher;
    std::array<std::uint8_t, 16> inner_mac{};
    std::string rec_err;
    const bool record_ok =
        (version >= kContainerVersionV2)
            ? ReadOuterRecordV2(in, master_key_, has_record, rec_group, rec_conv,
                                inner_nonce, inner_cipher, inner_mac, rec_err)
            : ReadOuterRecord(in, master_key_, has_record, rec_group, rec_conv,
                              inner_nonce, inner_cipher, inner_mac, rec_err);
    if (!record_ok || !has_record) {
      break;
    }
    if (rec_conv.empty() || rec_conv == kFileMetaConvId) {
      continue;
    }
    std::array<std::uint8_t, 32> conv_key{};
    std::string key_err;
    if (!DeriveConversationKey(rec_group, rec_conv, conv_key, key_err)) {
      continue;
    }
    std::vector<std::uint8_t> record_plain;
    std::string decode_err;
    if (!DecodeInnerRecordPlain(conv_key, rec_group, rec_conv, inner_nonce,
                                inner_cipher, inner_mac, record_plain,
                                decode_err)) {
      continue;
    }
    if (record_plain.empty()) {
      continue;
    }
    const std::uint8_t kind = record_plain[0];
    bool is_message = false;
    std::uint64_t ts = 0;
    if (kind == kRecordMessage) {
      std::size_t off = 1 + 1 + 1 + 1 + 1;
      if (record_plain.size() >= off + 8) {
        (void)mi::server::proto::ReadUint64(record_plain, off, ts);
      }
      is_message = true;
    } else if (kind == kRecordStatus) {
      std::size_t off = 1 + 1 + 1;
      if (record_plain.size() >= off + 8) {
        (void)mi::server::proto::ReadUint64(record_plain, off, ts);
      }
      is_message = false;
    } else if (kind == kRecordMeta) {
      if (record_plain.size() >= 2 && record_plain[1] == kMetaKindFlush) {
        std::size_t off = 2;
        if (record_plain.size() >= off + 8) {
          (void)mi::server::proto::ReadUint64(record_plain, off, ts);
        }
      }
    }

    const std::string conv_key_id = MakeConvKey(rec_group, rec_conv);
    entry.conv_keys.insert(conv_key_id);
    UpdateConvStats(entry, conv_key_id, ts, is_message);

    record_count++;
    if (is_message) {
      message_count++;
    }
    if (ts != 0) {
      if (min_ts == 0 || ts < min_ts) {
        min_ts = ts;
      }
      if (ts > max_ts) {
        max_ts = ts;
      }
    }
  }
  entry.conv_keys_complete = true;
  if (!entry.conv_keys.empty() &&
      entry.conv_stats.size() >= entry.conv_keys.size()) {
    entry.conv_stats_complete = true;
  }
  if (entry.record_count == 0 && record_count > 0) {
    entry.record_count = record_count;
    entry.message_count = message_count;
    entry.min_ts = min_ts;
    entry.max_ts = max_ts;
  }
  index_dirty_ = true;
  return true;
}

bool ChatHistoryStore::EnsureConversationMapped(bool is_group,
                                                const std::string& conv_id,
                                                std::string& error) {
  error.clear();
  if (conv_id.empty()) {
    return false;
  }
  const std::string conv_key = MakeConvKey(is_group, conv_id);
  auto it = conv_to_file_.find(conv_key);
  if (it != conv_to_file_.end()) {
    return true;
  }

  std::vector<std::size_t> candidates;
  if (tag_key_loaded_ && !IsAllZero(tag_key_.data(), tag_key_.size())) {
    const auto hash = DeriveConvHash(tag_key_, conv_key);
    const std::string hk = ConvHashKey(hash);
    auto hit = conv_hash_to_files_.find(hk);
    if (hit != conv_hash_to_files_.end()) {
      candidates = hit->second;
    }
  }
  if (candidates.empty()) {
    candidates.reserve(history_files_.size());
    for (std::size_t i = 0; i < history_files_.size(); ++i) {
      candidates.push_back(i);
    }
  }

  for (const auto idx : candidates) {
    if (idx >= history_files_.size()) {
      continue;
    }
    auto& entry = history_files_[idx];
    if (!entry.conv_keys_complete) {
      std::string scan_err;
      ScanFileForConversations(entry, scan_err);
    }
    if (entry.conv_keys.find(conv_key) != entry.conv_keys.end()) {
      conv_to_file_[conv_key] = idx;
      RebuildConvHashIndex();
      return true;
    }
  }
  return false;
}

bool ChatHistoryStore::EnsureAttachmentsLoaded(std::string& error) {
  error.clear();
  if (attachments_loaded_) {
    return true;
  }
  return LoadAttachmentsIndex(error);
}

bool ChatHistoryStore::LoadAttachmentsIndex(std::string& error) {
  error.clear();
  attachments_.clear();
  attachments_loaded_ = true;
  attachments_dirty_ = false;
  if (attachments_index_path_.empty() || attachments_dir_.empty()) {
    return true;
  }
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    error = "history key missing";
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(attachments_index_path_, ec)) {
    return true;
  }
  if (ec) {
    error = "attachments index read failed";
    return false;
  }

  std::vector<std::uint8_t> bytes;
  {
    std::ifstream in(attachments_index_path_, std::ios::binary);
    if (!in.is_open()) {
      error = "attachments index read failed";
      return false;
    }
    bytes.assign(std::istreambuf_iterator<char>(in),
                 std::istreambuf_iterator<char>());
  }
  if (bytes.size() < sizeof(kAttachmentIndexMagic) + 1 + 3 +
                         kAttachmentIndexNonceBytes + 4 +
                         kAttachmentIndexMacBytes) {
    error = "attachments index read failed";
    return false;
  }
  std::size_t off = 0;
  if (std::memcmp(bytes.data(), kAttachmentIndexMagic,
                  sizeof(kAttachmentIndexMagic)) != 0) {
    error = "attachments index read failed";
    return false;
  }
  off += sizeof(kAttachmentIndexMagic);
  const std::uint8_t version = bytes[off++];
  if (version != kAttachmentIndexVersion) {
    error = "attachments index read failed";
    return false;
  }
  off += 3;
  if (off + kAttachmentIndexNonceBytes + 4 > bytes.size()) {
    error = "attachments index read failed";
    return false;
  }
  std::array<std::uint8_t, kAttachmentIndexNonceBytes> nonce{};
  std::memcpy(nonce.data(), bytes.data() + off, nonce.size());
  off += nonce.size();
  const std::uint32_t cipher_len =
      static_cast<std::uint32_t>(bytes[off]) |
      (static_cast<std::uint32_t>(bytes[off + 1]) << 8) |
      (static_cast<std::uint32_t>(bytes[off + 2]) << 16) |
      (static_cast<std::uint32_t>(bytes[off + 3]) << 24);
  off += 4;
  if (cipher_len == 0 ||
      off + cipher_len + kAttachmentIndexMacBytes != bytes.size()) {
    error = "attachments index read failed";
    return false;
  }
  const std::uint8_t* cipher = bytes.data() + off;
  const std::uint8_t* mac = bytes.data() + off + cipher_len;

  std::array<std::uint8_t, 32> index_key{};
  std::string key_err;
  if (!DeriveAttachmentIndexKey(master_key_, index_key, key_err)) {
    error = key_err.empty() ? "attachments index read failed" : key_err;
    return false;
  }
  std::vector<std::uint8_t> plain;
  plain.resize(cipher_len);
  const int ok = crypto_aead_unlock(plain.data(), mac, index_key.data(),
                                    nonce.data(), nullptr, 0, cipher,
                                    cipher_len);
  crypto_wipe(index_key.data(), index_key.size());
  if (ok != 0) {
    error = "attachments index auth failed";
    return false;
  }
  if (plain.size() < sizeof(kAttachmentIndexPlainMagic) + 1) {
    error = "attachments index read failed";
    return false;
  }
  std::size_t poff = 0;
  if (std::memcmp(plain.data(), kAttachmentIndexPlainMagic,
                  sizeof(kAttachmentIndexPlainMagic)) != 0) {
    error = "attachments index read failed";
    return false;
  }
  poff += sizeof(kAttachmentIndexPlainMagic);
  const std::uint8_t plain_ver = plain[poff++];
  if (plain_ver != kAttachmentIndexVersion) {
    error = "attachments index read failed";
    return false;
  }
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(plain, poff, count)) {
    error = "attachments index read failed";
    return false;
  }
  if (count > kMaxAttachmentEntries) {
    error = "attachments index read failed";
    return false;
  }
  for (std::uint32_t i = 0; i < count; ++i) {
    std::string file_id;
    std::string file_name;
    std::uint64_t file_size = 0;
    if (!mi::server::proto::ReadString(plain, poff, file_id) ||
        file_id.empty() ||
        !mi::server::proto::ReadString(plain, poff, file_name) ||
        !mi::server::proto::ReadUint64(plain, poff, file_size)) {
      error = "attachments index read failed";
      return false;
    }
    if (poff + 1 > plain.size()) {
      error = "attachments index read failed";
      return false;
    }
    const std::uint8_t kind = plain[poff++];
    std::uint32_t ref_count = 0;
    std::uint32_t preview_size = 0;
    std::uint64_t last_ts = 0;
    if (!mi::server::proto::ReadUint32(plain, poff, ref_count) ||
        !mi::server::proto::ReadUint32(plain, poff, preview_size) ||
        !mi::server::proto::ReadUint64(plain, poff, last_ts)) {
      error = "attachments index read failed";
      return false;
    }
    AttachmentEntry entry;
    entry.file_name = std::move(file_name);
    entry.file_size = file_size;
    entry.kind = kind;
    entry.ref_count = ref_count;
    entry.preview_size = preview_size;
    entry.last_ts = last_ts;
    attachments_.emplace(std::move(file_id), std::move(entry));
  }
  if (poff != plain.size()) {
    error = "attachments index read failed";
    return false;
  }
  return true;
}

bool ChatHistoryStore::SaveAttachmentsIndex(std::string& error) {
  error.clear();
  if (!attachments_dirty_) {
    return true;
  }
  if (read_only_) {
    return true;
  }
  if (attachments_index_path_.empty() || attachments_dir_.empty()) {
    return true;
  }
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    return true;
  }

  std::vector<std::pair<std::string, AttachmentEntry>> entries;
  entries.reserve(attachments_.size());
  for (const auto& kv : attachments_) {
    entries.emplace_back(kv.first, kv.second);
  }
  std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  if (entries.size() > kMaxAttachmentEntries) {
    error = "attachments index write failed";
    return false;
  }

  std::vector<std::uint8_t> plain;
  plain.reserve(128);
  plain.insert(plain.end(), kAttachmentIndexPlainMagic,
               kAttachmentIndexPlainMagic + sizeof(kAttachmentIndexPlainMagic));
  plain.push_back(kAttachmentIndexVersion);
  if (!mi::server::proto::WriteUint32(
          static_cast<std::uint32_t>(entries.size()), plain)) {
    error = "attachments index write failed";
    return false;
  }
  for (const auto& kv : entries) {
    if (!mi::server::proto::WriteString(kv.first, plain) ||
        !mi::server::proto::WriteString(kv.second.file_name, plain) ||
        !mi::server::proto::WriteUint64(kv.second.file_size, plain)) {
      error = "attachments index write failed";
      return false;
    }
    plain.push_back(kv.second.kind);
    mi::server::proto::WriteUint32(kv.second.ref_count, plain);
    mi::server::proto::WriteUint32(kv.second.preview_size, plain);
    mi::server::proto::WriteUint64(kv.second.last_ts, plain);
  }

  std::array<std::uint8_t, 32> index_key{};
  std::string key_err;
  if (!DeriveAttachmentIndexKey(master_key_, index_key, key_err)) {
    error = key_err.empty() ? "attachments index write failed" : key_err;
    return false;
  }
  std::array<std::uint8_t, kAttachmentIndexNonceBytes> nonce{};
  if (!mi::server::crypto::RandomBytes(nonce.data(), nonce.size())) {
    crypto_wipe(index_key.data(), index_key.size());
    error = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> cipher(plain.size());
  std::array<std::uint8_t, kAttachmentIndexMacBytes> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), index_key.data(), nonce.data(),
                   nullptr, 0, plain.data(), plain.size());
  crypto_wipe(index_key.data(), index_key.size());

  std::vector<std::uint8_t> out;
  out.reserve(sizeof(kAttachmentIndexMagic) + 1 + 3 + nonce.size() + 4 +
              cipher.size() + mac.size());
  out.insert(out.end(), kAttachmentIndexMagic,
             kAttachmentIndexMagic + sizeof(kAttachmentIndexMagic));
  out.push_back(kAttachmentIndexVersion);
  out.push_back(0);
  out.push_back(0);
  out.push_back(0);
  out.insert(out.end(), nonce.begin(), nonce.end());
  const std::uint32_t cipher_len =
      static_cast<std::uint32_t>(cipher.size());
  out.push_back(static_cast<std::uint8_t>(cipher_len & 0xFF));
  out.push_back(static_cast<std::uint8_t>((cipher_len >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((cipher_len >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((cipher_len >> 24) & 0xFF));
  out.insert(out.end(), cipher.begin(), cipher.end());
  out.insert(out.end(), mac.begin(), mac.end());

  std::error_code ec;
  const auto tmp = attachments_index_path_.string() + ".tmp";
  {
    std::ofstream fout(tmp, std::ios::binary | std::ios::trunc);
    if (!fout) {
      error = "attachments index write failed";
      return false;
    }
    fout.write(reinterpret_cast<const char*>(out.data()),
               static_cast<std::streamsize>(out.size()));
    fout.close();
    if (!fout) {
      std::filesystem::remove(tmp, ec);
      error = "attachments index write failed";
      return false;
    }
  }
  std::filesystem::rename(tmp, attachments_index_path_, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    error = "attachments index write failed";
    return false;
  }
  attachments_dirty_ = false;
  return true;
}

bool ChatHistoryStore::TouchAttachmentFromEnvelope(
    const std::vector<std::uint8_t>& envelope,
    std::uint64_t timestamp_sec,
    std::string& error) {
  error.clear();
  if (envelope.empty()) {
    return true;
  }
  if (!EnsureAttachmentsLoaded(error)) {
    return false;
  }
  std::uint8_t type = 0;
  std::size_t off = 0;
  if (!DecodeChatHeaderBrief(envelope, type, off)) {
    return true;
  }
  std::uint64_t file_size = 0;
  std::string file_name;
  std::string file_id;
  if (type == kChatTypeFile) {
    if (!mi::server::proto::ReadUint64(envelope, off, file_size) ||
        !mi::server::proto::ReadString(envelope, off, file_name) ||
        !mi::server::proto::ReadString(envelope, off, file_id)) {
      return true;
    }
    if (off + 32 != envelope.size()) {
      return true;
    }
  } else if (type == kChatTypeGroupFile) {
    std::string group_id;
    if (!mi::server::proto::ReadString(envelope, off, group_id) ||
        !mi::server::proto::ReadUint64(envelope, off, file_size) ||
        !mi::server::proto::ReadString(envelope, off, file_name) ||
        !mi::server::proto::ReadString(envelope, off, file_id)) {
      return true;
    }
    if (off + 32 != envelope.size()) {
      return true;
    }
  } else {
    return true;
  }
  if (file_id.empty()) {
    return true;
  }
  AttachmentEntry& entry = attachments_[file_id];
  if (!file_name.empty()) {
    entry.file_name = file_name;
    entry.kind = static_cast<std::uint8_t>(GuessAttachmentKind(file_name));
  }
  if (file_size > 0) {
    entry.file_size = file_size;
  }
  if (entry.ref_count < (std::numeric_limits<std::uint32_t>::max)()) {
    entry.ref_count++;
  }
  if (timestamp_sec != 0) {
    entry.last_ts = std::max(entry.last_ts, timestamp_sec);
  }
  attachments_dirty_ = true;
  return true;
}

bool ChatHistoryStore::ReleaseAttachmentFromEnvelope(
    const std::vector<std::uint8_t>& envelope,
    std::string& error) {
  error.clear();
  if (envelope.empty()) {
    return true;
  }
  if (!EnsureAttachmentsLoaded(error)) {
    return false;
  }
  std::uint8_t type = 0;
  std::size_t off = 0;
  if (!DecodeChatHeaderBrief(envelope, type, off)) {
    return true;
  }
  std::string file_id;
  if (type == kChatTypeFile) {
    std::uint64_t file_size = 0;
    std::string file_name;
    if (!mi::server::proto::ReadUint64(envelope, off, file_size) ||
        !mi::server::proto::ReadString(envelope, off, file_name) ||
        !mi::server::proto::ReadString(envelope, off, file_id)) {
      return true;
    }
    if (off + 32 != envelope.size()) {
      return true;
    }
  } else if (type == kChatTypeGroupFile) {
    std::string group_id;
    std::uint64_t file_size = 0;
    std::string file_name;
    if (!mi::server::proto::ReadString(envelope, off, group_id) ||
        !mi::server::proto::ReadUint64(envelope, off, file_size) ||
        !mi::server::proto::ReadString(envelope, off, file_name) ||
        !mi::server::proto::ReadString(envelope, off, file_id)) {
      return true;
    }
    if (off + 32 != envelope.size()) {
      return true;
    }
  } else {
    return true;
  }
  if (file_id.empty()) {
    return true;
  }
  auto it = attachments_.find(file_id);
  if (it == attachments_.end()) {
    return true;
  }
  if (it->second.ref_count > 0) {
    it->second.ref_count--;
  }
  if (it->second.ref_count == 0) {
    std::error_code ec;
    const auto preview_path = attachments_dir_ / AttachmentPreviewName(file_id);
    std::filesystem::remove(preview_path, ec);
    attachments_.erase(it);
  }
  attachments_dirty_ = true;
  return true;
}

bool ChatHistoryStore::UpdateAttachmentPreview(
    const std::string& file_id,
    const std::string& file_name,
    std::uint64_t file_size,
    const std::vector<std::uint8_t>& plain,
    std::string& error) {
  error.clear();
  if (file_id.empty()) {
    error = "file id empty";
    return false;
  }
  if (plain.empty()) {
    return true;
  }
  if (!EnsureAttachmentsLoaded(error)) {
    return false;
  }
  const std::size_t preview_len =
      std::min<std::size_t>(plain.size(), kAttachmentPreviewMaxBytes);
  if (preview_len == 0) {
    return true;
  }
  std::vector<std::uint8_t> preview(plain.begin(),
                                    plain.begin() + preview_len);

  std::array<std::uint8_t, 32> preview_key{};
  std::string key_err;
  if (!DeriveAttachmentPreviewKey(master_key_, file_id, preview_key, key_err)) {
    error = key_err.empty() ? "attachments preview write failed" : key_err;
    return false;
  }
  std::array<std::uint8_t, kAttachmentPreviewNonceBytes> nonce{};
  if (!mi::server::crypto::RandomBytes(nonce.data(), nonce.size())) {
    crypto_wipe(preview_key.data(), preview_key.size());
    error = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> cipher(preview.size());
  std::array<std::uint8_t, kAttachmentPreviewMacBytes> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), preview_key.data(), nonce.data(),
                   nullptr, 0, preview.data(), preview.size());
  crypto_wipe(preview_key.data(), preview_key.size());

  std::vector<std::uint8_t> out;
  out.reserve(sizeof(kAttachmentPreviewMagic) + 1 + nonce.size() + 4 +
              cipher.size() + mac.size());
  out.insert(out.end(), kAttachmentPreviewMagic,
             kAttachmentPreviewMagic + sizeof(kAttachmentPreviewMagic));
  out.push_back(kAttachmentPreviewVersion);
  out.insert(out.end(), nonce.begin(), nonce.end());
  const std::uint32_t cipher_len =
      static_cast<std::uint32_t>(cipher.size());
  out.push_back(static_cast<std::uint8_t>(cipher_len & 0xFF));
  out.push_back(static_cast<std::uint8_t>((cipher_len >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((cipher_len >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((cipher_len >> 24) & 0xFF));
  out.insert(out.end(), cipher.begin(), cipher.end());
  out.insert(out.end(), mac.begin(), mac.end());

  std::error_code ec;
  const auto preview_path = attachments_dir_ / AttachmentPreviewName(file_id);
  const auto tmp = preview_path.string() + ".tmp";
  {
    std::ofstream fout(tmp, std::ios::binary | std::ios::trunc);
    if (!fout) {
      error = "attachments preview write failed";
      return false;
    }
    fout.write(reinterpret_cast<const char*>(out.data()),
               static_cast<std::streamsize>(out.size()));
    fout.close();
    if (!fout) {
      std::filesystem::remove(tmp, ec);
      error = "attachments preview write failed";
      return false;
    }
  }
  std::filesystem::rename(tmp, preview_path, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    error = "attachments preview write failed";
    return false;
  }

  AttachmentEntry& entry = attachments_[file_id];
  if (!file_name.empty()) {
    entry.file_name = file_name;
    entry.kind = static_cast<std::uint8_t>(GuessAttachmentKind(file_name));
  }
  if (file_size > 0) {
    entry.file_size = file_size;
  }
  entry.preview_size = static_cast<std::uint32_t>(preview_len);
  const std::uint64_t now_ts = NowUnixSeconds();
  entry.last_ts = std::max(entry.last_ts, now_ts);
  if (entry.ref_count == 0) {
    entry.ref_count = 1;
  }
  attachments_dirty_ = true;
  return true;
}

bool ChatHistoryStore::AppendHistoryJournal(const std::vector<std::uint8_t>& plain,
                                            std::string& error) {
  error.clear();
  if (read_only_) {
    return true;
  }
  if (journal_path_.empty() || plain.empty()) {
    return false;
  }
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    return false;
  }
  std::array<std::uint8_t, 32> index_key{};
  std::string key_err;
  if (!DeriveIndexKey(index_key, key_err)) {
    return false;
  }
  std::array<std::uint8_t, kIndexNonceBytes> nonce{};
  if (!mi::server::crypto::RandomBytes(nonce.data(), nonce.size())) {
    crypto_wipe(index_key.data(), index_key.size());
    return false;
  }
  std::vector<std::uint8_t> cipher(plain.size());
  std::array<std::uint8_t, kIndexMacBytes> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), index_key.data(), nonce.data(),
                   nullptr, 0, plain.data(), plain.size());
  crypto_wipe(index_key.data(), index_key.size());

  std::ofstream out(journal_path_, std::ios::binary | std::ios::app);
  if (!out) {
    return false;
  }
  if (out.tellp() == std::streampos(0)) {
    out.write(reinterpret_cast<const char*>(kJournalMagic),
              sizeof(kJournalMagic));
    out.put(static_cast<char>(kJournalVersion));
    out.put(0);
    out.put(0);
    out.put(0);
  }
  if (cipher.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    return false;
  }
  const std::uint32_t cipher_len =
      static_cast<std::uint32_t>(cipher.size());
  const std::uint8_t len_bytes[4] = {
      static_cast<std::uint8_t>(cipher_len & 0xFF),
      static_cast<std::uint8_t>((cipher_len >> 8) & 0xFF),
      static_cast<std::uint8_t>((cipher_len >> 16) & 0xFF),
      static_cast<std::uint8_t>((cipher_len >> 24) & 0xFF),
  };
  out.write(reinterpret_cast<const char*>(len_bytes), sizeof(len_bytes));
  out.write(reinterpret_cast<const char*>(nonce.data()),
            static_cast<std::streamsize>(nonce.size()));
  out.write(reinterpret_cast<const char*>(cipher.data()),
            static_cast<std::streamsize>(cipher.size()));
  out.write(reinterpret_cast<const char*>(mac.data()),
            static_cast<std::streamsize>(mac.size()));
  return out.good();
}

bool ChatHistoryStore::LoadHistoryJournal(std::string& error) {
  error.clear();
  if (journal_path_.empty()) {
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(journal_path_, ec) || ec) {
    return false;
  }
  std::ifstream in(journal_path_, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }
  std::vector<std::uint8_t> header(sizeof(kJournalMagic) + 4);
  if (!ReadExact(in, header.data(), header.size())) {
    return false;
  }
  if (std::memcmp(header.data(), kJournalMagic, sizeof(kJournalMagic)) != 0) {
    return false;
  }
  const std::uint8_t version = header[sizeof(kJournalMagic)];
  if (version != kJournalVersion) {
    return false;
  }

  std::array<std::uint8_t, 32> index_key{};
  std::string key_err;
  if (!DeriveIndexKey(index_key, key_err)) {
    return false;
  }

  bool applied = false;
  while (true) {
    std::uint8_t len_bytes[4];
    if (!ReadExact(in, len_bytes, sizeof(len_bytes))) {
      break;
    }
    const std::uint32_t cipher_len =
        static_cast<std::uint32_t>(len_bytes[0]) |
        (static_cast<std::uint32_t>(len_bytes[1]) << 8) |
        (static_cast<std::uint32_t>(len_bytes[2]) << 16) |
        (static_cast<std::uint32_t>(len_bytes[3]) << 24);
    if (cipher_len == 0 || cipher_len > (64u * 1024u)) {
      break;
    }
    std::array<std::uint8_t, kIndexNonceBytes> nonce{};
    if (!ReadExact(in, nonce.data(), nonce.size())) {
      break;
    }
    std::vector<std::uint8_t> cipher(cipher_len);
    if (!ReadExact(in, cipher.data(), cipher.size())) {
      break;
    }
    std::array<std::uint8_t, kIndexMacBytes> mac{};
    if (!ReadExact(in, mac.data(), mac.size())) {
      break;
    }
    std::vector<std::uint8_t> plain(cipher_len);
    const int ok = crypto_aead_unlock(plain.data(), mac.data(),
                                      index_key.data(), nonce.data(), nullptr,
                                      0, cipher.data(), cipher.size());
    if (ok != 0 || plain.empty()) {
      break;
    }

    std::size_t off = 0;
    const std::uint8_t type = plain[off++];
    auto findByName = [&](const std::string& name) -> HistoryFileEntry* {
      for (auto& entry : history_files_) {
        if (entry.path.filename().string() == name) {
          return &entry;
        }
      }
      return nullptr;
    };
    if (type == kJournalEntryFileCreate) {
      std::string name;
      std::uint32_t seq = 0;
      std::uint32_t internal_seq = 0;
      std::uint8_t version_byte = 0;
      std::string tag;
      if (!mi::server::proto::ReadString(plain, off, name) ||
          !mi::server::proto::ReadUint32(plain, off, seq) ||
          !mi::server::proto::ReadUint32(plain, off, internal_seq) ||
          off >= plain.size()) {
        continue;
      }
      version_byte = plain[off++];
      if (!mi::server::proto::ReadString(plain, off, tag)) {
        continue;
      }
      if (off + 16 + 32 > plain.size()) {
        continue;
      }
      HistoryFileEntry* entry = findByName(name);
      if (!entry) {
        HistoryFileEntry created;
        created.path = history_dir_ / name;
        created.seq = seq;
        created.internal_seq = internal_seq;
        created.has_internal_seq = true;
        created.version = version_byte;
        created.tag = std::move(tag);
        std::memcpy(created.file_uuid.data(), plain.data() + off,
                    created.file_uuid.size());
        off += created.file_uuid.size();
        std::memcpy(created.prev_hash.data(), plain.data() + off,
                    created.prev_hash.size());
        off += created.prev_hash.size();
        created.has_prev_hash = true;
        if (std::filesystem::exists(created.path, ec)) {
          history_files_.push_back(std::move(created));
        }
      } else {
        entry->seq = seq;
        entry->internal_seq = internal_seq;
        entry->has_internal_seq = true;
        entry->version = version_byte;
        entry->tag = std::move(tag);
        std::memcpy(entry->file_uuid.data(), plain.data() + off,
                    entry->file_uuid.size());
        off += entry->file_uuid.size();
        std::memcpy(entry->prev_hash.data(), plain.data() + off,
                    entry->prev_hash.size());
        off += entry->prev_hash.size();
        entry->has_prev_hash = true;
      }
      applied = true;
      continue;
    }
    if (type == kJournalEntryConvAdd) {
      std::string name;
      std::string conv_key;
      if (!mi::server::proto::ReadString(plain, off, name) ||
          !mi::server::proto::ReadString(plain, off, conv_key) ||
          conv_key.empty()) {
        continue;
      }
      if (HistoryFileEntry* entry = findByName(name)) {
        entry->conv_keys.insert(conv_key);
        applied = true;
      }
      continue;
    }
    if (type == kJournalEntryFileStats) {
      std::string name;
      std::uint64_t min_ts = 0;
      std::uint64_t max_ts = 0;
      std::uint64_t record_count = 0;
      std::uint64_t message_count = 0;
      if (!mi::server::proto::ReadString(plain, off, name) ||
          !mi::server::proto::ReadUint64(plain, off, min_ts) ||
          !mi::server::proto::ReadUint64(plain, off, max_ts) ||
          !mi::server::proto::ReadUint64(plain, off, record_count) ||
          !mi::server::proto::ReadUint64(plain, off, message_count)) {
        continue;
      }
      if (HistoryFileEntry* entry = findByName(name)) {
        entry->min_ts = min_ts;
        entry->max_ts = max_ts;
        entry->record_count = record_count;
        entry->message_count = message_count;
        applied = true;
      }
      continue;
    }
    if (type == kJournalEntryConvStats) {
      std::string name;
      std::uint32_t conv_count = 0;
      if (!mi::server::proto::ReadString(plain, off, name) ||
          !mi::server::proto::ReadUint32(plain, off, conv_count)) {
        continue;
      }
      if (conv_count > 256) {
        continue;
      }
      if (HistoryFileEntry* entry = findByName(name)) {
        for (std::uint32_t i = 0; i < conv_count; ++i) {
          std::string conv_key;
          ChatHistoryConvStats stats;
          if (!mi::server::proto::ReadString(plain, off, conv_key) ||
              conv_key.empty() ||
              !mi::server::proto::ReadUint64(plain, off, stats.min_ts) ||
              !mi::server::proto::ReadUint64(plain, off, stats.max_ts) ||
              !mi::server::proto::ReadUint64(plain, off, stats.record_count) ||
              !mi::server::proto::ReadUint64(plain, off, stats.message_count)) {
            break;
          }
          entry->conv_stats[conv_key] = stats;
          entry->conv_keys.insert(conv_key);
        }
        if (!entry->conv_keys.empty() &&
            entry->conv_stats.size() >= entry->conv_keys.size()) {
          entry->conv_stats_complete = true;
        }
        applied = true;
      }
      continue;
    }
  }
  crypto_wipe(index_key.data(), index_key.size());

  if (!applied) {
    return false;
  }

  const auto effectiveSeq = [](const HistoryFileEntry& entry) {
    return entry.has_internal_seq ? entry.internal_seq : entry.seq;
  };
  std::uint32_t max_seq = 0;
  for (const auto& f : history_files_) {
    max_seq = std::max(max_seq, effectiveSeq(f));
  }
  next_seq_ = max_seq + 1;
  std::sort(history_files_.begin(), history_files_.end(),
            [&](const HistoryFileEntry& a, const HistoryFileEntry& b) {
              return effectiveSeq(a) < effectiveSeq(b);
            });
  ValidateFileChain(history_files_);
  conv_to_file_.clear();
  for (std::size_t i = 0; i < history_files_.size(); ++i) {
    for (const auto& key : history_files_[i].conv_keys) {
      conv_to_file_[key] = i;
    }
  }
  RebuildConvHashIndex();
  index_dirty_ = true;
  return true;
}

void ChatHistoryStore::ClearHistoryJournal() {
  if (journal_path_.empty()) {
    return;
  }
  std::error_code ec;
  std::filesystem::remove(journal_path_, ec);
}

bool ChatHistoryStore::MigrateLegacyHistoryFiles(const std::string& legacy_tag,
                                                 const std::string& new_tag,
                                                 std::string& error) {
  error.clear();
  if (history_dir_.empty() || legacy_tag.empty() || new_tag.empty() ||
      legacy_tag == new_tag) {
    return true;
  }
  std::error_code ec;
  if (!std::filesystem::exists(history_dir_, ec)) {
    return true;
  }

  bool ok = true;
  for (const auto& entry :
       std::filesystem::directory_iterator(history_dir_, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto name = entry.path().filename().string();
    std::uint32_t seq = 0;
    if (!ParseHistoryFileName(name, legacy_tag, seq)) {
      continue;
    }
    const std::string new_name = BuildHistoryFileName(new_tag, seq);
    if (new_name.empty()) {
      continue;
    }
    const auto target = entry.path().parent_path() / new_name;
    if (std::filesystem::exists(target, ec)) {
      continue;
    }
    std::filesystem::rename(entry.path(), target, ec);
    if (ec) {
      ok = false;
      ec.clear();
    }
  }
  if (!ok && error.empty()) {
    error = "history migrate failed";
  }
  return ok;
}

bool ChatHistoryStore::DeriveConversationKey(
    bool is_group,
    const std::string& conv_id,
    std::array<std::uint8_t, 32>& out_key,
    std::string& error) const {
  error.clear();
  out_key.fill(0);
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    error = "history key missing";
    return false;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }

  std::vector<std::uint8_t> info;
  static constexpr char kPrefix[] = "MI_E2EE_HISTORY_CONV_KEY_V1";
  info.insert(info.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  info.push_back(0);
  info.push_back(is_group ? 1 : 0);
  info.push_back(0);
  info.insert(info.end(), conv_id.begin(), conv_id.end());

  std::array<std::uint8_t, 32> salt{};
  static constexpr char kSalt[] = "MI_E2EE_HISTORY_SALT_V1";
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(reinterpret_cast<const std::uint8_t*>(kSalt),
                             sizeof(kSalt) - 1, d);
  salt = d.bytes;

  if (!mi::server::crypto::HkdfSha256(master_key_.data(), master_key_.size(),
                                      salt.data(), salt.size(), info.data(),
                                      info.size(), out_key.data(),
                                      out_key.size())) {
    error = "history hkdf failed";
    return false;
  }
  return true;
}

bool ChatHistoryStore::LoadHistoryFiles(std::string& error) {
  error.clear();
  history_files_.clear();
  conv_to_file_.clear();
  next_seq_ = 1;
  if (history_dir_.empty() || user_tag_.empty()) {
    return true;
  }
  std::string idx_err;
  if (LoadHistoryIndex(idx_err)) {
    std::string journal_err;
    if (LoadHistoryJournal(journal_err)) {
      std::string save_err;
      (void)SaveHistoryIndex(save_err);
    }
    return true;
  }

  std::error_code ec;
  if (!std::filesystem::exists(history_dir_, ec)) {
    return true;
  }

  struct CandidateFile {
    std::filesystem::path path;
    std::uint32_t seq{0};
    std::string tag;
  };
  std::vector<CandidateFile> candidates;
  for (const auto& entry : std::filesystem::directory_iterator(history_dir_, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto name = entry.path().filename().string();
    std::uint32_t seq = 0;
    std::string tag;
    if (ParseHistoryFileName(name, user_tag_, seq)) {
      tag = user_tag_;
    } else if (!legacy_tag_.empty() &&
               ParseHistoryFileName(name, legacy_tag_, seq)) {
      tag = legacy_tag_;
    } else if (!legacy_tag_alt_.empty() &&
               ParseHistoryFileName(name, legacy_tag_alt_, seq)) {
      tag = legacy_tag_alt_;
    } else {
      continue;
    }
    CandidateFile c;
    c.path = entry.path();
    c.seq = seq;
    c.tag = std::move(tag);
    candidates.push_back(std::move(c));
  }

  std::vector<HistoryFileEntry> files;
  if (!candidates.empty()) {
    std::vector<HistoryFileEntry> temp(candidates.size());
    std::vector<bool> ok(candidates.size(), false);
    std::atomic<std::size_t> next{0};
    const std::size_t thread_count = std::min<std::size_t>(
        std::max<std::size_t>(1, std::thread::hardware_concurrency()), 4);
    auto worker = [&]() {
      for (;;) {
        const std::size_t i = next.fetch_add(1);
        if (i >= candidates.size()) {
          break;
        }
        HistoryFileEntry file;
        file.path = candidates[i].path;
        file.seq = candidates[i].seq;
        file.tag = candidates[i].tag;

        std::ifstream in(file.path, std::ios::binary);
        if (!in.is_open()) {
          continue;
        }
        std::uint32_t container_offset = 0;
        std::uint8_t version = 0;
        std::string hdr_err;
        if (!LocateContainerOffset(in, container_offset, hdr_err)) {
          continue;
        }
        in.clear();
        in.seekg(container_offset, std::ios::beg);
        if (!ReadContainerHeader(in, version, hdr_err)) {
          continue;
        }
        if (version != kContainerVersionV2) {
          continue;
        }
        file.version = version;
        Mih3Summary mih3;
        if (ConsumeMih3Header(in, master_key_, &mih3)) {
          if (!IsAllZero(mih3.file_uuid.data(), mih3.file_uuid.size())) {
            file.file_uuid = mih3.file_uuid;
          }
          if (!IsAllZero(mih3.prev_hash.data(), mih3.prev_hash.size())) {
            file.prev_hash = mih3.prev_hash;
            file.has_prev_hash = true;
          }
          if (mih3.file_seq != 0) {
            file.internal_seq = mih3.file_seq;
            file.has_internal_seq = true;
          }
          file.min_ts = mih3.min_ts;
          file.max_ts = mih3.max_ts;
          file.record_count = mih3.record_count;
          file.message_count = mih3.message_count;
          if (mih3.conv_count > 0) {
            file.conv_stats.reserve(mih3.conv_count);
          }
        }

        std::array<std::uint8_t, 32> meta_key{};
        std::string key_err;
        const bool have_meta_key =
            DeriveConversationKey(false, kFileMetaConvId, meta_key, key_err);
        bool found_meta = false;
        bool found_summary = false;
        for (int pass = 0; pass < 8; ++pass) {
          bool has_record = false;
          bool is_group = false;
          std::string conv_id;
          std::array<std::uint8_t, 24> inner_nonce{};
          std::vector<std::uint8_t> inner_cipher;
          std::array<std::uint8_t, 16> inner_mac{};
          std::string rec_err;
          const bool ok_record =
              (version >= kContainerVersionV2)
                  ? ReadOuterRecordV2(in, master_key_, has_record, is_group,
                                      conv_id, inner_nonce, inner_cipher,
                                      inner_mac, rec_err)
                  : ReadOuterRecord(in, master_key_, has_record, is_group,
                                    conv_id, inner_nonce, inner_cipher,
                                    inner_mac, rec_err);
          if (!ok_record || !has_record) {
            break;
          }
          if (conv_id != kFileMetaConvId || !have_meta_key) {
            continue;
          }
          std::vector<std::uint8_t> record_plain;
          std::string decode_err;
          if (!DecodeInnerRecordPlain(meta_key, false, kFileMetaConvId,
                                      inner_nonce, inner_cipher, inner_mac,
                                      record_plain, decode_err)) {
            continue;
          }
          std::uint32_t meta_seq = 0;
          std::array<std::uint8_t, 16> meta_uuid{};
          std::uint64_t meta_ts = 0;
          if (ParseFileMetaRecord(record_plain, meta_seq, meta_uuid, meta_ts)) {
            file.internal_seq = meta_seq;
            file.has_internal_seq = true;
            file.file_uuid = meta_uuid;
            found_meta = true;
          }
          std::array<std::uint8_t, 32> prev_hash{};
          std::uint64_t min_ts = 0;
          std::uint64_t max_ts = 0;
          std::uint64_t record_count = 0;
          std::uint64_t message_count = 0;
          std::vector<std::array<std::uint8_t, 16>> conv_hashes;
          std::vector<ChatHistoryConvStats> conv_stats;
          if (ParseFileSummaryRecord(record_plain, meta_seq, meta_uuid,
                                     prev_hash, min_ts, max_ts, record_count,
                                     message_count, conv_hashes, &conv_stats)) {
            file.internal_seq = meta_seq;
            file.has_internal_seq = true;
            file.file_uuid = meta_uuid;
            file.prev_hash = prev_hash;
            file.has_prev_hash = true;
            file.min_ts = min_ts;
            file.max_ts = max_ts;
            file.record_count = record_count;
            file.message_count = message_count;
            file.conv_hashes = std::move(conv_hashes);
            file.has_conv_hashes = true;
            if (!conv_stats.empty() && file.has_conv_hashes) {
              const std::size_t limit =
                  std::min(conv_stats.size(), file.conv_hashes.size());
              for (std::size_t i = 0; i < limit; ++i) {
                const std::string hk = ConvHashKey(file.conv_hashes[i]);
                file.conv_stats.emplace(hk, conv_stats[i]);
              }
            }
            found_summary = true;
          }
          if (found_summary && file.has_internal_seq) {
            break;
          }
        }
        file.conv_keys_complete = false;
        if (!found_meta && !found_summary) {
          file.has_conv_hashes = false;
        }
        temp[i] = std::move(file);
        ok[i] = true;
      }
    };

    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
      threads.emplace_back(worker);
    }
    for (auto& t : threads) {
      t.join();
    }

    files.reserve(candidates.size());
    for (std::size_t i = 0; i < candidates.size(); ++i) {
      if (ok[i]) {
        files.push_back(std::move(temp[i]));
      }
    }
  }

  if (!files.empty()) {
    history_files_ = std::move(files);
    std::string journal_err;
    (void)LoadHistoryJournal(journal_err);
    files = std::move(history_files_);
    history_files_.clear();
    conv_to_file_.clear();
    conv_hash_to_files_.clear();
  }

  for (auto& file : files) {
    if (!file.has_conv_hashes) {
      std::string scan_err;
      ScanFileForConversations(file, scan_err);
    }
  }

  std::vector<std::size_t> stats_targets;
  stats_targets.reserve(files.size());
  for (std::size_t i = 0; i < files.size(); ++i) {
    if (!files[i].conv_stats_complete) {
      stats_targets.push_back(i);
    }
  }
  if (!stats_targets.empty()) {
    std::atomic<std::size_t> next_idx{0};
    const std::size_t thread_count = std::min<std::size_t>(
        std::max<std::size_t>(1, std::thread::hardware_concurrency()), 4);
    auto stats_worker = [&]() {
      for (;;) {
        const std::size_t pos = next_idx.fetch_add(1);
        if (pos >= stats_targets.size()) {
          break;
        }
        const std::size_t idx = stats_targets[pos];
        if (idx >= files.size()) {
          continue;
        }
        std::string scan_err;
        (void)ScanFileForConvStats(files[idx], scan_err);
      }
    };
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
      threads.emplace_back(stats_worker);
    }
    for (auto& t : threads) {
      t.join();
    }
  }

  const auto effectiveSeq = [](const HistoryFileEntry& entry) {
    return entry.has_internal_seq ? entry.internal_seq : entry.seq;
  };
  std::uint32_t max_seq = 0;
  for (const auto& f : files) {
    max_seq = std::max(max_seq, effectiveSeq(f));
  }
  next_seq_ = max_seq + 1;

  std::sort(files.begin(), files.end(),
            [&](const HistoryFileEntry& a, const HistoryFileEntry& b) {
              return effectiveSeq(a) < effectiveSeq(b);
            });
  ValidateFileChain(files);
  history_files_ = std::move(files);
  for (std::size_t i = 0; i < history_files_.size(); ++i) {
    for (const auto& key : history_files_[i].conv_keys) {
      conv_to_file_[key] = i;
    }
  }
  RebuildConvHashIndex();
  index_dirty_ = true;
  std::string save_err;
  (void)SaveHistoryIndex(save_err);
  return true;
}

bool ChatHistoryStore::EnsureHistoryFile(
    bool is_group,
    const std::string& conv_id,
    std::filesystem::path& out_path,
    std::array<std::uint8_t, 32>& out_conv_key,
    std::uint8_t& out_version,
    std::string& error) {
  error.clear();
  out_path.clear();
  out_conv_key.fill(0);
  out_version = kContainerVersionV2;
  if (history_dir_.empty()) {
    error = "history dir empty";
    return false;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }
  if (!DeriveConversationKey(is_group, conv_id, out_conv_key, error)) {
    return false;
  }

  const std::string conv_key = MakeConvKey(is_group, conv_id);
  bool had_existing = false;
  bool loaded_existing = false;
  std::size_t old_index = history_files_.size();
  std::vector<ChatHistoryMessage> migrate_messages;
  auto it = conv_to_file_.find(conv_key);
  if (it != conv_to_file_.end() && it->second < history_files_.size()) {
    HistoryFileEntry& entry = history_files_[it->second];
    had_existing = true;
    old_index = it->second;
    if (!entry.tag.empty() && !user_tag_.empty() && entry.tag != user_tag_) {
      const std::uint32_t rename_seq =
          entry.has_internal_seq ? entry.internal_seq : entry.seq;
      const std::string new_name = BuildHistoryFileName(user_tag_, rename_seq);
    if (!new_name.empty()) {
      std::error_code ec;
      const auto new_path = entry.path.parent_path() / new_name;
      if (!std::filesystem::exists(new_path, ec)) {
        std::filesystem::rename(entry.path, new_path, ec);
        if (!ec) {
          entry.path = new_path;
          entry.tag = user_tag_;
          entry.seq = rename_seq;
          index_dirty_ = true;
        }
      }
    }
    }
    if (entry.version >= kContainerVersionV2) {
      out_path = entry.path;
      out_version = entry.version;
      return true;
    }
    std::string load_err;
    loaded_existing =
        LoadConversation(is_group, conv_id, 0, migrate_messages, load_err);
  }

  std::size_t target = history_files_.size();
  for (std::size_t i = history_files_.size(); i > 0; --i) {
    if (history_files_[i - 1].version >= kContainerVersionV2 &&
        history_files_[i - 1].conv_keys.size() < kMaxConversationsPerFile &&
        history_files_[i - 1].record_count < kMaxRecordsPerFile) {
      target = i - 1;
      break;
    }
  }
  if (target == history_files_.size()) {
    const std::uint32_t seq = next_seq_++;
    const std::string name = BuildHistoryFileName(user_tag_, seq);
    if (name.empty()) {
      error = "history create failed";
      return false;
    }
    const std::filesystem::path path = history_dir_ / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
      error = "history create failed";
      return false;
    }
    std::uint32_t hist_offset = 0;
    std::vector<std::uint8_t> stub = BuildPeContainer(hist_offset);
    (void)hist_offset;
    out.write(reinterpret_cast<const char*>(stub.data()),
              static_cast<std::streamsize>(stub.size()));
    if (!out.good()) {
      error = "history create failed";
      return false;
    }
    if (!WriteContainerHeader(out, kContainerVersionV2, error)) {
      return false;
    }
    std::array<std::uint8_t, 16> file_uuid{};
    if (!mi::server::crypto::RandomBytes(file_uuid.data(), file_uuid.size())) {
      error = "rng failed";
      return false;
    }
    std::array<std::uint8_t, 32> prev_hash{};
    if (!history_files_.empty()) {
      const auto& last = history_files_.back();
      if (last.has_prev_hash || last.file_uuid != std::array<std::uint8_t, 16>{}) {
        const std::array<std::uint8_t, 32> use_prev =
            last.has_prev_hash ? last.prev_hash
                               : std::array<std::uint8_t, 32>{};
        prev_hash = ComputeFileChainHash(last.file_uuid,
                                         last.has_internal_seq ? last.internal_seq
                                                               : last.seq,
                                         use_prev);
      }
    }
    Mih3Summary mih3;
    mih3.file_seq = seq;
    mih3.file_uuid = file_uuid;
    mih3.prev_hash = prev_hash;
    if (!WriteMih3Block(out, master_key_, mih3, 0, error)) {
      return false;
    }
    const std::uint64_t create_ts = NowUnixSeconds();
    std::vector<std::uint8_t> meta_rec;
    meta_rec.reserve(1 + 1 + 1 + 4 + file_uuid.size() + 8);
    meta_rec.push_back(kRecordMeta);
    meta_rec.push_back(kMetaKindFileInfo);
    meta_rec.push_back(kMetaFileInfoVersion);
    if (!mi::server::proto::WriteUint32(seq, meta_rec)) {
      error = "history create failed";
      return false;
    }
    meta_rec.insert(meta_rec.end(), file_uuid.begin(), file_uuid.end());
    mi::server::proto::WriteUint64(create_ts, meta_rec);

    std::array<std::uint8_t, 32> meta_key{};
    std::string meta_err;
    if (!DeriveConversationKey(false, kFileMetaConvId, meta_key, meta_err)) {
      error = meta_err.empty() ? "history create failed" : meta_err;
      return false;
    }
    if (!WriteEncryptedRecord(out, master_key_, meta_key, false,
                              kFileMetaConvId, meta_rec, kContainerVersionV2,
                              error)) {
      return false;
    }
    std::vector<std::uint8_t> summary;
    summary.reserve(1 + 1 + 1 + 4 + file_uuid.size() + prev_hash.size() +
                    8 * 4 + 4);
    summary.push_back(kRecordMeta);
    summary.push_back(kMetaKindFileSummary);
    summary.push_back(kMetaFileSummaryVersion);
    mi::server::proto::WriteUint32(seq, summary);
    summary.insert(summary.end(), file_uuid.begin(), file_uuid.end());
    summary.insert(summary.end(), prev_hash.begin(), prev_hash.end());
    mi::server::proto::WriteUint64(0, summary);
    mi::server::proto::WriteUint64(0, summary);
    mi::server::proto::WriteUint64(0, summary);
    mi::server::proto::WriteUint64(0, summary);
    mi::server::proto::WriteUint32(0, summary);
    if (!WriteEncryptedRecord(out, master_key_, meta_key, false,
                              kFileMetaConvId, summary, kContainerVersionV2,
                              error)) {
      return false;
    }
    out.flush();
    if (!out.good()) {
      error = "history create failed";
      return false;
    }
    HistoryFileEntry entry;
    entry.path = path;
    entry.seq = seq;
    entry.version = kContainerVersionV2;
    entry.internal_seq = seq;
    entry.has_internal_seq = true;
    entry.file_uuid = file_uuid;
    entry.prev_hash = prev_hash;
    entry.has_prev_hash = true;
    entry.tag = user_tag_;
    history_files_.push_back(std::move(entry));
    target = history_files_.size() - 1;
    index_dirty_ = true;
    std::vector<std::uint8_t> journal;
    journal.push_back(kJournalEntryFileCreate);
    const std::string file_name = history_files_[target].path.filename().string();
    mi::server::proto::WriteString(file_name, journal);
    mi::server::proto::WriteUint32(seq, journal);
    mi::server::proto::WriteUint32(seq, journal);
    journal.push_back(kContainerVersionV2);
    mi::server::proto::WriteString(user_tag_, journal);
    journal.insert(journal.end(), file_uuid.begin(), file_uuid.end());
    journal.insert(journal.end(), prev_hash.begin(), prev_hash.end());
    std::string journal_err;
    (void)AppendHistoryJournal(journal, journal_err);
  }

  if (had_existing && old_index < history_files_.size()) {
    history_files_[old_index].conv_keys.erase(conv_key);
    history_files_[old_index].conv_stats.erase(conv_key);
    index_dirty_ = true;
  }

  const bool inserted =
      history_files_[target].conv_keys.insert(conv_key).second;
  conv_to_file_[conv_key] = target;
  index_dirty_ = true;
  if (inserted && tag_key_loaded_ &&
      !IsAllZero(tag_key_.data(), tag_key_.size())) {
    auto& entry = history_files_[target];
    const auto h = DeriveConvHash(tag_key_, conv_key);
    entry.conv_hashes.push_back(h);
    entry.has_conv_hashes = true;
    const std::string hk = ConvHashKey(h);
    auto& list = conv_hash_to_files_[hk];
    if (std::find(list.begin(), list.end(), target) == list.end()) {
      list.push_back(target);
    }
  }
  if (inserted) {
    std::vector<std::uint8_t> journal;
    journal.push_back(kJournalEntryConvAdd);
    const std::string file_name =
        history_files_[target].path.filename().string();
    mi::server::proto::WriteString(file_name, journal);
    mi::server::proto::WriteString(conv_key, journal);
    std::string journal_err;
    (void)AppendHistoryJournal(journal, journal_err);
  }
  out_path = history_files_[target].path;
  out_version = history_files_[target].version;

  HistoryFileEntry* target_entry = nullptr;
  if (target < history_files_.size()) {
    target_entry = &history_files_[target];
  }
  const auto append_messages =
      [&](const std::vector<ChatHistoryMessage>& messages) {
        if (messages.empty()) {
          return;
        }
        std::ofstream out(out_path, std::ios::binary | std::ios::app);
        if (!out) {
          return;
        }
        for (const auto& m : messages) {
          if (m.is_group != is_group) {
            continue;
          }
          std::vector<std::uint8_t> rec;
          bool ok = false;
          if (m.is_system) {
            rec.reserve(5 + 8 + 2 + m.system_text_utf8.size());
            rec.push_back(kRecordMessage);
            rec.push_back(kMessageKindSystem);
            rec.push_back(m.is_group ? 1 : 0);
            rec.push_back(0);
            rec.push_back(static_cast<std::uint8_t>(ChatHistoryStatus::kSent));
            mi::server::proto::WriteUint64(m.timestamp_sec, rec);
            ok = mi::server::proto::WriteString(m.system_text_utf8, rec);
          } else {
            rec.reserve(5 + 8 + 2 + m.sender.size() + 4 + m.envelope.size());
            rec.push_back(kRecordMessage);
            rec.push_back(kMessageKindEnvelope);
            rec.push_back(m.is_group ? 1 : 0);
            rec.push_back(m.outgoing ? 1 : 0);
            rec.push_back(static_cast<std::uint8_t>(m.status));
            mi::server::proto::WriteUint64(m.timestamp_sec, rec);
            ok = mi::server::proto::WriteString(m.sender, rec) &&
                 mi::server::proto::WriteBytes(m.envelope, rec);
          }
          if (!ok) {
            continue;
          }
          std::string write_err;
          if (!WriteEncryptedRecord(out, master_key_, out_conv_key, is_group,
                                    conv_id, rec, out_version, write_err)) {
            break;
          }
          if (target_entry) {
            UpdateEntryStats(*target_entry, m.timestamp_sec, true);
            UpdateConvStats(*target_entry, conv_key, m.timestamp_sec, true);
            index_dirty_ = true;
          }
        }
      };

  if (had_existing) {
    append_messages(migrate_messages);
  }

  if (!had_existing || (!loaded_existing && migrate_messages.empty())) {
    std::vector<ChatHistoryMessage> legacy;
    std::string legacy_err;
    if (LoadLegacyConversation(is_group, conv_id, 0, legacy, legacy_err) &&
        !legacy.empty()) {
      append_messages(legacy);
    }
  }
  return true;
}

bool ChatHistoryStore::LoadLegacyConversation(
    bool is_group,
    const std::string& conv_id,
    std::size_t limit,
    std::vector<ChatHistoryMessage>& out_messages,
    std::string& error) const {
  error.clear();
  out_messages.clear();
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    return true;
  }
  if (legacy_conv_dir_.empty()) {
    return true;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }

  std::array<std::uint8_t, 32> conv_key{};
  if (!DeriveConversationKey(is_group, conv_id, conv_key, error)) {
    return false;
  }
  const auto path = LegacyConversationPath(legacy_conv_dir_, is_group, conv_id);
  if (path.empty()) {
    error = "history path failed";
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return true;
  }

  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    error = "history open failed";
    return false;
  }

  if (limit > 0) {
    out_messages.reserve(limit);
  }
  std::uint8_t hdr[sizeof(kLegacyMagic) + 1];
  if (!ReadExact(in, hdr, sizeof(hdr))) {
    return true;
  }
  if (std::memcmp(hdr, kLegacyMagic, sizeof(kLegacyMagic)) != 0 ||
      hdr[sizeof(kLegacyMagic)] != kLegacyVersion) {
    error = "history magic mismatch";
    return false;
  }

  std::unordered_map<std::string, ChatHistoryStatus> status_by_id;
  std::unordered_map<std::string, std::size_t> index_by_id;
  std::size_t reserve_hint = 512;
  if (limit > 0) {
    reserve_hint = std::min<std::size_t>(limit * 2, 8192);
  }
  status_by_id.reserve(reserve_hint);
  index_by_id.reserve(reserve_hint);

  const auto statusRank = [](ChatHistoryStatus status) -> int {
    switch (status) {
      case ChatHistoryStatus::kFailed:
        return 0;
      case ChatHistoryStatus::kSent:
        return 1;
      case ChatHistoryStatus::kDelivered:
        return 2;
      case ChatHistoryStatus::kRead:
        return 3;
    }
    return 0;
  };
  const auto betterStatus = [&](ChatHistoryStatus a,
                                ChatHistoryStatus b) -> ChatHistoryStatus {
    return statusRank(a) >= statusRank(b) ? a : b;
  };
  const auto tryParseStatus = [](std::uint8_t raw,
                                 ChatHistoryStatus& out) -> bool {
    if (raw > static_cast<std::uint8_t>(ChatHistoryStatus::kFailed)) {
      return false;
    }
    out = static_cast<ChatHistoryStatus>(raw);
    return true;
  };

  while (true) {
    std::vector<std::uint8_t> plain;
    std::string rec_err;
    if (!ReadLegacyRecord(in, conv_key, master_key_, plain, rec_err)) {
      error = rec_err.empty() ? "history read failed" : rec_err;
      return false;
    }
    if (plain.empty()) {
      break;
    }
    std::size_t off = 0;
    const std::uint8_t type = plain[off++];
    if (type == kRecordMeta) {
      continue;
    }
    if (type == kRecordStatus) {
      if (off + 1 + 1 + 8 + 16 > plain.size()) {
        continue;
      }
      const bool rec_group = plain[off++] != 0;
      const std::uint8_t raw_st = plain[off++];
      if (rec_group != is_group) {
        continue;
      }
      ChatHistoryStatus st;
      if (!tryParseStatus(raw_st, st)) {
        continue;
      }
      std::uint64_t ts = 0;
      if (!mi::server::proto::ReadUint64(plain, off, ts) ||
          off + 16 != plain.size()) {
        continue;
      }
      std::array<std::uint8_t, 16> msg_id{};
      std::memcpy(msg_id.data(), plain.data() + off, msg_id.size());
      const std::string id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
      auto st_it = status_by_id.find(id_hex);
      if (st_it == status_by_id.end()) {
        st_it = status_by_id.emplace(id_hex, st).first;
      } else {
        st_it->second = betterStatus(st_it->second, st);
      }

      const auto it = index_by_id.find(id_hex);
      if (it != index_by_id.end() && it->second < out_messages.size()) {
        out_messages[it->second].status =
            betterStatus(out_messages[it->second].status, st_it->second);
      }
      continue;
    }
    if (type != kRecordMessage) {
      continue;
    }
    if (off + 1 + 1 + 1 + 1 + 8 > plain.size()) {
      continue;
    }
    const std::uint8_t kind = plain[off++];
    const bool rec_group = plain[off++] != 0;
    const bool outgoing = plain[off++] != 0;
    const std::uint8_t raw_st = plain[off++];
    if (rec_group != is_group) {
      continue;
    }
    ChatHistoryStatus st;
    if (!tryParseStatus(raw_st, st)) {
      continue;
    }
    std::uint64_t ts = 0;
    if (!mi::server::proto::ReadUint64(plain, off, ts)) {
      continue;
    }

    ChatHistoryMessage m;
    m.is_group = rec_group;
    m.outgoing = outgoing;
    m.status = st;
    m.timestamp_sec = ts;
    m.conv_id = conv_id;

    if (kind == kMessageKindEnvelope) {
      if (!mi::server::proto::ReadString(plain, off, m.sender) ||
          !mi::server::proto::ReadBytes(plain, off, m.envelope)) {
        continue;
      }
      if (off < plain.size()) {
        std::size_t summary_off = off;
        std::vector<std::uint8_t> summary;
        if (mi::server::proto::ReadBytes(plain, summary_off, summary) &&
            summary_off == plain.size()) {
          m.summary = std::move(summary);
        }
      }
      m.is_system = false;
      std::array<std::uint8_t, 16> msg_id{};
      if (LooksLikeChatEnvelopeId(m.envelope, msg_id)) {
        const std::string id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
        const auto it = status_by_id.find(id_hex);
        if (it != status_by_id.end()) {
          m.status = betterStatus(m.status, it->second);
        }
        const auto prev = index_by_id.find(id_hex);
        if (prev != index_by_id.end() && prev->second < out_messages.size()) {
          ChatHistoryMessage& existing = out_messages[prev->second];
          existing.is_group = rec_group;
          existing.outgoing = outgoing;
          existing.is_system = false;
          existing.status = betterStatus(existing.status, m.status);
          existing.sender = std::move(m.sender);
          existing.envelope = std::move(m.envelope);
          existing.summary = std::move(m.summary);
          continue;
        }
        index_by_id.emplace(id_hex, out_messages.size());
      }
      out_messages.push_back(std::move(m));
      continue;
    }
    if (kind == kMessageKindSystem) {
      std::string text;
      if (!mi::server::proto::ReadString(plain, off, text) ||
          off != plain.size()) {
        continue;
      }
      m.is_system = true;
      m.system_text_utf8 = std::move(text);
      out_messages.push_back(std::move(m));
      continue;
    }
  }

  if (limit > 0 && out_messages.size() > limit) {
    out_messages.erase(out_messages.begin(),
                       out_messages.end() - static_cast<std::ptrdiff_t>(limit));
  }
  return true;
}

bool ChatHistoryStore::AppendEnvelope(bool is_group,
                                      bool outgoing,
                                      const std::string& conv_id,
                                      const std::string& sender,
                                      const std::vector<std::uint8_t>& envelope,
                                      ChatHistoryStatus status,
                                      std::uint64_t timestamp_sec,
                                      std::string& error) {
  error.clear();
  if (read_only_) {
    return true;
  }
  if (!EnsureKeyLoaded(error)) {
    return false;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }
  if (envelope.empty()) {
    error = "envelope empty";
    return false;
  }

  std::filesystem::path path;
  std::array<std::uint8_t, 32> conv_key{};
  std::uint8_t file_version = kContainerVersionV2;
  if (!EnsureHistoryFile(is_group, conv_id, path, conv_key, file_version,
                         error)) {
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out) {
    error = "history write failed";
    return false;
  }

  std::vector<std::uint8_t> summary;
  (void)BuildEnvelopeSummary(envelope, summary);

  std::vector<std::uint8_t> rec;
  rec.reserve(5 + 8 + 2 + sender.size() + 4 + envelope.size() +
              (summary.empty() ? 0 : (4 + summary.size())));
  rec.push_back(kRecordMessage);
  rec.push_back(kMessageKindEnvelope);
  rec.push_back(is_group ? 1 : 0);
  rec.push_back(outgoing ? 1 : 0);
  rec.push_back(static_cast<std::uint8_t>(status));
  mi::server::proto::WriteUint64(timestamp_sec, rec);
  if (!mi::server::proto::WriteString(sender, rec) ||
      !mi::server::proto::WriteBytes(envelope, rec)) {
    error = "history write failed";
    return false;
  }
  if (!summary.empty()) {
    if (!mi::server::proto::WriteBytes(summary, rec)) {
      error = "history write failed";
      return false;
    }
  }
  if (!WriteEncryptedRecord(out, master_key_, conv_key, is_group, conv_id, rec,
                            file_version, error)) {
    return false;
  }
  const std::string conv_key_id = MakeConvKey(is_group, conv_id);
  auto it = conv_to_file_.find(conv_key_id);
  if (it != conv_to_file_.end() && it->second < history_files_.size()) {
    UpdateEntryStats(history_files_[it->second], timestamp_sec, true);
    UpdateConvStats(history_files_[it->second], conv_key_id, timestamp_sec,
                    true);
    index_dirty_ = true;
  }
  std::string attach_err;
  (void)TouchAttachmentFromEnvelope(envelope, timestamp_sec, attach_err);
  return true;
}

bool ChatHistoryStore::AppendSystem(bool is_group,
                                    const std::string& conv_id,
                                    const std::string& text_utf8,
                                    std::uint64_t timestamp_sec,
                                    std::string& error) {
  error.clear();
  if (read_only_) {
    return true;
  }
  if (!EnsureKeyLoaded(error)) {
    return false;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }
  if (text_utf8.empty()) {
    error = "system text empty";
    return false;
  }

  std::filesystem::path path;
  std::array<std::uint8_t, 32> conv_key{};
  std::uint8_t file_version = kContainerVersionV2;
  if (!EnsureHistoryFile(is_group, conv_id, path, conv_key, file_version,
                         error)) {
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out) {
    error = "history write failed";
    return false;
  }

  std::vector<std::uint8_t> rec;
  rec.reserve(5 + 8 + 2 + text_utf8.size());
  rec.push_back(kRecordMessage);
  rec.push_back(kMessageKindSystem);
  rec.push_back(is_group ? 1 : 0);
  rec.push_back(0);
  rec.push_back(static_cast<std::uint8_t>(ChatHistoryStatus::kSent));
  mi::server::proto::WriteUint64(timestamp_sec, rec);
  if (!mi::server::proto::WriteString(text_utf8, rec)) {
    error = "history write failed";
    return false;
  }
  if (!WriteEncryptedRecord(out, master_key_, conv_key, is_group, conv_id, rec,
                            file_version, error)) {
    return false;
  }
  const std::string conv_key_id = MakeConvKey(is_group, conv_id);
  auto it = conv_to_file_.find(conv_key_id);
  if (it != conv_to_file_.end() && it->second < history_files_.size()) {
    UpdateEntryStats(history_files_[it->second], timestamp_sec, true);
    UpdateConvStats(history_files_[it->second], conv_key_id, timestamp_sec,
                    true);
    index_dirty_ = true;
  }
  return true;
}

bool ChatHistoryStore::AppendStatusUpdate(
    bool is_group,
    const std::string& conv_id,
    const std::array<std::uint8_t, 16>& msg_id,
    ChatHistoryStatus status,
    std::uint64_t timestamp_sec,
    std::string& error) {
  error.clear();
  if (read_only_) {
    return true;
  }
  if (!EnsureKeyLoaded(error)) {
    return false;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }
  if (IsAllZero(msg_id.data(), msg_id.size())) {
    error = "msg id empty";
    return false;
  }

  std::filesystem::path path;
  std::array<std::uint8_t, 32> conv_key{};
  std::uint8_t file_version = kContainerVersionV2;
  if (!EnsureHistoryFile(is_group, conv_id, path, conv_key, file_version,
                         error)) {
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out) {
    error = "history write failed";
    return false;
  }

  std::vector<std::uint8_t> rec;
  rec.reserve(1 + 1 + 1 + 8 + 16);
  rec.push_back(kRecordStatus);
  rec.push_back(is_group ? 1 : 0);
  rec.push_back(static_cast<std::uint8_t>(status));
  mi::server::proto::WriteUint64(timestamp_sec, rec);
  rec.insert(rec.end(), msg_id.begin(), msg_id.end());
  if (!WriteEncryptedRecord(out, master_key_, conv_key, is_group, conv_id, rec,
                            file_version, error)) {
    return false;
  }
  const std::string conv_key_id = MakeConvKey(is_group, conv_id);
  auto it = conv_to_file_.find(conv_key_id);
  if (it != conv_to_file_.end() && it->second < history_files_.size()) {
    UpdateEntryStats(history_files_[it->second], timestamp_sec, false);
    UpdateConvStats(history_files_[it->second], conv_key_id, timestamp_sec,
                    false);
    index_dirty_ = true;
  }
  return true;
}

bool ChatHistoryStore::StoreAttachmentPreview(
    const std::string& file_id,
    const std::string& file_name,
    std::uint64_t file_size,
    const std::vector<std::uint8_t>& plain,
    std::string& error) {
  error.clear();
  if (read_only_) {
    return true;
  }
  return UpdateAttachmentPreview(file_id, file_name, file_size, plain, error);
}

bool ChatHistoryStore::DeleteConversation(bool is_group,
                                          const std::string& conv_id,
                                          bool delete_attachments,
                                          bool secure_wipe,
                                          std::string& error) {
  error.clear();
  if (read_only_) {
    return true;
  }
  if (!EnsureKeyLoaded(error)) {
    return false;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }
  if (history_files_.empty()) {
    return true;
  }

  const std::string conv_key = MakeConvKey(is_group, conv_id);
  bool changed = false;

  if (delete_attachments) {
    std::string attach_err;
    (void)EnsureAttachmentsLoaded(attach_err);
  }

  auto rewrite_file = [&](HistoryFileEntry& entry) -> bool {
    std::ifstream in(entry.path, std::ios::binary);
    if (!in.is_open()) {
      error = "history open failed";
      return false;
    }
    std::uint32_t container_offset = 0;
    std::string hdr_err;
    if (!LocateContainerOffset(in, container_offset, hdr_err)) {
      error = "history read failed";
      return false;
    }
    in.clear();
    in.seekg(container_offset, std::ios::beg);
    std::uint8_t version = 0;
    if (!ReadContainerHeader(in, version, hdr_err)) {
      error = "history read failed";
      return false;
    }
    if (version != kContainerVersionV2) {
      error = "history read failed";
      return false;
    }
    (void)ConsumeMih3Header(in, master_key_, nullptr);

    const std::string tmp_path = entry.path.string() + ".purge";
    std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
    if (!out) {
      error = "history write failed";
      return false;
    }
    std::uint32_t hist_offset = 0;
    std::vector<std::uint8_t> stub = BuildPeContainer(hist_offset);
    out.write(reinterpret_cast<const char*>(stub.data()),
              static_cast<std::streamsize>(stub.size()));
    if (!out.good()) {
      error = "history write failed";
      return false;
    }
    if (!WriteContainerHeader(out, kContainerVersionV2, error)) {
      return false;
    }

    HistoryFileEntry new_entry = entry;
    new_entry.version = kContainerVersionV2;
    new_entry.conv_keys.clear();
    new_entry.conv_stats.clear();
    new_entry.conv_hashes.clear();
    new_entry.has_conv_hashes = false;
    new_entry.conv_keys_complete = false;
    new_entry.conv_stats_complete = false;
    new_entry.min_ts = 0;
    new_entry.max_ts = 0;
    new_entry.record_count = 0;
    new_entry.message_count = 0;
    new_entry.chain_valid = true;

    if (IsAllZero(new_entry.file_uuid.data(), new_entry.file_uuid.size())) {
      (void)mi::server::crypto::RandomBytes(new_entry.file_uuid.data(),
                                            new_entry.file_uuid.size());
    }
    std::uint32_t file_seq =
        new_entry.has_internal_seq ? new_entry.internal_seq : new_entry.seq;
    Mih3Summary mih3;
    mih3.file_seq = file_seq;
    mih3.file_uuid = new_entry.file_uuid;
    mih3.prev_hash = new_entry.prev_hash;
    if (!WriteMih3Block(out, master_key_, mih3, 0, error)) {
      return false;
    }

    std::array<std::uint8_t, 32> meta_key{};
    std::string meta_err;
    if (!DeriveConversationKey(false, kFileMetaConvId, meta_key, meta_err)) {
      error = meta_err.empty() ? "history write failed" : meta_err;
      return false;
    }
    std::vector<std::uint8_t> meta_rec;
    meta_rec.reserve(1 + 1 + 1 + 4 + new_entry.file_uuid.size() + 8);
    meta_rec.push_back(kRecordMeta);
    meta_rec.push_back(kMetaKindFileInfo);
    meta_rec.push_back(kMetaFileInfoVersion);
    if (!mi::server::proto::WriteUint32(file_seq, meta_rec)) {
      error = "history write failed";
      return false;
    }
    meta_rec.insert(meta_rec.end(), new_entry.file_uuid.begin(),
                    new_entry.file_uuid.end());
    mi::server::proto::WriteUint64(NowUnixSeconds(), meta_rec);
    std::string write_err;
    if (!WriteEncryptedRecord(out, master_key_, meta_key, false,
                              kFileMetaConvId, meta_rec, kContainerVersionV2,
                              write_err)) {
      error = write_err.empty() ? "history write failed" : write_err;
      return false;
    }

    while (true) {
      bool has_record = false;
      bool rec_group = false;
      std::string rec_conv;
      std::array<std::uint8_t, 24> inner_nonce{};
      std::vector<std::uint8_t> inner_cipher;
      std::array<std::uint8_t, 16> inner_mac{};
      std::string rec_err;
      const bool record_ok =
          (version >= kContainerVersionV2)
              ? ReadOuterRecordV2(in, master_key_, has_record, rec_group,
                                  rec_conv, inner_nonce, inner_cipher,
                                  inner_mac, rec_err)
              : ReadOuterRecord(in, master_key_, has_record, rec_group, rec_conv,
                                inner_nonce, inner_cipher, inner_mac, rec_err);
      if (!record_ok || !has_record) {
        break;
      }
      if (rec_conv.empty()) {
        continue;
      }
      if (rec_conv == kFileMetaConvId) {
        continue;
      }
      if (rec_group == is_group && rec_conv == conv_id) {
        if (delete_attachments) {
          std::array<std::uint8_t, 32> conv_key_bytes{};
          std::string key_err;
          if (DeriveConversationKey(rec_group, rec_conv, conv_key_bytes,
                                    key_err)) {
            std::vector<std::uint8_t> record_plain;
            std::string decode_err;
            if (DecodeInnerRecordPlain(conv_key_bytes, rec_group, rec_conv,
                                       inner_nonce, inner_cipher, inner_mac,
                                       record_plain, decode_err)) {
              if (!record_plain.empty() &&
                  record_plain[0] == kRecordMessage &&
                  record_plain.size() > 6 &&
                  record_plain[1] == kMessageKindEnvelope) {
                std::size_t off = 1 + 1 + 1 + 1 + 1;
                std::uint64_t ts = 0;
                (void)mi::server::proto::ReadUint64(record_plain, off, ts);
                std::string sender;
                std::vector<std::uint8_t> envelope;
                if (mi::server::proto::ReadString(record_plain, off, sender) &&
                    mi::server::proto::ReadBytes(record_plain, off, envelope)) {
                  std::string attach_err;
                  (void)ReleaseAttachmentFromEnvelope(envelope, attach_err);
                }
              }
            }
          }
        }
        continue;
      }

      std::array<std::uint8_t, 32> conv_key_bytes{};
      std::string key_err;
      if (!DeriveConversationKey(rec_group, rec_conv, conv_key_bytes, key_err)) {
        continue;
      }
      std::vector<std::uint8_t> record_plain;
      std::string decode_err;
      if (!DecodeInnerRecordPlain(conv_key_bytes, rec_group, rec_conv,
                                  inner_nonce, inner_cipher, inner_mac,
                                  record_plain, decode_err)) {
        continue;
      }
      if (!WriteEncryptedRecord(out, master_key_, conv_key_bytes, rec_group,
                                rec_conv, record_plain, kContainerVersionV2,
                                write_err)) {
        error = write_err.empty() ? "history write failed" : write_err;
        return false;
      }
      const std::string conv_key_id = MakeConvKey(rec_group, rec_conv);
      new_entry.conv_keys.insert(conv_key_id);
      bool is_message = false;
      std::uint64_t ts = 0;
      if (!record_plain.empty()) {
        const std::uint8_t kind = record_plain[0];
        if (kind == kRecordMessage) {
          std::size_t off = 1 + 1 + 1 + 1 + 1;
          if (record_plain.size() >= off + 8) {
            (void)mi::server::proto::ReadUint64(record_plain, off, ts);
          }
          is_message = true;
        } else if (kind == kRecordStatus) {
          std::size_t off = 1 + 1 + 1;
          if (record_plain.size() >= off + 8) {
            (void)mi::server::proto::ReadUint64(record_plain, off, ts);
          }
        } else if (kind == kRecordMeta &&
                   record_plain.size() >= 2 &&
                   record_plain[1] == kMetaKindFlush) {
          std::size_t off = 2;
          if (record_plain.size() >= off + 8) {
            (void)mi::server::proto::ReadUint64(record_plain, off, ts);
          }
        }
      }
      UpdateEntryStats(new_entry, ts, is_message);
      UpdateConvStats(new_entry, conv_key_id, ts, is_message);
    }

    new_entry.conv_keys_complete = true;
    if (!new_entry.conv_keys.empty() &&
        new_entry.conv_stats.size() >= new_entry.conv_keys.size()) {
      new_entry.conv_stats_complete = true;
    }
    std::vector<std::array<std::uint8_t, 16>> conv_hashes;
    std::vector<ChatHistoryConvStats> conv_stats;
    if (tag_key_loaded_ && !IsAllZero(tag_key_.data(), tag_key_.size())) {
      std::vector<std::pair<std::array<std::uint8_t, 16>, ChatHistoryConvStats>>
          conv_meta;
      conv_meta.reserve(new_entry.conv_keys.size());
      for (const auto& key : new_entry.conv_keys) {
        const auto h = DeriveConvHash(tag_key_, key);
        ChatHistoryConvStats stats;
        auto it = new_entry.conv_stats.find(key);
        if (it != new_entry.conv_stats.end()) {
          stats = it->second;
        }
        conv_meta.emplace_back(h, stats);
      }
      std::sort(conv_meta.begin(), conv_meta.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
      conv_hashes.reserve(conv_meta.size());
      conv_stats.reserve(conv_meta.size());
      for (const auto& pair : conv_meta) {
        conv_hashes.push_back(pair.first);
        conv_stats.push_back(pair.second);
      }
      new_entry.conv_hashes = conv_hashes;
      new_entry.has_conv_hashes = true;
    }

    std::vector<std::uint8_t> summary;
    summary.reserve(1 + 1 + 1 + 4 + new_entry.file_uuid.size() +
                    new_entry.prev_hash.size() + 8 * 4 + 4 +
                    conv_hashes.size() * 16 + conv_stats.size() * 8 * 4);
    summary.push_back(kRecordMeta);
    summary.push_back(kMetaKindFileSummary);
    summary.push_back(kMetaFileSummaryVersion);
    mi::server::proto::WriteUint32(file_seq, summary);
    summary.insert(summary.end(), new_entry.file_uuid.begin(),
                   new_entry.file_uuid.end());
    summary.insert(summary.end(), new_entry.prev_hash.begin(),
                   new_entry.prev_hash.end());
    mi::server::proto::WriteUint64(new_entry.min_ts, summary);
    mi::server::proto::WriteUint64(new_entry.max_ts, summary);
    mi::server::proto::WriteUint64(new_entry.record_count, summary);
    mi::server::proto::WriteUint64(new_entry.message_count, summary);
    mi::server::proto::WriteUint32(
        static_cast<std::uint32_t>(conv_hashes.size()), summary);
    for (const auto& h : conv_hashes) {
      summary.insert(summary.end(), h.begin(), h.end());
    }
    for (const auto& stats : conv_stats) {
      mi::server::proto::WriteUint64(stats.min_ts, summary);
      mi::server::proto::WriteUint64(stats.max_ts, summary);
      mi::server::proto::WriteUint64(stats.record_count, summary);
      mi::server::proto::WriteUint64(stats.message_count, summary);
    }
    if (!WriteEncryptedRecord(out, master_key_, meta_key, false,
                              kFileMetaConvId, summary, kContainerVersionV2,
                              write_err)) {
      error = write_err.empty() ? "history write failed" : write_err;
      return false;
    }

    Mih3Summary final_mih3;
    final_mih3.file_seq = file_seq;
    final_mih3.file_uuid = new_entry.file_uuid;
    final_mih3.prev_hash = new_entry.prev_hash;
    final_mih3.min_ts = new_entry.min_ts;
    final_mih3.max_ts = new_entry.max_ts;
    final_mih3.record_count = new_entry.record_count;
    final_mih3.message_count = new_entry.message_count;
    final_mih3.conv_count = static_cast<std::uint32_t>(conv_hashes.size());
    std::string mih3_err;
    (void)WriteMih3Block(out, master_key_, final_mih3, kMih3FlagTrailer,
                         mih3_err);
    out.flush();
    if (!out) {
      error = "history write failed";
      return false;
    }
    (void)UpdateMih3HeaderOnDisk(std::filesystem::path(tmp_path), master_key_,
                                 final_mih3, mih3_err);

    const std::string bak_path = entry.path.string() + ".bak";
    std::error_code ec;
    std::filesystem::remove(bak_path, ec);
    std::filesystem::rename(entry.path, bak_path, ec);
    if (ec) {
      std::filesystem::remove(tmp_path, ec);
      error = "history write failed";
      return false;
    }
    std::filesystem::rename(tmp_path, entry.path, ec);
    if (ec) {
      std::filesystem::rename(bak_path, entry.path, ec);
      std::filesystem::remove(tmp_path, ec);
      error = "history write failed";
      return false;
    }
    if (secure_wipe) {
      BestEffortWipeFile(bak_path);
    } else {
      std::filesystem::remove(bak_path, ec);
    }
    entry = std::move(new_entry);
    return true;
  };

  std::vector<HistoryFileEntry> updated;
  updated.reserve(history_files_.size());
  for (auto& entry : history_files_) {
    if (entry.conv_keys.find(conv_key) == entry.conv_keys.end()) {
      updated.push_back(entry);
      continue;
    }
    changed = true;
    if (entry.conv_keys.size() <= 1) {
      if (secure_wipe) {
        BestEffortWipeFile(entry.path);
      } else {
        std::error_code ec;
        std::filesystem::remove(entry.path, ec);
      }
      continue;
    }
    if (!rewrite_file(entry)) {
      return false;
    }
    updated.push_back(entry);
  }

  if (!changed) {
    return true;
  }
  history_files_ = std::move(updated);
  conv_to_file_.clear();
  for (std::size_t i = 0; i < history_files_.size(); ++i) {
    for (const auto& key : history_files_[i].conv_keys) {
      conv_to_file_[key] = i;
    }
  }
  RebuildConvHashIndex();
  const auto effectiveSeq = [](const HistoryFileEntry& entry) {
    return entry.has_internal_seq ? entry.internal_seq : entry.seq;
  };
  std::uint32_t max_seq = 0;
  for (const auto& f : history_files_) {
    max_seq = std::max(max_seq, effectiveSeq(f));
  }
  next_seq_ = max_seq + 1;
  index_dirty_ = true;
  std::string save_err;
  (void)SaveHistoryIndex(save_err);
  (void)SaveAttachmentsIndex(save_err);
  std::string flush_err;
  (void)Flush(flush_err);
  return true;
}

bool ChatHistoryStore::ClearAll(bool delete_attachments,
                                bool secure_wipe,
                                std::string& error) {
  error.clear();
  if (read_only_) {
    return true;
  }
  if (history_dir_.empty() || user_tag_.empty()) {
    error = "history dir empty";
    return false;
  }
  std::string lock_err;
  (void)AcquireProfileLock(lock_err);

  const std::string prefix = "main_" + user_tag_ + "_";
  std::error_code ec;
  if (std::filesystem::exists(history_dir_, ec) && !ec) {
    for (const auto& entry : std::filesystem::directory_iterator(history_dir_, ec)) {
      if (ec) {
        break;
      }
      if (!entry.is_regular_file(ec)) {
        continue;
      }
      const auto name = entry.path().filename().string();
      if (name.rfind(prefix, 0) != 0 || name.size() <= prefix.size() + 4 ||
          name.substr(name.size() - 4) != ".dll") {
        continue;
      }
      if (secure_wipe) {
        BestEffortWipeFile(entry.path());
      } else {
        std::filesystem::remove(entry.path(), ec);
      }
    }
  }

  if (!index_path_.empty()) {
    if (secure_wipe) {
      BestEffortWipeFile(index_path_);
    } else {
      std::filesystem::remove(index_path_, ec);
    }
  }
  if (!journal_path_.empty()) {
    if (secure_wipe) {
      BestEffortWipeFile(journal_path_);
    } else {
      std::filesystem::remove(journal_path_, ec);
    }
  }
  if (!key_path_.empty()) {
    if (secure_wipe) {
      BestEffortWipeFile(key_path_);
    } else {
      std::filesystem::remove(key_path_, ec);
    }
  }
  if (delete_attachments) {
    if (!attachments_index_path_.empty()) {
      if (secure_wipe) {
        BestEffortWipeFile(attachments_index_path_);
      } else {
        std::filesystem::remove(attachments_index_path_, ec);
      }
    }
    if (!attachments_dir_.empty() && std::filesystem::exists(attachments_dir_, ec) &&
        !ec) {
      for (const auto& entry :
           std::filesystem::directory_iterator(attachments_dir_, ec)) {
        if (ec) {
          break;
        }
        if (!entry.is_regular_file(ec)) {
          continue;
        }
        if (secure_wipe) {
          BestEffortWipeFile(entry.path());
        } else {
          std::filesystem::remove(entry.path(), ec);
        }
      }
      std::filesystem::remove_all(attachments_dir_, ec);
    }
  }

  history_files_.clear();
  conv_to_file_.clear();
  attachments_.clear();
  attachments_loaded_ = false;
  attachments_dirty_ = false;
  index_dirty_ = false;
  next_seq_ = 1;
  return true;
}

bool ChatHistoryStore::LoadConversation(bool is_group,
                                        const std::string& conv_id,
                                        std::size_t limit,
                                        std::vector<ChatHistoryMessage>& out_messages,
                                        std::string& error) {
  error.clear();
  out_messages.clear();
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    return true;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }

  if (!EnsureConversationMapped(is_group, conv_id, error)) {
    return true;
  }
  const std::string conv_key_id = MakeConvKey(is_group, conv_id);
  auto it = conv_to_file_.find(conv_key_id);
  if (it == conv_to_file_.end() || it->second >= history_files_.size()) {
    return true;
  }

  std::array<std::uint8_t, 32> conv_key{};
  if (!DeriveConversationKey(is_group, conv_id, conv_key, error)) {
    return false;
  }
  const auto path = history_files_[it->second].path;
  if (path.empty()) {
    error = "history path failed";
    return false;
  }

  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    error = "history open failed";
    return false;
  }

  std::uint32_t container_offset = 0;
  std::uint8_t version = 0;
  if (!LocateContainerOffset(in, container_offset, error)) {
    return false;
  }
  in.clear();
  in.seekg(container_offset, std::ios::beg);
  if (!ReadContainerHeader(in, version, error)) {
    return false;
  }
  (void)ConsumeMih3Header(in, master_key_, nullptr);
  if (version != kContainerVersionV2) {
    error = "history version mismatch";
    return false;
  }

  if (limit > 0) {
    out_messages.reserve(limit);
  }

  std::unordered_map<std::string, ChatHistoryStatus> status_by_id;
  std::unordered_map<std::string, std::size_t> index_by_id;
  std::size_t reserve_hint = 512;
  if (limit > 0) {
    reserve_hint = std::min<std::size_t>(limit * 2, 8192);
  }
  status_by_id.reserve(reserve_hint);
  index_by_id.reserve(reserve_hint);

  const auto statusRank = [](ChatHistoryStatus status) -> int {
    switch (status) {
      case ChatHistoryStatus::kFailed:
        return 0;
      case ChatHistoryStatus::kSent:
        return 1;
      case ChatHistoryStatus::kDelivered:
        return 2;
      case ChatHistoryStatus::kRead:
        return 3;
    }
    return 0;
  };
  const auto betterStatus = [&](ChatHistoryStatus a,
                                ChatHistoryStatus b) -> ChatHistoryStatus {
    return statusRank(a) >= statusRank(b) ? a : b;
  };
  const auto tryParseStatus = [](std::uint8_t raw,
                                 ChatHistoryStatus& out) -> bool {
    if (raw > static_cast<std::uint8_t>(ChatHistoryStatus::kFailed)) {
      return false;
    }
    out = static_cast<ChatHistoryStatus>(raw);
    return true;
  };

  while (true) {
    bool has_record = false;
    bool rec_group = false;
    std::string rec_conv;
    std::array<std::uint8_t, 24> inner_nonce{};
    std::vector<std::uint8_t> inner_cipher;
    std::array<std::uint8_t, 16> inner_mac{};
    std::string rec_err;
    const bool record_ok =
        (version >= kContainerVersionV2)
            ? ReadOuterRecordV2(in, master_key_, has_record, rec_group, rec_conv,
                                inner_nonce, inner_cipher, inner_mac, rec_err)
            : ReadOuterRecord(in, master_key_, has_record, rec_group, rec_conv,
                              inner_nonce, inner_cipher, inner_mac, rec_err);
    if (!record_ok) {
      error = rec_err.empty() ? "history read failed" : rec_err;
      return false;
    }
    if (!has_record) {
      break;
    }
    if (rec_group != is_group || rec_conv != conv_id) {
      continue;
    }
    if (inner_cipher.empty()) {
      continue;
    }
    std::vector<std::uint8_t> plain(inner_cipher.size());
    const int ok = crypto_aead_unlock(plain.data(), inner_mac.data(),
                                      conv_key.data(), inner_nonce.data(),
                                      nullptr, 0, inner_cipher.data(),
                                      inner_cipher.size());
    if (ok != 0) {
      error = "history auth failed";
      return false;
    }
    std::vector<std::uint8_t> padded;
    bool used_aes = false;
    std::string aes_err;
    if (!DecodeAesLayer(conv_key, is_group, conv_id, plain, padded, used_aes,
                        aes_err)) {
      error = aes_err.empty() ? "history read failed" : aes_err;
      return false;
    }
    (void)used_aes;
    std::vector<std::uint8_t> unpadded;
    std::string pad_err;
    if (!UnpadPlain(padded, unpadded, pad_err)) {
      error = pad_err.empty() ? "history read failed" : pad_err;
      return false;
    }
    std::vector<std::uint8_t> record_plain;
    bool used_compress = false;
    std::string comp_err;
    if (!DecodeCompressionLayer(unpadded, record_plain, used_compress,
                                comp_err)) {
      error = comp_err.empty() ? "history read failed" : comp_err;
      return false;
    }
    (void)used_compress;
    if (record_plain.empty()) {
      continue;
    }
    std::size_t off = 0;
    const std::uint8_t type = record_plain[off++];
    if (type == kRecordMeta) {
      continue;
    }
    if (type == kRecordStatus) {
      if (off + 1 + 1 + 8 + 16 > record_plain.size()) {
        continue;
      }
      const bool rec_is_group = record_plain[off++] != 0;
      const std::uint8_t raw_st = record_plain[off++];
      if (rec_is_group != is_group) {
        continue;
      }
      ChatHistoryStatus st;
      if (!tryParseStatus(raw_st, st)) {
        continue;
      }
      std::uint64_t ts = 0;
      if (!mi::server::proto::ReadUint64(record_plain, off, ts) ||
          off + 16 != record_plain.size()) {
        continue;
      }
      std::array<std::uint8_t, 16> msg_id{};
      std::memcpy(msg_id.data(), record_plain.data() + off, msg_id.size());
      const std::string id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
      auto st_it = status_by_id.find(id_hex);
      if (st_it == status_by_id.end()) {
        st_it = status_by_id.emplace(id_hex, st).first;
      } else {
        st_it->second = betterStatus(st_it->second, st);
      }

      const auto it = index_by_id.find(id_hex);
      if (it != index_by_id.end() && it->second < out_messages.size()) {
        out_messages[it->second].status =
            betterStatus(out_messages[it->second].status, st_it->second);
      }
      continue;
    }
    if (type != kRecordMessage) {
      continue;
    }
    if (off + 1 + 1 + 1 + 1 + 8 > record_plain.size()) {
      continue;
    }
    const std::uint8_t kind = record_plain[off++];
    const bool rec_is_group = record_plain[off++] != 0;
    const bool outgoing = record_plain[off++] != 0;
    const std::uint8_t raw_st = record_plain[off++];
    if (rec_is_group != is_group) {
      continue;
    }
    ChatHistoryStatus st;
    if (!tryParseStatus(raw_st, st)) {
      continue;
    }
    std::uint64_t ts = 0;
    if (!mi::server::proto::ReadUint64(record_plain, off, ts)) {
      continue;
    }

    ChatHistoryMessage m;
    m.is_group = rec_is_group;
    m.outgoing = outgoing;
    m.status = st;
    m.timestamp_sec = ts;
    m.conv_id = conv_id;

    if (kind == kMessageKindEnvelope) {
      if (!mi::server::proto::ReadString(record_plain, off, m.sender) ||
          !mi::server::proto::ReadBytes(record_plain, off, m.envelope)) {
        continue;
      }
      if (off < record_plain.size()) {
        std::size_t summary_off = off;
        std::vector<std::uint8_t> summary;
        if (mi::server::proto::ReadBytes(record_plain, summary_off, summary) &&
            summary_off == record_plain.size()) {
          m.summary = std::move(summary);
        }
      }
      m.is_system = false;
      std::array<std::uint8_t, 16> msg_id{};
      if (LooksLikeChatEnvelopeId(m.envelope, msg_id)) {
        const std::string id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
        const auto it = status_by_id.find(id_hex);
        if (it != status_by_id.end()) {
          m.status = betterStatus(m.status, it->second);
        }
        const auto prev = index_by_id.find(id_hex);
        if (prev != index_by_id.end() && prev->second < out_messages.size()) {
          ChatHistoryMessage& existing = out_messages[prev->second];
          existing.is_group = rec_is_group;
          existing.outgoing = outgoing;
          existing.is_system = false;
          existing.status = betterStatus(existing.status, m.status);
          existing.sender = std::move(m.sender);
          existing.envelope = std::move(m.envelope);
          existing.summary = std::move(m.summary);
          continue;
        }
        index_by_id.emplace(id_hex, out_messages.size());
      }
      out_messages.push_back(std::move(m));
      continue;
    }
    if (kind == kMessageKindSystem) {
      std::string text;
      if (!mi::server::proto::ReadString(record_plain, off, text) ||
          off != record_plain.size()) {
        continue;
      }
      m.is_system = true;
      m.system_text_utf8 = std::move(text);
      out_messages.push_back(std::move(m));
      continue;
    }
  }

  if (limit > 0 && out_messages.size() > limit) {
    out_messages.erase(out_messages.begin(),
                       out_messages.end() - static_cast<std::ptrdiff_t>(limit));
  }
  return true;
}

bool ChatHistoryStore::ExportRecentSnapshot(
    std::size_t max_conversations,
    std::size_t max_messages_per_conversation,
    std::vector<ChatHistoryMessage>& out_messages,
    std::string& error) {
  error.clear();
  out_messages.clear();
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    return true;
  }
  if (conv_to_file_.empty()) {
    return true;
  }

  struct ConvSnapshot {
    bool is_group{false};
    std::string conv_id;
    std::uint64_t last_ts{0};
    std::vector<ChatHistoryMessage> msgs;
  };

  struct ConvCandidate {
    bool is_group{false};
    std::string conv_id;
    std::string conv_key;
    std::uint64_t last_ts{0};
  };

  std::vector<ConvCandidate> candidates;
  candidates.reserve(conv_to_file_.size());
  for (const auto& kv : conv_to_file_) {
    const std::string& key = kv.first;
    if (key.size() < 3 || key[1] != ':') {
      continue;
    }
    const bool is_group = key[0] == 'g';
    const std::string conv_id = key.substr(2);
    if (conv_id.empty()) {
      continue;
    }
    std::uint64_t last_ts = 0;
    if (kv.second < history_files_.size()) {
      const auto& entry = history_files_[kv.second];
      auto it = entry.conv_stats.find(key);
      if (it != entry.conv_stats.end()) {
        last_ts = it->second.max_ts;
      } else if (entry.max_ts != 0) {
        last_ts = entry.max_ts;
      }
    }
    ConvCandidate c;
    c.is_group = is_group;
    c.conv_id = conv_id;
    c.conv_key = key;
    c.last_ts = last_ts;
    candidates.push_back(std::move(c));
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const ConvCandidate& a, const ConvCandidate& b) {
              if (a.last_ts != b.last_ts) {
                return a.last_ts > b.last_ts;
              }
              return a.conv_key < b.conv_key;
            });
  if (max_conversations > 0 && candidates.size() > max_conversations) {
    candidates.resize(max_conversations);
  }

  std::vector<ConvSnapshot> convs;
  convs.reserve(candidates.size());
  for (const auto& cand : candidates) {
    std::vector<ChatHistoryMessage> msgs;
    std::string load_err;
    if (!LoadConversation(cand.is_group, cand.conv_id,
                          max_messages_per_conversation, msgs, load_err) ||
        msgs.empty()) {
      continue;
    }
    ConvSnapshot s;
    s.is_group = cand.is_group;
    s.conv_id = cand.conv_id;
    s.msgs = std::move(msgs);
    s.last_ts = 0;
    for (const auto& m : s.msgs) {
      s.last_ts = std::max(s.last_ts, m.timestamp_sec);
    }
    convs.push_back(std::move(s));
  }

  std::sort(convs.begin(), convs.end(),
            [](const ConvSnapshot& a, const ConvSnapshot& b) {
              return a.last_ts > b.last_ts;
            });

  for (auto& c : convs) {
    out_messages.insert(out_messages.end(),
                        std::make_move_iterator(c.msgs.begin()),
                        std::make_move_iterator(c.msgs.end()));
  }
  return true;
}

bool ChatHistoryStore::Flush(std::string& error) {
  error.clear();
  if (read_only_) {
    return true;
  }
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    return true;
  }
  if (history_files_.empty()) {
    return true;
  }

  const std::uint64_t now_ts = NowUnixSeconds();
  for (const auto& entry : history_files_) {
    if (entry.path.empty() || entry.conv_keys.empty()) {
      continue;
    }
    const std::string& conv_key = *entry.conv_keys.begin();
    bool is_group = false;
    std::string conv_id;
    if (!ParseConvKey(conv_key, is_group, conv_id)) {
      continue;
    }

    std::array<std::uint8_t, 32> conv_key_bytes{};
    std::string key_err;
    if (!DeriveConversationKey(is_group, conv_id, conv_key_bytes, key_err)) {
      continue;
    }

    std::vector<std::uint8_t> rec;
    rec.reserve(1 + 1 + 8);
    rec.push_back(kRecordMeta);
    rec.push_back(kMetaKindFlush);
    mi::server::proto::WriteUint64(now_ts, rec);

    std::ofstream out(entry.path, std::ios::binary | std::ios::app);
    if (!out) {
      error = "history write failed";
      return false;
    }
    std::string write_err;
    if (!WriteEncryptedRecord(out, master_key_, conv_key_bytes, is_group,
                              conv_id, rec, entry.version, write_err)) {
      error = write_err.empty() ? "history write failed" : write_err;
      return false;
    }

    std::array<std::uint8_t, 32> meta_key{};
    std::string meta_err;
    if (DeriveConversationKey(false, kFileMetaConvId, meta_key, meta_err)) {
      const std::uint32_t file_seq =
          entry.has_internal_seq ? entry.internal_seq : entry.seq;
      const std::array<std::uint8_t, 32> prev_hash =
          entry.has_prev_hash ? entry.prev_hash
                              : std::array<std::uint8_t, 32>{};
      std::vector<std::array<std::uint8_t, 16>> conv_hashes;
      std::vector<ChatHistoryConvStats> conv_stats;
      if (tag_key_loaded_ && !IsAllZero(tag_key_.data(), tag_key_.size())) {
        std::vector<std::pair<std::array<std::uint8_t, 16>, ChatHistoryConvStats>>
            conv_meta;
        conv_meta.reserve(entry.conv_keys.size());
        for (const auto& key : entry.conv_keys) {
          const auto h = DeriveConvHash(tag_key_, key);
          ChatHistoryConvStats stats;
          auto it = entry.conv_stats.find(key);
          if (it != entry.conv_stats.end()) {
            stats = it->second;
          }
          conv_meta.emplace_back(h, stats);
        }
        std::sort(conv_meta.begin(), conv_meta.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        conv_hashes.reserve(conv_meta.size());
        conv_stats.reserve(conv_meta.size());
        for (const auto& pair : conv_meta) {
          conv_hashes.push_back(pair.first);
          conv_stats.push_back(pair.second);
        }
      }

      std::vector<std::uint8_t> summary;
      summary.reserve(1 + 1 + 1 + 4 + entry.file_uuid.size() +
                      prev_hash.size() + 8 * 4 + 4 +
                      conv_hashes.size() * 16 + conv_stats.size() * 8 * 4);
      summary.push_back(kRecordMeta);
      summary.push_back(kMetaKindFileSummary);
      summary.push_back(kMetaFileSummaryVersion);
      mi::server::proto::WriteUint32(file_seq, summary);
      summary.insert(summary.end(), entry.file_uuid.begin(),
                     entry.file_uuid.end());
      summary.insert(summary.end(), prev_hash.begin(), prev_hash.end());
      mi::server::proto::WriteUint64(entry.min_ts, summary);
      mi::server::proto::WriteUint64(entry.max_ts, summary);
      mi::server::proto::WriteUint64(entry.record_count, summary);
      mi::server::proto::WriteUint64(entry.message_count, summary);
      mi::server::proto::WriteUint32(
          static_cast<std::uint32_t>(conv_hashes.size()), summary);
      for (const auto& h : conv_hashes) {
        summary.insert(summary.end(), h.begin(), h.end());
      }
      for (const auto& stats : conv_stats) {
        mi::server::proto::WriteUint64(stats.min_ts, summary);
        mi::server::proto::WriteUint64(stats.max_ts, summary);
        mi::server::proto::WriteUint64(stats.record_count, summary);
        mi::server::proto::WriteUint64(stats.message_count, summary);
      }
      std::string sum_err;
      if (!WriteEncryptedRecord(out, master_key_, meta_key, false,
                                kFileMetaConvId, summary, entry.version,
                                sum_err)) {
        error = sum_err.empty() ? "history write failed" : sum_err;
        return false;
      }
      Mih3Summary mih3;
      mih3.file_seq = file_seq;
      mih3.file_uuid = entry.file_uuid;
      mih3.prev_hash = prev_hash;
      mih3.min_ts = entry.min_ts;
      mih3.max_ts = entry.max_ts;
      mih3.record_count = entry.record_count;
      mih3.message_count = entry.message_count;
      mih3.conv_count = static_cast<std::uint32_t>(conv_hashes.size());
      std::string mih3_err;
      (void)UpdateMih3HeaderOnDisk(entry.path, master_key_, mih3, mih3_err);
      (void)WriteMih3Block(out, master_key_, mih3, kMih3FlagTrailer, mih3_err);
    }
    std::vector<std::uint8_t> journal;
    journal.push_back(kJournalEntryFileStats);
    const std::string file_name = entry.path.filename().string();
    mi::server::proto::WriteString(file_name, journal);
    mi::server::proto::WriteUint64(entry.min_ts, journal);
    mi::server::proto::WriteUint64(entry.max_ts, journal);
    mi::server::proto::WriteUint64(entry.record_count, journal);
    mi::server::proto::WriteUint64(entry.message_count, journal);
    std::string journal_err;
    (void)AppendHistoryJournal(journal, journal_err);

    if (!entry.conv_keys.empty()) {
      std::vector<std::uint8_t> conv_journal;
      conv_journal.push_back(kJournalEntryConvStats);
      mi::server::proto::WriteString(file_name, conv_journal);
      mi::server::proto::WriteUint32(
          static_cast<std::uint32_t>(entry.conv_keys.size()), conv_journal);
      std::vector<std::string> conv_keys;
      conv_keys.reserve(entry.conv_keys.size());
      for (const auto& key : entry.conv_keys) {
        conv_keys.push_back(key);
      }
      std::sort(conv_keys.begin(), conv_keys.end());
      for (const auto& key : conv_keys) {
        ChatHistoryConvStats stats;
        auto it = entry.conv_stats.find(key);
        if (it != entry.conv_stats.end()) {
          stats = it->second;
        }
        mi::server::proto::WriteString(key, conv_journal);
        mi::server::proto::WriteUint64(stats.min_ts, conv_journal);
        mi::server::proto::WriteUint64(stats.max_ts, conv_journal);
        mi::server::proto::WriteUint64(stats.record_count, conv_journal);
        mi::server::proto::WriteUint64(stats.message_count, conv_journal);
      }
      (void)AppendHistoryJournal(conv_journal, journal_err);
    }
  }
  std::string save_err;
  if (!SaveHistoryIndex(save_err)) {
    error = save_err.empty() ? "history write failed" : save_err;
    return false;
  }
  if (!SaveAttachmentsIndex(save_err)) {
    error = save_err.empty() ? "history write failed" : save_err;
    return false;
  }
  return true;
}

}  // namespace mi::client
