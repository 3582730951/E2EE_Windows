#include "offline_storage.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>

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
#include "monocypher.h"

namespace mi::server {

namespace {

constexpr std::uint64_t kMaxBlobBytes = 320u * 1024u * 1024u;
constexpr std::uint32_t kMaxBlobChunkBytes = 4u * 1024u * 1024u;
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

mi::shard::ByteBufferPool& OfflineStorageBufferPool() {
  static mi::shard::ByteBufferPool pool(32, 16u * 1024u * 1024u);
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

OfflineStorage::OfflineStorage(std::filesystem::path base_dir,
                               std::chrono::seconds ttl,
                               SecureDeleteConfig secure_delete)
    : base_dir_(std::move(base_dir)),
      ttl_(ttl),
      secure_delete_(std::move(secure_delete)) {
  std::error_code ec;
  std::filesystem::create_directories(base_dir_, ec);
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
  std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
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
  mi::shard::ScopedBuffer cipher_buf(pool, chunk_bytes, false);
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
      WipeFile(path);
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

  {
    std::lock_guard<std::mutex> lock(mutex_);
    metadata_[id] = meta;
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
  std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    result.error = "open file failed";
    return result;
  }
  ofs.write(reinterpret_cast<const char*>(blob.data()),
            static_cast<std::streamsize>(blob.size()));
  ofs.close();

  StoredFileMeta meta;
  meta.id = id;
  meta.owner = owner;
  meta.size = static_cast<std::uint64_t>(blob.size());
  meta.created_at = std::chrono::steady_clock::now();

  {
    std::lock_guard<std::mutex> lock(mutex_);
    metadata_[id] = meta;
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

  const std::string file_id = GenerateId();
  const std::string upload_id = GenerateSessionId();
  const auto temp_path = ResolveUploadTempPath(file_id);

  std::ofstream ofs(temp_path, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    result.error = "open file failed";
    return result;
  }
  ofs.close();

  BlobUploadSession sess;
  sess.upload_id = upload_id;
  sess.owner = owner;
  sess.expected_size = expected_size;
  sess.bytes_received = 0;
  sess.temp_path = temp_path;
  sess.created_at = std::chrono::steady_clock::now();
  sess.last_activity = sess.created_at;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (metadata_.find(file_id) != metadata_.end() ||
        blob_uploads_.find(file_id) != blob_uploads_.end()) {
      result.error = "id collision";
      return result;
    }
    blob_uploads_[file_id] = std::move(sess);
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

  std::filesystem::path temp_path;
  std::uint64_t expected = 0;
  std::uint64_t received = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = blob_uploads_.find(file_id);
    if (it == blob_uploads_.end()) {
      result.error = "upload session not found";
      return result;
    }
    if (it->second.upload_id != upload_id || it->second.owner != owner) {
      result.error = "unauthorized";
      return result;
    }
    if (offset != it->second.bytes_received) {
      result.error = "invalid offset";
      return result;
    }
    if (it->second.expected_size > 0) {
      expected = it->second.expected_size;
    }
    if (it->second.bytes_received + chunk.size() > kMaxBlobBytes) {
      result.error = "payload too large";
      return result;
    }
    if (expected > 0 &&
        it->second.bytes_received + chunk.size() > expected) {
      result.error = "payload too large";
      return result;
    }
    temp_path = it->second.temp_path;
    received = it->second.bytes_received;
  }

  std::ofstream ofs(temp_path, std::ios::binary | std::ios::app);
  if (!ofs) {
    result.error = "open file failed";
    return result;
  }
  ofs.write(reinterpret_cast<const char*>(chunk.data()),
            static_cast<std::streamsize>(chunk.size()));
  ofs.close();
  if (!ofs.good()) {
    result.error = "write failed";
    return result;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = blob_uploads_.find(file_id);
    if (it == blob_uploads_.end()) {
      result.error = "upload session not found";
      return result;
    }
    if (it->second.upload_id != upload_id || it->second.owner != owner) {
      result.error = "unauthorized";
      return result;
    }
    it->second.bytes_received += static_cast<std::uint64_t>(chunk.size());
    it->second.last_activity = std::chrono::steady_clock::now();
    received = it->second.bytes_received;
  }

  result.success = true;
  result.bytes_received = received;
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

  BlobUploadSession sess;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = blob_uploads_.find(file_id);
    if (it == blob_uploads_.end()) {
      result.error = "upload session not found";
      return result;
    }
    if (it->second.upload_id != upload_id || it->second.owner != owner) {
      result.error = "unauthorized";
      return result;
    }
    if (it->second.bytes_received != total_size) {
      result.error = "size mismatch";
      return result;
    }
    sess = it->second;
    blob_uploads_.erase(it);
  }

  const auto final_path = ResolvePath(file_id);
  std::error_code ec;
  std::filesystem::rename(sess.temp_path, final_path, ec);
  if (ec) {
    result.error = "finalize failed";
    return result;
  }

  StoredFileMeta meta;
  meta.id = file_id;
  meta.owner = owner;
  meta.size = total_size;
  meta.created_at = sess.created_at;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    metadata_[file_id] = meta;
  }

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

  const std::string download_id = GenerateSessionId();
  BlobDownloadSession sess;
  sess.download_id = download_id;
  sess.file_id = file_id;
  sess.owner = owner;
  sess.total_size = size;
  sess.next_offset = 0;
  sess.wipe_after_read = wipe_after_read;
  sess.created_at = std::chrono::steady_clock::now();
  sess.last_activity = sess.created_at;

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
    blob_downloads_[download_id] = std::move(sess);
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

  BlobDownloadSession sess;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = blob_downloads_.find(download_id);
    if (it == blob_downloads_.end()) {
      result.error = "download session not found";
      return result;
    }
    if (it->second.owner != owner || it->second.file_id != file_id) {
      result.error = "unauthorized";
      return result;
    }
    if (offset != it->second.next_offset) {
      result.error = "invalid offset";
      return result;
    }
    sess = it->second;
  }

  if (sess.total_size == 0 || offset >= sess.total_size) {
    result.error = "invalid offset";
    return result;
  }

  const auto path = ResolvePath(file_id);
  std::vector<std::uint8_t> buf;
  {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
      result.error = "file not found";
      return result;
    }
    ifs.seekg(static_cast<std::streamoff>(offset));
    const std::uint64_t remaining64 = sess.total_size - offset;
    const std::size_t to_read =
        static_cast<std::size_t>(std::min<std::uint64_t>(remaining64, max_len));
    buf.resize(to_read);
    ifs.read(reinterpret_cast<char*>(buf.data()),
             static_cast<std::streamsize>(buf.size()));
    if (!ifs) {
      result.error = "read failed";
      return result;
    }
  }

  const std::uint64_t next_off = offset + static_cast<std::uint64_t>(buf.size());
  const bool eof = (next_off >= sess.total_size);

  bool wipe = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = blob_downloads_.find(download_id);
    if (it == blob_downloads_.end()) {
      result.error = "download session not found";
      return result;
    }
    if (it->second.owner != owner || it->second.file_id != file_id) {
      result.error = "unauthorized";
      return result;
    }
    it->second.next_offset = next_off;
    it->second.last_activity = std::chrono::steady_clock::now();
    if (eof) {
      wipe = it->second.wipe_after_read;
      blob_downloads_.erase(it);
      if (wipe) {
        metadata_.erase(file_id);
      }
    }
  }
  if (wipe) {
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
      mi::shard::ScopedBuffer cipher_buf(pool, chunk_bytes, false);
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
      mi::shard::ScopedBuffer content_buf(
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

      mi::shard::ScopedBuffer cipher_buf(pool, cipher_len, false);
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
    mi::shard::ScopedBuffer content_buf(
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

    mi::shard::ScopedBuffer cipher_buf(pool, cipher_len, false);
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
    metadata_.erase(file_id);
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
  const auto path = ResolvePath(file_id);
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    error = "file not found";
    return std::nullopt;
  }
  std::vector<std::uint8_t> content((std::istreambuf_iterator<char>(ifs)),
                                    std::istreambuf_iterator<char>());
  ifs.close();
  if (content.empty()) {
    error = "empty file";
    return std::nullopt;
  }

  if (wipe_after_read) {
    WipeFile(path);
    std::lock_guard<std::mutex> lock(mutex_);
    metadata_.erase(file_id);
  }

  error.clear();
  return content;
}

std::optional<StoredFileMeta> OfflineStorage::Meta(
    const std::string& file_id) const {
  if (!IsValidFileId(file_id)) {
    return std::nullopt;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = metadata_.find(file_id);
  if (it == metadata_.end()) {
    return std::nullopt;
  }
  return it->second;
}

OfflineStorageStats OfflineStorage::GetStats() const {
  OfflineStorageStats stats;
  std::lock_guard<std::mutex> lock(mutex_);
  stats.files = static_cast<std::uint64_t>(metadata_.size());
  for (const auto& kv : metadata_) {
    stats.bytes += kv.second.size;
  }
  return stats;
}

void OfflineStorage::CleanupExpired() {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = metadata_.begin(); it != metadata_.end();) {
    if (now - it->second.created_at > ttl_) {
      const auto path = ResolvePath(it->first);
      WipeFile(path);
      it = metadata_.erase(it);
    } else {
      ++it;
    }
  }

  const auto sess_ttl = std::chrono::minutes(15);
  for (auto it = blob_uploads_.begin(); it != blob_uploads_.end();) {
    if (now - it->second.last_activity > sess_ttl) {
      WipeFile(it->second.temp_path);
      it = blob_uploads_.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = blob_downloads_.begin(); it != blob_downloads_.end();) {
    if (now - it->second.last_activity > sess_ttl) {
      it = blob_downloads_.erase(it);
    } else {
      ++it;
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
  const auto tmp = key_path->string() + ".tmp";
  {
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      error = "key write failed";
      return false;
    }
    ofs.write(reinterpret_cast<const char*>(erase_key.data()),
              static_cast<std::streamsize>(erase_key.size()));
    if (!ofs) {
      error = "key write failed";
      return false;
    }
  }
  std::error_code ec;
  std::filesystem::rename(tmp, *key_path, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    error = "key write failed";
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
  const std::size_t wipe_len = size < 16 ? static_cast<std::size_t>(size) : 16;
  const std::vector<std::uint8_t> ff(wipe_len, 0xFF);
  fs.seekp(0);
  fs.write(reinterpret_cast<const char*>(ff.data()),
           static_cast<std::streamsize>(wipe_len));
  if (size > wipe_len) {
    const std::size_t mid = static_cast<std::size_t>(size / 2);
    fs.seekp(static_cast<std::streamoff>(mid));
    fs.write(reinterpret_cast<const char*>(ff.data()),
             static_cast<std::streamsize>(
                 std::min(wipe_len, static_cast<std::size_t>(size - mid))));
    if (size > wipe_len * 2) {
      const auto tail_pos =
          static_cast<std::streamoff>(size > wipe_len ? size - wipe_len : 0);
      fs.seekp(tail_pos);
      fs.write(reinterpret_cast<const char*>(ff.data()),
               static_cast<std::streamsize>(wipe_len));
    }
  }
  fs.flush();
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
  if (!CallSecureDeletePlugin(path)) {
    BestEffortWipe(path);
  } else {
    std::filesystem::remove(path, ec);
  }
}

OfflineQueue::OfflineQueue(std::chrono::seconds default_ttl)
    : default_ttl_(default_ttl == std::chrono::seconds::zero()
                       ? std::chrono::hours(24)
                       : default_ttl) {}

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
    if (queue.messages.empty()) {
      shard.recipients.erase(rit);
    }
  }
}

void OfflineQueue::Enqueue(const std::string& recipient,
                           std::vector<std::uint8_t> payload,
                           std::chrono::seconds ttl) {
  const auto now = std::chrono::steady_clock::now();
  StoredMessage stored;
  stored.msg.kind = QueueMessageKind::kGeneric;
  stored.msg.recipient = recipient;
  stored.msg.payload = std::move(payload);
  stored.msg.created_at = now;
  stored.msg.ttl = (ttl == std::chrono::seconds::zero()) ? default_ttl_ : ttl;
  stored.expires_at = stored.msg.created_at + stored.msg.ttl;

  auto& shard = shards_[ShardIndexFor(recipient)];
  std::lock_guard<std::mutex> lock(shard.mutex);
  CleanupExpiredLocked(shard, now);
  stored.message_id = shard.next_id++;
  auto& queue = shard.recipients[recipient];
  queue.messages.push_back(std::move(stored));
  const auto it = std::prev(queue.messages.end());
  queue.by_id.emplace(it->message_id, it);
  shard.expiries.push(ExpiryItem{it->expires_at, recipient, it->message_id});
}

void OfflineQueue::EnqueuePrivate(const std::string& recipient,
                                  const std::string& sender,
                                  std::vector<std::uint8_t> payload,
                                  std::chrono::seconds ttl) {
  const auto now = std::chrono::steady_clock::now();
  StoredMessage stored;
  stored.msg.kind = QueueMessageKind::kPrivate;
  stored.msg.sender = sender;
  stored.msg.recipient = recipient;
  stored.msg.payload = std::move(payload);
  stored.msg.created_at = now;
  stored.msg.ttl = (ttl == std::chrono::seconds::zero()) ? default_ttl_ : ttl;
  stored.expires_at = stored.msg.created_at + stored.msg.ttl;

  auto& shard = shards_[ShardIndexFor(recipient)];
  std::lock_guard<std::mutex> lock(shard.mutex);
  CleanupExpiredLocked(shard, now);
  stored.message_id = shard.next_id++;
  auto& queue = shard.recipients[recipient];
  queue.messages.push_back(std::move(stored));
  const auto it = std::prev(queue.messages.end());
  queue.by_id.emplace(it->message_id, it);
  shard.expiries.push(ExpiryItem{it->expires_at, recipient, it->message_id});
}

void OfflineQueue::EnqueueGroupCipher(const std::string& recipient,
                                      const std::string& group_id,
                                      const std::string& sender,
                                      std::vector<std::uint8_t> payload,
                                      std::chrono::seconds ttl) {
  const auto now = std::chrono::steady_clock::now();
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
  std::lock_guard<std::mutex> lock(shard.mutex);
  CleanupExpiredLocked(shard, now);
  stored.message_id = shard.next_id++;
  auto& queue = shard.recipients[recipient];
  queue.messages.push_back(std::move(stored));
  const auto it = std::prev(queue.messages.end());
  queue.by_id.emplace(it->message_id, it);
  shard.expiries.push(ExpiryItem{it->expires_at, recipient, it->message_id});
}

void OfflineQueue::EnqueueGroupNotice(const std::string& recipient,
                                      const std::string& group_id,
                                      const std::string& sender,
                                      std::vector<std::uint8_t> payload,
                                      std::chrono::seconds ttl) {
  const auto now = std::chrono::steady_clock::now();
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
  std::lock_guard<std::mutex> lock(shard.mutex);
  CleanupExpiredLocked(shard, now);
  stored.message_id = shard.next_id++;
  auto& queue = shard.recipients[recipient];
  queue.messages.push_back(std::move(stored));
  const auto it = std::prev(queue.messages.end());
  queue.by_id.emplace(it->message_id, it);
  shard.expiries.push(ExpiryItem{it->expires_at, recipient, it->message_id});
}

void OfflineQueue::EnqueueDeviceSync(const std::string& recipient,
                                     std::vector<std::uint8_t> payload,
                                     std::chrono::seconds ttl) {
  const auto now = std::chrono::steady_clock::now();
  StoredMessage stored;
  stored.msg.kind = QueueMessageKind::kDeviceSync;
  stored.msg.recipient = recipient;
  stored.msg.payload = std::move(payload);
  stored.msg.created_at = now;
  stored.msg.ttl = (ttl == std::chrono::seconds::zero()) ? default_ttl_ : ttl;
  stored.expires_at = stored.msg.created_at + stored.msg.ttl;

  auto& shard = shards_[ShardIndexFor(recipient)];
  std::lock_guard<std::mutex> lock(shard.mutex);
  CleanupExpiredLocked(shard, now);
  stored.message_id = shard.next_id++;
  auto& queue = shard.recipients[recipient];
  queue.messages.push_back(std::move(stored));
  const auto it = std::prev(queue.messages.end());
  queue.by_id.emplace(it->message_id, it);
  shard.expiries.push(ExpiryItem{it->expires_at, recipient, it->message_id});
}

std::vector<std::vector<std::uint8_t>> OfflineQueue::Drain(
    const std::string& recipient) {
  std::vector<std::vector<std::uint8_t>> out;
  const auto now = std::chrono::steady_clock::now();
  auto& shard = shards_[ShardIndexFor(recipient)];
  std::lock_guard<std::mutex> lock(shard.mutex);
  CleanupExpiredLocked(shard, now);

  auto it = shard.recipients.find(recipient);
  if (it == shard.recipients.end()) {
    return out;
  }
  auto& queue = it->second;
  out.reserve(queue.messages.size());
  for (auto msg_it = queue.messages.begin(); msg_it != queue.messages.end();) {
    if (msg_it->expires_at <= now) {
      queue.by_id.erase(msg_it->message_id);
      msg_it = queue.messages.erase(msg_it);
      continue;
    }
    if (msg_it->msg.kind == QueueMessageKind::kGeneric) {
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
  return out;
}

std::vector<OfflineMessage> OfflineQueue::DrainPrivate(
    const std::string& recipient) {
  std::vector<OfflineMessage> out;
  const auto now = std::chrono::steady_clock::now();
  auto& shard = shards_[ShardIndexFor(recipient)];
  std::lock_guard<std::mutex> lock(shard.mutex);
  CleanupExpiredLocked(shard, now);

  auto it = shard.recipients.find(recipient);
  if (it == shard.recipients.end()) {
    return out;
  }
  auto& queue = it->second;
  out.reserve(queue.messages.size());
  for (auto msg_it = queue.messages.begin(); msg_it != queue.messages.end();) {
    if (msg_it->expires_at <= now) {
      queue.by_id.erase(msg_it->message_id);
      msg_it = queue.messages.erase(msg_it);
      continue;
    }
    if (msg_it->msg.kind == QueueMessageKind::kPrivate) {
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
  return out;
}

std::vector<OfflineMessage> OfflineQueue::DrainGroupCipher(
    const std::string& recipient) {
  std::vector<OfflineMessage> out;
  const auto now = std::chrono::steady_clock::now();
  auto& shard = shards_[ShardIndexFor(recipient)];
  std::lock_guard<std::mutex> lock(shard.mutex);
  CleanupExpiredLocked(shard, now);

  auto it = shard.recipients.find(recipient);
  if (it == shard.recipients.end()) {
    return out;
  }
  auto& queue = it->second;
  out.reserve(queue.messages.size());
  for (auto msg_it = queue.messages.begin(); msg_it != queue.messages.end();) {
    if (msg_it->expires_at <= now) {
      queue.by_id.erase(msg_it->message_id);
      msg_it = queue.messages.erase(msg_it);
      continue;
    }
    if (msg_it->msg.kind == QueueMessageKind::kGroupCipher) {
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
  return out;
}

std::vector<OfflineMessage> OfflineQueue::DrainGroupNotice(
    const std::string& recipient) {
  std::vector<OfflineMessage> out;
  const auto now = std::chrono::steady_clock::now();
  auto& shard = shards_[ShardIndexFor(recipient)];
  std::lock_guard<std::mutex> lock(shard.mutex);
  CleanupExpiredLocked(shard, now);

  auto it = shard.recipients.find(recipient);
  if (it == shard.recipients.end()) {
    return out;
  }
  auto& queue = it->second;
  out.reserve(queue.messages.size());
  for (auto msg_it = queue.messages.begin(); msg_it != queue.messages.end();) {
    if (msg_it->expires_at <= now) {
      queue.by_id.erase(msg_it->message_id);
      msg_it = queue.messages.erase(msg_it);
      continue;
    }
    if (msg_it->msg.kind == QueueMessageKind::kGroupNotice) {
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
  return out;
}

std::vector<std::vector<std::uint8_t>> OfflineQueue::DrainDeviceSync(
    const std::string& recipient) {
  std::vector<std::vector<std::uint8_t>> out;
  const auto now = std::chrono::steady_clock::now();
  auto& shard = shards_[ShardIndexFor(recipient)];
  std::lock_guard<std::mutex> lock(shard.mutex);
  CleanupExpiredLocked(shard, now);

  auto it = shard.recipients.find(recipient);
  if (it == shard.recipients.end()) {
    return out;
  }
  auto& queue = it->second;
  out.reserve(queue.messages.size());
  for (auto msg_it = queue.messages.begin(); msg_it != queue.messages.end();) {
    if (msg_it->expires_at <= now) {
      queue.by_id.erase(msg_it->message_id);
      msg_it = queue.messages.erase(msg_it);
      continue;
    }
    if (msg_it->msg.kind == QueueMessageKind::kDeviceSync) {
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
  return out;
}

OfflineQueueStats OfflineQueue::GetStats() const {
  OfflineQueueStats stats;
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
  const auto now = std::chrono::steady_clock::now();
  for (auto& shard : shards_) {
    std::lock_guard<std::mutex> lock(shard.mutex);
    CleanupExpiredLocked(shard, now);
  }
}

}  // namespace mi::server
