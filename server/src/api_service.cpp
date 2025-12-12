#include "api_service.h"

#include <algorithm>
#include <cstring>
#include <type_traits>
#include <utility>

#ifdef MI_E2EE_ENABLE_MYSQL
#include <mysql.h>
#endif

namespace mi::server {

ApiService::ApiService(SessionManager* sessions, GroupManager* groups,
                       GroupDirectory* directory, OfflineStorage* storage,
                       OfflineQueue* queue, std::uint32_t group_threshold,
                       std::optional<MySqlConfig> friend_mysql)
    : sessions_(sessions),
      groups_(groups),
      directory_(directory),
      storage_(storage),
      queue_(queue),
      group_threshold_(group_threshold == 0 ? 10000 : group_threshold),
      friend_mysql_(std::move(friend_mysql)) {}

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

FriendListResponse ApiService::ListFriends(const std::string& token) {
  FriendListResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  auto sess = sessions_->GetSession(token);
  if (!sess.has_value()) {
    resp.error = "unauthorized";
    return resp;
  }

#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
      resp.error = "mysql_init failed";
      return resp;
    }
    MYSQL* res = mysql_real_connect(conn, friend_mysql_->host.c_str(),
                                    friend_mysql_->username.c_str(),
                                    friend_mysql_->password.get().c_str(),
                                    friend_mysql_->database.c_str(),
                                    friend_mysql_->port, nullptr, 0);
    if (!res) {
      resp.error = "mysql_connect failed";
      mysql_close(conn);
      return resp;
    }

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS user_friend ("
        "username VARCHAR(64) NOT NULL,"
        "friend_username VARCHAR(64) NOT NULL,"
        "remark VARCHAR(128) NOT NULL DEFAULT '',"
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY(username, friend_username),"
        "INDEX idx_friend_username(friend_username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
    if (mysql_query(conn, ddl) != 0) {
      resp.error = "mysql_schema_failed";
      mysql_close(conn);
      return resp;
    }
    // Best-effort migration for older schemas missing remark column.
    mysql_query(conn,
                "ALTER TABLE user_friend "
                "ADD COLUMN remark VARCHAR(128) NOT NULL DEFAULT ''");

    const char* query =
        "SELECT friend_username, remark FROM user_friend WHERE username=? "
        "ORDER BY friend_username";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
      resp.error = "mysql_stmt_init failed";
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_prepare(stmt, query,
                           static_cast<unsigned long>(std::strlen(query))) !=
        0) {
      resp.error = "mysql_stmt_prepare failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }

    MYSQL_BIND bind_param[1];
    std::memset(bind_param, 0, sizeof(bind_param));
    bind_param[0].buffer_type = MYSQL_TYPE_STRING;
    bind_param[0].buffer = const_cast<char*>(sess->username.c_str());
    bind_param[0].buffer_length =
        static_cast<unsigned long>(sess->username.size());

    if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
      resp.error = "mysql_stmt_bind_param failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_execute(stmt) != 0) {
      resp.error = "mysql_stmt_execute failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }

    char name_buf[256] = {0};
    unsigned long name_len = 0;
    char remark_buf[256] = {0};
    unsigned long remark_len = 0;
    MYSQL_BIND bind_result[2];
    std::memset(bind_result, 0, sizeof(bind_result));
    using BindBool0 = std::remove_pointer_t<decltype(bind_result[0].is_null)>;
    using BindBool1 = std::remove_pointer_t<decltype(bind_result[1].is_null)>;
    BindBool0 name_is_null = 0;
    BindBool0 name_error_flag = 0;
    BindBool1 remark_is_null = 0;
    BindBool1 remark_error_flag = 0;
    bind_result[0].buffer_type = MYSQL_TYPE_STRING;
    bind_result[0].buffer = name_buf;
    bind_result[0].buffer_length = sizeof(name_buf) - 1;
    bind_result[0].length = &name_len;
    bind_result[0].is_null = &name_is_null;
    bind_result[0].error = &name_error_flag;
    bind_result[1].buffer_type = MYSQL_TYPE_STRING;
    bind_result[1].buffer = remark_buf;
    bind_result[1].buffer_length = sizeof(remark_buf) - 1;
    bind_result[1].length = &remark_len;
    bind_result[1].is_null = &remark_is_null;
    bind_result[1].error = &remark_error_flag;

    if (mysql_stmt_bind_result(stmt, bind_result) != 0) {
      resp.error = "mysql_stmt_bind_result failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_store_result(stmt) != 0) {
      resp.error = "mysql_stmt_store_result failed";
      mysql_stmt_free_result(stmt);
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }

    std::vector<FriendListResponse::Entry> out;
    while (true) {
      const int fetch_status = mysql_stmt_fetch(stmt);
      if (fetch_status == MYSQL_NO_DATA) {
        break;
      }
      if (fetch_status != 0 && fetch_status != MYSQL_DATA_TRUNCATED) {
        resp.error = "mysql_stmt_fetch failed";
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      if (name_is_null) {
        continue;
      }
      if (name_len >= sizeof(name_buf)) {
        resp.error = "friend name too long";
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      name_buf[name_len] = '\0';
      if (!remark_is_null) {
        if (remark_len >= sizeof(remark_buf)) {
          resp.error = "remark too long";
          mysql_stmt_free_result(stmt);
          mysql_stmt_close(stmt);
          mysql_close(conn);
          return resp;
        }
        remark_buf[remark_len] = '\0';
      } else {
        remark_len = 0;
      }
      FriendListResponse::Entry e;
      e.username.assign(name_buf, name_len);
      if (remark_len > 0) {
        e.remark.assign(remark_buf, remark_len);
      }
      out.push_back(std::move(e));
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    mysql_close(conn);

    std::sort(out.begin(), out.end(),
              [](const FriendListResponse::Entry& a,
                 const FriendListResponse::Entry& b) {
                return a.username < b.username;
              });
    resp.success = true;
    resp.friends = std::move(out);
    return resp;
  }
#endif

  std::vector<FriendListResponse::Entry> out;
  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    auto it = friends_.find(sess->username);
    if (it != friends_.end()) {
      out.reserve(it->second.size());
      const auto remarks_it = friend_remarks_.find(sess->username);
      for (const auto& f : it->second) {
        FriendListResponse::Entry e;
        e.username = f;
        if (remarks_it != friend_remarks_.end()) {
          const auto r = remarks_it->second.find(f);
          if (r != remarks_it->second.end()) {
            e.remark = r->second;
          }
        }
        out.push_back(std::move(e));
      }
    }
  }
  std::sort(out.begin(), out.end(),
            [](const FriendListResponse::Entry& a,
               const FriendListResponse::Entry& b) {
              return a.username < b.username;
            });
  resp.success = true;
  resp.friends = std::move(out);
  return resp;
}

FriendAddResponse ApiService::AddFriend(const std::string& token,
                                        const std::string& friend_username,
                                        const std::string& remark) {
  FriendAddResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  auto sess = sessions_->GetSession(token);
  if (!sess.has_value()) {
    resp.error = "unauthorized";
    return resp;
  }
  if (friend_username.empty()) {
    resp.error = "friend username empty";
    return resp;
  }
  if (friend_username == sess->username) {
    resp.error = "cannot add self";
    return resp;
  }
  if (remark.size() > 128) {
    resp.error = "remark too long";
    return resp;
  }

  std::string exist_err;
  if (!sessions_->UserExists(friend_username, exist_err)) {
    resp.error = exist_err.empty() ? "friend not found" : exist_err;
    return resp;
  }

#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
      resp.error = "mysql_init failed";
      return resp;
    }
    MYSQL* res = mysql_real_connect(conn, friend_mysql_->host.c_str(),
                                    friend_mysql_->username.c_str(),
                                    friend_mysql_->password.get().c_str(),
                                    friend_mysql_->database.c_str(),
                                    friend_mysql_->port, nullptr, 0);
    if (!res) {
      resp.error = "mysql_connect failed";
      mysql_close(conn);
      return resp;
    }

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS user_friend ("
        "username VARCHAR(64) NOT NULL,"
        "friend_username VARCHAR(64) NOT NULL,"
        "remark VARCHAR(128) NOT NULL DEFAULT '',"
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY(username, friend_username),"
        "INDEX idx_friend_username(friend_username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
    if (mysql_query(conn, ddl) != 0) {
      resp.error = "mysql_schema_failed";
      mysql_close(conn);
      return resp;
    }
    mysql_query(conn,
                "ALTER TABLE user_friend "
                "ADD COLUMN remark VARCHAR(128) NOT NULL DEFAULT ''");

    const char* query =
        "INSERT IGNORE INTO user_friend(username, friend_username, remark) "
        "VALUES(?, ?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
      resp.error = "mysql_stmt_init failed";
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_prepare(stmt, query,
                           static_cast<unsigned long>(std::strlen(query))) !=
        0) {
      resp.error = "mysql_stmt_prepare failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }

    auto bind_and_exec = [&](const std::string& u,
                             const std::string& f,
                             const std::string& r) -> bool {
      MYSQL_BIND bind_param[3];
      std::memset(bind_param, 0, sizeof(bind_param));
      bind_param[0].buffer_type = MYSQL_TYPE_STRING;
      bind_param[0].buffer = const_cast<char*>(u.c_str());
      bind_param[0].buffer_length = static_cast<unsigned long>(u.size());
      bind_param[1].buffer_type = MYSQL_TYPE_STRING;
      bind_param[1].buffer = const_cast<char*>(f.c_str());
      bind_param[1].buffer_length = static_cast<unsigned long>(f.size());
      bind_param[2].buffer_type = MYSQL_TYPE_STRING;
      bind_param[2].buffer = const_cast<char*>(r.c_str());
      bind_param[2].buffer_length = static_cast<unsigned long>(r.size());
      if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
        return false;
      }
      return mysql_stmt_execute(stmt) == 0;
    };

    const bool ok1 = bind_and_exec(sess->username, friend_username, remark);
    const bool ok2 = bind_and_exec(friend_username, sess->username, "");

    mysql_stmt_close(stmt);
    mysql_close(conn);

    if (!ok1 || !ok2) {
      resp.error = "mysql insert failed";
      return resp;
    }
    resp.success = true;
    return resp;
  }
#endif

  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    friends_[sess->username].insert(friend_username);
    friends_[friend_username].insert(sess->username);
    if (!remark.empty()) {
      friend_remarks_[sess->username][friend_username] = remark;
    }
  }
  resp.success = true;
  return resp;
}

FriendRemarkResponse ApiService::SetFriendRemark(const std::string& token,
                                                 const std::string& friend_username,
                                                 const std::string& remark) {
  FriendRemarkResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  auto sess = sessions_->GetSession(token);
  if (!sess.has_value()) {
    resp.error = "unauthorized";
    return resp;
  }
  if (friend_username.empty()) {
    resp.error = "friend username empty";
    return resp;
  }
  if (remark.size() > 128) {
    resp.error = "remark too long";
    return resp;
  }

#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
      resp.error = "mysql_init failed";
      return resp;
    }
    MYSQL* res = mysql_real_connect(conn, friend_mysql_->host.c_str(),
                                    friend_mysql_->username.c_str(),
                                    friend_mysql_->password.get().c_str(),
                                    friend_mysql_->database.c_str(),
                                    friend_mysql_->port, nullptr, 0);
    if (!res) {
      resp.error = "mysql_connect failed";
      mysql_close(conn);
      return resp;
    }

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS user_friend ("
        "username VARCHAR(64) NOT NULL,"
        "friend_username VARCHAR(64) NOT NULL,"
        "remark VARCHAR(128) NOT NULL DEFAULT '',"
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY(username, friend_username),"
        "INDEX idx_friend_username(friend_username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
    if (mysql_query(conn, ddl) != 0) {
      resp.error = "mysql_schema_failed";
      mysql_close(conn);
      return resp;
    }
    mysql_query(conn,
                "ALTER TABLE user_friend "
                "ADD COLUMN remark VARCHAR(128) NOT NULL DEFAULT ''");

    // Ensure friend relation exists.
    {
      const char* exist_query =
          "SELECT 1 FROM user_friend WHERE username=? AND friend_username=? "
          "LIMIT 1";
      MYSQL_STMT* stmt = mysql_stmt_init(conn);
      if (!stmt) {
        resp.error = "mysql_stmt_init failed";
        mysql_close(conn);
        return resp;
      }
      if (mysql_stmt_prepare(stmt, exist_query,
                             static_cast<unsigned long>(
                                 std::strlen(exist_query))) != 0) {
        resp.error = "mysql_stmt_prepare failed";
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      MYSQL_BIND bind_param[2];
      std::memset(bind_param, 0, sizeof(bind_param));
      bind_param[0].buffer_type = MYSQL_TYPE_STRING;
      bind_param[0].buffer = const_cast<char*>(sess->username.c_str());
      bind_param[0].buffer_length =
          static_cast<unsigned long>(sess->username.size());
      bind_param[1].buffer_type = MYSQL_TYPE_STRING;
      bind_param[1].buffer = const_cast<char*>(friend_username.c_str());
      bind_param[1].buffer_length =
          static_cast<unsigned long>(friend_username.size());
      if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
        resp.error = "mysql_stmt_bind_param failed";
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      if (mysql_stmt_execute(stmt) != 0) {
        resp.error = "mysql_stmt_execute failed";
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      int value = 0;
      MYSQL_BIND bind_result[1];
      std::memset(bind_result, 0, sizeof(bind_result));
      using BindBool = std::remove_pointer_t<decltype(bind_result[0].is_null)>;
      BindBool is_null = 0;
      BindBool error_flag = 0;
      bind_result[0].buffer_type = MYSQL_TYPE_LONG;
      bind_result[0].buffer = &value;
      bind_result[0].is_null = &is_null;
      bind_result[0].error = &error_flag;
      if (mysql_stmt_bind_result(stmt, bind_result) != 0) {
        resp.error = "mysql_stmt_bind_result failed";
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      if (mysql_stmt_store_result(stmt) != 0) {
        resp.error = "mysql_stmt_store_result failed";
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      const int fetch_status = mysql_stmt_fetch(stmt);
      mysql_stmt_free_result(stmt);
      mysql_stmt_close(stmt);
      if (fetch_status == MYSQL_NO_DATA || is_null) {
        resp.error = "not friends";
        mysql_close(conn);
        return resp;
      }
      if (fetch_status != 0 && fetch_status != MYSQL_DATA_TRUNCATED) {
        resp.error = "mysql_stmt_fetch failed";
        mysql_close(conn);
        return resp;
      }
    }

    const char* query =
        "UPDATE user_friend SET remark=? WHERE username=? AND friend_username=?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
      resp.error = "mysql_stmt_init failed";
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_prepare(stmt, query,
                           static_cast<unsigned long>(std::strlen(query))) !=
        0) {
      resp.error = "mysql_stmt_prepare failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    MYSQL_BIND bind_param[3];
    std::memset(bind_param, 0, sizeof(bind_param));
    bind_param[0].buffer_type = MYSQL_TYPE_STRING;
    bind_param[0].buffer = const_cast<char*>(remark.c_str());
    bind_param[0].buffer_length = static_cast<unsigned long>(remark.size());
    bind_param[1].buffer_type = MYSQL_TYPE_STRING;
    bind_param[1].buffer = const_cast<char*>(sess->username.c_str());
    bind_param[1].buffer_length =
        static_cast<unsigned long>(sess->username.size());
    bind_param[2].buffer_type = MYSQL_TYPE_STRING;
    bind_param[2].buffer = const_cast<char*>(friend_username.c_str());
    bind_param[2].buffer_length =
        static_cast<unsigned long>(friend_username.size());
    if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
      resp.error = "mysql_stmt_bind_param failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_execute(stmt) != 0) {
      resp.error = "mysql_stmt_execute failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    mysql_stmt_close(stmt);
    mysql_close(conn);

    resp.success = true;
    return resp;
  }
#endif

  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    auto it = friends_.find(sess->username);
    if (it == friends_.end() ||
        it->second.find(friend_username) == it->second.end()) {
      resp.error = "not friends";
      return resp;
    }
    if (remark.empty()) {
      auto r = friend_remarks_.find(sess->username);
      if (r != friend_remarks_.end()) {
        r->second.erase(friend_username);
      }
    } else {
      friend_remarks_[sess->username][friend_username] = remark;
    }
  }
  resp.success = true;
  return resp;
}

}  // namespace mi::server
