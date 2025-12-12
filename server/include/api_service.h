#ifndef MI_E2EE_SERVER_API_SERVICE_H
#define MI_E2EE_SERVER_API_SERVICE_H

#include <array>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "config.h"
#include "group_manager.h"
#include "group_directory.h"
#include "offline_storage.h"
#include "session_manager.h"

namespace mi::server {

struct LoginRequest {
  std::string username;
  std::string password;
};

struct LoginResponse {
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

class ApiService {
 public:
  ApiService(SessionManager* sessions, GroupManager* groups,
             GroupDirectory* directory = nullptr,
             OfflineStorage* storage = nullptr,
             OfflineQueue* queue = nullptr,
             std::uint32_t group_threshold = 10000,
             std::optional<MySqlConfig> friend_mysql = std::nullopt);

  LoginResponse Login(const LoginRequest& req);
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

  std::vector<std::string> GroupMembers(const std::string& group_id);

  FileUploadResponse StoreEphemeralFile(const std::string& token,
                                        const std::vector<std::uint8_t>& data);

  FileDownloadResponse LoadEphemeralFile(
      const std::string& token, const std::string& file_id,
      const std::array<std::uint8_t, 32>& key, bool wipe_after_read = true);

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

 std::uint32_t default_group_threshold() const { return group_threshold_; }

 private:
  SessionManager* sessions_;
  GroupManager* groups_;
  GroupDirectory* directory_;
  OfflineStorage* storage_;
  OfflineQueue* queue_;
  std::uint32_t group_threshold_;
  std::optional<MySqlConfig> friend_mysql_;

  std::mutex friends_mutex_;
  std::unordered_map<std::string, std::unordered_set<std::string>> friends_;
  std::unordered_map<std::string,
                     std::unordered_map<std::string, std::string>>
      friend_remarks_;
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_API_SERVICE_H
