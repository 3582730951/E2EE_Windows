#ifndef MI_E2EE_SERVER_API_SERVICE_H
#define MI_E2EE_SERVER_API_SERVICE_H

#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "config.h"
#include "group_manager.h"
#include "group_directory.h"
#include "key_transparency.h"
#include "offline_storage.h"
#include "session_manager.h"

namespace mi::server {

struct LoginRequest {
  std::string username;
  std::string password;
  std::uint32_t kex_version{0};
  std::array<std::uint8_t, 32> client_dh_pk{};
  std::vector<std::uint8_t> client_kem_pk;
};

struct LoginResponse {
  bool success{false};
  std::string token;
  std::uint32_t kex_version{0};
  std::array<std::uint8_t, 32> server_dh_pk{};
  std::vector<std::uint8_t> kem_ct;
  std::string error;
};

struct OpaqueRegisterStartResponse {
  bool success{false};
  OpaqueRegisterStartServerHello hello;
  std::string error;
};

struct OpaqueRegisterFinishResponse {
  bool success{false};
  std::string error;
};

struct OpaqueLoginStartResponse {
  bool success{false};
  OpaqueLoginStartServerHello hello;
  std::string error;
};

struct OpaqueLoginFinishResponse {
  bool success{false};
  std::string token;
  std::string error;
};

struct LogoutRequest {
  std::string token;
};

struct LogoutResponse {
  bool success{false};
  std::string error;
};

struct GroupEventResponse {
  bool success{false};
  std::uint32_t version{0};
  RotationReason reason{RotationReason::kPeriodic};
  std::string error;
};

struct GroupMessageResponse {
  bool success{false};
  std::optional<GroupKey> rotated;
  std::string error;
};

struct GroupMembersResponse {
  bool success{false};
  std::vector<std::string> members;
  std::string error;
};

struct GroupMembersInfoResponse {
  bool success{false};
  std::vector<GroupMemberInfo> members;
  std::string error;
};

struct GroupRoleSetResponse {
  bool success{false};
  std::string error;
};

struct FileUploadResponse {
  bool success{false};
  std::string file_id;
  std::array<std::uint8_t, 32> file_key{};
  StoredFileMeta meta;
  std::string error;
};

struct FileDownloadResponse {
  bool success{false};
  StoredFileMeta meta;
  std::vector<std::uint8_t> plaintext;
  std::string error;
};

struct FileBlobUploadResponse {
  bool success{false};
  std::string file_id;
  StoredFileMeta meta;
  std::string error;
};

struct FileBlobDownloadResponse {
  bool success{false};
  StoredFileMeta meta;
  std::vector<std::uint8_t> blob;
  std::string error;
};

struct FileBlobUploadStartResponse {
  bool success{false};
  std::string file_id;
  std::string upload_id;
  std::string error;
};

struct FileBlobUploadChunkResponse {
  bool success{false};
  std::uint64_t bytes_received{0};
  std::string error;
};

struct FileBlobUploadFinishResponse {
  bool success{false};
  StoredFileMeta meta;
  std::string error;
};

struct FileBlobDownloadStartResponse {
  bool success{false};
  std::string download_id;
  StoredFileMeta meta;
  std::uint64_t size{0};
  std::string error;
};

struct FileBlobDownloadChunkResponse {
  bool success{false};
  std::uint64_t offset{0};
  bool eof{false};
  std::vector<std::uint8_t> chunk;
  std::string error;
};

struct OfflinePushResponse {
  bool success{false};
  std::string error;
};

struct OfflinePullResponse {
  bool success{false};
  std::vector<std::vector<std::uint8_t>> messages;
  std::string error;
};

struct FriendListResponse {
  bool success{false};
  struct Entry {
    std::string username;
    std::string remark;
  };
  std::vector<Entry> friends;
  std::string error;
};

struct FriendAddResponse {
  bool success{false};
  std::string error;
};

struct FriendRemarkResponse {
  bool success{false};
  std::string error;
};

struct FriendRequestSendResponse {
  bool success{false};
  std::string error;
};

struct FriendRequestListResponse {
  bool success{false};
  struct Entry {
    std::string requester_username;
    std::string requester_remark;
  };
  std::vector<Entry> requests;
  std::string error;
};

struct FriendRequestRespondResponse {
  bool success{false};
  std::string error;
};

struct FriendDeleteResponse {
  bool success{false};
  std::string error;
};

struct UserBlockSetResponse {
  bool success{false};
  std::string error;
};

struct PreKeyPublishResponse {
  bool success{false};
  std::string error;
};

struct PreKeyFetchResponse {
  bool success{false};
  std::vector<std::uint8_t> bundle;
  std::uint32_t kt_version{0};
  std::uint64_t kt_tree_size{0};
  Sha256Hash kt_root{};
  std::vector<std::uint8_t> kt_signature;
  std::uint64_t kt_leaf_index{0};
  std::vector<Sha256Hash> kt_audit_path;
  std::vector<Sha256Hash> kt_consistency_path;
  std::string error;
};

struct KeyTransparencyHeadResponse {
  bool success{false};
  KeyTransparencySth sth{};
  std::string error;
};

struct KeyTransparencyConsistencyResponse {
  bool success{false};
  std::uint64_t old_size{0};
  std::uint64_t new_size{0};
  std::vector<Sha256Hash> proof;
  std::string error;
};

struct PrivateSendResponse {
  bool success{false};
  std::string error;
};

struct PrivatePullResponse {
  bool success{false};
  struct Entry {
    std::string sender;
    std::vector<std::uint8_t> payload;
  };
  std::vector<Entry> messages;
  std::string error;
};

struct GroupCipherSendResponse {
  bool success{false};
  std::string error;
};

struct GroupCipherPullResponse {
  bool success{false};
  struct Entry {
    std::string group_id;
    std::string sender;
    std::vector<std::uint8_t> payload;
  };
  std::vector<Entry> messages;
  std::string error;
};

struct GroupNoticePullResponse {
  bool success{false};
  struct Entry {
    std::string group_id;
    std::string sender;
    std::vector<std::uint8_t> payload;
  };
  std::vector<Entry> notices;
  std::string error;
};

struct DeviceSyncPushResponse {
  bool success{false};
  std::string error;
};

struct DeviceSyncPullResponse {
  bool success{false};
  std::vector<std::vector<std::uint8_t>> messages;
  std::string error;
};

struct DeviceListResponse {
  bool success{false};
  struct Entry {
    std::string device_id;
    std::uint32_t last_seen_sec{0};
  };
  std::vector<Entry> devices;
  std::string error;
};

struct DeviceKickResponse {
  bool success{false};
  std::string error;
};

struct DevicePairingPushResponse {
  bool success{false};
  std::string error;
};

struct DevicePairingPullResponse {
  bool success{false};
  std::vector<std::vector<std::uint8_t>> messages;
  std::string error;
};

class ApiService {
 public:
  ApiService(SessionManager* sessions, GroupManager* groups,
             GroupDirectory* directory = nullptr,
             OfflineStorage* storage = nullptr,
             OfflineQueue* queue = nullptr,
             std::uint32_t group_threshold = 10000,
             std::optional<MySqlConfig> friend_mysql = std::nullopt,
             std::filesystem::path kt_dir = {},
             std::filesystem::path kt_signing_key = {});

  LoginResponse Login(const LoginRequest& req, TransportKind transport);
  OpaqueRegisterStartResponse OpaqueRegisterStart(
      const OpaqueRegisterStartRequest& req);
  OpaqueRegisterFinishResponse OpaqueRegisterFinish(
      const OpaqueRegisterFinishRequest& req);
  OpaqueLoginStartResponse OpaqueLoginStart(const OpaqueLoginStartRequest& req);
  OpaqueLoginFinishResponse OpaqueLoginFinish(
      const OpaqueLoginFinishRequest& req, TransportKind transport);
  LogoutResponse Logout(const LogoutRequest& req);

  GroupEventResponse JoinGroup(const std::string& token,
                               const std::string& group_id);
  GroupEventResponse LeaveGroup(const std::string& token,
                                const std::string& group_id);
  GroupEventResponse KickGroup(const std::string& token,
                               const std::string& group_id);

  GroupMessageResponse OnGroupMessage(const std::string& token,
                                      const std::string& group_id,
                                      std::uint64_t threshold = 10000);

  std::optional<GroupKey> CurrentGroupKey(const std::string& group_id);

  GroupMembersResponse GroupMembers(const std::string& token,
                                    const std::string& group_id);

  GroupMembersInfoResponse GroupMembersInfo(const std::string& token,
                                            const std::string& group_id);

  GroupRoleSetResponse SetGroupRole(const std::string& token,
                                    const std::string& group_id,
                                    const std::string& target_username,
                                    GroupRole role);

  GroupEventResponse KickGroupMember(const std::string& token,
                                     const std::string& group_id,
                                     const std::string& target_username);

  FileUploadResponse StoreEphemeralFile(const std::string& token,
                                        const std::vector<std::uint8_t>& data);

  FileDownloadResponse LoadEphemeralFile(
      const std::string& token, const std::string& file_id,
      const std::array<std::uint8_t, 32>& key, bool wipe_after_read = true);

  FileBlobUploadResponse StoreE2eeFileBlob(
      const std::string& token, const std::vector<std::uint8_t>& blob);

  FileBlobDownloadResponse LoadE2eeFileBlob(const std::string& token,
                                             const std::string& file_id,
                                             bool wipe_after_read = true);

  FileBlobUploadStartResponse StartE2eeFileBlobUpload(
      const std::string& token, std::uint64_t expected_size = 0);

  FileBlobUploadChunkResponse UploadE2eeFileBlobChunk(
      const std::string& token, const std::string& file_id,
      const std::string& upload_id, std::uint64_t offset,
      const std::vector<std::uint8_t>& chunk);

  FileBlobUploadFinishResponse FinishE2eeFileBlobUpload(
      const std::string& token, const std::string& file_id,
      const std::string& upload_id, std::uint64_t total_size);

  FileBlobDownloadStartResponse StartE2eeFileBlobDownload(
      const std::string& token, const std::string& file_id,
      bool wipe_after_read);

  FileBlobDownloadChunkResponse DownloadE2eeFileBlobChunk(
      const std::string& token, const std::string& file_id,
      const std::string& download_id, std::uint64_t offset,
      std::uint32_t max_len);

  OfflinePushResponse EnqueueOffline(const std::string& token,
                                     const std::string& recipient,
                                     std::vector<std::uint8_t> payload);

  OfflinePullResponse PullOffline(const std::string& token);

  FriendListResponse ListFriends(const std::string& token);

  FriendAddResponse AddFriend(const std::string& token,
                              const std::string& friend_username,
                              const std::string& remark = "");

  FriendRemarkResponse SetFriendRemark(const std::string& token,
                                       const std::string& friend_username,
                                       const std::string& remark);

  FriendRequestSendResponse SendFriendRequest(const std::string& token,
                                              const std::string& target_username,
                                              const std::string& requester_remark = "");

  FriendRequestListResponse ListFriendRequests(const std::string& token);

  FriendRequestRespondResponse RespondFriendRequest(
      const std::string& token, const std::string& requester_username,
      bool accept);

  FriendDeleteResponse DeleteFriend(const std::string& token,
                                    const std::string& friend_username);

  UserBlockSetResponse SetUserBlocked(const std::string& token,
                                      const std::string& blocked_username,
                                      bool blocked);

  PreKeyPublishResponse PublishPreKeyBundle(const std::string& token,
                                           std::vector<std::uint8_t> bundle);

  PreKeyFetchResponse FetchPreKeyBundle(const std::string& token,
                                       const std::string& friend_username,
                                       std::uint64_t client_kt_tree_size = 0);

  KeyTransparencyHeadResponse GetKeyTransparencyHead(const std::string& token);

  KeyTransparencyConsistencyResponse GetKeyTransparencyConsistency(
      const std::string& token, std::uint64_t old_size, std::uint64_t new_size);

  PrivateSendResponse SendPrivate(const std::string& token,
                                  const std::string& recipient,
                                  std::vector<std::uint8_t> payload);

  PrivatePullResponse PullPrivate(const std::string& token);

  GroupCipherSendResponse SendGroupCipher(const std::string& token,
                                          const std::string& group_id,
                                          std::vector<std::uint8_t> payload);

  GroupCipherPullResponse PullGroupCipher(const std::string& token);

  GroupNoticePullResponse PullGroupNotices(const std::string& token);

  DeviceSyncPushResponse PushDeviceSync(const std::string& token,
                                        const std::string& device_id,
                                        std::vector<std::uint8_t> payload);

  DeviceSyncPullResponse PullDeviceSync(const std::string& token,
                                        const std::string& device_id);

  DeviceListResponse ListDevices(const std::string& token,
                                 const std::string& device_id);

  DeviceKickResponse KickDevice(const std::string& token,
                                const std::string& requester_device_id,
                                const std::string& target_device_id);

  DevicePairingPushResponse PushDevicePairingRequest(
      const std::string& token, const std::string& pairing_id_hex,
      std::vector<std::uint8_t> payload);

  DevicePairingPullResponse PullDevicePairing(const std::string& token,
                                              std::uint8_t mode,
                                              const std::string& pairing_id_hex,
                                              const std::string& device_id);

  DevicePairingPushResponse PushDevicePairingResponse(
      const std::string& token, const std::string& pairing_id_hex,
      const std::string& target_device_id, std::vector<std::uint8_t> payload);

 std::uint32_t default_group_threshold() const { return group_threshold_; }

 private:
  class RateLimiter {
   public:
    RateLimiter() = default;
    RateLimiter(double capacity, double refill_per_sec,
                std::chrono::seconds ttl = std::chrono::minutes(10));

    bool Allow(const std::string& key);

   private:
    struct Bucket {
      double tokens{0.0};
      std::chrono::steady_clock::time_point last{};
      std::chrono::steady_clock::time_point last_seen{};
    };

    struct ExpiryItem {
      std::chrono::steady_clock::time_point expires_at{};
      std::string key;
    };

    struct ExpiryItemCompare {
      bool operator()(const ExpiryItem& a, const ExpiryItem& b) const {
        return a.expires_at > b.expires_at;
      }
    };

    struct Shard {
      std::mutex mutex;
      std::unordered_map<std::string, Bucket> buckets;
      std::priority_queue<ExpiryItem, std::vector<ExpiryItem>, ExpiryItemCompare>
          expiries;
      std::uint64_t ops{0};
    };

    bool AllowAt(const std::string& key,
                 std::chrono::steady_clock::time_point now);
    void CleanupShardLocked(Shard& shard,
                            std::chrono::steady_clock::time_point now);

    double capacity_{0.0};
    double refill_per_sec_{0.0};
    std::chrono::seconds ttl_{std::chrono::minutes(10)};
    static constexpr std::size_t kShardCount = 16;
    std::array<Shard, kShardCount> shards_{};
  };

  bool RateLimitUnauth(const std::string& action, const std::string& username,
                       std::string& out_error);

  bool RateLimitAuth(const std::string& action, const std::string& token,
                     std::optional<Session>& out_session,
                     std::string& out_error);

  bool RateLimitFile(const std::string& action, const std::string& token,
                     std::optional<Session>& out_session,
                     std::string& out_error);
  bool SignKtSth(KeyTransparencySth& sth, std::string& out_error);

  struct PendingFriendRequest {
    std::string requester_remark;
    std::chrono::steady_clock::time_point created_at{};
  };

  SessionManager* sessions_;
  GroupManager* groups_;
  GroupDirectory* directory_;
  OfflineStorage* storage_;
  OfflineQueue* queue_;
  std::uint32_t group_threshold_;
  std::optional<MySqlConfig> friend_mysql_;

  RateLimiter rl_global_unauth_;
  RateLimiter rl_user_unauth_;
  RateLimiter rl_user_api_;
  RateLimiter rl_user_file_;

  std::mutex friends_mutex_;
  std::unordered_map<std::string, std::unordered_set<std::string>> friends_;
  std::unordered_map<std::string,
                     std::unordered_map<std::string, std::string>>
      friend_remarks_;
  std::unordered_map<std::string,
                     std::unordered_map<std::string, PendingFriendRequest>>
      friend_requests_by_target_;
  std::unordered_map<std::string, std::unordered_set<std::string>> blocks_;

  std::mutex prekeys_mutex_;
  std::unordered_map<std::string, std::vector<std::uint8_t>> prekey_bundles_;

  std::mutex devices_mutex_;
  struct DeviceRecord {
    std::chrono::steady_clock::time_point last_seen{};
    std::string last_token;
  };
  std::unordered_map<std::string, std::unordered_map<std::string, DeviceRecord>>
      devices_by_user_;

  std::unique_ptr<KeyTransparencyLog> kt_log_;
  std::array<std::uint8_t, kKtSthSigSecretKeyBytes> kt_signing_sk_{};
  bool kt_signing_ready_{false};
  std::string kt_signing_error_;
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_API_SERVICE_H
