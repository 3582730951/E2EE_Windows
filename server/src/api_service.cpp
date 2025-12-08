#include "api_service.h"

#include <utility>

namespace mi::server {

ApiService::ApiService(SessionManager* sessions, GroupManager* groups,
                       GroupDirectory* directory, OfflineStorage* storage,
                       OfflineQueue* queue, std::uint32_t group_threshold)
    : sessions_(sessions),
      groups_(groups),
      directory_(directory),
      storage_(storage),
      queue_(queue),
      group_threshold_(group_threshold == 0 ? 10000 : group_threshold) {}

LoginResponse ApiService::Login(const LoginRequest& req) {
  LoginResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  Session session;
  std::string err;
  if (!sessions_->Login(req.username, req.password, session, err)) {
    resp.error = err;
    return resp;
  }
  resp.success = true;
  resp.token = session.token;
  return resp;
}

LogoutResponse ApiService::Logout(const LogoutRequest& req) {
  LogoutResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  sessions_->Logout(req.token);
  resp.success = true;
  return resp;
}

GroupEventResponse ApiService::JoinGroup(const std::string& token,
                                         const std::string& group_id) {
  GroupEventResponse resp;
  if (!groups_ || !sessions_) {
    resp.error = "group manager unavailable";
    return resp;
  }
  if (!sessions_->GetSession(token).has_value()) {
    resp.error = "unauthorized";
    return resp;
  }
  auto key = groups_->Rotate(group_id, RotationReason::kJoin);
  if (directory_) {
    if (auto s = sessions_->GetSession(token)) {
      directory_->AddGroup(group_id, s->username);
      directory_->AddMember(group_id, s->username);
    }
  }
  resp.success = true;
  resp.version = key.version;
  resp.reason = key.reason;
  return resp;
}

GroupEventResponse ApiService::LeaveGroup(const std::string& token,
                                          const std::string& group_id) {
  GroupEventResponse resp;
  if (!groups_ || !sessions_) {
    resp.error = "group manager unavailable";
    return resp;
  }
  if (!sessions_->GetSession(token).has_value()) {
    resp.error = "unauthorized";
    return resp;
  }
  auto key = groups_->Rotate(group_id, RotationReason::kLeave);
  if (directory_) {
    if (auto s = sessions_->GetSession(token)) {
      directory_->RemoveMember(group_id, s->username);
    }
  }
  resp.success = true;
  resp.version = key.version;
  resp.reason = key.reason;
  return resp;
}

GroupEventResponse ApiService::KickGroup(const std::string& token,
                                         const std::string& group_id) {
  GroupEventResponse resp;
  if (!groups_ || !sessions_) {
    resp.error = "group manager unavailable";
    return resp;
  }
  if (!sessions_->GetSession(token).has_value()) {
    resp.error = "unauthorized";
    return resp;
  }
  auto key = groups_->Rotate(group_id, RotationReason::kKick);
  if (directory_) {
    if (auto s = sessions_->GetSession(token)) {
      directory_->RemoveMember(group_id, s->username);
    }
  }
  resp.success = true;
  resp.version = key.version;
  resp.reason = key.reason;
  return resp;
}

GroupMessageResponse ApiService::OnGroupMessage(const std::string& token,
                                                const std::string& group_id,
                                                std::uint64_t threshold) {
  GroupMessageResponse resp;
  if (!groups_ || !sessions_) {
    resp.error = "group manager unavailable";
    return resp;
  }
  auto sess = sessions_->GetSession(token);
  if (!sess.has_value()) {
    resp.error = "unauthorized";
    return resp;
  }
  if (directory_) {
    if (!directory_->HasMember(group_id, sess->username)) {
      resp.error = "not in group";
      return resp;
    }
  }
  const std::uint64_t use_threshold =
      (threshold == 0) ? group_threshold_ : threshold;
  auto rotated = groups_->OnMessage(group_id, use_threshold);
  resp.success = true;
  resp.rotated = rotated;
  return resp;
}

std::optional<GroupKey> ApiService::CurrentGroupKey(
    const std::string& group_id) {
  if (!groups_) {
    return std::nullopt;
  }
  return groups_->GetKey(group_id);
}

std::vector<std::string> ApiService::GroupMembers(const std::string& group_id) {
  if (!directory_) {
    return {};
  }
  return directory_->Members(group_id);
}

FileUploadResponse ApiService::StoreEphemeralFile(
    const std::string& token, const std::vector<std::uint8_t>& data) {
  FileUploadResponse resp;
  if (!sessions_ || !storage_) {
    resp.error = "storage unavailable";
    return resp;
  }
  auto sess = sessions_->GetSession(token);
  if (!sess.has_value()) {
    resp.error = "unauthorized";
    return resp;
  }
  auto put = storage_->Put(sess->username, data);
  if (!put.success) {
    resp.error = put.error;
    return resp;
  }
  resp.success = true;
  resp.file_id = put.file_id;
  resp.file_key = put.file_key;
  resp.meta = put.meta;
  return resp;
}

FileDownloadResponse ApiService::LoadEphemeralFile(
    const std::string& token, const std::string& file_id,
    const std::array<std::uint8_t, 32>& key, bool wipe_after_read) {
  FileDownloadResponse resp;
  if (!sessions_ || !storage_) {
    resp.error = "storage unavailable";
    return resp;
  }
  auto sess = sessions_->GetSession(token);
  if (!sess.has_value()) {
    resp.error = "unauthorized";
    return resp;
  }
  std::string err;
  auto data = storage_->Fetch(file_id, key, wipe_after_read, err);
  if (!data.has_value()) {
    resp.error = err;
    return resp;
  }
  resp.success = true;
  resp.plaintext = std::move(*data);
  auto meta = storage_->Meta(file_id);
  if (meta.has_value()) {
    resp.meta = *meta;
  }
  return resp;
}

OfflinePushResponse ApiService::EnqueueOffline(const std::string& token,
                                               const std::string& recipient,
                                               std::vector<std::uint8_t> payload) {
  OfflinePushResponse resp;
  if (!sessions_ || !queue_) {
    resp.error = "queue unavailable";
    return resp;
  }
  auto sess = sessions_->GetSession(token);
  if (!sess.has_value()) {
    resp.error = "unauthorized";
    return resp;
  }
  queue_->Enqueue(recipient, std::move(payload));
  resp.success = true;
  return resp;
}

OfflinePullResponse ApiService::PullOffline(const std::string& token) {
  OfflinePullResponse resp;
  if (!sessions_ || !queue_) {
    resp.error = "queue unavailable";
    return resp;
  }
  auto sess = sessions_->GetSession(token);
  if (!sess.has_value()) {
    resp.error = "unauthorized";
    return resp;
  }
  resp.messages = queue_->Drain(sess->username);
  resp.success = true;
  return resp;
}

}  // namespace mi::server
