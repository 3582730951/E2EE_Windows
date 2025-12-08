#ifndef MI_E2EE_SERVER_OFFLINE_STORAGE_H
#define MI_E2EE_SERVER_OFFLINE_STORAGE_H

#include <array>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mi::server {

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

class OfflineStorage {
 public:
  OfflineStorage(std::filesystem::path base_dir,
                 std::chrono::seconds ttl = std::chrono::hours(12));

  PutResult Put(const std::string& owner,
                const std::vector<std::uint8_t>& plaintext);

  std::optional<std::vector<std::uint8_t>> Fetch(
      const std::string& file_id, const std::array<std::uint8_t, 32>& file_key,
      bool wipe_after_read, std::string& error);

  std::optional<StoredFileMeta> Meta(const std::string& file_id) const;

  void CleanupExpired();

 private:
  std::filesystem::path ResolvePath(const std::string& file_id) const;
  std::string GenerateId() const;
  std::array<std::uint8_t, 32> GenerateKey() const;
  bool Encrypt(const std::vector<std::uint8_t>& plaintext,
               const std::array<std::uint8_t, 32>& key,
               const std::array<std::uint8_t, 16>& nonce,
               std::vector<std::uint8_t>& cipher,
               std::array<std::uint8_t, 32>& tag) const;
  bool Decrypt(const std::vector<std::uint8_t>& cipher,
               const std::array<std::uint8_t, 32>& key,
               const std::array<std::uint8_t, 16>& nonce,
               const std::array<std::uint8_t, 32>& tag,
               std::vector<std::uint8_t>& plaintext) const;
  void WipeFile(const std::filesystem::path& path) const;

  std::filesystem::path base_dir_;
  std::chrono::seconds ttl_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, StoredFileMeta> metadata_;
};

struct OfflineMessage {
  std::string recipient;
  std::vector<std::uint8_t> payload;
  std::chrono::steady_clock::time_point created_at{};
  std::chrono::seconds ttl{std::chrono::hours(24)};
};

class OfflineQueue {
 public:
  explicit OfflineQueue(std::chrono::seconds default_ttl =
                            std::chrono::hours(24));

  void Enqueue(const std::string& recipient,
               std::vector<std::uint8_t> payload,
               std::chrono::seconds ttl = std::chrono::seconds::zero());

  std::vector<std::vector<std::uint8_t>> Drain(const std::string& recipient);

  void CleanupExpired();

 private:
  std::chrono::seconds default_ttl_;
  std::unordered_map<std::string, std::vector<OfflineMessage>> messages_;
  std::mutex mutex_;
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_OFFLINE_STORAGE_H
