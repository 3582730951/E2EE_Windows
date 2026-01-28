#include "state_store_mysql.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#ifdef MI_E2EE_ENABLE_MYSQL
#include <mysql.h>
#endif

namespace mi::server {

namespace {

#ifdef MI_E2EE_ENABLE_MYSQL

class MysqlStateStore final : public StateStore {
 public:
  MysqlStateStore(MYSQL* conn, MetadataProtector* metadata_protector)
      : conn_(conn), metadata_protector_(metadata_protector) {}

  ~MysqlStateStore() override {
    if (conn_) {
      mysql_close(conn_);
      conn_ = nullptr;
    }
  }

  bool LoadBlob(const std::string& key, BlobLoadResult& out,
                std::string& error) override {
    error.clear();
    out = BlobLoadResult{};
    const auto [scope, name] = SplitKey(key);
    std::lock_guard<std::mutex> lock(mutex_);
    const char* query =
        "SELECT payload FROM mi_state_blob WHERE scope=? AND key_name=?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (!stmt) {
      error = "mysql_stmt_init failed";
      return false;
    }
    if (mysql_stmt_prepare(stmt, query, std::strlen(query)) != 0) {
      error = "mysql_stmt_prepare failed";
      mysql_stmt_close(stmt);
      return false;
    }

    MYSQL_BIND params[2]{};
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = const_cast<char*>(scope.c_str());
    params[0].buffer_length = static_cast<unsigned long>(scope.size());
    params[0].length = &params[0].buffer_length;
    params[1].buffer_type = MYSQL_TYPE_STRING;
    params[1].buffer = const_cast<char*>(name.c_str());
    params[1].buffer_length = static_cast<unsigned long>(name.size());
    params[1].length = &params[1].buffer_length;
    if (mysql_stmt_bind_param(stmt, params) != 0) {
      error = "mysql_stmt_bind_param failed";
      mysql_stmt_close(stmt);
      return false;
    }
    if (mysql_stmt_execute(stmt) != 0) {
      error = "mysql_stmt_execute failed";
      mysql_stmt_close(stmt);
      return false;
    }

    MYSQL_BIND result[1]{};
    std::vector<std::uint8_t> buffer;
    unsigned long blob_len = 0;
    result[0].buffer_type = MYSQL_TYPE_BLOB;
    result[0].buffer = nullptr;
    result[0].buffer_length = 0;
    result[0].length = &blob_len;
    if (mysql_stmt_bind_result(stmt, result) != 0) {
      error = "mysql_stmt_bind_result failed";
      mysql_stmt_close(stmt);
      return false;
    }
    if (mysql_stmt_store_result(stmt) != 0) {
      error = "mysql_stmt_store_result failed";
      mysql_stmt_free_result(stmt);
      mysql_stmt_close(stmt);
      return false;
    }

    const int fetch_status = mysql_stmt_fetch(stmt);
    if (fetch_status == MYSQL_NO_DATA) {
      mysql_stmt_free_result(stmt);
      mysql_stmt_close(stmt);
      return true;
    }
    if (fetch_status != 0 && fetch_status != MYSQL_DATA_TRUNCATED) {
      error = "mysql_stmt_fetch failed";
      mysql_stmt_free_result(stmt);
      mysql_stmt_close(stmt);
      return false;
    }

    if (blob_len > 0) {
      buffer.resize(blob_len);
      MYSQL_BIND col{};
      col.buffer_type = MYSQL_TYPE_BLOB;
      col.buffer = buffer.data();
      col.buffer_length = blob_len;
      col.length = &blob_len;
      if (mysql_stmt_fetch_column(stmt, &col, 0, 0) != 0) {
        error = "mysql_stmt_fetch_column failed";
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        return false;
      }
      if (blob_len != buffer.size()) {
        buffer.resize(blob_len);
      }
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    out.found = true;
    if (!MaybeDecrypt(buffer, out.data, error)) {
      return false;
    }
    return true;
  }

  bool SaveBlob(const std::string& key, const std::vector<std::uint8_t>& data,
                std::string& error) override {
    error.clear();
    const auto [scope, name] = SplitKey(key);
    std::vector<std::uint8_t> payload;
    if (!MaybeEncrypt(data, payload, error)) {
      return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const char* query =
        "INSERT INTO mi_state_blob (scope, key_name, version, payload) "
        "VALUES (?, ?, 1, ?) "
        "ON DUPLICATE KEY UPDATE version=version+1, payload=VALUES(payload)";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (!stmt) {
      error = "mysql_stmt_init failed";
      return false;
    }
    if (mysql_stmt_prepare(stmt, query, std::strlen(query)) != 0) {
      error = "mysql_stmt_prepare failed";
      mysql_stmt_close(stmt);
      return false;
    }

    MYSQL_BIND params[3]{};
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = const_cast<char*>(scope.c_str());
    params[0].buffer_length = static_cast<unsigned long>(scope.size());
    params[0].length = &params[0].buffer_length;
    params[1].buffer_type = MYSQL_TYPE_STRING;
    params[1].buffer = const_cast<char*>(name.c_str());
    params[1].buffer_length = static_cast<unsigned long>(name.size());
    params[1].length = &params[1].buffer_length;
    params[2].buffer_type = MYSQL_TYPE_BLOB;
    params[2].buffer = payload.empty() ? nullptr : payload.data();
    params[2].buffer_length =
        static_cast<unsigned long>(payload.size());
    params[2].length = &params[2].buffer_length;

    if (mysql_stmt_bind_param(stmt, params) != 0) {
      error = "mysql_stmt_bind_param failed";
      mysql_stmt_close(stmt);
      return false;
    }
    if (mysql_stmt_execute(stmt) != 0) {
      error = "mysql_stmt_execute failed";
      mysql_stmt_close(stmt);
      return false;
    }
    mysql_stmt_close(stmt);
    return true;
  }

  bool DeleteBlob(const std::string& key, std::string& error) override {
    error.clear();
    const auto [scope, name] = SplitKey(key);
    std::lock_guard<std::mutex> lock(mutex_);
    const char* query =
        "DELETE FROM mi_state_blob WHERE scope=? AND key_name=?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (!stmt) {
      error = "mysql_stmt_init failed";
      return false;
    }
    if (mysql_stmt_prepare(stmt, query, std::strlen(query)) != 0) {
      error = "mysql_stmt_prepare failed";
      mysql_stmt_close(stmt);
      return false;
    }
    MYSQL_BIND params[2]{};
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = const_cast<char*>(scope.c_str());
    params[0].buffer_length = static_cast<unsigned long>(scope.size());
    params[0].length = &params[0].buffer_length;
    params[1].buffer_type = MYSQL_TYPE_STRING;
    params[1].buffer = const_cast<char*>(name.c_str());
    params[1].buffer_length = static_cast<unsigned long>(name.size());
    params[1].length = &params[1].buffer_length;
    if (mysql_stmt_bind_param(stmt, params) != 0) {
      error = "mysql_stmt_bind_param failed";
      mysql_stmt_close(stmt);
      return false;
    }
    if (mysql_stmt_execute(stmt) != 0) {
      error = "mysql_stmt_execute failed";
      mysql_stmt_close(stmt);
      return false;
    }
    mysql_stmt_close(stmt);
    return true;
  }

  bool AcquireLock(const std::string& key,
                   std::chrono::milliseconds timeout,
                   std::string& error) override {
    error.clear();
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string lock_name = "mi_state:" + key;
    const char* query = "SELECT GET_LOCK(?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (!stmt) {
      error = "mysql_stmt_init failed";
      return false;
    }
    if (mysql_stmt_prepare(stmt, query, std::strlen(query)) != 0) {
      error = "mysql_stmt_prepare failed";
      mysql_stmt_close(stmt);
      return false;
    }
    const int timeout_sec =
        static_cast<int>(std::max<std::int64_t>(
            0, timeout.count() / 1000));
    MYSQL_BIND params[2]{};
    unsigned long name_len =
        static_cast<unsigned long>(lock_name.size());
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = const_cast<char*>(lock_name.c_str());
    params[0].buffer_length = name_len;
    params[0].length = &name_len;
    params[1].buffer_type = MYSQL_TYPE_LONG;
    params[1].buffer = &timeout_sec;
    params[1].is_unsigned = false;
    if (mysql_stmt_bind_param(stmt, params) != 0) {
      error = "mysql_stmt_bind_param failed";
      mysql_stmt_close(stmt);
      return false;
    }
    if (mysql_stmt_execute(stmt) != 0) {
      error = "mysql_stmt_execute failed";
      mysql_stmt_close(stmt);
      return false;
    }
    MYSQL_BIND result[1]{};
    int got = 0;
    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = &got;
    if (mysql_stmt_bind_result(stmt, result) != 0) {
      error = "mysql_stmt_bind_result failed";
      mysql_stmt_close(stmt);
      return false;
    }
    if (mysql_stmt_fetch(stmt) != 0) {
      error = "mysql_stmt_fetch failed";
      mysql_stmt_close(stmt);
      return false;
    }
    mysql_stmt_close(stmt);
    if (got != 1) {
      error = "mysql lock busy";
      return false;
    }
    held_locks_.insert(lock_name);
    return true;
  }

  void ReleaseLock(const std::string& key) override {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string lock_name = "mi_state:" + key;
    if (held_locks_.find(lock_name) == held_locks_.end()) {
      return;
    }
    const char* query = "SELECT RELEASE_LOCK(?)";
    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (!stmt) {
      return;
    }
    if (mysql_stmt_prepare(stmt, query, std::strlen(query)) != 0) {
      mysql_stmt_close(stmt);
      return;
    }
    MYSQL_BIND params[1]{};
    unsigned long name_len =
        static_cast<unsigned long>(lock_name.size());
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = const_cast<char*>(lock_name.c_str());
    params[0].buffer_length = name_len;
    params[0].length = &name_len;
    if (mysql_stmt_bind_param(stmt, params) != 0) {
      mysql_stmt_close(stmt);
      return;
    }
    mysql_stmt_execute(stmt);
    mysql_stmt_close(stmt);
    held_locks_.erase(lock_name);
  }

  bool HasAnyData(bool& out_has_data, std::string& error) override {
    error.clear();
    out_has_data = false;
    std::lock_guard<std::mutex> lock(mutex_);
    const char* query = "SELECT 1 FROM mi_state_blob LIMIT 1";
    if (mysql_query(conn_, query) != 0) {
      error = "mysql_query failed";
      return false;
    }
    MYSQL_RES* res = mysql_store_result(conn_);
    if (!res) {
      error = "mysql_store_result failed";
      return false;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    out_has_data = (row != nullptr);
    mysql_free_result(res);
    return true;
  }

 private:
  std::pair<std::string, std::string> SplitKey(
      const std::string& key) const {
    const auto pos = key.find(':');
    if (pos == std::string::npos) {
      return {key, "default"};
    }
    return {key.substr(0, pos), key.substr(pos + 1)};
  }

  bool MaybeEncrypt(const std::vector<std::uint8_t>& in,
                    std::vector<std::uint8_t>& out,
                    std::string& error) const {
    if (!metadata_protector_) {
      out = in;
      return true;
    }
    return metadata_protector_->EncryptBlob(in, out, error);
  }

  bool MaybeDecrypt(const std::vector<std::uint8_t>& in,
                    std::vector<std::uint8_t>& out,
                    std::string& error) const {
    if (!metadata_protector_) {
      out = in;
      return true;
    }
    return metadata_protector_->DecryptBlob(in, out, error);
  }

  MYSQL* conn_{nullptr};
  MetadataProtector* metadata_protector_{nullptr};
  std::mutex mutex_;
  std::unordered_set<std::string> held_locks_;
};

MYSQL* ConnectMysql(const MySqlConfig& cfg, std::string& error) {
  error.clear();
  constexpr int kMaxAttempts = 2;
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
      error = "mysql_init failed";
      return nullptr;
    }
    unsigned int timeout = 5;
    mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(conn, MYSQL_OPT_READ_TIMEOUT, &timeout);
    mysql_options(conn, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
#ifdef MYSQL_OPT_RECONNECT
    bool reconnect = true;
    mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);
#endif
    MYSQL* res = mysql_real_connect(conn, cfg.host.c_str(),
                                    cfg.username.c_str(),
                                    cfg.password.get().c_str(),
                                    cfg.database.c_str(), cfg.port, nullptr, 0);
    if (res) {
      return conn;
    }
    error = "mysql_connect failed";
    mysql_close(conn);
    if (attempt + 1 < kMaxAttempts) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  }
  return nullptr;
}

bool EnsureSchema(MYSQL* conn, std::string& error) {
  error.clear();
  const char* query =
      "CREATE TABLE IF NOT EXISTS mi_state_blob ("
      "scope VARCHAR(64) NOT NULL,"
      "key_name VARCHAR(191) NOT NULL,"
      "version BIGINT NOT NULL DEFAULT 0,"
      "payload LONGBLOB NOT NULL,"
      "updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP "
      "ON UPDATE CURRENT_TIMESTAMP,"
      "PRIMARY KEY (scope, key_name)"
      ") ENGINE=InnoDB";
  if (mysql_query(conn, query) != 0) {
    error = "mysql schema create failed";
    return false;
  }
  return true;
}

#endif  // MI_E2EE_ENABLE_MYSQL

}  // namespace

std::unique_ptr<StateStore> CreateMysqlStateStore(
    const MySqlConfig& cfg,
    MetadataProtector* metadata_protector,
    std::string& error) {
#ifdef MI_E2EE_ENABLE_MYSQL
  MYSQL* conn = ConnectMysql(cfg, error);
  if (!conn) {
    return nullptr;
  }
  if (!EnsureSchema(conn, error)) {
    mysql_close(conn);
    return nullptr;
  }
  return std::make_unique<MysqlStateStore>(conn, metadata_protector);
#else
  (void)cfg;
  (void)metadata_protector;
  error = "mysql backend disabled";
  return nullptr;
#endif
}

}  // namespace mi::server
