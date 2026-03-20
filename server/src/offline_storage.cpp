#include "offline_storage.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "buffer_pool.h"
#include "crypto.h"
#include "hex_utils.h"
#include "monocypher.h"
#include "path_security.h"
#include "protected_store.h"
#include "platform_fs.h"
#include "secure_buffer.h"

namespace mi::server {

namespace pfs = mi::platform::fs;

namespace {

constexpr std::uint64_t kMaxBlobBytes = 320u * 1024u * 1024u;
constexpr std::uint32_t kMaxBlobChunkBytes = 4u * 1024u * 1024u;
constexpr auto kBlobSessionTtl = std::chrono::minutes(15);
constexpr std::size_t kOfflineFileAeadNonceBytes = 24;
constexpr std::size_t kOfflineFileAeadTagBytes = 16;
constexpr std::size_t kOfflineFileLegacyNonceBytes = 16;
constexpr std::size_t kOfflineFileLegacyTagBytes = 32;
constexpr std::array<std::uint8_t, 8> kOfflineFileMagic = {
    'M', 'I', 'O', 'F', 'A', 'E', 'A', 'D'};
constexpr std::uint8_t kOfflineFileMagicVersionV1 = 1;
constexpr std::uint8_t kOfflineFileMagicVersionV2 = 2;
constexpr std::uint8_t kOfflineFileMagicVersionV3 = 3;
constexpr std::uint8_t kOfflineFileMagicVersionLatest =
    kOfflineFileMagicVersionV3;
constexpr std::size_t kOfflineFileHeaderBytes =
    kOfflineFileMagic.size() + 1;
constexpr std::uint32_t kOfflineFileStreamChunkBytes = 1u * 1024u * 1024u;
constexpr std::uint32_t kOfflineFileStreamMaxChunkBytes = 8u * 1024u * 1024u;
constexpr std::size_t kOfflineFileV3PrefixBytes =
    kOfflineFileMagic.size() + 1 + 4 + 8;
constexpr std::size_t kOfflineFileV3HeaderBytes =
    kOfflineFileV3PrefixBytes + kOfflineFileAeadNonceBytes;
constexpr std::size_t kOfflineFileV3AdBytes = kOfflineFileV3PrefixBytes + 8;
constexpr std::array<std::uint8_t, 8> kOfflineQueueMagic = {
    'M', 'I', 'O', 'Q', 'M', 'S', 'G', '1'};
constexpr std::uint8_t kOfflineQueueVersion = 1;
constexpr std::size_t kOfflineQueueHeaderBytes =
    kOfflineQueueMagic.size() + 1 + 1 + 2 + 8 + 8 + 4 + 4 + 4 + 4 + 4;
constexpr std::array<std::uint8_t, 8> kOfflineMetaMagic = {
    'M', 'I', 'O', 'F', 'M', 'E', 'T', 'A'};
constexpr std::uint8_t kOfflineMetaVersion = 1;
constexpr std::size_t kOfflineMetaHeaderBytes =
    kOfflineMetaMagic.size() + 1 + 3 + 8 + 8 + 4;
constexpr std::array<std::uint8_t, 8> kOfflineMetaMapMagic = {
    'M', 'I', 'O', 'F', 'M', 'A', 'P', '1'};
constexpr std::uint8_t kOfflineMetaMapVersion = 1;
constexpr std::size_t kOfflineMetaMapHeaderBytes =
    kOfflineMetaMapMagic.size() + 1 + 3 + 4;
constexpr std::array<std::uint8_t, 8> kOfflineQueueStoreMagic = {
    'M', 'I', 'O', 'Q', 'S', 'T', 'R', '1'};
constexpr std::uint8_t kOfflineQueueStoreVersion = 1;
constexpr std::size_t kOfflineQueueStoreHeaderBytes =
    kOfflineQueueStoreMagic.size() + 1 + 3 + 4;
constexpr std::array<std::uint8_t, 8> kBlobUploadTokenMagic = {
    'M', 'I', 'U', 'P', 'T', 'O', 'K', '1'};
constexpr std::array<std::uint8_t, 8> kBlobDownloadTokenMagic = {
    'M', 'I', 'D', 'L', 'T', 'O', 'K', '1'};
constexpr std::size_t kBlobTokenMacBytes = 32;
constexpr std::size_t kBlobUploadTokenPrefixBytes = 8 + 8 + 8 + 16 + 16 + 32;
constexpr std::size_t kBlobUploadTokenBytes =
    kBlobUploadTokenPrefixBytes + kBlobTokenMacBytes;
constexpr std::size_t kBlobDownloadTokenPrefixBytes = 8 + 8 + 8 + 16 + 32;
constexpr std::size_t kBlobDownloadTokenBytes =
    kBlobDownloadTokenPrefixBytes + kBlobTokenMacBytes;
constexpr std::array<std::uint8_t, 8> kBlobUploadTagMagic = {
    'M', 'I', 'U', 'P', 'T', 'A', 'G', '1'};
constexpr std::uint8_t kBlobUploadTagVersion = 1;
constexpr std::size_t kBlobUploadTagBytes =
    kBlobUploadTagMagic.size() + 1 + 3 + 8 + 32;

mi::common::ByteBufferPool& OfflineStorageBufferPool() {
  static mi::common::ByteBufferPool pool(32, 16u * 1024u * 1024u);
  return pool;
}

bool FillRandom(std::uint8_t* out, std::size_t len) {
  if (!out || len == 0) {
    return false;
  }
  return crypto::RandomBytes(out, len);
}

std::array<std::uint8_t, kOfflineFileAeadNonceBytes> RandomAeadNonce() {
  std::array<std::uint8_t, kOfflineFileAeadNonceBytes> nonce{};
  (void)FillRandom(nonce.data(), nonce.size());
  return nonce;
}

std::array<std::uint8_t, kOfflineFileLegacyNonceBytes> RandomLegacyNonce() {
  std::array<std::uint8_t, kOfflineFileLegacyNonceBytes> nonce{};
  (void)FillRandom(nonce.data(), nonce.size());
  return nonce;
}

bool ReadExact(std::istream& is, std::uint8_t* out, std::size_t len) {
  if (!out || len == 0) {
    return true;
  }
  is.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(len));
  return is && static_cast<std::size_t>(is.gcount()) == len;
}

void WriteUint32Le(std::uint32_t v, std::uint8_t* out) {
  out[0] = static_cast<std::uint8_t>(v & 0xFFu);
  out[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
  out[2] = static_cast<std::uint8_t>((v >> 16) & 0xFFu);
  out[3] = static_cast<std::uint8_t>((v >> 24) & 0xFFu);
}

void WriteUint64Le(std::uint64_t v, std::uint8_t* out) {
  out[0] = static_cast<std::uint8_t>(v & 0xFFu);
  out[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
  out[2] = static_cast<std::uint8_t>((v >> 16) & 0xFFu);
  out[3] = static_cast<std::uint8_t>((v >> 24) & 0xFFu);
  out[4] = static_cast<std::uint8_t>((v >> 32) & 0xFFu);
  out[5] = static_cast<std::uint8_t>((v >> 40) & 0xFFu);
  out[6] = static_cast<std::uint8_t>((v >> 48) & 0xFFu);
  out[7] = static_cast<std::uint8_t>((v >> 56) & 0xFFu);
}

std::uint32_t ReadUint32Le(const std::uint8_t* in) {
  return static_cast<std::uint32_t>(in[0]) |
         (static_cast<std::uint32_t>(in[1]) << 8) |
         (static_cast<std::uint32_t>(in[2]) << 16) |
         (static_cast<std::uint32_t>(in[3]) << 24);
}

std::uint64_t ReadUint64Le(const std::uint8_t* in) {
  return static_cast<std::uint64_t>(in[0]) |
         (static_cast<std::uint64_t>(in[1]) << 8) |
         (static_cast<std::uint64_t>(in[2]) << 16) |
         (static_cast<std::uint64_t>(in[3]) << 24) |
         (static_cast<std::uint64_t>(in[4]) << 32) |
         (static_cast<std::uint64_t>(in[5]) << 40) |
         (static_cast<std::uint64_t>(in[6]) << 48) |
         (static_cast<std::uint64_t>(in[7]) << 56);
}

std::optional<std::uint64_t> RecoverOfflinePlainSize(
    const std::filesystem::path& path,
    std::uint64_t file_size) {
  if (file_size == 0) {
    return std::nullopt;
  }
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    return std::nullopt;
  }
  std::array<std::uint8_t, kOfflineFileMagic.size()> magic{};
  if (!ReadExact(ifs, magic.data(), magic.size())) {
    return std::nullopt;
  }
  if (!std::equal(kOfflineFileMagic.begin(), kOfflineFileMagic.end(),
                  magic.begin())) {
    return std::nullopt;
  }
  std::uint8_t version = 0;
  if (!ReadExact(ifs, &version, 1)) {
    return std::nullopt;
  }
  if (version == kOfflineFileMagicVersionV3) {
    if (file_size < kOfflineFileV3HeaderBytes) {
      return std::nullopt;
    }
    std::array<std::uint8_t, 4> chunk_buf{};
    std::array<std::uint8_t, 8> size_buf{};
    std::array<std::uint8_t, kOfflineFileAeadNonceBytes> nonce{};
    if (!ReadExact(ifs, chunk_buf.data(), chunk_buf.size()) ||
        !ReadExact(ifs, size_buf.data(), size_buf.size()) ||
        !ReadExact(ifs, nonce.data(), nonce.size())) {
      return std::nullopt;
    }
    const std::uint32_t chunk_bytes = ReadUint32Le(chunk_buf.data());
    const std::uint64_t plain_size = ReadUint64Le(size_buf.data());
    if (chunk_bytes == 0 || chunk_bytes > kOfflineFileStreamMaxChunkBytes ||
        plain_size == 0) {
      return std::nullopt;
    }
    const std::uint64_t chunk_count =
        (plain_size + chunk_bytes - 1) / chunk_bytes;
    if (chunk_count == 0 ||
        chunk_count >
            (std::numeric_limits<std::uint64_t>::max)() /
                static_cast<std::uint64_t>(kOfflineFileAeadTagBytes)) {
      return std::nullopt;
    }
    const std::uint64_t tag_overhead =
        chunk_count * static_cast<std::uint64_t>(kOfflineFileAeadTagBytes);
    if (tag_overhead >
        (std::numeric_limits<std::uint64_t>::max)() -
            kOfflineFileV3HeaderBytes - plain_size) {
      return std::nullopt;
    }
    const std::uint64_t expected_size =
        kOfflineFileV3HeaderBytes + plain_size + tag_overhead;
    if (file_size != expected_size) {
      return std::nullopt;
    }
    return plain_size;
  }
  if (version == kOfflineFileMagicVersionV1 ||
      version == kOfflineFileMagicVersionV2) {
    const std::uint64_t overhead =
        kOfflineFileHeaderBytes + kOfflineFileAeadNonceBytes +
        kOfflineFileAeadTagBytes;
    if (file_size <= overhead) {
      return std::nullopt;
    }
    return file_size - overhead;
  }
  return std::nullopt;
}

std::uint64_t UnixMsFrom(const std::chrono::system_clock::time_point& tp) {
  const auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          tp.time_since_epoch())
          .count();
  return ms < 0 ? 0u : static_cast<std::uint64_t>(ms);
}

std::chrono::system_clock::time_point UnixMsToTimepoint(std::uint64_t ms) {
  return std::chrono::system_clock::time_point(
      std::chrono::milliseconds(ms));
}

std::chrono::system_clock::time_point FileTimeToSystem(
    const std::filesystem::file_time_type& ft) {
  using namespace std::chrono;
  const auto now_file = std::filesystem::file_time_type::clock::now();
  const auto now_sys = system_clock::now();
  const auto delta = ft - now_file;
  return now_sys + duration_cast<system_clock::duration>(delta);
}

std::chrono::steady_clock::time_point SteadyFromSystem(
    const std::chrono::system_clock::time_point& tp,
    const std::chrono::system_clock::time_point& now_sys,
    const std::chrono::steady_clock::time_point& now_steady) {
  auto sys = tp;
  if (sys > now_sys) {
    sys = now_sys;
  }
  const auto age = now_sys - sys;
  return now_steady -
         std::chrono::duration_cast<std::chrono::steady_clock::duration>(age);
}

std::chrono::system_clock::time_point SystemFromSteady(
    const std::chrono::steady_clock::time_point& tp,
    const std::chrono::system_clock::time_point& now_sys,
    const std::chrono::steady_clock::time_point& now_steady) {
  const auto age =
      now_steady > tp ? (now_steady - tp) : std::chrono::steady_clock::duration{};
  return now_sys -
         std::chrono::duration_cast<std::chrono::system_clock::duration>(age);
}

bool EncodeOfflineMeta(const StoredFileMeta& meta,
                       const std::chrono::system_clock::time_point& now_sys,
                       const std::chrono::steady_clock::time_point& now_steady,
                       std::vector<std::uint8_t>& out) {
  out.clear();
  if (meta.owner.size() >
      static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())) {
    return false;
  }
  out.reserve(kOfflineMetaHeaderBytes + meta.owner.size());
  out.insert(out.end(), kOfflineMetaMagic.begin(), kOfflineMetaMagic.end());
  out.push_back(kOfflineMetaVersion);
  out.push_back(0);
  out.push_back(0);
  out.push_back(0);
  const auto created_sys = SystemFromSteady(meta.created_at, now_sys, now_steady);
  std::uint8_t buf[8] = {};
  WriteUint64Le(UnixMsFrom(created_sys), buf);
  out.insert(out.end(), buf, buf + 8);
  WriteUint64Le(meta.size, buf);
  out.insert(out.end(), buf, buf + 8);
  std::uint8_t buf32[4] = {};
  WriteUint32Le(static_cast<std::uint32_t>(meta.owner.size()), buf32);
  out.insert(out.end(), buf32, buf32 + 4);
  out.insert(out.end(), meta.owner.begin(), meta.owner.end());
  return true;
}

bool DecodeOfflineMeta(const std::vector<std::uint8_t>& data,
                       const std::chrono::system_clock::time_point& now_sys,
                       const std::chrono::steady_clock::time_point& now_steady,
                       StoredFileMeta& out) {
  out = StoredFileMeta{};
  if (data.size() < kOfflineMetaHeaderBytes) {
    return false;
  }
  if (!std::equal(kOfflineMetaMagic.begin(), kOfflineMetaMagic.end(),
                  data.begin())) {
    return false;
  }
  std::size_t off = kOfflineMetaMagic.size();
  const std::uint8_t version = data[off++];
  if (version != kOfflineMetaVersion) {
    return false;
  }
  off += 3;
  if (off + 8 + 8 + 4 > data.size()) {
    return false;
  }
  const std::uint64_t created_ms = ReadUint64Le(data.data() + off);
  off += 8;
  out.size = ReadUint64Le(data.data() + off);
  off += 8;
  const std::uint32_t owner_len = ReadUint32Le(data.data() + off);
  off += 4;
  if (off + owner_len != data.size()) {
    return false;
  }
  if (owner_len > 0) {
    out.owner.assign(reinterpret_cast<const char*>(data.data() + off),
                     reinterpret_cast<const char*>(data.data() + off + owner_len));
  }
  const auto created_sys = UnixMsToTimepoint(created_ms);
  out.created_at = SteadyFromSystem(created_sys, now_sys, now_steady);
  return true;
}

bool EncodeOfflineMetaMap(
    const std::unordered_map<std::string, StoredFileMeta>& metadata,
    const std::chrono::system_clock::time_point& now_sys,
    const std::chrono::steady_clock::time_point& now_steady,
    std::vector<std::uint8_t>& out) {
  out.clear();
  if (metadata.size() >
      static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())) {
    return false;
  }
  std::vector<std::string> keys;
  keys.reserve(metadata.size());
  for (const auto& kv : metadata) {
    keys.push_back(kv.first);
  }
  std::sort(keys.begin(), keys.end());

  out.reserve(kOfflineMetaMapHeaderBytes + keys.size() * 64);
  out.insert(out.end(), kOfflineMetaMapMagic.begin(),
             kOfflineMetaMapMagic.end());
  out.push_back(kOfflineMetaMapVersion);
  out.push_back(0);
  out.push_back(0);
  out.push_back(0);
  std::uint8_t buf32[4] = {};
  WriteUint32Le(static_cast<std::uint32_t>(keys.size()), buf32);
  out.insert(out.end(), buf32, buf32 + 4);

  for (const auto& id : keys) {
    const auto it = metadata.find(id);
    if (it == metadata.end()) {
      continue;
    }
    if (id.empty() ||
        id.size() >
            static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)())) {
      return false;
    }
    std::vector<std::uint8_t> meta_bytes;
    StoredFileMeta meta = it->second;
    if (!EncodeOfflineMeta(meta, now_sys, now_steady, meta_bytes)) {
      return false;
    }
    if (meta_bytes.size() >
        static_cast<std::size_t>(
            (std::numeric_limits<std::uint32_t>::max)())) {
      return false;
    }
    WriteUint32Le(static_cast<std::uint32_t>(id.size()), buf32);
    out.insert(out.end(), buf32, buf32 + 4);
    WriteUint32Le(static_cast<std::uint32_t>(meta_bytes.size()), buf32);
    out.insert(out.end(), buf32, buf32 + 4);
    out.insert(out.end(), id.begin(), id.end());
    out.insert(out.end(), meta_bytes.begin(), meta_bytes.end());
  }
  return true;
}

bool DecodeOfflineMetaMap(
    const std::vector<std::uint8_t>& data,
    const std::chrono::system_clock::time_point& now_sys,
    const std::chrono::steady_clock::time_point& now_steady,
    std::unordered_map<std::string, StoredFileMeta>& out) {
  out.clear();
  if (data.size() < kOfflineMetaMapHeaderBytes) {
    return false;
  }
  if (!std::equal(kOfflineMetaMapMagic.begin(), kOfflineMetaMapMagic.end(),
                  data.begin())) {
    return false;
  }
  std::size_t off = kOfflineMetaMapMagic.size();
  const std::uint8_t version = data[off++];
  if (version != kOfflineMetaMapVersion) {
    return false;
  }
  off += 3;
  if (off + 4 > data.size()) {
    return false;
  }
  const std::uint32_t count = ReadUint32Le(data.data() + off);
  off += 4;
  out.reserve(count);

  for (std::uint32_t i = 0; i < count; ++i) {
    if (off + 8 > data.size()) {
      return false;
    }
    const std::uint32_t id_len = ReadUint32Le(data.data() + off);
    off += 4;
    const std::uint32_t meta_len = ReadUint32Le(data.data() + off);
    off += 4;
    if (id_len == 0 || off + id_len + meta_len > data.size()) {
      return false;
    }
    std::string id(
        reinterpret_cast<const char*>(data.data() + off),
        reinterpret_cast<const char*>(data.data() + off + id_len));
    off += id_len;
    std::vector<std::uint8_t> meta_bytes;
    if (meta_len != 0) {
      meta_bytes.assign(data.begin() + static_cast<std::ptrdiff_t>(off),
                        data.begin() +
                            static_cast<std::ptrdiff_t>(off + meta_len));
      off += meta_len;
    }
    StoredFileMeta meta;
    if (!DecodeOfflineMeta(meta_bytes, now_sys, now_steady, meta)) {
      return false;
    }
    meta.id = id;
    out.emplace(std::move(id), std::move(meta));
  }

  return off == data.size();
}

std::string FormatMessageId(std::uint64_t id) {
  std::ostringstream oss;
  oss << std::setw(20) << std::setfill('0') << id;
  return oss.str();
}

bool EnforceOwnerOnlyPermissions(const std::filesystem::path& path,
                                 std::string& error) {
  error.clear();
#ifdef _WIN32
  std::string acl_err;
  if (!mi::shard::security::HardenPathAcl(path, acl_err)) {
    error = acl_err.empty() ? "file permissions set failed" : acl_err;
    return false;
  }
#else
  std::error_code ec;
  std::filesystem::permissions(
      path, std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace, ec);
  if (ec) {
    error = "file permissions set failed";
    return false;
  }
#endif

  std::string perm_err;
  if (!mi::shard::security::CheckPathNotWorldWritable(path, perm_err)) {
    error = perm_err.empty() ? "file permissions insecure" : perm_err;
    return false;
  }
  return true;
}

bool AtomicWriteOwnerOnly(const std::filesystem::path& path,
                          const std::uint8_t* data,
                          std::size_t len,
                          std::string& error) {
  error.clear();
  std::error_code ec;
  if (!pfs::AtomicWrite(path, data, len, ec) || ec) {
    error = "write file failed";
    return false;
  }
  std::string perm_err;
  if (!EnforceOwnerOnlyPermissions(path, perm_err)) {
    std::filesystem::remove(path, ec);
    error = perm_err.empty() ? "file permissions insecure" : perm_err;
    return false;
  }
  return true;
}

std::string BytesToHexLower(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return {};
  }
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    const std::uint8_t v = data[i];
    out[i * 2] = kHex[v >> 4];
    out[i * 2 + 1] = kHex[v & 0x0F];
  }
  return out;
}

bool IsValidFileId(const std::string& file_id);

bool FileIdHexToBytes(const std::string& file_id,
                      std::array<std::uint8_t, 16>& out) {
  if (!IsValidFileId(file_id)) {
    return false;
  }
  std::vector<std::uint8_t> bytes;
  if (!mi::common::HexToBytes(file_id, bytes) || bytes.size() != out.size()) {
    return false;
  }
  std::memcpy(out.data(), bytes.data(), out.size());
  return true;
}

std::array<std::uint8_t, 32> HashOwner(const std::string& owner) {
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(
      reinterpret_cast<const std::uint8_t*>(owner.data()), owner.size(), d);
  return d.bytes;
}

bool GetBlobTokenMasterKey(std::array<std::uint8_t, 32>& out) {
  struct KeyState {
    std::array<std::uint8_t, 32> key{};
    bool ready{false};
  };
  static const KeyState state = [] {
    KeyState s;
    s.ready = FillRandom(s.key.data(), s.key.size());
    return s;
  }();
  if (!state.ready) {
    out.fill(0);
    return false;
  }
  out = state.key;
  return true;
}

struct BlobUploadTokenClaims {
  std::array<std::uint8_t, 16> nonce{};
  std::uint64_t expected_size{0};
};

struct BlobDownloadTokenClaims {
  bool wipe_after_read{false};
};

std::string EncodeBlobUploadToken(const std::string& owner,
                                  const std::string& file_id,
                                  std::uint64_t expected_size,
                                  std::string& error) {
  error.clear();
  std::array<std::uint8_t, 16> file_id_bytes{};
  if (!FileIdHexToBytes(file_id, file_id_bytes)) {
    error = "invalid file id";
    return {};
  }
  std::array<std::uint8_t, 16> nonce{};
  if (!FillRandom(nonce.data(), nonce.size())) {
    error = "rng failed";
    return {};
  }

  std::array<std::uint8_t, kBlobUploadTokenBytes> token{};
  std::size_t off = 0;
  std::memcpy(token.data() + off, kBlobUploadTokenMagic.data(),
              kBlobUploadTokenMagic.size());
  off += kBlobUploadTokenMagic.size();
  const std::uint64_t exp_ms = UnixMsFrom(std::chrono::system_clock::now() +
                                          kBlobSessionTtl);
  WriteUint64Le(exp_ms, token.data() + off);
  off += 8;
  WriteUint64Le(expected_size, token.data() + off);
  off += 8;
  std::memcpy(token.data() + off, nonce.data(), nonce.size());
  off += nonce.size();
  std::memcpy(token.data() + off, file_id_bytes.data(), file_id_bytes.size());
  off += file_id_bytes.size();
  const auto owner_hash = HashOwner(owner);
  std::memcpy(token.data() + off, owner_hash.data(), owner_hash.size());
  off += owner_hash.size();

  mi::server::crypto::Sha256Digest mac;
  std::array<std::uint8_t, 32> key{};
  if (!GetBlobTokenMasterKey(key)) {
    error = "rng failed";
    return {};
  }
  mi::server::crypto::HmacSha256(key.data(), key.size(), token.data(),
                                 kBlobUploadTokenPrefixBytes, mac);
  std::memcpy(token.data() + off, mac.bytes.data(), mac.bytes.size());
  return BytesToHexLower(token.data(), token.size());
}

bool DecodeBlobUploadToken(const std::string& token_hex,
                           const std::string& owner,
                           const std::string& file_id,
                           BlobUploadTokenClaims& claims,
                           std::string& error) {
  error.clear();
  std::vector<std::uint8_t> token;
  if (!mi::common::HexToBytes(token_hex, token) ||
      token.size() != kBlobUploadTokenBytes) {
    error = "invalid session";
    return false;
  }
  if (!std::equal(kBlobUploadTokenMagic.begin(), kBlobUploadTokenMagic.end(),
                  token.begin())) {
    error = "invalid session";
    return false;
  }
  std::array<std::uint8_t, 16> file_id_bytes{};
  if (!FileIdHexToBytes(file_id, file_id_bytes)) {
    error = "invalid file id";
    return false;
  }
  std::size_t off = kBlobUploadTokenMagic.size();
  const std::uint64_t exp_ms = ReadUint64Le(token.data() + off);
  off += 8;
  claims.expected_size = ReadUint64Le(token.data() + off);
  off += 8;
  std::memcpy(claims.nonce.data(), token.data() + off, claims.nonce.size());
  off += claims.nonce.size();
  if (!std::equal(file_id_bytes.begin(), file_id_bytes.end(),
                  token.begin() + static_cast<std::ptrdiff_t>(off))) {
    error = "invalid session";
    return false;
  }
  off += file_id_bytes.size();
  const auto owner_hash = HashOwner(owner);
  if (crypto_verify32(owner_hash.data(),
                      token.data() + static_cast<std::ptrdiff_t>(off)) != 0) {
    error = "unauthorized";
    return false;
  }
  mi::server::crypto::Sha256Digest mac;
  std::array<std::uint8_t, 32> key{};
  if (!GetBlobTokenMasterKey(key)) {
    error = "session subsystem unavailable";
    return false;
  }
  mi::server::crypto::HmacSha256(key.data(), key.size(), token.data(),
                                 kBlobUploadTokenPrefixBytes, mac);
  if (crypto_verify32(
          mac.bytes.data(),
          token.data() + static_cast<std::ptrdiff_t>(kBlobUploadTokenPrefixBytes)) !=
      0) {
    error = "invalid session";
    return false;
  }
  if (UnixMsFrom(std::chrono::system_clock::now()) > exp_ms) {
    error = "upload session expired";
    return false;
  }
  return true;
}

std::string EncodeBlobDownloadToken(const std::string& owner,
                                    const std::string& file_id,
                                    bool wipe_after_read,
                                    std::string& error) {
  error.clear();
  std::array<std::uint8_t, 16> file_id_bytes{};
  if (!FileIdHexToBytes(file_id, file_id_bytes)) {
    error = "invalid file id";
    return {};
  }
  std::array<std::uint8_t, kBlobDownloadTokenBytes> token{};
  std::size_t off = 0;
  std::memcpy(token.data() + off, kBlobDownloadTokenMagic.data(),
              kBlobDownloadTokenMagic.size());
  off += kBlobDownloadTokenMagic.size();
  const std::uint64_t exp_ms = UnixMsFrom(std::chrono::system_clock::now() +
                                          kBlobSessionTtl);
  WriteUint64Le(exp_ms, token.data() + off);
  off += 8;
  token[off++] = wipe_after_read ? 1u : 0u;
  token[off++] = 0;
  token[off++] = 0;
  token[off++] = 0;
  token[off++] = 0;
  token[off++] = 0;
  token[off++] = 0;
  token[off++] = 0;
  std::memcpy(token.data() + off, file_id_bytes.data(), file_id_bytes.size());
  off += file_id_bytes.size();
  const auto owner_hash = HashOwner(owner);
  std::memcpy(token.data() + off, owner_hash.data(), owner_hash.size());
  off += owner_hash.size();

  mi::server::crypto::Sha256Digest mac;
  std::array<std::uint8_t, 32> key{};
  if (!GetBlobTokenMasterKey(key)) {
    error = "rng failed";
    return {};
  }
  mi::server::crypto::HmacSha256(key.data(), key.size(), token.data(),
                                 kBlobDownloadTokenPrefixBytes, mac);
  std::memcpy(token.data() + off, mac.bytes.data(), mac.bytes.size());
  return BytesToHexLower(token.data(), token.size());
}

bool DecodeBlobDownloadToken(const std::string& token_hex,
                             const std::string& owner,
                             const std::string& file_id,
                             BlobDownloadTokenClaims& claims,
                             std::string& error) {
  error.clear();
  std::vector<std::uint8_t> token;
  if (!mi::common::HexToBytes(token_hex, token) ||
      token.size() != kBlobDownloadTokenBytes) {
    error = "invalid session";
    return false;
  }
  if (!std::equal(kBlobDownloadTokenMagic.begin(), kBlobDownloadTokenMagic.end(),
                  token.begin())) {
    error = "invalid session";
    return false;
  }
  std::array<std::uint8_t, 16> file_id_bytes{};
  if (!FileIdHexToBytes(file_id, file_id_bytes)) {
    error = "invalid file id";
    return false;
  }
  std::size_t off = kBlobDownloadTokenMagic.size();
  const std::uint64_t exp_ms = ReadUint64Le(token.data() + off);
  off += 8;
  claims.wipe_after_read = token[off] != 0;
  off += 8;
  if (!std::equal(file_id_bytes.begin(), file_id_bytes.end(),
                  token.begin() + static_cast<std::ptrdiff_t>(off))) {
    error = "invalid session";
    return false;
  }
  off += file_id_bytes.size();
  const auto owner_hash = HashOwner(owner);
  if (crypto_verify32(owner_hash.data(),
                      token.data() + static_cast<std::ptrdiff_t>(off)) != 0) {
    error = "unauthorized";
    return false;
  }
  mi::server::crypto::Sha256Digest mac;
  std::array<std::uint8_t, 32> key{};
  if (!GetBlobTokenMasterKey(key)) {
    error = "session subsystem unavailable";
    return false;
  }
  mi::server::crypto::HmacSha256(key.data(), key.size(), token.data(),
                                 kBlobDownloadTokenPrefixBytes, mac);
  if (crypto_verify32(
          mac.bytes.data(),
          token.data() +
              static_cast<std::ptrdiff_t>(kBlobDownloadTokenPrefixBytes)) != 0) {
    error = "invalid session";
    return false;
  }
  if (UnixMsFrom(std::chrono::system_clock::now()) > exp_ms) {
    error = "download session expired";
    return false;
  }
  return true;
}

bool DeriveBlobUploadIntegrityKey(
    const BlobUploadTokenClaims& claims,
    const std::string& owner,
    const std::string& file_id,
    std::array<std::uint8_t, 32>& out_key) {
  static constexpr std::array<std::uint8_t, 18> kInfo = {
      'M', 'I', '_', 'B', 'L', 'O', 'B', '_', 'I',
      'N', 'T', 'E', 'G', 'R', 'I', 'T', 'Y', '1'};
  std::vector<std::uint8_t> info;
  info.reserve(kInfo.size() + claims.nonce.size() + owner.size() +
               file_id.size());
  info.insert(info.end(), kInfo.begin(), kInfo.end());
  info.insert(info.end(), claims.nonce.begin(), claims.nonce.end());
  info.insert(info.end(), owner.begin(), owner.end());
  info.insert(info.end(), file_id.begin(), file_id.end());
  std::array<std::uint8_t, 32> key{};
  if (!GetBlobTokenMasterKey(key)) {
    out_key.fill(0);
    return false;
  }
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::HmacSha256(key.data(), key.size(), info.data(),
                                 info.size(), d);
  out_key = d.bytes;
  return true;
}

bool CheckBlobTempStorageBudget(const std::filesystem::path& base_dir,
                                std::uint64_t max_upload_temp_bytes,
                                std::uint64_t additional_bytes,
                                std::string& error) {
  error.clear();
  std::error_code ec;
  std::filesystem::directory_iterator it(base_dir, ec);
  if (ec) {
    error = "offline storage unavailable";
    return false;
  }
  std::uint64_t total = 0;
  const std::filesystem::directory_iterator end;
  for (; it != end; it.increment(ec)) {
    if (ec) {
      error = "offline storage unavailable";
      return false;
    }
    if (!it->is_regular_file(ec) || ec) {
      ec.clear();
      continue;
    }
    if (it->path().extension() != ".part") {
      continue;
    }
    const auto s = std::filesystem::file_size(it->path(), ec);
    if (ec) {
      ec.clear();
      continue;
    }
    if (total > (std::numeric_limits<std::uint64_t>::max)() - s) {
      error = "upload temp storage exhausted";
      return false;
    }
    total += s;
  }
  if (total > max_upload_temp_bytes ||
      additional_bytes > max_upload_temp_bytes - total) {
    error = "upload temp storage exhausted";
    return false;
  }
  return true;
}

std::filesystem::path UploadLockPath(const std::filesystem::path& temp_path) {
  return temp_path.string() + ".lock";
}

std::filesystem::path UploadTagPath(const std::filesystem::path& temp_path) {
  return temp_path.string() + ".itag";
}

void RemoveUploadSidecars(const std::filesystem::path& temp_path) {
  std::error_code ec;
  std::filesystem::remove(UploadTagPath(temp_path), ec);
  ec.clear();
  std::filesystem::remove(UploadLockPath(temp_path), ec);
}

bool AcquireUploadLock(const std::filesystem::path& temp_path,
                       pfs::FileLock& lock,
                       std::string& error) {
  error.clear();
  const auto status =
      pfs::AcquireExclusiveFileLock(UploadLockPath(temp_path), lock);
  if (status == pfs::FileLockStatus::kOk) {
    return true;
  }
  if (status == pfs::FileLockStatus::kBusy) {
    error = "upload busy";
  } else {
    error = "upload lock failed";
  }
  return false;
}

void UpdateBlobUploadIntegrityTag(
    const std::array<std::uint8_t, 32>& key,
    const std::uint8_t* chunk,
    std::size_t chunk_len,
    std::array<std::uint8_t, 32>& inout_tag) {
  std::vector<std::uint8_t> mac_input;
  mac_input.reserve(inout_tag.size() + chunk_len);
  mac_input.insert(mac_input.end(), inout_tag.begin(), inout_tag.end());
  if (chunk_len != 0 && chunk != nullptr) {
    mac_input.insert(mac_input.end(), chunk, chunk + chunk_len);
  }
  mi::server::crypto::Sha256Digest digest;
  mi::server::crypto::HmacSha256(key.data(), key.size(), mac_input.data(),
                                 mac_input.size(), digest);
  inout_tag = digest.bytes;
}

bool PersistBlobUploadIntegrityTag(
    const std::filesystem::path& temp_path,
    std::uint64_t bytes_received,
    const std::array<std::uint8_t, 32>& tag,
    std::string& error) {
  std::array<std::uint8_t, kBlobUploadTagBytes> out{};
  std::size_t off = 0;
  std::memcpy(out.data() + off, kBlobUploadTagMagic.data(),
              kBlobUploadTagMagic.size());
  off += kBlobUploadTagMagic.size();
  out[off++] = kBlobUploadTagVersion;
  out[off++] = 0;
  out[off++] = 0;
  out[off++] = 0;
  WriteUint64Le(bytes_received, out.data() + off);
  off += 8;
  std::memcpy(out.data() + off, tag.data(), tag.size());
  return AtomicWriteOwnerOnly(UploadTagPath(temp_path), out.data(), out.size(),
                              error);
}

bool LoadBlobUploadIntegrityTag(
    const std::filesystem::path& temp_path,
    std::uint64_t& bytes_received,
    std::array<std::uint8_t, 32>& tag,
    std::string& error) {
  error.clear();
  std::ifstream ifs(UploadTagPath(temp_path), std::ios::binary);
  if (!ifs) {
    error = "upload integrity tag missing";
    return false;
  }
  std::array<std::uint8_t, kBlobUploadTagBytes> in{};
  ifs.read(reinterpret_cast<char*>(in.data()),
           static_cast<std::streamsize>(in.size()));
  if (!ifs || ifs.gcount() != static_cast<std::streamsize>(in.size())) {
    error = "upload integrity tag invalid";
    return false;
  }
  std::uint8_t extra = 0;
  if (ifs.read(reinterpret_cast<char*>(&extra), 1)) {
    error = "upload integrity tag invalid";
    return false;
  }
  if (!std::equal(kBlobUploadTagMagic.begin(), kBlobUploadTagMagic.end(),
                  in.begin())) {
    error = "upload integrity tag invalid";
    return false;
  }
  std::size_t off = kBlobUploadTagMagic.size();
  if (in[off++] != kBlobUploadTagVersion) {
    error = "upload integrity tag invalid";
    return false;
  }
  off += 3;
  bytes_received = ReadUint64Le(in.data() + off);
  off += 8;
  std::memcpy(tag.data(), in.data() + off, tag.size());
  return true;
}

bool ComputeBlobUploadIntegrityTagFromFile(
    const std::filesystem::path& temp_path,
    const std::array<std::uint8_t, 32>& key,
    std::array<std::uint8_t, 32>& tag,
    std::uint64_t& bytes,
    std::string& error) {
  error.clear();
  tag.fill(0);
  bytes = 0;
  std::ifstream ifs(temp_path, std::ios::binary);
  if (!ifs) {
    error = "open file failed";
    return false;
  }
  std::vector<std::uint8_t> buffer;
  buffer.resize(kOfflineFileStreamChunkBytes);
  while (true) {
    ifs.read(reinterpret_cast<char*>(buffer.data()),
             static_cast<std::streamsize>(buffer.size()));
    const auto got = ifs.gcount();
    if (got > 0) {
      const auto got_u64 = static_cast<std::uint64_t>(got);
      if (bytes > (std::numeric_limits<std::uint64_t>::max)() - got_u64) {
        error = "upload integrity overflow";
        return false;
      }
      UpdateBlobUploadIntegrityTag(key, buffer.data(),
                                   static_cast<std::size_t>(got), tag);
      bytes += got_u64;
    }
    if (ifs.eof()) {
      break;
    }
    if (!ifs) {
      error = "read file failed";
      return false;
    }
  }
  return true;
}

std::array<std::uint8_t, kOfflineFileAeadNonceBytes> DeriveChunkNonce(
    const std::array<std::uint8_t, kOfflineFileAeadNonceBytes>& base_nonce,
    std::uint64_t index) {
  auto nonce = base_nonce;
  WriteUint64Le(index, nonce.data() + (kOfflineFileAeadNonceBytes - 8));
  return nonce;
}

bool IsValidFileId(const std::string& file_id) {
  if (file_id.size() != 32) {
    return false;
  }
  for (unsigned char c : file_id) {
    if (!std::isxdigit(c)) {
      return false;
    }
  }
  return true;
}

void DeriveBlock(const std::array<std::uint8_t, 32>& key,
                 const std::array<std::uint8_t, kOfflineFileLegacyNonceBytes>& nonce,
                 std::uint64_t counter,
                 std::array<std::uint8_t, 32>& out) {
  std::array<std::uint8_t, 24> buf{};
  std::memcpy(buf.data(), nonce.data(), nonce.size());
  for (int i = 0; i < 8; ++i) {
    buf[16 + i] = static_cast<std::uint8_t>((counter >> (56 - 8 * i)) & 0xFF);
  }
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::HmacSha256(key.data(), key.size(), buf.data(),
                                 buf.size(), d);
  std::memcpy(out.data(), d.bytes.data(), out.size());
}

}  // namespace

BlobDownloadChunkResult::~BlobDownloadChunkResult() {
  if (!chunk.empty()) {
    OfflineStorageBufferPool().Release(std::move(chunk));
  }
}

OfflineStorage::OfflineStorage(std::filesystem::path base_dir,
                               std::chrono::seconds ttl,
                               SecureDeleteConfig secure_delete,
                               KeyProtectionMode state_protection,
                               StateStore* state_store,
                               std::uint64_t blob_upload_temp_budget_bytes)
    : base_dir_(std::move(base_dir)),
      ttl_(ttl),
      secure_delete_(std::move(secure_delete)),
      state_protection_(state_protection),
      state_store_(state_store),
      blob_upload_temp_budget_bytes_(blob_upload_temp_budget_bytes) {
  if (blob_upload_temp_budget_bytes_ == 0) {
    blob_upload_temp_budget_bytes_ = 4ull * 1024ull * 1024ull * 1024ull;
  }
  std::error_code ec;
  std::filesystem::create_directories(base_dir_, ec);
  if (state_store_) {
    (void)LoadMetadataFromStore();
  } else {
    LoadMetadataFromDisk();
  }
  if (secure_delete_.enabled) {
    std::string err;
    if (!LoadSecureDeletePlugin(secure_delete_.plugin_path, err)) {
      secure_delete_error_ = err.empty() ? "secure delete plugin load failed"
                                         : err;
      secure_delete_ready_ = false;
    } else {
      secure_delete_ready_ = true;
    }
  }
}

OfflineStorage::~OfflineStorage() {
  if (secure_delete_handle_) {
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(secure_delete_handle_));
#else
    dlclose(secure_delete_handle_);
#endif
    secure_delete_handle_ = nullptr;
  }
  secure_delete_fn_ = nullptr;
}

bool OfflineStorage::PersistMetadata(const StoredFileMeta& meta,
                                     std::string& error) const {
  error.clear();
  if (meta.id.empty()) {
    error = "file id empty";
    return false;
  }
  std::vector<std::uint8_t> bytes;
  const auto now_sys = std::chrono::system_clock::now();
  const auto now_steady = std::chrono::steady_clock::now();
  if (!EncodeOfflineMeta(meta, now_sys, now_steady, bytes)) {
    error = "metadata encode failed";
    return false;
  }
  std::vector<std::uint8_t> protected_bytes;
  std::string protect_err;
  if (!EncodeProtectedFileBytes(bytes, state_protection_, protected_bytes,
                                protect_err)) {
    error = protect_err.empty() ? "metadata protect failed" : protect_err;
    return false;
  }
  const auto path = ResolveMetaPath(meta.id);
  if (path.empty()) {
    error = "metadata path invalid";
    return false;
  }
  if (!AtomicWriteOwnerOnly(path, protected_bytes.data(), protected_bytes.size(),
                            error)) {
    if (error.empty()) {
      error = "metadata write failed";
    }
    return false;
  }
  return true;
}

void OfflineStorage::LoadMetadataFromDisk() {
  if (base_dir_.empty()) {
    return;
  }
  std::error_code ec;
  if (!std::filesystem::exists(base_dir_, ec) || ec) {
    return;
  }

  const auto now_sys = std::chrono::system_clock::now();
  const auto now_steady = std::chrono::steady_clock::now();
  std::unordered_map<std::string, StoredFileMeta> loaded;
  std::unordered_set<std::string> meta_ids;
  std::unordered_set<std::string> data_ids;

  const auto drop_file = [](const std::filesystem::path& path) {
    std::error_code rm_ec;
    std::filesystem::remove(path, rm_ec);
  };

  std::filesystem::directory_iterator it(base_dir_, ec);
  const std::filesystem::directory_iterator end;
  for (; it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (!it->is_regular_file(ec) || ec) {
      continue;
    }
    const auto path = it->path();
    const auto ext = path.extension().string();
    const std::string stem = path.stem().string();
    if (stem.empty() || !IsValidFileId(stem)) {
      continue;
    }
    if (ext == ".meta") {
      const auto data_path = ResolvePath(stem);
      if (!std::filesystem::exists(data_path, ec) || ec) {
        drop_file(path);
        ec.clear();
        continue;
      }
      std::ifstream ifs(path, std::ios::binary);
      if (!ifs) {
        continue;
      }
      ifs.seekg(0, std::ios::end);
      const std::streamsize size = ifs.tellg();
      ifs.seekg(0, std::ios::beg);
      if (size <= 0) {
        drop_file(path);
        continue;
      }
      std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
      if (!ifs.read(reinterpret_cast<char*>(bytes.data()), size)) {
        drop_file(path);
        continue;
      }
      std::vector<std::uint8_t> plain;
      bool was_protected = false;
      std::string protect_err;
      if (!DecodeProtectedFileBytes(bytes, state_protection_, plain,
                                    was_protected, protect_err)) {
        drop_file(path);
        continue;
      }
      StoredFileMeta meta;
      if (!DecodeOfflineMeta(plain, now_sys, now_steady, meta)) {
        drop_file(path);
        continue;
      }
      meta.id = stem;
      loaded.emplace(stem, std::move(meta));
      meta_ids.insert(stem);
      if (!was_protected && state_protection_ != KeyProtectionMode::kNone) {
        std::string write_err;
        (void)PersistMetadata(meta, write_err);
      }
      continue;
    }
    if (ext == ".bin") {
      data_ids.insert(stem);
    }
  }

  for (const auto& id : data_ids) {
    if (meta_ids.find(id) != meta_ids.end()) {
      continue;
    }
    const auto path = ResolvePath(id);
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
      ec.clear();
      continue;
    }
    if (size > (std::numeric_limits<std::uint64_t>::max)()) {
      continue;
    }
    const std::uint64_t file_size = static_cast<std::uint64_t>(size);
    std::uint64_t plain_size = file_size;
    if (const auto recovered = RecoverOfflinePlainSize(path, file_size);
        recovered.has_value()) {
      plain_size = *recovered;
    }
    std::chrono::system_clock::time_point created_sys = now_sys;
    const auto ft = std::filesystem::last_write_time(path, ec);
    if (!ec) {
      created_sys = FileTimeToSystem(ft);
    } else {
      ec.clear();
    }
    StoredFileMeta meta;
    meta.id = id;
    meta.owner.clear();
    meta.size = plain_size;
    meta.created_at = SteadyFromSystem(created_sys, now_sys, now_steady);
    loaded.emplace(id, meta);
    std::string write_err;
    (void)PersistMetadata(meta, write_err);
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    metadata_.swap(loaded);
  }
}

bool OfflineStorage::LoadMetadataFromStore() {
  if (!state_store_) {
    return true;
  }
  BlobLoadResult blob;
  std::string load_err;
  if (!state_store_->LoadBlob("offline_storage_meta", blob, load_err)) {
    return false;
  }
  if (!blob.found || blob.data.empty()) {
    std::error_code ec;
    if (!base_dir_.empty() && std::filesystem::exists(base_dir_, ec) && !ec) {
      LoadMetadataFromDisk();
      return SaveMetadataToStore();
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      metadata_.clear();
    }
    return true;
  }

  const auto now_sys = std::chrono::system_clock::now();
  const auto now_steady = std::chrono::steady_clock::now();
  std::unordered_map<std::string, StoredFileMeta> loaded;
  if (!DecodeOfflineMetaMap(blob.data, now_sys, now_steady, loaded)) {
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    metadata_.swap(loaded);
  }
  return true;
}

bool OfflineStorage::LoadMetadataFromStoreLocked() {
  if (!state_store_) {
    return true;
  }
  BlobLoadResult blob;
  std::string load_err;
  if (!state_store_->LoadBlob("offline_storage_meta", blob, load_err)) {
    return false;
  }
  if (!blob.found || blob.data.empty()) {
    metadata_.clear();
    return true;
  }
  const auto now_sys = std::chrono::system_clock::now();
  const auto now_steady = std::chrono::steady_clock::now();
  std::unordered_map<std::string, StoredFileMeta> loaded;
  if (!DecodeOfflineMetaMap(blob.data, now_sys, now_steady, loaded)) {
    return false;
  }
  metadata_.swap(loaded);
  return true;
}

bool OfflineStorage::SaveMetadataToStore() {
  if (!state_store_) {
    return true;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  return SaveMetadataToStoreLocked();
}

bool OfflineStorage::SaveMetadataToStoreLocked() {
  if (!state_store_) {
    return true;
  }
  std::string lock_err;
  StateStoreLock lock(state_store_, "offline_storage_meta",
                      std::chrono::milliseconds(5000), lock_err);
  if (!lock.locked()) {
    return false;
  }
  return SaveMetadataToStoreLockedUnlocked();
}

bool OfflineStorage::SaveMetadataToStoreLockedUnlocked() {
  if (!state_store_) {
    return true;
  }
  std::vector<std::uint8_t> out;
  const auto now_sys = std::chrono::system_clock::now();
  const auto now_steady = std::chrono::steady_clock::now();
  if (!EncodeOfflineMetaMap(metadata_, now_sys, now_steady, out)) {
    return false;
  }
  std::string store_err;
  if (!state_store_->SaveBlob("offline_storage_meta", out, store_err)) {
    return false;
  }
  return true;
}

PutResult OfflineStorage::Put(const std::string& owner,
                              const std::vector<std::uint8_t>& plaintext) {
  PutResult result;
  if (plaintext.empty()) {
    result.error = "empty payload";
    return result;
  }

  const std::string id = GenerateId();
  std::array<std::uint8_t, 32> file_key = GenerateKey();
  std::array<std::uint8_t, 32> erase_key = GenerateKey();
  std::array<std::uint8_t, 32> storage_key =
      DeriveStorageKey(file_key, erase_key);
  [[maybe_unused]] mi::common::ScopedWipe wipe_file_key(file_key);
  [[maybe_unused]] mi::common::ScopedWipe wipe_erase_key(erase_key);
  [[maybe_unused]] mi::common::ScopedWipe wipe_storage_key(storage_key);
  const auto base_nonce = RandomAeadNonce();
  const std::uint32_t chunk_bytes = kOfflineFileStreamChunkBytes;
  const std::uint64_t plain_size =
      static_cast<std::uint64_t>(plaintext.size());
  std::array<std::uint8_t, kOfflineFileV3PrefixBytes> ad_prefix{};
  std::memcpy(ad_prefix.data(), kOfflineFileMagic.data(),
              kOfflineFileMagic.size());
  ad_prefix[kOfflineFileMagic.size()] = kOfflineFileMagicVersionLatest;
  WriteUint32Le(chunk_bytes,
                ad_prefix.data() + kOfflineFileMagic.size() + 1);
  WriteUint64Le(plain_size,
                ad_prefix.data() + kOfflineFileMagic.size() + 1 + 4);

  const auto path = ResolvePath(id);
  const std::filesystem::path tmp_path = path.string() + ".tmp";
  std::ofstream ofs(tmp_path, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    result.error = "open file failed";
    crypto_wipe(storage_key.data(), storage_key.size());
    crypto_wipe(erase_key.data(), erase_key.size());
    crypto_wipe(file_key.data(), file_key.size());
    return result;
  }
  ofs.write(reinterpret_cast<const char*>(kOfflineFileMagic.data()),
            static_cast<std::streamsize>(kOfflineFileMagic.size()));
  const char ver = static_cast<char>(kOfflineFileMagicVersionLatest);
  ofs.write(&ver, 1);
  ofs.write(reinterpret_cast<const char*>(ad_prefix.data() +
                                          kOfflineFileMagic.size() + 1),
            static_cast<std::streamsize>(4 + 8));
  ofs.write(reinterpret_cast<const char*>(base_nonce.data()),
            static_cast<std::streamsize>(base_nonce.size()));

  std::array<std::uint8_t, kOfflineFileV3AdBytes> ad{};
  std::memcpy(ad.data(), ad_prefix.data(), ad_prefix.size());
  auto& pool = OfflineStorageBufferPool();
  mi::common::ScopedBuffer cipher_buf(pool, chunk_bytes, false);
  auto& cipher = cipher_buf.get();
  std::array<std::uint8_t, kOfflineFileAeadTagBytes> tag{};
  std::uint64_t offset = 0;
  std::uint64_t chunk_index = 0;
  while (offset < plain_size) {
    const std::size_t to_copy = static_cast<std::size_t>(
        std::min<std::uint64_t>(plain_size - offset, chunk_bytes));
    cipher.resize(to_copy);
    WriteUint64Le(chunk_index, ad.data() + kOfflineFileV3PrefixBytes);
    const auto nonce = DeriveChunkNonce(base_nonce, chunk_index);
    crypto_aead_lock(cipher.data(), tag.data(), storage_key.data(),
                     nonce.data(), ad.data(), ad.size(),
                     plaintext.data() + static_cast<std::size_t>(offset),
                     to_copy);
    ofs.write(reinterpret_cast<const char*>(cipher.data()),
              static_cast<std::streamsize>(cipher.size()));
    ofs.write(reinterpret_cast<const char*>(tag.data()),
              static_cast<std::streamsize>(tag.size()));
    if (!ofs) {
      result.error = "write file failed";
      crypto_wipe(storage_key.data(), storage_key.size());
      crypto_wipe(erase_key.data(), erase_key.size());
      crypto_wipe(file_key.data(), file_key.size());
      ofs.close();
      std::error_code rm_ec;
      std::filesystem::remove(tmp_path, rm_ec);
      return result;
    }
    offset += to_copy;
    ++chunk_index;
  }
  ofs.close();
  if (!ofs.good()) {
    result.error = "write file failed";
    crypto_wipe(storage_key.data(), storage_key.size());
    crypto_wipe(erase_key.data(), erase_key.size());
    crypto_wipe(file_key.data(), file_key.size());
    std::error_code rm_ec;
    std::filesystem::remove(tmp_path, rm_ec);
    return result;
  }
  std::string perm_err;
  if (!EnforceOwnerOnlyPermissions(tmp_path, perm_err)) {
    result.error = perm_err.empty() ? "file permissions insecure" : perm_err;
    crypto_wipe(storage_key.data(), storage_key.size());
    crypto_wipe(erase_key.data(), erase_key.size());
    crypto_wipe(file_key.data(), file_key.size());
    std::error_code rm_ec;
    std::filesystem::remove(tmp_path, rm_ec);
    return result;
  }
  {
    std::error_code ec;
    if (!pfs::Rename(tmp_path, path, ec) || ec) {
      result.error = "write file failed";
      crypto_wipe(storage_key.data(), storage_key.size());
      crypto_wipe(erase_key.data(), erase_key.size());
      crypto_wipe(file_key.data(), file_key.size());
      std::error_code rm_ec;
      std::filesystem::remove(tmp_path, rm_ec);
      return result;
    }
  }
  if (!EnforceOwnerOnlyPermissions(path, perm_err)) {
    result.error = perm_err.empty() ? "file permissions insecure" : perm_err;
    crypto_wipe(storage_key.data(), storage_key.size());
    crypto_wipe(erase_key.data(), erase_key.size());
    crypto_wipe(file_key.data(), file_key.size());
    std::error_code rm_ec;
    std::filesystem::remove(path, rm_ec);
    return result;
  }

  std::string key_err;
  if (!SaveEraseKey(path, erase_key, key_err)) {
    result.error = key_err.empty() ? "key store failed" : key_err;
    crypto_wipe(storage_key.data(), storage_key.size());
    crypto_wipe(erase_key.data(), erase_key.size());
    crypto_wipe(file_key.data(), file_key.size());
    WipeFile(path);
    return result;
  }
  crypto_wipe(storage_key.data(), storage_key.size());
  crypto_wipe(erase_key.data(), erase_key.size());

  StoredFileMeta meta;
  meta.id = id;
  meta.owner = owner;
  meta.size = static_cast<std::uint64_t>(plaintext.size());
  meta.created_at = std::chrono::steady_clock::now();

  std::string meta_err;
  if (state_store_) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string lock_err;
    StateStoreLock store_lock(state_store_, "offline_storage_meta",
                              std::chrono::milliseconds(5000), lock_err);
    if (!store_lock.locked()) {
      meta_err = "metadata store lock failed";
    } else if (!LoadMetadataFromStoreLocked()) {
      meta_err = "metadata store load failed";
    } else {
      metadata_[id] = meta;
      if (!SaveMetadataToStoreLockedUnlocked()) {
        metadata_.erase(id);
        meta_err = "metadata store save failed";
      }
    }
  } else {
    if (!PersistMetadata(meta, meta_err)) {
      result.error = meta_err.empty() ? "metadata write failed" : meta_err;
      crypto_wipe(file_key.data(), file_key.size());
      WipeFile(path);
      return result;
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      metadata_[id] = meta;
    }
  }
  if (!meta_err.empty()) {
    result.error = meta_err;
    crypto_wipe(file_key.data(), file_key.size());
    WipeFile(path);
    return result;
  }

  result.success = true;
  result.file_id = id;
  result.file_key = file_key;
  result.meta = meta;
  crypto_wipe(file_key.data(), file_key.size());
  return result;
}

PutBlobResult OfflineStorage::PutBlob(const std::string& owner,
                                      const std::vector<std::uint8_t>& blob) {
  PutBlobResult result;
  if (blob.empty()) {
    result.error = "empty payload";
    return result;
  }

  const std::string id = GenerateId();
  const auto path = ResolvePath(id);
  std::string write_err;
  if (!AtomicWriteOwnerOnly(path, blob.data(), blob.size(), write_err)) {
    result.error = write_err.empty() ? "write file failed" : write_err;
    return result;
  }

  StoredFileMeta meta;
  meta.id = id;
  meta.owner = owner;
  meta.size = static_cast<std::uint64_t>(blob.size());
  meta.created_at = std::chrono::steady_clock::now();

  std::string meta_err;
  if (state_store_) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string lock_err;
    StateStoreLock store_lock(state_store_, "offline_storage_meta",
                              std::chrono::milliseconds(5000), lock_err);
    if (!store_lock.locked()) {
      meta_err = "metadata store lock failed";
    } else if (!LoadMetadataFromStoreLocked()) {
      meta_err = "metadata store load failed";
    } else {
      metadata_[id] = meta;
      if (!SaveMetadataToStoreLockedUnlocked()) {
        metadata_.erase(id);
        meta_err = "metadata store save failed";
      }
    }
  } else {
    if (!PersistMetadata(meta, meta_err)) {
      result.error = meta_err.empty() ? "metadata write failed" : meta_err;
      WipeFile(path);
      return result;
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      metadata_[id] = meta;
    }
  }
  if (!meta_err.empty()) {
    result.error = meta_err;
    WipeFile(path);
    return result;
  }

  result.success = true;
  result.file_id = id;
  result.meta = meta;
  return result;
}

BlobUploadStartResult OfflineStorage::BeginBlobUpload(const std::string& owner,
                                                      std::uint64_t expected_size) {
  BlobUploadStartResult result;
  if (owner.empty()) {
    result.error = "owner empty";
    return result;
  }
  if (expected_size > 0 && expected_size > kMaxBlobBytes) {
    result.error = "payload too large";
    return result;
  }

  CleanupExpiredBlobArtifacts();

  std::string budget_err;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!CheckBlobTempStorageBudget(base_dir_, blob_upload_temp_budget_bytes_, 0,
                                    budget_err)) {
      result.error = budget_err.empty() ? "upload temp storage exhausted"
                                        : budget_err;
      return result;
    }
  }

  std::string file_id;
  bool unique_id = false;
  for (int i = 0; i < 8; ++i) {
    file_id = GenerateId();
    std::error_code ec;
    const auto path = ResolvePath(file_id);
    const auto temp_path = ResolveUploadTempPath(file_id);
    const auto tag_path = UploadTagPath(temp_path);
    const auto lock_path = UploadLockPath(temp_path);
    if (std::filesystem::exists(path, ec) && !ec) {
      continue;
    }
    ec.clear();
    if (std::filesystem::exists(temp_path, ec) && !ec) {
      continue;
    }
    ec.clear();
    if (std::filesystem::exists(tag_path, ec) && !ec) {
      continue;
    }
    ec.clear();
    if (std::filesystem::exists(lock_path, ec) && !ec) {
      continue;
    }
    unique_id = true;
    break;
  }
  if (!unique_id || file_id.empty()) {
    result.error = "id collision";
    return result;
  }

  std::string token_err;
  const std::string upload_id =
      EncodeBlobUploadToken(owner, file_id, expected_size, token_err);
  if (upload_id.empty()) {
    result.error = token_err.empty() ? "session issue failed" : token_err;
    return result;
  }

  result.success = true;
  result.file_id = file_id;
  result.upload_id = upload_id;
  return result;
}

BlobUploadChunkResult OfflineStorage::AppendBlobUploadChunk(
    const std::string& owner, const std::string& file_id,
    const std::string& upload_id, std::uint64_t offset,
    const std::vector<std::uint8_t>& chunk) {
  BlobUploadChunkResult result;
  if (owner.empty()) {
    result.error = "owner empty";
    return result;
  }
  if (file_id.empty() || upload_id.empty()) {
    result.error = "invalid session";
    return result;
  }
  if (chunk.empty()) {
    result.error = "empty payload";
    return result;
  }
  if (chunk.size() > kMaxBlobChunkBytes) {
    result.error = "chunk too large";
    return result;
  }

  BlobUploadTokenClaims claims;
  std::string token_err;
  if (!DecodeBlobUploadToken(upload_id, owner, file_id, claims, token_err)) {
    result.error = token_err.empty() ? "invalid session" : token_err;
    return result;
  }
  if (claims.expected_size != 0 && offset > claims.expected_size) {
    result.error = "invalid offset";
    return result;
  }
  if (offset > kMaxBlobBytes ||
      chunk.size() > static_cast<std::size_t>(kMaxBlobBytes - offset)) {
    result.error = "payload too large";
    return result;
  }
  if (claims.expected_size != 0 &&
      chunk.size() > static_cast<std::size_t>(claims.expected_size - offset)) {
    result.error = "payload too large";
    return result;
  }

  CleanupExpiredBlobArtifacts();

  std::string budget_err;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!CheckBlobTempStorageBudget(base_dir_, blob_upload_temp_budget_bytes_,
                                    static_cast<std::uint64_t>(chunk.size()),
                                    budget_err)) {
      result.error = budget_err.empty() ? "upload temp storage exhausted"
                                        : budget_err;
      return result;
    }
  }

  const auto temp_path = ResolveUploadTempPath(file_id);

  pfs::FileLock upload_lock{};
  std::string lock_err;
  if (!AcquireUploadLock(temp_path, upload_lock, lock_err)) {
    result.error = lock_err;
    return result;
  }

  std::error_code ec;
  if (!std::filesystem::exists(temp_path, ec) || ec) {
    if (offset != 0) {
      pfs::ReleaseFileLock(upload_lock);
      result.error = "invalid offset";
      return result;
    }
    std::string write_err;
    if (!AtomicWriteOwnerOnly(temp_path, nullptr, 0, write_err)) {
      pfs::ReleaseFileLock(upload_lock);
      result.error = write_err.empty() ? "open file failed" : write_err;
      return result;
    }
    std::array<std::uint8_t, 32> initial_tag{};
    if (!PersistBlobUploadIntegrityTag(temp_path, 0, initial_tag, write_err)) {
      pfs::ReleaseFileLock(upload_lock);
      WipeFile(temp_path);
      RemoveUploadSidecars(temp_path);
      result.error = write_err.empty() ? "write file failed" : write_err;
      return result;
    }
  }

  {
    std::string perm_err;
    if (!mi::shard::security::CheckPathNotWorldWritable(temp_path, perm_err)) {
      pfs::ReleaseFileLock(upload_lock);
      WipeFile(temp_path);
      RemoveUploadSidecars(temp_path);
      result.error =
          perm_err.empty() ? "upload file permissions insecure" : perm_err;
      return result;
    }
  }

  std::uint64_t received = 0;
  std::array<std::uint8_t, 32> integrity_tag{};
  std::string tag_err;
  if (!LoadBlobUploadIntegrityTag(temp_path, received, integrity_tag, tag_err)) {
    pfs::ReleaseFileLock(upload_lock);
    WipeFile(temp_path);
    RemoveUploadSidecars(temp_path);
    result.error = "upload integrity mismatch";
    return result;
  }
  if (received != offset) {
    pfs::ReleaseFileLock(upload_lock);
    result.error = "invalid offset";
    return result;
  }
  if (claims.expected_size != 0 &&
      chunk.size() >
          static_cast<std::size_t>(claims.expected_size - received)) {
    pfs::ReleaseFileLock(upload_lock);
    result.error = "payload too large";
    return result;
  }
  if (received > kMaxBlobBytes ||
      chunk.size() > static_cast<std::size_t>(kMaxBlobBytes - received)) {
    pfs::ReleaseFileLock(upload_lock);
    result.error = "payload too large";
    return result;
  }

  std::ofstream ofs(temp_path, std::ios::binary | std::ios::app);
  if (!ofs) {
    pfs::ReleaseFileLock(upload_lock);
    result.error = "open file failed";
    return result;
  }
  ofs.write(reinterpret_cast<const char*>(chunk.data()),
            static_cast<std::streamsize>(chunk.size()));
  ofs.close();
  if (!ofs.good()) {
    pfs::ReleaseFileLock(upload_lock);
    result.error = "write failed";
    return result;
  }

  std::array<std::uint8_t, 32> integrity_key{};
  if (!DeriveBlobUploadIntegrityKey(claims, owner, file_id, integrity_key)) {
    pfs::ReleaseFileLock(upload_lock);
    result.error = "session subsystem unavailable";
    return result;
  }
  std::uint64_t verified_bytes = 0;
  std::string verify_err;
  if (!ComputeBlobUploadIntegrityTagFromFile(temp_path, integrity_key,
                                             integrity_tag, verified_bytes,
                                             verify_err) ||
      verified_bytes != received + static_cast<std::uint64_t>(chunk.size())) {
    pfs::ReleaseFileLock(upload_lock);
    WipeFile(temp_path);
    RemoveUploadSidecars(temp_path);
    result.error = "upload integrity mismatch";
    return result;
  }
  std::string tag_write_err;
  if (!PersistBlobUploadIntegrityTag(
          temp_path, verified_bytes, integrity_tag,
          tag_write_err)) {
    pfs::ReleaseFileLock(upload_lock);
    WipeFile(temp_path);
    RemoveUploadSidecars(temp_path);
    result.error =
        tag_write_err.empty() ? "upload integrity persist failed" : tag_write_err;
    return result;
  }

  pfs::ReleaseFileLock(upload_lock);
  result.success = true;
  result.bytes_received = verified_bytes;
  return result;
}

BlobUploadFinishResult OfflineStorage::FinishBlobUpload(
    const std::string& owner, const std::string& file_id,
    const std::string& upload_id, std::uint64_t total_size) {
  BlobUploadFinishResult result;
  if (owner.empty()) {
    result.error = "owner empty";
    return result;
  }
  if (file_id.empty() || upload_id.empty()) {
    result.error = "invalid session";
    return result;
  }
  if (total_size == 0 || total_size > kMaxBlobBytes) {
    result.error = "payload too large";
    return result;
  }
  BlobUploadTokenClaims claims;
  std::string token_err;
  if (!DecodeBlobUploadToken(upload_id, owner, file_id, claims, token_err)) {
    result.error = token_err.empty() ? "invalid session" : token_err;
    return result;
  }
  if (claims.expected_size != 0 && total_size != claims.expected_size) {
    result.error = "size mismatch";
    return result;
  }
  if (state_store_) {
    if (!LoadMetadataFromStore()) {
      result.error = "metadata store load failed";
      return result;
    }
  }

  const auto temp_path = ResolveUploadTempPath(file_id);

  pfs::FileLock upload_lock{};
  std::string lock_err;
  if (!AcquireUploadLock(temp_path, upload_lock, lock_err)) {
    result.error = lock_err;
    return result;
  }

  const auto final_path = ResolvePath(file_id);
  std::error_code ec;
  std::string perm_err;
  if (!std::filesystem::exists(temp_path, ec) || ec) {
    pfs::ReleaseFileLock(upload_lock);
    result.error = "upload session not found";
    return result;
  }
  if (!mi::shard::security::CheckPathNotWorldWritable(temp_path,
                                                       perm_err)) {
    pfs::ReleaseFileLock(upload_lock);
    result.error =
        perm_err.empty() ? "upload file permissions insecure" : perm_err;
    return result;
  }

  std::uint64_t tagged_bytes = 0;
  std::array<std::uint8_t, 32> tagged_digest{};
  std::string tag_err;
  if (!LoadBlobUploadIntegrityTag(temp_path, tagged_bytes, tagged_digest,
                                  tag_err) ||
      tagged_bytes != total_size) {
    pfs::ReleaseFileLock(upload_lock);
    WipeFile(temp_path);
    RemoveUploadSidecars(temp_path);
    result.error = "upload integrity mismatch";
    return result;
  }

  std::array<std::uint8_t, 32> integrity_key{};
  if (!DeriveBlobUploadIntegrityKey(claims, owner, file_id, integrity_key)) {
    pfs::ReleaseFileLock(upload_lock);
    result.error = "session subsystem unavailable";
    return result;
  }
  std::array<std::uint8_t, 32> computed_tag{};
  std::uint64_t computed_bytes = 0;
  std::string verify_err;
  if (!ComputeBlobUploadIntegrityTagFromFile(temp_path, integrity_key,
                                             computed_tag, computed_bytes,
                                             verify_err) ||
      computed_bytes != total_size ||
      crypto_verify32(computed_tag.data(), tagged_digest.data()) != 0) {
    pfs::ReleaseFileLock(upload_lock);
    WipeFile(temp_path);
    RemoveUploadSidecars(temp_path);
    result.error = "upload integrity mismatch";
    return result;
  }

  if (!pfs::Rename(temp_path, final_path, ec) || ec) {
    pfs::ReleaseFileLock(upload_lock);
    result.error = "finalize failed";
    return result;
  }
  if (!EnforceOwnerOnlyPermissions(final_path, perm_err)) {
    pfs::ReleaseFileLock(upload_lock);
    result.error = perm_err.empty() ? "file permissions insecure" : perm_err;
    std::filesystem::remove(final_path, ec);
    RemoveUploadSidecars(temp_path);
    return result;
  }

  StoredFileMeta meta;
  meta.id = file_id;
  meta.owner = owner;
  meta.size = total_size;
  meta.created_at = std::chrono::steady_clock::now();

  std::string meta_err;
  if (state_store_) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string lock_err;
    StateStoreLock store_lock(state_store_, "offline_storage_meta",
                              std::chrono::milliseconds(5000), lock_err);
    if (!store_lock.locked()) {
      meta_err = "metadata store lock failed";
    } else if (!LoadMetadataFromStoreLocked()) {
      meta_err = "metadata store load failed";
    } else {
      metadata_[file_id] = meta;
      if (!SaveMetadataToStoreLockedUnlocked()) {
        metadata_.erase(file_id);
        meta_err = "metadata store save failed";
      }
    }
  } else {
    if (!PersistMetadata(meta, meta_err)) {
      pfs::ReleaseFileLock(upload_lock);
      result.error = meta_err.empty() ? "metadata write failed" : meta_err;
      WipeFile(final_path);
      RemoveUploadSidecars(temp_path);
      return result;
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      metadata_[file_id] = meta;
    }
  }
  if (!meta_err.empty()) {
    pfs::ReleaseFileLock(upload_lock);
    result.error = meta_err;
    WipeFile(final_path);
    RemoveUploadSidecars(temp_path);
    return result;
  }

  pfs::ReleaseFileLock(upload_lock);
  RemoveUploadSidecars(temp_path);
  result.success = true;
  result.meta = meta;
  return result;
}

BlobDownloadStartResult OfflineStorage::BeginBlobDownload(
    const std::string& owner, const std::string& file_id, bool wipe_after_read) {
  BlobDownloadStartResult result;
  if (owner.empty()) {
    result.error = "owner empty";
    return result;
  }
  if (!IsValidFileId(file_id)) {
    result.error = "invalid file id";
    return result;
  }
  if (file_id.empty()) {
    result.error = "file id empty";
    return result;
  }
  const auto path = ResolvePath(file_id);
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    result.error = "file not found";
    return result;
  }
  const std::uint64_t size = std::filesystem::file_size(path, ec);
  if (ec || size == 0) {
    result.error = "file not found";
    return result;
  }
  if (state_store_) {
    if (!LoadMetadataFromStore()) {
      result.error = "metadata store load failed";
      return result;
    }
  }

  StoredFileMeta meta;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = metadata_.find(file_id);
    if (it != metadata_.end()) {
      meta = it->second;
    } else {
      meta.id = file_id;
      meta.size = size;
      meta.owner.clear();
      meta.created_at = std::chrono::steady_clock::now();
    }
  }

  std::string token_err;
  const std::string download_id =
      EncodeBlobDownloadToken(owner, file_id, wipe_after_read, token_err);
  if (download_id.empty()) {
    result.error = token_err.empty() ? "session issue failed" : token_err;
    return result;
  }

  result.success = true;
  result.download_id = download_id;
  result.meta = meta;
  return result;
}

BlobDownloadChunkResult OfflineStorage::ReadBlobDownloadChunk(
    const std::string& owner, const std::string& file_id,
    const std::string& download_id, std::uint64_t offset,
    std::uint32_t max_len) {
  BlobDownloadChunkResult result;
  if (owner.empty()) {
    result.error = "owner empty";
    return result;
  }
  if (!IsValidFileId(file_id)) {
    result.error = "invalid file id";
    return result;
  }
  if (file_id.empty() || download_id.empty()) {
    result.error = "invalid session";
    return result;
  }
  if (max_len == 0 || max_len > kMaxBlobChunkBytes) {
    max_len = kMaxBlobChunkBytes;
  }

  BlobDownloadTokenClaims claims;
  std::string token_err;
  if (!DecodeBlobDownloadToken(download_id, owner, file_id, claims, token_err)) {
    result.error = token_err.empty() ? "invalid session" : token_err;
    return result;
  }

  const auto path = ResolvePath(file_id);
  std::error_code size_ec;
  const std::uint64_t total_size = std::filesystem::file_size(path, size_ec);
  if (size_ec || total_size == 0 || offset >= total_size) {
    result.error = "invalid offset";
    return result;
  }

  std::vector<std::uint8_t> buf;
  {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
      result.error = "file not found";
      return result;
    }
    ifs.seekg(static_cast<std::streamoff>(offset));
    const std::uint64_t remaining64 = total_size - offset;
    const std::size_t to_read =
        static_cast<std::size_t>(std::min<std::uint64_t>(remaining64, max_len));
    auto& pool = OfflineStorageBufferPool();
    buf = pool.Acquire(to_read);
    buf.resize(to_read);
    ifs.read(reinterpret_cast<char*>(buf.data()),
             static_cast<std::streamsize>(buf.size()));
    if (!ifs) {
      result.error = "read failed";
      return result;
    }
  }

  const std::uint64_t next_off = offset + static_cast<std::uint64_t>(buf.size());
  const bool eof = (next_off >= total_size);

  if (claims.wipe_after_read && eof) {
    if (state_store_) {
      std::lock_guard<std::mutex> lock(mutex_);
      std::string lock_err;
      StateStoreLock store_lock(state_store_, "offline_storage_meta",
                                std::chrono::milliseconds(5000), lock_err);
      if (store_lock.locked() && LoadMetadataFromStoreLocked()) {
        metadata_.erase(file_id);
        (void)SaveMetadataToStoreLockedUnlocked();
      }
    } else {
      std::lock_guard<std::mutex> lock(mutex_);
      metadata_.erase(file_id);
    }
    WipeFile(path);
  }

  result.success = true;
  result.offset = offset;
  result.eof = eof;
  result.chunk = std::move(buf);
  return result;
}

std::optional<std::vector<std::uint8_t>> OfflineStorage::Fetch(
    const std::string& file_id, const std::array<std::uint8_t, 32>& file_key,
    bool wipe_after_read, std::string& error) {
  if (!IsValidFileId(file_id)) {
    error = "invalid file id";
    return std::nullopt;
  }
  if (state_store_) {
    if (!LoadMetadataFromStore()) {
      error = "metadata store load failed";
      return std::nullopt;
    }
  }
  const auto path = ResolvePath(file_id);
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    error = "file not found";
    return std::nullopt;
  }

  std::vector<std::uint8_t> plaintext;
  std::array<std::uint8_t, kOfflineFileMagic.size()> magic{};
  if (!ReadExact(ifs, magic.data(), magic.size())) {
    error = "file truncated";
    return std::nullopt;
  }

  if (std::equal(kOfflineFileMagic.begin(), kOfflineFileMagic.end(),
                 magic.begin())) {
    std::uint8_t version = 0;
    if (!ReadExact(ifs, &version, 1)) {
      error = "file truncated";
      return std::nullopt;
    }

    if (version == kOfflineFileMagicVersionV3) {
      std::array<std::uint8_t, 4> chunk_buf{};
      std::array<std::uint8_t, 8> size_buf{};
      std::array<std::uint8_t, kOfflineFileAeadNonceBytes> base_nonce{};
      if (!ReadExact(ifs, chunk_buf.data(), chunk_buf.size()) ||
          !ReadExact(ifs, size_buf.data(), size_buf.size()) ||
          !ReadExact(ifs, base_nonce.data(), base_nonce.size())) {
        error = "file truncated";
        return std::nullopt;
      }

      const std::uint32_t chunk_bytes = ReadUint32Le(chunk_buf.data());
      const std::uint64_t plain_size = ReadUint64Le(size_buf.data());
      if (chunk_bytes == 0 || chunk_bytes > kOfflineFileStreamMaxChunkBytes) {
        error = "chunk size invalid";
        return std::nullopt;
      }
      if (plain_size == 0) {
        error = "plain size invalid";
        return std::nullopt;
      }

      std::error_code ec;
      const std::uint64_t file_size =
          std::filesystem::file_size(path, ec);
      if (ec) {
        error = "file size failed";
        return std::nullopt;
      }
      const std::uint64_t chunk_count =
          (plain_size + chunk_bytes - 1) / chunk_bytes;
      if (chunk_count == 0 ||
          chunk_count >
              (std::numeric_limits<std::uint64_t>::max)() /
                  static_cast<std::uint64_t>(kOfflineFileAeadTagBytes)) {
        error = "file size invalid";
        return std::nullopt;
      }
      const std::uint64_t tag_overhead =
          chunk_count * static_cast<std::uint64_t>(kOfflineFileAeadTagBytes);
      if (tag_overhead >
          (std::numeric_limits<std::uint64_t>::max)() -
              kOfflineFileV3HeaderBytes - plain_size) {
        error = "file size invalid";
        return std::nullopt;
      }
      const std::uint64_t expected_size =
          kOfflineFileV3HeaderBytes + plain_size + tag_overhead;
      if (file_size != expected_size) {
        error = "file truncated";
        return std::nullopt;
      }

      if (plain_size > static_cast<std::uint64_t>(
                           (std::numeric_limits<std::size_t>::max)())) {
        error = "plain size invalid";
        return std::nullopt;
      }

      std::array<std::uint8_t, 32> file_key_copy = file_key;
      std::array<std::uint8_t, 32> storage_key = file_key_copy;
      std::array<std::uint8_t, 32> erase_key{};
      [[maybe_unused]] mi::common::ScopedWipe wipe_file_key(file_key_copy);
      [[maybe_unused]] mi::common::ScopedWipe wipe_erase_key(erase_key);
      [[maybe_unused]] mi::common::ScopedWipe wipe_storage_key(storage_key);
      std::string key_err;
      if (!LoadEraseKey(path, erase_key, key_err)) {
        error = key_err.empty() ? "erase key missing" : key_err;
        crypto_wipe(file_key_copy.data(), file_key_copy.size());
        crypto_wipe(erase_key.data(), erase_key.size());
        return std::nullopt;
      }
      storage_key = DeriveStorageKey(file_key_copy, erase_key);

      plaintext.resize(static_cast<std::size_t>(plain_size));
      std::array<std::uint8_t, kOfflineFileV3PrefixBytes> ad_prefix{};
      std::memcpy(ad_prefix.data(), kOfflineFileMagic.data(),
                  kOfflineFileMagic.size());
      ad_prefix[kOfflineFileMagic.size()] = version;
      WriteUint32Le(chunk_bytes,
                    ad_prefix.data() + kOfflineFileMagic.size() + 1);
      WriteUint64Le(plain_size,
                    ad_prefix.data() + kOfflineFileMagic.size() + 1 + 4);
      std::array<std::uint8_t, kOfflineFileV3AdBytes> ad{};
      std::memcpy(ad.data(), ad_prefix.data(), ad_prefix.size());
      auto& pool = OfflineStorageBufferPool();
      mi::common::ScopedBuffer cipher_buf(pool, chunk_bytes, false);
      auto& cipher = cipher_buf.get();
      std::array<std::uint8_t, kOfflineFileAeadTagBytes> tag{};
      std::uint64_t offset = 0;
      std::uint64_t chunk_index = 0;
      while (offset < plain_size) {
        const std::size_t to_read = static_cast<std::size_t>(
            std::min<std::uint64_t>(plain_size - offset, chunk_bytes));
        cipher.resize(to_read);
        if (!ReadExact(ifs, cipher.data(), cipher.size()) ||
            !ReadExact(ifs, tag.data(), tag.size())) {
          crypto_wipe(storage_key.data(), storage_key.size());
          crypto_wipe(file_key_copy.data(), file_key_copy.size());
          crypto_wipe(erase_key.data(), erase_key.size());
          error = "file truncated";
          return std::nullopt;
        }
        WriteUint64Le(chunk_index, ad.data() + kOfflineFileV3PrefixBytes);
        const auto nonce = DeriveChunkNonce(base_nonce, chunk_index);
        const int ok = crypto_aead_unlock(
            plaintext.data() + static_cast<std::size_t>(offset), tag.data(),
            storage_key.data(), nonce.data(), ad.data(), ad.size(),
            cipher.data(), cipher.size());
        if (ok != 0) {
          crypto_wipe(storage_key.data(), storage_key.size());
          crypto_wipe(file_key_copy.data(), file_key_copy.size());
          crypto_wipe(erase_key.data(), erase_key.size());
          error = "auth failed";
          return std::nullopt;
        }
        offset += to_read;
        ++chunk_index;
      }
      crypto_wipe(storage_key.data(), storage_key.size());
      crypto_wipe(file_key_copy.data(), file_key_copy.size());
      crypto_wipe(erase_key.data(), erase_key.size());
    } else if (version == kOfflineFileMagicVersionV1 ||
               version == kOfflineFileMagicVersionV2) {
      ifs.clear();
      ifs.seekg(0, std::ios::beg);
      std::error_code ec2;
      const std::uint64_t file_size2 =
          std::filesystem::file_size(path, ec2);
      if (ec2 || file_size2 == 0 ||
          file_size2 > static_cast<std::uint64_t>(
                            (std::numeric_limits<std::size_t>::max)())) {
        error = "file truncated";
        return std::nullopt;
      }
      auto& pool = OfflineStorageBufferPool();
      mi::common::ScopedBuffer content_buf(
          pool, static_cast<std::size_t>(file_size2), false);
      auto& content = content_buf.get();
      content.resize(static_cast<std::size_t>(file_size2));
      if (!ReadExact(ifs, content.data(), content.size())) {
        error = "file truncated";
        return std::nullopt;
      }
      ifs.close();
      if (content.size() <
          (kOfflineFileHeaderBytes + kOfflineFileAeadNonceBytes +
           kOfflineFileAeadTagBytes)) {
        error = "file truncated";
        return std::nullopt;
      }

      const auto nonce = [&]() {
        std::array<std::uint8_t, kOfflineFileAeadNonceBytes> n{};
        std::memcpy(n.data(), content.data() + kOfflineFileHeaderBytes,
                    n.size());
        return n;
      }();

      const std::size_t cipher_len =
          content.size() - kOfflineFileHeaderBytes - nonce.size() -
          kOfflineFileAeadTagBytes;
      if (cipher_len == 0) {
        error = "cipher empty";
        return std::nullopt;
      }

      const auto tag = [&]() {
        std::array<std::uint8_t, kOfflineFileAeadTagBytes> t{};
        std::memcpy(t.data(),
                    content.data() + kOfflineFileHeaderBytes + nonce.size() +
                        cipher_len,
                    t.size());
        return t;
      }();

      mi::common::ScopedBuffer cipher_buf(pool, cipher_len, false);
      auto& cipher = cipher_buf.get();
      cipher.resize(cipher_len);
      std::memcpy(cipher.data(),
                  content.data() + kOfflineFileHeaderBytes + nonce.size(),
                  cipher_len);

      std::array<std::uint8_t, kOfflineFileHeaderBytes> ad{};
      std::memcpy(ad.data(), kOfflineFileMagic.data(),
                  kOfflineFileMagic.size());
      ad[kOfflineFileMagic.size()] = version;

      std::array<std::uint8_t, 32> file_key_copy = file_key;
      std::array<std::uint8_t, 32> storage_key = file_key_copy;
      std::array<std::uint8_t, 32> erase_key{};
      [[maybe_unused]] mi::common::ScopedWipe wipe_file_key(file_key_copy);
      [[maybe_unused]] mi::common::ScopedWipe wipe_erase_key(erase_key);
      [[maybe_unused]] mi::common::ScopedWipe wipe_storage_key(storage_key);
      if (version == kOfflineFileMagicVersionV2) {
        std::string key_err;
        if (!LoadEraseKey(path, erase_key, key_err)) {
          error = key_err.empty() ? "erase key missing" : key_err;
          crypto_wipe(file_key_copy.data(), file_key_copy.size());
          crypto_wipe(erase_key.data(), erase_key.size());
          return std::nullopt;
        }
        storage_key = DeriveStorageKey(file_key_copy, erase_key);
      }

      if (!DecryptAead(cipher, storage_key, nonce, ad.data(), ad.size(), tag,
                       plaintext)) {
        crypto_wipe(storage_key.data(), storage_key.size());
        crypto_wipe(file_key_copy.data(), file_key_copy.size());
        crypto_wipe(erase_key.data(), erase_key.size());
        error = "auth failed";
        return std::nullopt;
      }
      crypto_wipe(storage_key.data(), storage_key.size());
      crypto_wipe(file_key_copy.data(), file_key_copy.size());
      crypto_wipe(erase_key.data(), erase_key.size());
    } else {
      error = "unsupported format";
      return std::nullopt;
    }
  } else {
    ifs.clear();
    ifs.seekg(0, std::ios::beg);
    std::error_code ec2;
    const std::uint64_t file_size2 =
        std::filesystem::file_size(path, ec2);
    if (ec2 || file_size2 == 0 ||
        file_size2 > static_cast<std::uint64_t>(
                          (std::numeric_limits<std::size_t>::max)())) {
      error = "file truncated";
      return std::nullopt;
    }
    auto& pool = OfflineStorageBufferPool();
    mi::common::ScopedBuffer content_buf(
        pool, static_cast<std::size_t>(file_size2), false);
    auto& content = content_buf.get();
    content.resize(static_cast<std::size_t>(file_size2));
    if (!ReadExact(ifs, content.data(), content.size())) {
      error = "file truncated";
      return std::nullopt;
    }
    ifs.close();
    if (content.size() <
        (kOfflineFileLegacyNonceBytes + kOfflineFileLegacyTagBytes)) {
      error = "file truncated";
      return std::nullopt;
    }

    const auto nonce = [&]() {
      std::array<std::uint8_t, kOfflineFileLegacyNonceBytes> n{};
      std::memcpy(n.data(), content.data(), n.size());
      return n;
    }();

    const std::size_t cipher_len =
        content.size() - nonce.size() - kOfflineFileLegacyTagBytes;
    if (cipher_len == 0) {
      error = "cipher empty";
      return std::nullopt;
    }

    const auto tag = [&]() {
      std::array<std::uint8_t, kOfflineFileLegacyTagBytes> t{};
      std::memcpy(t.data(), content.data() + nonce.size() + cipher_len,
                  t.size());
      return t;
    }();

    mi::common::ScopedBuffer cipher_buf(pool, cipher_len, false);
    auto& cipher = cipher_buf.get();
    cipher.resize(cipher_len);
    std::memcpy(cipher.data(), content.data() + nonce.size(), cipher_len);

    if (!DecryptLegacy(cipher, file_key, nonce, tag, plaintext)) {
      error = "auth failed";
      return std::nullopt;
    }
  }

  ifs.close();
  if (wipe_after_read) {
    WipeFile(path);
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_store_) {
      std::string lock_err;
      StateStoreLock store_lock(state_store_, "offline_storage_meta",
                                std::chrono::milliseconds(5000), lock_err);
      if (store_lock.locked()) {
        if (LoadMetadataFromStoreLocked()) {
          metadata_.erase(file_id);
          (void)SaveMetadataToStoreLockedUnlocked();
        }
      }
    } else {
      metadata_.erase(file_id);
    }
  }

  error.clear();
  return plaintext;
}

std::optional<std::vector<std::uint8_t>> OfflineStorage::FetchBlob(
    const std::string& file_id, bool wipe_after_read, std::string& error) {
  if (!IsValidFileId(file_id)) {
    error = "invalid file id";
    return std::nullopt;
  }
  if (state_store_) {
    if (!LoadMetadataFromStore()) {
      error = "metadata store load failed";
      return std::nullopt;
    }
  }
  const auto path = ResolvePath(file_id);
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    error = "file not found";
    return std::nullopt;
  }
  std::error_code ec;
  const std::uint64_t size = std::filesystem::file_size(path, ec);
  if (ec) {
    error = "file read failed";
    return std::nullopt;
  }
  if (size == 0) {
    error = "empty file";
    return std::nullopt;
  }
  if (size > static_cast<std::uint64_t>(
                 (std::numeric_limits<std::size_t>::max)())) {
    error = "file too large";
    return std::nullopt;
  }
  std::vector<std::uint8_t> content;
  content.resize(static_cast<std::size_t>(size));
  ifs.read(reinterpret_cast<char*>(content.data()),
           static_cast<std::streamsize>(content.size()));
  if (!ifs || ifs.gcount() != static_cast<std::streamsize>(content.size())) {
    error = "file read failed";
    return std::nullopt;
  }
  ifs.close();

  if (wipe_after_read) {
    WipeFile(path);
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_store_) {
      std::string lock_err;
      StateStoreLock store_lock(state_store_, "offline_storage_meta",
                                std::chrono::milliseconds(5000), lock_err);
      if (store_lock.locked()) {
        if (LoadMetadataFromStoreLocked()) {
          metadata_.erase(file_id);
          (void)SaveMetadataToStoreLockedUnlocked();
        }
      }
    } else {
      metadata_.erase(file_id);
    }
  }

  error.clear();
  return content;
}

std::optional<StoredFileMeta> OfflineStorage::Meta(
    const std::string& file_id) {
  if (!IsValidFileId(file_id)) {
    return std::nullopt;
  }
  if (state_store_) {
    (void)LoadMetadataFromStore();
  }
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = metadata_.find(file_id);
  if (it == metadata_.end()) {
    return std::nullopt;
  }
  return it->second;
}

OfflineStorageStats OfflineStorage::GetStats() {
  OfflineStorageStats stats;
  if (state_store_) {
    (void)LoadMetadataFromStore();
  }
  std::lock_guard<std::mutex> lock(mutex_);
  stats.files = static_cast<std::uint64_t>(metadata_.size());
  for (const auto& kv : metadata_) {
    stats.bytes += kv.second.size;
  }
  return stats;
}

void OfflineStorage::CleanupExpired() {
  const auto now = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    bool removed = false;
    if (state_store_) {
      std::string lock_err;
      StateStoreLock store_lock(state_store_, "offline_storage_meta",
                                std::chrono::milliseconds(5000), lock_err);
      if (store_lock.locked() && LoadMetadataFromStoreLocked()) {
        for (auto it = metadata_.begin(); it != metadata_.end();) {
          if (now - it->second.created_at > ttl_) {
            const auto path = ResolvePath(it->first);
            WipeFile(path);
            it = metadata_.erase(it);
            removed = true;
          } else {
            ++it;
          }
        }
        if (removed) {
          (void)SaveMetadataToStoreLockedUnlocked();
        }
      }
    } else {
      for (auto it = metadata_.begin(); it != metadata_.end();) {
        if (now - it->second.created_at > ttl_) {
          const auto path = ResolvePath(it->first);
          WipeFile(path);
          it = metadata_.erase(it);
        } else {
          ++it;
        }
      }
    }
  }
  CleanupExpiredBlobArtifacts();
}

void OfflineStorage::CleanupExpiredBlobArtifacts() {
  if (base_dir_.empty()) {
    return;
  }
  std::error_code ec;
  std::filesystem::directory_iterator it(base_dir_, ec);
  if (ec) {
    return;
  }
  const auto now = std::filesystem::file_time_type::clock::now();
  const auto expire_before = now - kBlobSessionTtl;
  const std::filesystem::directory_iterator end;
  for (; it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (!it->is_regular_file(ec) || ec) {
      ec.clear();
      continue;
    }
    const auto path = it->path();
    const auto ext = path.extension();
    if (ext != ".part" && ext != ".itag" && ext != ".lock") {
      continue;
    }
    const auto mtime = std::filesystem::last_write_time(path, ec);
    if (ec || mtime > expire_before) {
      ec.clear();
      continue;
    }
    if (ext == ".part") {
      WipeFile(path);
      RemoveUploadSidecars(path);
    } else {
      std::filesystem::remove(path, ec);
      ec.clear();
    }
  }
}

std::filesystem::path OfflineStorage::ResolvePath(
    const std::string& file_id) const {
  return base_dir_ / (file_id + ".bin");
}

std::filesystem::path OfflineStorage::ResolveUploadTempPath(
    const std::string& file_id) const {
  return base_dir_ / (file_id + ".part");
}

std::filesystem::path OfflineStorage::ResolveKeyPath(
    const std::string& file_id) const {
  return base_dir_ / (file_id + ".key");
}

std::filesystem::path OfflineStorage::ResolveMetaPath(
    const std::string& file_id) const {
  return base_dir_ / (file_id + ".meta");
}

std::optional<std::filesystem::path> OfflineStorage::ResolveKeyPathForData(
    const std::filesystem::path& data_path) const {
  if (data_path.extension() != ".bin") {
    return std::nullopt;
  }
  const auto stem = data_path.stem().string();
  if (stem.empty()) {
    return std::nullopt;
  }
  return data_path.parent_path() / (stem + ".key");
}

std::optional<std::filesystem::path> OfflineStorage::ResolveMetaPathForData(
    const std::filesystem::path& data_path) const {
  if (data_path.extension() != ".bin") {
    return std::nullopt;
  }
  const auto stem = data_path.stem().string();
  if (stem.empty()) {
    return std::nullopt;
  }
  return data_path.parent_path() / (stem + ".meta");
}

std::string OfflineStorage::GenerateId() const {
  std::array<std::uint8_t, 16> rnd{};
  (void)FillRandom(rnd.data(), rnd.size());
  const char* hex = "0123456789abcdef";
  std::string out;
  out.resize(32);
  for (int i = 0; i < 16; ++i) {
    const std::uint8_t v = rnd[static_cast<std::size_t>(i)];
    out[i * 2] = hex[v >> 4];
    out[i * 2 + 1] = hex[v & 0x0F];
  }
  return out;
}

std::array<std::uint8_t, 32> OfflineStorage::GenerateKey() const {
  std::array<std::uint8_t, 32> key{};
  (void)FillRandom(key.data(), key.size());
  return key;
}

std::string OfflineStorage::GenerateSessionId() const { return GenerateId(); }

bool OfflineStorage::SaveEraseKey(const std::filesystem::path& data_path,
                                  const std::array<std::uint8_t, 32>& erase_key,
                                  std::string& error) const {
  error.clear();
  const auto key_path = ResolveKeyPathForData(data_path);
  if (!key_path.has_value()) {
    error = "key path invalid";
    return false;
  }
  std::error_code ec;
  if (!pfs::AtomicWrite(*key_path, erase_key.data(), erase_key.size(), ec)) {
    error = "key write failed";
    return false;
  }
  std::string perm_err;
  if (!EnforceOwnerOnlyPermissions(*key_path, perm_err)) {
    std::filesystem::remove(*key_path, ec);
    error = perm_err.empty() ? "key permissions insecure" : perm_err;
    return false;
  }
  return true;
}

bool OfflineStorage::LoadEraseKey(const std::filesystem::path& data_path,
                                  std::array<std::uint8_t, 32>& erase_key,
                                  std::string& error) const {
  error.clear();
  erase_key.fill(0);
  const auto key_path = ResolveKeyPathForData(data_path);
  if (!key_path.has_value()) {
    error = "key path invalid";
    return false;
  }
  std::error_code ec;
  const auto size = std::filesystem::file_size(*key_path, ec);
  if (ec || size != erase_key.size()) {
    error = "erase key invalid";
    return false;
  }
  std::ifstream ifs(*key_path, std::ios::binary);
  if (!ifs) {
    error = "erase key not found";
    return false;
  }
  ifs.read(reinterpret_cast<char*>(erase_key.data()),
           static_cast<std::streamsize>(erase_key.size()));
  if (!ifs || ifs.gcount() != static_cast<std::streamsize>(erase_key.size())) {
    error = "erase key invalid";
    return false;
  }
  return true;
}

std::array<std::uint8_t, 32> OfflineStorage::DeriveStorageKey(
    const std::array<std::uint8_t, 32>& file_key,
    const std::array<std::uint8_t, 32>& erase_key) const {
  crypto::Sha256Digest digest;
  crypto::HmacSha256(file_key.data(), file_key.size(), erase_key.data(),
                     erase_key.size(), digest);
  std::array<std::uint8_t, 32> out{};
  std::memcpy(out.data(), digest.bytes.data(), out.size());
  return out;
}

bool OfflineStorage::EncryptAead(
    const std::vector<std::uint8_t>& plaintext,
    const std::array<std::uint8_t, 32>& key,
    const std::array<std::uint8_t, kOfflineFileAeadNonceBytes>& nonce,
    const std::uint8_t* ad, std::size_t ad_len,
    std::vector<std::uint8_t>& cipher,
    std::array<std::uint8_t, kOfflineFileAeadTagBytes>& mac) const {
  cipher.resize(plaintext.size());
  crypto_aead_lock(cipher.data(), mac.data(), key.data(), nonce.data(), ad, ad_len,
                   plaintext.data(), plaintext.size());
  return true;
}

bool OfflineStorage::DecryptAead(
    const std::vector<std::uint8_t>& cipher,
    const std::array<std::uint8_t, 32>& key,
    const std::array<std::uint8_t, kOfflineFileAeadNonceBytes>& nonce,
    const std::uint8_t* ad, std::size_t ad_len,
    const std::array<std::uint8_t, kOfflineFileAeadTagBytes>& mac,
    std::vector<std::uint8_t>& plaintext) const {
  plaintext.resize(cipher.size());
  const int ok =
      crypto_aead_unlock(plaintext.data(), mac.data(), key.data(), nonce.data(),
                         ad, ad_len, cipher.data(), cipher.size());
  if (ok != 0) {
    plaintext.clear();
    return false;
  }
  return true;
}

bool OfflineStorage::EncryptLegacy(const std::vector<std::uint8_t>& plaintext,
                                  const std::array<std::uint8_t, 32>& key,
                                  const std::array<std::uint8_t, 16>& nonce,
                                  std::vector<std::uint8_t>& cipher,
                                  std::array<std::uint8_t, 32>& tag) const {
  cipher.resize(plaintext.size());
  std::array<std::uint8_t, 32> block{};
  std::uint64_t counter = 0;
  std::size_t offset = 0;
  while (offset < plaintext.size()) {
    DeriveBlock(key, nonce, counter, block);
    const std::size_t to_copy =
        std::min(block.size(), plaintext.size() - offset);
    for (std::size_t i = 0; i < to_copy; ++i) {
      cipher[offset + i] = static_cast<std::uint8_t>(
          plaintext[offset + i] ^ block[i]);
    }
    ++counter;
    offset += to_copy;
  }

  std::vector<std::uint8_t> mac_buf;
  mac_buf.reserve(nonce.size() + cipher.size());
  mac_buf.insert(mac_buf.end(), nonce.begin(), nonce.end());
  mac_buf.insert(mac_buf.end(), cipher.begin(), cipher.end());

  crypto::Sha256Digest digest;
  crypto::HmacSha256(key.data(), key.size(), mac_buf.data(),
                     mac_buf.size(), digest);
  std::memcpy(tag.data(), digest.bytes.data(), tag.size());
  return true;
}

bool OfflineStorage::DecryptLegacy(const std::vector<std::uint8_t>& cipher,
                                  const std::array<std::uint8_t, 32>& key,
                                  const std::array<std::uint8_t, 16>& nonce,
                                  const std::array<std::uint8_t, 32>& tag,
                                  std::vector<std::uint8_t>& plaintext) const {
  std::vector<std::uint8_t> mac_buf;
  mac_buf.reserve(nonce.size() + cipher.size());
  mac_buf.insert(mac_buf.end(), nonce.begin(), nonce.end());
  mac_buf.insert(mac_buf.end(), cipher.begin(), cipher.end());

  crypto::Sha256Digest digest;
  crypto::HmacSha256(key.data(), key.size(), mac_buf.data(),
                     mac_buf.size(), digest);
  std::uint8_t diff = 0;
  for (std::size_t i = 0; i < tag.size(); ++i) {
    diff |= static_cast<std::uint8_t>(tag[i] ^ digest.bytes[i]);
  }
  if (diff != 0) {
    return false;
  }

  plaintext.resize(cipher.size());
  std::array<std::uint8_t, 32> block{};
  std::uint64_t counter = 0;
  std::size_t offset = 0;
  while (offset < cipher.size()) {
    DeriveBlock(key, nonce, counter, block);
    const std::size_t to_copy = std::min(block.size(), cipher.size() - offset);
    for (std::size_t i = 0; i < to_copy; ++i) {
      plaintext[offset + i] = static_cast<std::uint8_t>(
          cipher[offset + i] ^ block[i]);
    }
    ++counter;
    offset += to_copy;
  }
  return true;
}

bool OfflineStorage::LoadSecureDeletePlugin(const std::filesystem::path& path,
                                            std::string& error) {
  error.clear();
  if (path.empty()) {
    error = "secure delete plugin path empty";
    return false;
  }
#ifdef _WIN32
  const std::wstring wpath = path.wstring();
  HMODULE handle = LoadLibraryW(wpath.c_str());
  if (!handle) {
    error = "secure delete plugin load failed";
    return false;
  }
  auto fn = reinterpret_cast<SecureDeleteFn>(
      GetProcAddress(handle, "mi_secure_delete"));
  if (!fn) {
    FreeLibrary(handle);
    error = "secure delete plugin missing mi_secure_delete";
    return false;
  }
  secure_delete_handle_ = handle;
  secure_delete_fn_ = fn;
#else
  void* handle = dlopen(path.c_str(), RTLD_NOW);
  if (!handle) {
    error = "secure delete plugin load failed";
    return false;
  }
  auto fn = reinterpret_cast<SecureDeleteFn>(dlsym(handle, "mi_secure_delete"));
  if (!fn) {
    dlclose(handle);
    error = "secure delete plugin missing mi_secure_delete";
    return false;
  }
  secure_delete_handle_ = handle;
  secure_delete_fn_ = fn;
#endif
  return true;
}

bool OfflineStorage::CallSecureDeletePlugin(
    const std::filesystem::path& path) const {
  if (!secure_delete_.enabled || !secure_delete_ready_ || !secure_delete_fn_) {
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    return true;
  }
  const std::string path_utf8 = path.u8string();
  if (path_utf8.empty()) {
    return false;
  }
  const int rc = secure_delete_fn_(path_utf8.c_str());
  return rc != 0;
}

void OfflineStorage::BestEffortWipe(const std::filesystem::path& path) const {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return;
  }
  const auto size = std::filesystem::file_size(path, ec);
  if (ec) {
    std::filesystem::remove(path, ec);
    return;
  }
  std::fstream fs(path, std::ios::binary | std::ios::in | std::ios::out);
  if (!fs) {
    std::filesystem::remove(path, ec);
    return;
  }
  constexpr std::size_t kChunkBytes = 64u * 1024u;
  std::vector<std::uint8_t> chunk(kChunkBytes, 0);
  auto write_pass = [&](int pass) -> bool {
    fs.clear();
    fs.seekp(0, std::ios::beg);
    if (!fs) {
      return false;
    }
    std::uint64_t remaining = size;
    while (remaining > 0) {
      const std::size_t n = static_cast<std::size_t>(
          std::min<std::uint64_t>(remaining, chunk.size()));
      if (pass == 0) {
        std::fill_n(chunk.begin(), n, static_cast<std::uint8_t>(0x00));
      } else if (pass == 1) {
        std::fill_n(chunk.begin(), n, static_cast<std::uint8_t>(0xFF));
      } else if (!crypto::RandomBytes(chunk.data(), n)) {
        std::fill_n(chunk.begin(), n, static_cast<std::uint8_t>(0xA5));
      }
      fs.write(reinterpret_cast<const char*>(chunk.data()),
               static_cast<std::streamsize>(n));
      if (!fs) {
        return false;
      }
      remaining -= static_cast<std::uint64_t>(n);
    }
    fs.flush();
    return static_cast<bool>(fs);
  };
  (void)write_pass(0);
  (void)write_pass(1);
  (void)write_pass(2);
  fs.close();
  std::filesystem::remove(path, ec);
}

void OfflineStorage::WipeFile(const std::filesystem::path& path) const {
  std::error_code ec;
  const auto key_path = ResolveKeyPathForData(path);
  if (key_path.has_value() && std::filesystem::exists(*key_path, ec) && !ec) {
    if (!CallSecureDeletePlugin(*key_path)) {
      BestEffortWipe(*key_path);
    } else {
      std::filesystem::remove(*key_path, ec);
    }
  }
  const auto meta_path = ResolveMetaPathForData(path);
  if (meta_path.has_value() && std::filesystem::exists(*meta_path, ec) && !ec) {
    std::filesystem::remove(*meta_path, ec);
  }
  if (!CallSecureDeletePlugin(path)) {
    BestEffortWipe(path);
  } else {
    std::filesystem::remove(path, ec);
  }
}

std::filesystem::path OfflineQueue::RecipientDir(
    const std::string& recipient) const {
  if (persist_dir_.empty() || recipient.empty()) {
    return {};
  }
  const std::string hash = mi::common::Sha256Hex(
      reinterpret_cast<const std::uint8_t*>(recipient.data()),
      recipient.size());
  if (hash.empty()) {
    return {};
  }
  return persist_dir_ / hash;
}

std::filesystem::path OfflineQueue::MessagePath(
    const std::string& recipient, std::uint64_t message_id) const {
  const auto dir = RecipientDir(recipient);
  if (dir.empty()) {
    return {};
  }
  return dir / (FormatMessageId(message_id) + ".msg");
}

void OfflineQueue::DeleteMessageFile(const std::string& recipient,
                                     std::uint64_t message_id) const {
  if (!persistence_enabled_ || state_store_) {
    return;
  }
  const auto path = MessagePath(recipient, message_id);
  if (path.empty()) {
    return;
  }
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    return;
  }
  const std::filesystem::path tomb = path.string() + ".del";
  std::filesystem::rename(path, tomb, ec);
  if (ec) {
    std::filesystem::remove(path, ec);
    return;
  }
  std::filesystem::remove(tomb, ec);
}

bool OfflineQueue::PersistMessage(
    const StoredMessage& stored,
    std::chrono::system_clock::time_point created_at_sys) {
  if (!persistence_enabled_ || state_store_) {
    return true;
  }
  if (stored.msg.recipient.empty()) {
    return false;
  }

  const auto path = MessagePath(stored.msg.recipient, stored.message_id);
  if (path.empty()) {
    return false;
  }
  const auto dir = path.parent_path();
  std::error_code ec;
  if (!dir.empty()) {
    std::filesystem::create_directories(dir, ec);
    if (ec) {
      return false;
    }
    std::string perm_err;
    if (!mi::shard::security::CheckPathNotWorldWritable(dir, perm_err)) {
      return false;
    }
  }

  if (stored.msg.recipient.size() >
          static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()) ||
      stored.msg.sender.size() >
          static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()) ||
      stored.msg.group_id.size() >
          static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()) ||
      stored.msg.payload.size() >
          static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())) {
    return false;
  }

  const std::uint32_t recipient_len =
      static_cast<std::uint32_t>(stored.msg.recipient.size());
  const std::uint32_t sender_len =
      static_cast<std::uint32_t>(stored.msg.sender.size());
  const std::uint32_t group_len =
      static_cast<std::uint32_t>(stored.msg.group_id.size());
  const std::uint32_t payload_len =
      static_cast<std::uint32_t>(stored.msg.payload.size());

  const std::int64_t ttl_in = stored.msg.ttl.count();
  std::uint64_t ttl_val =
      ttl_in > 0 ? static_cast<std::uint64_t>(ttl_in)
                 : static_cast<std::uint64_t>(default_ttl_.count());
  if (ttl_val >
      static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)())) {
    ttl_val =
        static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)());
  }
  const std::uint32_t ttl_sec = static_cast<std::uint32_t>(ttl_val);

  std::array<std::uint8_t, kOfflineQueueHeaderBytes> header{};
  std::size_t off = 0;
  std::memcpy(header.data(), kOfflineQueueMagic.data(),
              kOfflineQueueMagic.size());
  off += kOfflineQueueMagic.size();
  header[off++] = kOfflineQueueVersion;
  header[off++] = static_cast<std::uint8_t>(stored.msg.kind);
  header[off++] = 0;
  header[off++] = 0;
  WriteUint64Le(stored.message_id, header.data() + off);
  off += 8;
  WriteUint64Le(UnixMsFrom(created_at_sys), header.data() + off);
  off += 8;
  WriteUint32Le(ttl_sec, header.data() + off);
  off += 4;
  WriteUint32Le(recipient_len, header.data() + off);
  off += 4;
  WriteUint32Le(sender_len, header.data() + off);
  off += 4;
  WriteUint32Le(group_len, header.data() + off);
  off += 4;
  WriteUint32Le(payload_len, header.data() + off);
  off += 4;

  std::vector<std::uint8_t> plain;
  plain.reserve(kOfflineQueueHeaderBytes +
                static_cast<std::size_t>(recipient_len) +
                static_cast<std::size_t>(sender_len) +
                static_cast<std::size_t>(group_len) +
                static_cast<std::size_t>(payload_len));
  plain.insert(plain.end(), header.begin(), header.end());
  if (recipient_len != 0) {
    plain.insert(plain.end(), stored.msg.recipient.begin(),
                 stored.msg.recipient.end());
  }
  if (sender_len != 0) {
    plain.insert(plain.end(), stored.msg.sender.begin(),
                 stored.msg.sender.end());
  }
  if (group_len != 0) {
    plain.insert(plain.end(), stored.msg.group_id.begin(),
                 stored.msg.group_id.end());
  }
  if (payload_len != 0) {
    plain.insert(plain.end(), stored.msg.payload.begin(),
                 stored.msg.payload.end());
  }

  std::vector<std::uint8_t> protected_bytes;
  std::string protect_err;
  if (!EncodeProtectedFileBytes(plain, state_protection_, protected_bytes,
                                protect_err)) {
    return false;
  }

  std::string write_err;
  if (!AtomicWriteOwnerOnly(path, protected_bytes.data(),
                            protected_bytes.size(), write_err)) {
    return false;
  }
  return true;
}

bool OfflineQueue::LoadFromDisk() {
  if (!persistence_enabled_ || persist_dir_.empty()) {
    return true;
  }
  std::error_code ec;
  if (!std::filesystem::exists(persist_dir_, ec)) {
    std::filesystem::create_directories(persist_dir_, ec);
    return !ec;
  }

  const auto now_sys = std::chrono::system_clock::now();
  const auto now_steady = std::chrono::steady_clock::now();
  std::unordered_map<std::string, std::vector<StoredMessage>> loaded;
  std::array<std::uint64_t, kShardCount> max_ids{};

  const auto purge = [&](const std::filesystem::path& path) {
    std::error_code rm_ec;
    std::filesystem::remove(path, rm_ec);
  };

  std::filesystem::recursive_directory_iterator it(persist_dir_, ec);
  const std::filesystem::recursive_directory_iterator end;
  for (; it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (!it->is_regular_file(ec)) {
      continue;
    }
    const auto path = it->path();
    const auto ext = path.extension().string();
    if (ext == ".tmp" || ext == ".del") {
      purge(path);
      continue;
    }
    if (ext != ".msg") {
      continue;
    }
    std::string perm_err;
    if (!mi::shard::security::CheckPathNotWorldWritable(path, perm_err)) {
      purge(path);
      continue;
    }
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size < kOfflineQueueHeaderBytes ||
        size > static_cast<std::uint64_t>(
                   (std::numeric_limits<std::size_t>::max)())) {
      purge(path);
      continue;
    }
    std::vector<std::uint8_t> bytes;
    bytes.resize(static_cast<std::size_t>(size));
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
      continue;
    }
    ifs.read(reinterpret_cast<char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
    if (!ifs ||
        ifs.gcount() != static_cast<std::streamsize>(bytes.size())) {
      purge(path);
      continue;
    }

    bool need_rewrap = false;
    {
      std::vector<std::uint8_t> plain;
      bool was_protected = false;
      std::string protect_err;
      if (!DecodeProtectedFileBytes(bytes, state_protection_, plain,
                                    was_protected, protect_err)) {
        purge(path);
        continue;
      }
      need_rewrap =
          !was_protected && state_protection_ != KeyProtectionMode::kNone;
      bytes.swap(plain);
    }

    if (bytes.size() < kOfflineQueueHeaderBytes ||
        !std::equal(kOfflineQueueMagic.begin(), kOfflineQueueMagic.end(),
                    bytes.begin())) {
      purge(path);
      continue;
    }
    std::size_t off = kOfflineQueueMagic.size();
    const std::uint8_t version = bytes[off++];
    if (version != kOfflineQueueVersion) {
      purge(path);
      continue;
    }
    const std::uint8_t kind = bytes[off++];
    off += 2;
    const std::uint64_t message_id = ReadUint64Le(bytes.data() + off);
    off += 8;
    const std::uint64_t created_ms = ReadUint64Le(bytes.data() + off);
    off += 8;
    const std::uint32_t ttl_sec_raw = ReadUint32Le(bytes.data() + off);
    off += 4;
    const std::uint32_t recipient_len = ReadUint32Le(bytes.data() + off);
    off += 4;
    const std::uint32_t sender_len = ReadUint32Le(bytes.data() + off);
    off += 4;
    const std::uint32_t group_len = ReadUint32Le(bytes.data() + off);
    off += 4;
    const std::uint32_t payload_len = ReadUint32Le(bytes.data() + off);
    off += 4;

    const std::uint64_t expected =
        kOfflineQueueHeaderBytes +
        static_cast<std::uint64_t>(recipient_len) +
        static_cast<std::uint64_t>(sender_len) +
        static_cast<std::uint64_t>(group_len) +
        static_cast<std::uint64_t>(payload_len);
    if (recipient_len == 0 ||
        expected != static_cast<std::uint64_t>(bytes.size())) {
      purge(path);
      continue;
    }
    if (kind >
        static_cast<std::uint8_t>(QueueMessageKind::kGroupNotice)) {
      purge(path);
      continue;
    }

    const auto read_str = [&](std::string& out, std::uint32_t len) -> bool {
      if (len == 0) {
        out.clear();
        return true;
      }
      if (off + len > bytes.size()) {
        return false;
      }
      out.assign(reinterpret_cast<const char*>(bytes.data() + off),
                 reinterpret_cast<const char*>(bytes.data() + off + len));
      off += len;
      return true;
    };
    std::string recipient;
    std::string sender;
    std::string group_id;
    if (!read_str(recipient, recipient_len) ||
        !read_str(sender, sender_len) ||
        !read_str(group_id, group_len)) {
      purge(path);
      continue;
    }
    if (off + payload_len > bytes.size()) {
      purge(path);
      continue;
    }
    std::vector<std::uint8_t> payload;
    if (payload_len != 0) {
      payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(off),
                     bytes.begin() + static_cast<std::ptrdiff_t>(off + payload_len));
      off += payload_len;
    }

    std::uint64_t ttl_val =
        ttl_sec_raw == 0
            ? static_cast<std::uint64_t>(default_ttl_.count())
            : static_cast<std::uint64_t>(ttl_sec_raw);
    if (ttl_val >
        static_cast<std::uint64_t>(
            (std::numeric_limits<std::uint32_t>::max)())) {
      ttl_val =
          static_cast<std::uint64_t>(
              (std::numeric_limits<std::uint32_t>::max)());
    }
    const std::uint32_t ttl_sec = static_cast<std::uint32_t>(ttl_val);
    if (ttl_sec == 0) {
      purge(path);
      continue;
    }

    StoredMessage stored;
    stored.message_id = message_id;
    stored.msg.kind = static_cast<QueueMessageKind>(kind);
    stored.msg.recipient = recipient;
    stored.msg.sender = sender;
    stored.msg.group_id = group_id;
    stored.msg.payload = std::move(payload);
    stored.msg.ttl = std::chrono::seconds(ttl_sec);

    const auto created_sys = UnixMsToTimepoint(created_ms);
    const auto age = created_sys > now_sys
                         ? std::chrono::system_clock::duration::zero()
                         : now_sys - created_sys;
    if (age >= stored.msg.ttl) {
      purge(path);
      continue;
    }
    const auto age_steady =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(age);
    stored.msg.created_at = now_steady - age_steady;
    stored.expires_at = stored.msg.created_at + stored.msg.ttl;

    if (need_rewrap) {
      PersistMessage(stored, created_sys);
    }
    loaded[recipient].push_back(std::move(stored));
    const auto shard_index = ShardIndexFor(recipient);
    if (message_id > max_ids[shard_index]) {
      max_ids[shard_index] = message_id;
    }
  }

  for (auto& kv : loaded) {
    const std::string& recipient = kv.first;
    auto& items = kv.second;
    if (items.empty()) {
      continue;
    }
    std::sort(items.begin(), items.end(),
              [](const StoredMessage& a, const StoredMessage& b) {
                return a.message_id < b.message_id;
              });
    auto& shard = shards_[ShardIndexFor(recipient)];
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto& queue = shard.recipients[recipient];
    for (auto& stored : items) {
      queue.messages.push_back(std::move(stored));
      const auto it = std::prev(queue.messages.end());
      queue.by_id.emplace(it->message_id, it);
      shard.expiries.push(ExpiryItem{it->expires_at, recipient, it->message_id});
    }
  }

  for (std::size_t i = 0; i < kShardCount; ++i) {
    auto& shard = shards_[i];
    std::lock_guard<std::mutex> lock(shard.mutex);
    if (max_ids[i] >= shard.next_id) {
      shard.next_id = max_ids[i] + 1;
    }
  }
  return true;
}

bool OfflineQueue::LoadFromStore() {
  if (!state_store_) {
    return true;
  }
  BlobLoadResult blob;
  std::string load_err;
  if (!state_store_->LoadBlob("offline_queue", blob, load_err)) {
    return false;
  }
  if (!blob.found || blob.data.empty()) {
    if (persistence_enabled_ && !persist_dir_.empty()) {
      if (!LoadFromDisk()) {
        return false;
      }
      return SaveToStore();
    }
    return true;
  }
  return LoadFromStoreLocked();
}

bool OfflineQueue::LoadFromStoreLocked() {
  if (!state_store_) {
    return true;
  }
  BlobLoadResult blob;
  std::string load_err;
  if (!state_store_->LoadBlob("offline_queue", blob, load_err)) {
    return false;
  }
  auto locks = [&]() {
    std::vector<std::unique_lock<std::mutex>> held;
    held.reserve(kShardCount);
    for (auto& shard : shards_) {
      held.emplace_back(shard.mutex);
    }
    return held;
  }();

  if (!blob.found || blob.data.empty()) {
    for (auto& shard : shards_) {
      shard.recipients.clear();
      shard.expiries = decltype(shard.expiries)();
      shard.next_id = 1;
    }
    return true;
  }
  if (blob.data.size() < kOfflineQueueStoreHeaderBytes) {
    return false;
  }
  if (!std::equal(kOfflineQueueStoreMagic.begin(),
                  kOfflineQueueStoreMagic.end(),
                  blob.data.begin())) {
    return false;
  }
  std::size_t off = kOfflineQueueStoreMagic.size();
  const std::uint8_t version = blob.data[off++];
  if (version != kOfflineQueueStoreVersion) {
    return false;
  }
  off += 3;
  if (off + 4 > blob.data.size()) {
    return false;
  }
  const std::uint32_t count = ReadUint32Le(blob.data.data() + off);
  off += 4;

  const auto now_sys = std::chrono::system_clock::now();
  const auto now_steady = std::chrono::steady_clock::now();
  std::unordered_map<std::string, std::vector<StoredMessage>> loaded;
  std::array<std::uint64_t, kShardCount> max_ids{};
  constexpr std::size_t kRecordHeaderBytes = 1 + 3 + 8 + 8 + 4 + 4 + 4 + 4 + 4;

  for (std::uint32_t i = 0; i < count; ++i) {
    if (off + 4 > blob.data.size()) {
      return false;
    }
    const std::uint32_t record_len = ReadUint32Le(blob.data.data() + off);
    off += 4;
    if (record_len < kRecordHeaderBytes ||
        off + record_len > blob.data.size()) {
      return false;
    }
    std::size_t rec_off = off;
    const std::uint8_t kind = blob.data[rec_off++];
    rec_off += 3;
    const std::uint64_t message_id =
        ReadUint64Le(blob.data.data() + rec_off);
    rec_off += 8;
    const std::uint64_t created_ms =
        ReadUint64Le(blob.data.data() + rec_off);
    rec_off += 8;
    const std::uint32_t ttl_sec_raw =
        ReadUint32Le(blob.data.data() + rec_off);
    rec_off += 4;
    const std::uint32_t recipient_len =
        ReadUint32Le(blob.data.data() + rec_off);
    rec_off += 4;
    const std::uint32_t sender_len =
        ReadUint32Le(blob.data.data() + rec_off);
    rec_off += 4;
    const std::uint32_t group_len =
        ReadUint32Le(blob.data.data() + rec_off);
    rec_off += 4;
    const std::uint32_t payload_len =
        ReadUint32Le(blob.data.data() + rec_off);
    rec_off += 4;

    const std::uint64_t expected =
        kRecordHeaderBytes +
        static_cast<std::uint64_t>(recipient_len) +
        static_cast<std::uint64_t>(sender_len) +
        static_cast<std::uint64_t>(group_len) +
        static_cast<std::uint64_t>(payload_len);
    if (recipient_len == 0 ||
        expected != static_cast<std::uint64_t>(record_len)) {
      return false;
    }
    if (kind > static_cast<std::uint8_t>(QueueMessageKind::kGroupNotice)) {
      return false;
    }

    const auto read_str = [&](std::string& out, std::uint32_t len) -> bool {
      if (len == 0) {
        out.clear();
        return true;
      }
      if (rec_off + len > blob.data.size()) {
        return false;
      }
      out.assign(reinterpret_cast<const char*>(blob.data.data() + rec_off),
                 reinterpret_cast<const char*>(blob.data.data() + rec_off + len));
      rec_off += len;
      return true;
    };
    std::string recipient;
    std::string sender;
    std::string group_id;
    if (!read_str(recipient, recipient_len) ||
        !read_str(sender, sender_len) ||
        !read_str(group_id, group_len)) {
      return false;
    }
    if (rec_off + payload_len > blob.data.size()) {
      return false;
    }
    std::vector<std::uint8_t> payload;
    if (payload_len != 0) {
      payload.assign(
          blob.data.begin() + static_cast<std::ptrdiff_t>(rec_off),
          blob.data.begin() +
              static_cast<std::ptrdiff_t>(rec_off + payload_len));
      rec_off += payload_len;
    }
    if (rec_off != off + record_len) {
      return false;
    }

    std::uint64_t ttl_val =
        ttl_sec_raw == 0
            ? static_cast<std::uint64_t>(default_ttl_.count())
            : static_cast<std::uint64_t>(ttl_sec_raw);
    if (ttl_val >
        static_cast<std::uint64_t>(
            (std::numeric_limits<std::uint32_t>::max)())) {
      ttl_val =
          static_cast<std::uint64_t>(
              (std::numeric_limits<std::uint32_t>::max)());
    }
    const std::uint32_t ttl_sec = static_cast<std::uint32_t>(ttl_val);
    if (ttl_sec == 0) {
      off += record_len;
      continue;
    }

    StoredMessage stored;
    stored.message_id = message_id;
    stored.msg.kind = static_cast<QueueMessageKind>(kind);
    stored.msg.recipient = recipient;
    stored.msg.sender = sender;
    stored.msg.group_id = group_id;
    stored.msg.payload = std::move(payload);
    stored.msg.ttl = std::chrono::seconds(ttl_sec);

    const auto created_sys = UnixMsToTimepoint(created_ms);
    const auto age = created_sys > now_sys
                         ? std::chrono::system_clock::duration::zero()
                         : now_sys - created_sys;
    if (age >= stored.msg.ttl) {
      off += record_len;
      continue;
    }
    const auto age_steady =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(age);
    stored.msg.created_at = now_steady - age_steady;
    stored.expires_at = stored.msg.created_at + stored.msg.ttl;

    loaded[recipient].push_back(std::move(stored));
    const auto shard_index = ShardIndexFor(recipient);
    if (message_id > max_ids[shard_index]) {
      max_ids[shard_index] = message_id;
    }

    off += record_len;
  }
  if (off != blob.data.size()) {
    return false;
  }

  for (auto& shard : shards_) {
    shard.recipients.clear();
    shard.expiries = decltype(shard.expiries)();
    shard.next_id = 1;
  }

  for (auto& kv : loaded) {
    const std::string& recipient = kv.first;
    auto& items = kv.second;
    if (items.empty()) {
      continue;
    }
    std::sort(items.begin(), items.end(),
              [](const StoredMessage& a, const StoredMessage& b) {
                return a.message_id < b.message_id;
              });
    auto& shard = shards_[ShardIndexFor(recipient)];
    auto& queue = shard.recipients[recipient];
    for (auto& stored : items) {
      queue.messages.push_back(std::move(stored));
      const auto it = std::prev(queue.messages.end());
      queue.by_id.emplace(it->message_id, it);
      shard.expiries.push(ExpiryItem{it->expires_at, recipient, it->message_id});
    }
  }

  for (std::size_t i = 0; i < kShardCount; ++i) {
    auto& shard = shards_[i];
    if (max_ids[i] >= shard.next_id) {
      shard.next_id = max_ids[i] + 1;
    }
  }
  return true;
}

bool OfflineQueue::SaveToStore() {
  return SaveToStoreLocked();
}

bool OfflineQueue::SaveToStoreLocked() {
  if (!state_store_) {
    return true;
  }
  std::string lock_err;
  StateStoreLock lock(state_store_, "offline_queue",
                      std::chrono::milliseconds(5000), lock_err);
  if (!lock.locked()) {
    return false;
  }
  return SaveToStoreLockedUnlocked();
}

bool OfflineQueue::SaveToStoreLockedUnlocked() {
  if (!state_store_) {
    return true;
  }
  auto locks = [&]() {
    std::vector<std::unique_lock<std::mutex>> held;
    held.reserve(kShardCount);
    for (auto& shard : shards_) {
      held.emplace_back(shard.mutex);
    }
    return held;
  }();

  const auto now_steady = std::chrono::steady_clock::now();
  for (auto& shard : shards_) {
    CleanupExpiredLocked(shard, now_steady);
  }

  std::vector<std::uint8_t> out;
  out.reserve(kOfflineQueueStoreHeaderBytes + 128);
  out.insert(out.end(), kOfflineQueueStoreMagic.begin(),
             kOfflineQueueStoreMagic.end());
  out.push_back(kOfflineQueueStoreVersion);
  out.push_back(0);
  out.push_back(0);
  out.push_back(0);
  const std::size_t count_offset = out.size();
  std::uint8_t count_buf[4] = {};
  out.insert(out.end(), count_buf, count_buf + 4);

  std::uint32_t count = 0;
  const auto now_sys = std::chrono::system_clock::now();
  for (const auto& shard : shards_) {
    for (const auto& entry : shard.recipients) {
      for (const auto& stored : entry.second.messages) {
        if (stored.msg.recipient.empty()) {
          continue;
        }
        const std::size_t recipient_len = stored.msg.recipient.size();
        const std::size_t sender_len = stored.msg.sender.size();
        const std::size_t group_len = stored.msg.group_id.size();
        const std::size_t payload_len = stored.msg.payload.size();
        if (recipient_len >
                static_cast<std::size_t>(
                    (std::numeric_limits<std::uint32_t>::max)()) ||
            sender_len >
                static_cast<std::size_t>(
                    (std::numeric_limits<std::uint32_t>::max)()) ||
            group_len >
                static_cast<std::size_t>(
                    (std::numeric_limits<std::uint32_t>::max)()) ||
            payload_len >
                static_cast<std::size_t>(
                    (std::numeric_limits<std::uint32_t>::max)())) {
          return false;
        }
        const std::size_t record_len = (1 + 3 + 8 + 8 + 4 + 4 + 4 + 4 + 4) +
                                       recipient_len + sender_len + group_len +
                                       payload_len;
        if (record_len >
            static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)())) {
          return false;
        }

        std::uint8_t buf32[4] = {};
        WriteUint32Le(static_cast<std::uint32_t>(record_len), buf32);
        out.insert(out.end(), buf32, buf32 + 4);
        out.push_back(static_cast<std::uint8_t>(stored.msg.kind));
        out.push_back(0);
        out.push_back(0);
        out.push_back(0);
        std::uint8_t buf64[8] = {};
        WriteUint64Le(stored.message_id, buf64);
        out.insert(out.end(), buf64, buf64 + 8);
        const auto created_sys =
            SystemFromSteady(stored.msg.created_at, now_sys, now_steady);
        WriteUint64Le(UnixMsFrom(created_sys), buf64);
        out.insert(out.end(), buf64, buf64 + 8);
        std::uint64_t ttl_val =
            stored.msg.ttl.count() > 0
                ? static_cast<std::uint64_t>(stored.msg.ttl.count())
                : static_cast<std::uint64_t>(default_ttl_.count());
        if (ttl_val >
            static_cast<std::uint64_t>(
                (std::numeric_limits<std::uint32_t>::max)())) {
          ttl_val =
              static_cast<std::uint64_t>(
                  (std::numeric_limits<std::uint32_t>::max)());
        }
        WriteUint32Le(static_cast<std::uint32_t>(ttl_val), buf32);
        out.insert(out.end(), buf32, buf32 + 4);
        WriteUint32Le(static_cast<std::uint32_t>(recipient_len), buf32);
        out.insert(out.end(), buf32, buf32 + 4);
        WriteUint32Le(static_cast<std::uint32_t>(sender_len), buf32);
        out.insert(out.end(), buf32, buf32 + 4);
        WriteUint32Le(static_cast<std::uint32_t>(group_len), buf32);
        out.insert(out.end(), buf32, buf32 + 4);
        WriteUint32Le(static_cast<std::uint32_t>(payload_len), buf32);
        out.insert(out.end(), buf32, buf32 + 4);
        out.insert(out.end(), stored.msg.recipient.begin(),
                   stored.msg.recipient.end());
        out.insert(out.end(), stored.msg.sender.begin(),
                   stored.msg.sender.end());
        out.insert(out.end(), stored.msg.group_id.begin(),
                   stored.msg.group_id.end());
        out.insert(out.end(), stored.msg.payload.begin(),
                   stored.msg.payload.end());
        if (count == (std::numeric_limits<std::uint32_t>::max)()) {
          return false;
        }
        ++count;
      }
    }
  }

  std::uint8_t count_out[4] = {};
  WriteUint32Le(count, count_out);
  std::copy(count_out, count_out + 4, out.begin() + count_offset);

  std::string store_err;
  if (!state_store_->SaveBlob("offline_queue", out, store_err)) {
    return false;
  }
  return true;
}

OfflineQueue::OfflineQueue(std::chrono::seconds default_ttl,
                           std::filesystem::path persist_dir,
                           KeyProtectionMode state_protection,
                           StateStore* state_store)
    : default_ttl_(default_ttl == std::chrono::seconds::zero()
                       ? std::chrono::hours(24)
                       : default_ttl),
      persist_dir_(std::move(persist_dir)),
      state_protection_(state_protection),
      state_store_(state_store) {
  if (!persist_dir_.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(persist_dir_, ec);
    if (!ec) {
      persistence_enabled_ = true;
    }
  }
  if (state_store_) {
    (void)LoadFromStore();
  } else if (persistence_enabled_) {
    LoadFromDisk();
  }
}

std::size_t OfflineQueue::ShardIndexFor(const std::string& recipient) const {
  if (recipient.empty()) {
    return 0;
  }
  return std::hash<std::string>{}(recipient) % kShardCount;
}

void OfflineQueue::CleanupExpiredLocked(Shard& shard,
                                       std::chrono::steady_clock::time_point now) {
  while (!shard.expiries.empty()) {
    const auto& top = shard.expiries.top();
    if (top.expires_at > now) {
      break;
    }
    const std::string recipient = top.recipient;
    const std::uint64_t message_id = top.message_id;
    shard.expiries.pop();

    auto rit = shard.recipients.find(recipient);
    if (rit == shard.recipients.end()) {
      continue;
    }
    auto& queue = rit->second;
    auto mit = queue.by_id.find(message_id);
    if (mit == queue.by_id.end()) {
      continue;
    }
    const auto list_it = mit->second;
    if (list_it == queue.messages.end() || list_it->expires_at > now) {
      continue;
    }

    queue.messages.erase(list_it);
    queue.by_id.erase(mit);
    DeleteMessageFile(recipient, message_id);
    if (queue.messages.empty()) {
      shard.recipients.erase(rit);
    }
  }
}

void OfflineQueue::Enqueue(const std::string& recipient,
                           std::vector<std::uint8_t> payload,
                           std::chrono::seconds ttl) {
  bool store_ready = false;
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "offline_queue",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && store_lock.locked()) {
    store_ready = LoadFromStoreLocked();
  }
  const auto now = std::chrono::steady_clock::now();
  const auto now_sys = std::chrono::system_clock::now();
  StoredMessage stored;
  stored.msg.kind = QueueMessageKind::kGeneric;
  stored.msg.recipient = recipient;
  stored.msg.payload = std::move(payload);
  stored.msg.created_at = now;
  stored.msg.ttl = (ttl == std::chrono::seconds::zero()) ? default_ttl_ : ttl;
  stored.expires_at = stored.msg.created_at + stored.msg.ttl;

  auto& shard = shards_[ShardIndexFor(recipient)];
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    CleanupExpiredLocked(shard, now);
    stored.message_id = shard.next_id++;
  }
  PersistMessage(stored, now_sys);
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto& queue = shard.recipients[recipient];
    queue.messages.push_back(std::move(stored));
    const auto it = std::prev(queue.messages.end());
    queue.by_id.emplace(it->message_id, it);
    shard.expiries.push(ExpiryItem{it->expires_at, recipient, it->message_id});
  }
  if (state_store_ && store_ready) {
    (void)SaveToStoreLockedUnlocked();
  }
}

void OfflineQueue::EnqueuePrivate(const std::string& recipient,
                                  const std::string& sender,
                                  std::vector<std::uint8_t> payload,
                                  std::chrono::seconds ttl) {
  bool store_ready = false;
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "offline_queue",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && store_lock.locked()) {
    store_ready = LoadFromStoreLocked();
  }
  const auto now = std::chrono::steady_clock::now();
  const auto now_sys = std::chrono::system_clock::now();
  StoredMessage stored;
  stored.msg.kind = QueueMessageKind::kPrivate;
  stored.msg.sender = sender;
  stored.msg.recipient = recipient;
  stored.msg.payload = std::move(payload);
  stored.msg.created_at = now;
  stored.msg.ttl = (ttl == std::chrono::seconds::zero()) ? default_ttl_ : ttl;
  stored.expires_at = stored.msg.created_at + stored.msg.ttl;

  auto& shard = shards_[ShardIndexFor(recipient)];
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    CleanupExpiredLocked(shard, now);
    stored.message_id = shard.next_id++;
  }
  PersistMessage(stored, now_sys);
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto& queue = shard.recipients[recipient];
    queue.messages.push_back(std::move(stored));
    const auto it = std::prev(queue.messages.end());
    queue.by_id.emplace(it->message_id, it);
    shard.expiries.push(ExpiryItem{it->expires_at, recipient, it->message_id});
  }
  if (state_store_ && store_ready) {
    (void)SaveToStoreLockedUnlocked();
  }
}

void OfflineQueue::EnqueueGroupCipher(const std::string& recipient,
                                      const std::string& group_id,
                                      const std::string& sender,
                                      std::vector<std::uint8_t> payload,
                                      std::chrono::seconds ttl) {
  bool store_ready = false;
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "offline_queue",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && store_lock.locked()) {
    store_ready = LoadFromStoreLocked();
  }
  const auto now = std::chrono::steady_clock::now();
  const auto now_sys = std::chrono::system_clock::now();
  StoredMessage stored;
  stored.msg.kind = QueueMessageKind::kGroupCipher;
  stored.msg.sender = sender;
  stored.msg.recipient = recipient;
  stored.msg.group_id = group_id;
  stored.msg.payload = std::move(payload);
  stored.msg.created_at = now;
  stored.msg.ttl = (ttl == std::chrono::seconds::zero()) ? default_ttl_ : ttl;
  stored.expires_at = stored.msg.created_at + stored.msg.ttl;

  auto& shard = shards_[ShardIndexFor(recipient)];
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    CleanupExpiredLocked(shard, now);
    stored.message_id = shard.next_id++;
  }
  PersistMessage(stored, now_sys);
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto& queue = shard.recipients[recipient];
    queue.messages.push_back(std::move(stored));
    const auto it = std::prev(queue.messages.end());
    queue.by_id.emplace(it->message_id, it);
    shard.expiries.push(ExpiryItem{it->expires_at, recipient, it->message_id});
  }
  if (state_store_ && store_ready) {
    (void)SaveToStoreLockedUnlocked();
  }
}

void OfflineQueue::EnqueueGroupNotice(const std::string& recipient,
                                      const std::string& group_id,
                                      const std::string& sender,
                                      std::vector<std::uint8_t> payload,
                                      std::chrono::seconds ttl) {
  bool store_ready = false;
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "offline_queue",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && store_lock.locked()) {
    store_ready = LoadFromStoreLocked();
  }
  const auto now = std::chrono::steady_clock::now();
  const auto now_sys = std::chrono::system_clock::now();
  StoredMessage stored;
  stored.msg.kind = QueueMessageKind::kGroupNotice;
  stored.msg.sender = sender;
  stored.msg.recipient = recipient;
  stored.msg.group_id = group_id;
  stored.msg.payload = std::move(payload);
  stored.msg.created_at = now;
  stored.msg.ttl = (ttl == std::chrono::seconds::zero()) ? default_ttl_ : ttl;
  stored.expires_at = stored.msg.created_at + stored.msg.ttl;

  auto& shard = shards_[ShardIndexFor(recipient)];
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    CleanupExpiredLocked(shard, now);
    stored.message_id = shard.next_id++;
  }
  PersistMessage(stored, now_sys);
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto& queue = shard.recipients[recipient];
    queue.messages.push_back(std::move(stored));
    const auto it = std::prev(queue.messages.end());
    queue.by_id.emplace(it->message_id, it);
    shard.expiries.push(ExpiryItem{it->expires_at, recipient, it->message_id});
  }
  if (state_store_ && store_ready) {
    (void)SaveToStoreLockedUnlocked();
  }
}

void OfflineQueue::EnqueueDeviceSync(const std::string& recipient,
                                     std::vector<std::uint8_t> payload,
                                     std::chrono::seconds ttl) {
  bool store_ready = false;
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "offline_queue",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && store_lock.locked()) {
    store_ready = LoadFromStoreLocked();
  }
  const auto now = std::chrono::steady_clock::now();
  const auto now_sys = std::chrono::system_clock::now();
  StoredMessage stored;
  stored.msg.kind = QueueMessageKind::kDeviceSync;
  stored.msg.recipient = recipient;
  stored.msg.payload = std::move(payload);
  stored.msg.created_at = now;
  stored.msg.ttl = (ttl == std::chrono::seconds::zero()) ? default_ttl_ : ttl;
  stored.expires_at = stored.msg.created_at + stored.msg.ttl;

  auto& shard = shards_[ShardIndexFor(recipient)];
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    CleanupExpiredLocked(shard, now);
    stored.message_id = shard.next_id++;
  }
  PersistMessage(stored, now_sys);
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto& queue = shard.recipients[recipient];
    queue.messages.push_back(std::move(stored));
    const auto it = std::prev(queue.messages.end());
    queue.by_id.emplace(it->message_id, it);
    shard.expiries.push(ExpiryItem{it->expires_at, recipient, it->message_id});
  }
  if (state_store_ && store_ready) {
    (void)SaveToStoreLockedUnlocked();
  }
}

std::vector<std::vector<std::uint8_t>> OfflineQueue::Drain(
    const std::string& recipient) {
  std::vector<std::vector<std::uint8_t>> out;
  bool store_ready = false;
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "offline_queue",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && store_lock.locked()) {
    store_ready = LoadFromStoreLocked();
  }
  const auto now = std::chrono::steady_clock::now();
  std::vector<std::uint64_t> remove_ids;
  auto& shard = shards_[ShardIndexFor(recipient)];
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    CleanupExpiredLocked(shard, now);

    auto it = shard.recipients.find(recipient);
    if (it == shard.recipients.end()) {
      return out;
    }
    auto& queue = it->second;
    out.reserve(queue.messages.size());
    for (auto msg_it = queue.messages.begin();
         msg_it != queue.messages.end();) {
      if (msg_it->expires_at <= now) {
        remove_ids.push_back(msg_it->message_id);
        queue.by_id.erase(msg_it->message_id);
        msg_it = queue.messages.erase(msg_it);
        continue;
      }
      if (msg_it->msg.kind == QueueMessageKind::kGeneric) {
        remove_ids.push_back(msg_it->message_id);
        out.push_back(std::move(msg_it->msg.payload));
        queue.by_id.erase(msg_it->message_id);
        msg_it = queue.messages.erase(msg_it);
        continue;
      }
      ++msg_it;
    }
    if (queue.messages.empty()) {
      shard.recipients.erase(it);
    }
  }
  for (const auto id : remove_ids) {
    DeleteMessageFile(recipient, id);
  }
  if (state_store_ && store_ready) {
    (void)SaveToStoreLockedUnlocked();
  }
  return out;
}

std::vector<OfflineMessage> OfflineQueue::DrainPrivate(
    const std::string& recipient) {
  std::vector<OfflineMessage> out;
  bool store_ready = false;
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "offline_queue",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && store_lock.locked()) {
    store_ready = LoadFromStoreLocked();
  }
  const auto now = std::chrono::steady_clock::now();
  std::vector<std::uint64_t> remove_ids;
  auto& shard = shards_[ShardIndexFor(recipient)];
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    CleanupExpiredLocked(shard, now);

    auto it = shard.recipients.find(recipient);
    if (it == shard.recipients.end()) {
      return out;
    }
    auto& queue = it->second;
    out.reserve(queue.messages.size());
    for (auto msg_it = queue.messages.begin();
         msg_it != queue.messages.end();) {
      if (msg_it->expires_at <= now) {
        remove_ids.push_back(msg_it->message_id);
        queue.by_id.erase(msg_it->message_id);
        msg_it = queue.messages.erase(msg_it);
        continue;
      }
      if (msg_it->msg.kind == QueueMessageKind::kPrivate) {
        remove_ids.push_back(msg_it->message_id);
        out.push_back(std::move(msg_it->msg));
        queue.by_id.erase(msg_it->message_id);
        msg_it = queue.messages.erase(msg_it);
        continue;
      }
      ++msg_it;
    }
    if (queue.messages.empty()) {
      shard.recipients.erase(it);
    }
  }
  for (const auto id : remove_ids) {
    DeleteMessageFile(recipient, id);
  }
  if (state_store_ && store_ready) {
    (void)SaveToStoreLockedUnlocked();
  }
  return out;
}

std::vector<OfflineMessage> OfflineQueue::DrainGroupCipher(
    const std::string& recipient) {
  std::vector<OfflineMessage> out;
  bool store_ready = false;
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "offline_queue",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && store_lock.locked()) {
    store_ready = LoadFromStoreLocked();
  }
  const auto now = std::chrono::steady_clock::now();
  std::vector<std::uint64_t> remove_ids;
  auto& shard = shards_[ShardIndexFor(recipient)];
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    CleanupExpiredLocked(shard, now);

    auto it = shard.recipients.find(recipient);
    if (it == shard.recipients.end()) {
      return out;
    }
    auto& queue = it->second;
    out.reserve(queue.messages.size());
    for (auto msg_it = queue.messages.begin();
         msg_it != queue.messages.end();) {
      if (msg_it->expires_at <= now) {
        remove_ids.push_back(msg_it->message_id);
        queue.by_id.erase(msg_it->message_id);
        msg_it = queue.messages.erase(msg_it);
        continue;
      }
      if (msg_it->msg.kind == QueueMessageKind::kGroupCipher) {
        remove_ids.push_back(msg_it->message_id);
        out.push_back(std::move(msg_it->msg));
        queue.by_id.erase(msg_it->message_id);
        msg_it = queue.messages.erase(msg_it);
        continue;
      }
      ++msg_it;
    }
    if (queue.messages.empty()) {
      shard.recipients.erase(it);
    }
  }
  for (const auto id : remove_ids) {
    DeleteMessageFile(recipient, id);
  }
  if (state_store_ && store_ready) {
    (void)SaveToStoreLockedUnlocked();
  }
  return out;
}

std::vector<OfflineMessage> OfflineQueue::DrainGroupNotice(
    const std::string& recipient) {
  std::vector<OfflineMessage> out;
  bool store_ready = false;
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "offline_queue",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && store_lock.locked()) {
    store_ready = LoadFromStoreLocked();
  }
  const auto now = std::chrono::steady_clock::now();
  std::vector<std::uint64_t> remove_ids;
  auto& shard = shards_[ShardIndexFor(recipient)];
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    CleanupExpiredLocked(shard, now);

    auto it = shard.recipients.find(recipient);
    if (it == shard.recipients.end()) {
      return out;
    }
    auto& queue = it->second;
    out.reserve(queue.messages.size());
    for (auto msg_it = queue.messages.begin();
         msg_it != queue.messages.end();) {
      if (msg_it->expires_at <= now) {
        remove_ids.push_back(msg_it->message_id);
        queue.by_id.erase(msg_it->message_id);
        msg_it = queue.messages.erase(msg_it);
        continue;
      }
      if (msg_it->msg.kind == QueueMessageKind::kGroupNotice) {
        remove_ids.push_back(msg_it->message_id);
        out.push_back(std::move(msg_it->msg));
        queue.by_id.erase(msg_it->message_id);
        msg_it = queue.messages.erase(msg_it);
        continue;
      }
      ++msg_it;
    }
    if (queue.messages.empty()) {
      shard.recipients.erase(it);
    }
  }
  for (const auto id : remove_ids) {
    DeleteMessageFile(recipient, id);
  }
  if (state_store_ && store_ready) {
    (void)SaveToStoreLockedUnlocked();
  }
  return out;
}

std::vector<std::vector<std::uint8_t>> OfflineQueue::DrainDeviceSync(
    const std::string& recipient) {
  std::vector<std::vector<std::uint8_t>> out;
  bool store_ready = false;
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "offline_queue",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && store_lock.locked()) {
    store_ready = LoadFromStoreLocked();
  }
  const auto now = std::chrono::steady_clock::now();
  std::vector<std::uint64_t> remove_ids;
  auto& shard = shards_[ShardIndexFor(recipient)];
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    CleanupExpiredLocked(shard, now);

    auto it = shard.recipients.find(recipient);
    if (it == shard.recipients.end()) {
      return out;
    }
    auto& queue = it->second;
    out.reserve(queue.messages.size());
    for (auto msg_it = queue.messages.begin();
         msg_it != queue.messages.end();) {
      if (msg_it->expires_at <= now) {
        remove_ids.push_back(msg_it->message_id);
        queue.by_id.erase(msg_it->message_id);
        msg_it = queue.messages.erase(msg_it);
        continue;
      }
      if (msg_it->msg.kind == QueueMessageKind::kDeviceSync) {
        remove_ids.push_back(msg_it->message_id);
        out.push_back(std::move(msg_it->msg.payload));
        queue.by_id.erase(msg_it->message_id);
        msg_it = queue.messages.erase(msg_it);
        continue;
      }
      ++msg_it;
    }
    if (queue.messages.empty()) {
      shard.recipients.erase(it);
    }
  }
  for (const auto id : remove_ids) {
    DeleteMessageFile(recipient, id);
  }
  if (state_store_ && store_ready) {
    (void)SaveToStoreLockedUnlocked();
  }
  return out;
}

OfflineQueueStats OfflineQueue::GetStats() {
  OfflineQueueStats stats;
  if (state_store_) {
    (void)LoadFromStore();
  }
  for (const auto& shard : shards_) {
    std::lock_guard<std::mutex> lock(shard.mutex);
    stats.recipients += static_cast<std::uint64_t>(shard.recipients.size());
    for (const auto& entry : shard.recipients) {
      for (const auto& stored : entry.second.messages) {
        stats.messages++;
        stats.bytes +=
            static_cast<std::uint64_t>(stored.msg.payload.size());
        switch (stored.msg.kind) {
          case QueueMessageKind::kGeneric:
            stats.generic_messages++;
            break;
          case QueueMessageKind::kPrivate:
            stats.private_messages++;
            break;
          case QueueMessageKind::kGroupCipher:
            stats.group_cipher_messages++;
            break;
          case QueueMessageKind::kDeviceSync:
            stats.device_sync_messages++;
            break;
          case QueueMessageKind::kGroupNotice:
            stats.group_notice_messages++;
            break;
        }
      }
    }
  }
  return stats;
}

void OfflineQueue::CleanupExpired() {
  if (state_store_) {
    std::string lock_err;
    StateStoreLock store_lock(state_store_, "offline_queue",
                              std::chrono::milliseconds(5000), lock_err);
    if (store_lock.locked()) {
      if (LoadFromStoreLocked()) {
        (void)SaveToStoreLockedUnlocked();
      }
    }
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  for (auto& shard : shards_) {
    std::lock_guard<std::mutex> lock(shard.mutex);
    CleanupExpiredLocked(shard, now);
  }
}

}  // namespace mi::server
