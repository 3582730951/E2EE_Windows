#ifndef MI_E2EE_SERVER_API_SERVICE_H
#define MI_E2EE_SERVER_API_SERVICE_H

#include <array>
#include <optional>
#include <string>
#include <vector>

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

class ApiService {
 public:
  ApiService(SessionManager* sessions, GroupManager* groups,
             GroupDirectory* directory = nullptr,
             OfflineStorage* storage = nullptr,
             OfflineQueue* queue = nullptr,
             std::uint32_t group_threshold = 10000);

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

  std::uint32_t default_group_threshold() const { return group_threshold_; }

 private:
  SessionManager* sessions_;
  GroupManager* groups_;
  GroupDirectory* directory_;
  OfflineStorage* storage_;
  OfflineQueue* queue_;
  std::uint32_t group_threshold_;
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_API_SERVICE_H
