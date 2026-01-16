#ifndef MI_E2EE_SERVER_OFFLINE_STORAGE_H
#define MI_E2EE_SERVER_OFFLINE_STORAGE_H

#include <array>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mi::server {

enum class QueueMessageKind : std::uint8_t {
  kGeneric = 0,
  kPrivate = 1,
  kGroupCipher = 2,
  kDeviceSync = 3,
  kGroupNotice = 4
};

struct StoredFileMeta {
  std::string id;
  std::string owner;
  std::uint64_t size{0};
  std::chrono::steady_clock::time_point created_at{};
};

struct PutResult {
  bool success{false};
  std::string file_id;
  std::array<std::uint8_t, 32> file_key{};
  StoredFileMeta meta;
  std::string error;
};

struct PutBlobResult {
  bool success{false};
  std::string file_id;
  StoredFileMeta meta;
  std::string error;
};

struct BlobUploadStartResult {
  bool success{false};
  std::string file_id;
  std::string upload_id;
  std::string error;
};

struct BlobUploadChunkResult {
  bool success{false};
  std::uint64_t bytes_received{0};
  std::string error;
};

struct BlobUploadFinishResult {
  bool success{false};
  StoredFileMeta meta;
  std::string error;
};

struct BlobDownloadStartResult {
  bool success{false};
  std::string download_id;
  StoredFileMeta meta;
  std::string error;
};

struct BlobDownloadChunkResult {
  BlobDownloadChunkResult() = default;
  BlobDownloadChunkResult(const BlobDownloadChunkResult&) = default;
  BlobDownloadChunkResult& operator=(const BlobDownloadChunkResult&) = default;
  BlobDownloadChunkResult(BlobDownloadChunkResult&&) noexcept = default;
  BlobDownloadChunkResult& operator=(BlobDownloadChunkResult&&) noexcept = default;
  ~BlobDownloadChunkResult();

  bool success{false};
  std::uint64_t offset{0};
  bool eof{false};
  std::vector<std::uint8_t> chunk;
  std::string error;
};

struct OfflineStorageStats {
  std::uint64_t files{0};
  std::uint64_t bytes{0};
};

struct SecureDeleteConfig {
  bool enabled{false};
  std::filesystem::path plugin_path;
};

class OfflineStorage {
 public:
  OfflineStorage(std::filesystem::path base_dir,
                 std::chrono::seconds ttl = std::chrono::hours(12),
                 SecureDeleteConfig secure_delete = {});
  ~OfflineStorage();

  PutResult Put(const std::string& owner,
                const std::vector<std::uint8_t>& plaintext);

  PutBlobResult PutBlob(const std::string& owner,
                        const std::vector<std::uint8_t>& blob);

  BlobUploadStartResult BeginBlobUpload(const std::string& owner,
                                        std::uint64_t expected_size = 0);

  BlobUploadChunkResult AppendBlobUploadChunk(
      const std::string& owner, const std::string& file_id,
      const std::string& upload_id, std::uint64_t offset,
      const std::vector<std::uint8_t>& chunk);

  BlobUploadFinishResult FinishBlobUpload(const std::string& owner,
                                          const std::string& file_id,
                                          const std::string& upload_id,
                                          std::uint64_t total_size);

  BlobDownloadStartResult BeginBlobDownload(const std::string& owner,
                                            const std::string& file_id,
                                            bool wipe_after_read);

  BlobDownloadChunkResult ReadBlobDownloadChunk(
      const std::string& owner, const std::string& file_id,
      const std::string& download_id, std::uint64_t offset,
      std::uint32_t max_len);

  std::optional<std::vector<std::uint8_t>> Fetch(
      const std::string& file_id, const std::array<std::uint8_t, 32>& file_key,
      bool wipe_after_read, std::string& error);

  std::optional<std::vector<std::uint8_t>> FetchBlob(
      const std::string& file_id, bool wipe_after_read, std::string& error);

  std::optional<StoredFileMeta> Meta(const std::string& file_id) const;

  OfflineStorageStats GetStats() const;

  void CleanupExpired();
  bool SecureDeleteReady() const { return secure_delete_ready_; }
  const std::string& SecureDeleteError() const { return secure_delete_error_; }

 private:
  using SecureDeleteFn = int (*)(const char*);

  std::filesystem::path ResolvePath(const std::string& file_id) const;
  std::filesystem::path ResolveUploadTempPath(const std::string& file_id) const;
  std::filesystem::path ResolveKeyPath(const std::string& file_id) const;
  std::optional<std::filesystem::path> ResolveKeyPathForData(
      const std::filesystem::path& data_path) const;
  std::string GenerateId() const;
  std::array<std::uint8_t, 32> GenerateKey() const;
  std::string GenerateSessionId() const;
  bool SaveEraseKey(const std::filesystem::path& data_path,
                    const std::array<std::uint8_t, 32>& erase_key,
                    std::string& error) const;
  bool LoadEraseKey(const std::filesystem::path& data_path,
                    std::array<std::uint8_t, 32>& erase_key,
                    std::string& error) const;
  std::array<std::uint8_t, 32> DeriveStorageKey(
      const std::array<std::uint8_t, 32>& file_key,
      const std::array<std::uint8_t, 32>& erase_key) const;
  bool EncryptAead(const std::vector<std::uint8_t>& plaintext,
                   const std::array<std::uint8_t, 32>& key,
                   const std::array<std::uint8_t, 24>& nonce,
                   const std::uint8_t* ad, std::size_t ad_len,
                   std::vector<std::uint8_t>& cipher,
                   std::array<std::uint8_t, 16>& mac) const;
  bool DecryptAead(const std::vector<std::uint8_t>& cipher,
                   const std::array<std::uint8_t, 32>& key,
                   const std::array<std::uint8_t, 24>& nonce,
                   const std::uint8_t* ad, std::size_t ad_len,
                   const std::array<std::uint8_t, 16>& mac,
                   std::vector<std::uint8_t>& plaintext) const;
  bool EncryptLegacy(const std::vector<std::uint8_t>& plaintext,
                     const std::array<std::uint8_t, 32>& key,
                     const std::array<std::uint8_t, 16>& nonce,
                     std::vector<std::uint8_t>& cipher,
                     std::array<std::uint8_t, 32>& tag) const;
  bool DecryptLegacy(const std::vector<std::uint8_t>& cipher,
                     const std::array<std::uint8_t, 32>& key,
                     const std::array<std::uint8_t, 16>& nonce,
                     const std::array<std::uint8_t, 32>& tag,
                     std::vector<std::uint8_t>& plaintext) const;
  bool LoadSecureDeletePlugin(const std::filesystem::path& path,
                              std::string& error);
  bool CallSecureDeletePlugin(const std::filesystem::path& path) const;
  void BestEffortWipe(const std::filesystem::path& path) const;
  void WipeFile(const std::filesystem::path& path) const;

  std::filesystem::path base_dir_;
  std::chrono::seconds ttl_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, StoredFileMeta> metadata_;
  SecureDeleteConfig secure_delete_{};
  void* secure_delete_handle_{nullptr};
  SecureDeleteFn secure_delete_fn_{nullptr};
  bool secure_delete_ready_{false};
  std::string secure_delete_error_;
  struct BlobUploadSession {
    std::string upload_id;
    std::string owner;
    std::uint64_t expected_size{0};
    std::uint64_t bytes_received{0};
    std::filesystem::path temp_path;
    std::chrono::steady_clock::time_point created_at{};
    std::chrono::steady_clock::time_point last_activity{};
  };

  struct BlobDownloadSession {
    std::string download_id;
    std::string file_id;
    std::string owner;
    std::uint64_t total_size{0};
    std::uint64_t next_offset{0};
    bool wipe_after_read{false};
    std::chrono::steady_clock::time_point created_at{};
    std::chrono::steady_clock::time_point last_activity{};
  };
  std::unordered_map<std::string, BlobUploadSession> blob_uploads_;
  std::unordered_map<std::string, BlobDownloadSession> blob_downloads_;
};

struct OfflineMessage {
  QueueMessageKind kind{QueueMessageKind::kGeneric};
  std::string sender;
  std::string recipient;
  std::string group_id;
  std::vector<std::uint8_t> payload;
  std::chrono::steady_clock::time_point created_at{};
  std::chrono::seconds ttl{std::chrono::hours(24)};
};

struct OfflineQueueStats {
  std::uint64_t recipients{0};
  std::uint64_t messages{0};
  std::uint64_t bytes{0};
  std::uint64_t generic_messages{0};
  std::uint64_t private_messages{0};
  std::uint64_t group_cipher_messages{0};
  std::uint64_t device_sync_messages{0};
  std::uint64_t group_notice_messages{0};
};

class OfflineQueue {
 public:
  explicit OfflineQueue(std::chrono::seconds default_ttl =
                            std::chrono::hours(24));

  void Enqueue(const std::string& recipient,
               std::vector<std::uint8_t> payload,
               std::chrono::seconds ttl = std::chrono::seconds::zero());

  void EnqueuePrivate(const std::string& recipient, const std::string& sender,
                      std::vector<std::uint8_t> payload,
                      std::chrono::seconds ttl = std::chrono::seconds::zero());

  std::vector<std::vector<std::uint8_t>> Drain(const std::string& recipient);

  std::vector<OfflineMessage> DrainPrivate(const std::string& recipient);

  void EnqueueGroupCipher(const std::string& recipient, const std::string& group_id,
                          const std::string& sender,
                          std::vector<std::uint8_t> payload,
                          std::chrono::seconds ttl = std::chrono::seconds::zero());

  std::vector<OfflineMessage> DrainGroupCipher(const std::string& recipient);

  void EnqueueGroupNotice(const std::string& recipient, const std::string& group_id,
                          const std::string& sender,
                          std::vector<std::uint8_t> payload,
                          std::chrono::seconds ttl = std::chrono::seconds::zero());

  std::vector<OfflineMessage> DrainGroupNotice(const std::string& recipient);

  void EnqueueDeviceSync(const std::string& recipient,
                         std::vector<std::uint8_t> payload,
                         std::chrono::seconds ttl = std::chrono::seconds::zero());

  std::vector<std::vector<std::uint8_t>> DrainDeviceSync(
      const std::string& recipient);

  OfflineQueueStats GetStats() const;

  void CleanupExpired();

 private:
  struct StoredMessage {
    OfflineMessage msg;
    std::uint64_t message_id{0};
    std::chrono::steady_clock::time_point expires_at{};
  };

  struct ExpiryItem {
    std::chrono::steady_clock::time_point expires_at{};
    std::string recipient;
    std::uint64_t message_id{0};
  };

  struct ExpiryItemCompare {
    bool operator()(const ExpiryItem& a, const ExpiryItem& b) const {
      return a.expires_at > b.expires_at;
    }
  };

  struct RecipientQueue {
    std::list<StoredMessage> messages;
    std::unordered_map<std::uint64_t, std::list<StoredMessage>::iterator> by_id;
  };

  struct Shard {
    mutable std::mutex mutex;
    std::unordered_map<std::string, RecipientQueue> recipients;
    std::priority_queue<ExpiryItem, std::vector<ExpiryItem>, ExpiryItemCompare>
        expiries;
    std::uint64_t next_id{1};
  };

  static constexpr std::size_t kShardCount = 16;

  std::size_t ShardIndexFor(const std::string& recipient) const;
  void CleanupExpiredLocked(Shard& shard,
                            std::chrono::steady_clock::time_point now);

  std::chrono::seconds default_ttl_;
  std::array<Shard, kShardCount> shards_{};
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_OFFLINE_STORAGE_H
