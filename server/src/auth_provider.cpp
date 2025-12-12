#include "auth_provider.h"

#include <cstring>
#include <type_traits>
#include <utility>

#ifdef MI_E2EE_ENABLE_MYSQL
#include <mysql.h>
#include "crypto.h"
#endif

namespace mi::server {

namespace {

#ifdef MI_E2EE_ENABLE_MYSQL
std::string Sha256Hex(const std::string& data) {
  crypto::Sha256Digest d;
  crypto::Sha256(reinterpret_cast<const std::uint8_t*>(data.data()),
                 data.size(), d);
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(d.bytes.size() * 2);
  for (std::size_t i = 0; i < d.bytes.size(); ++i) {
    out[i * 2] = kHex[d.bytes[i] >> 4];
    out[i * 2 + 1] = kHex[d.bytes[i] & 0x0F];
  }
  return out;
}

bool VerifyPassword(const std::string& input, const std::string& stored) {
  if (stored == input) {
    return true;
  }
  //  salt:hash hash = SHA256(salt + password)
  const auto pos = stored.find(':');
  if (pos != std::string::npos) {
    const std::string salt = stored.substr(0, pos);
    const std::string hash = stored.substr(pos + 1);
    const std::string salted = Sha256Hex(salt + input);
    return salted == hash;
  }
  const std::string hashed = Sha256Hex(input);
  return stored == hashed;
}
#endif

}  // namespace

DemoAuthProvider::DemoAuthProvider(DemoUserTable users)
    : users_(std::move(users)) {}

bool DemoAuthProvider::Validate(const std::string& username,
                                const std::string& password,
                                std::string& error) {
  const auto it = users_.find(username);
  if (it == users_.end()) {
    error = "user not found";
    return false;
  }
  const std::string stored_user =
      it->second.username_plain.empty() ? it->second.username.get()
                                        : it->second.username_plain;
  const std::string stored_pass =
      it->second.password_plain.empty() ? it->second.password.get()
                                        : it->second.password_plain;
  if (stored_user != username || stored_pass != password) {
    error = "invalid credentials";
    return false;
  }
  return true;
}

bool DemoAuthProvider::UserExists(const std::string& username,
                                  std::string& error) {
  if (username.empty()) {
    error = "username empty";
    return false;
  }
  const auto it = users_.find(username);
  if (it == users_.end()) {
    error = "user not found";
    return false;
  }
  error.clear();
  return true;
}

MySqlAuthProvider::MySqlAuthProvider(MySqlConfig cfg) : cfg_(std::move(cfg)) {}

bool MySqlAuthProvider::Validate(const std::string& username,
                                 const std::string& password,
                                 std::string& error) {
#ifndef MI_E2EE_ENABLE_MYSQL
  error = "mysql provider not built (enable MI_E2EE_ENABLE_MYSQL)";
  return false;
#else
  MYSQL* conn = mysql_init(nullptr);
  if (!conn) {
    error = "mysql_init failed";
    return false;
  }

  MYSQL* res = mysql_real_connect(conn, cfg_.host.c_str(),
                                  cfg_.username.c_str(),
                                  cfg_.password.get().c_str(),
                                  cfg_.database.c_str(), cfg_.port, nullptr, 0);
  if (!res) {
    error = "mysql_connect failed";
    mysql_close(conn);
    return false;
  }

  const char* query = "SELECT password FROM user_auth WHERE username=? LIMIT 1";
  MYSQL_STMT* stmt = mysql_stmt_init(conn);
  if (!stmt) {
    error = "mysql_stmt_init failed";
    mysql_close(conn);
    return false;
  }
  if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(std::strlen(query))) != 0) {
    error = "mysql_stmt_prepare failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }

  MYSQL_BIND bind_param[1];
  std::memset(bind_param, 0, sizeof(bind_param));
  bind_param[0].buffer_type = MYSQL_TYPE_STRING;
  bind_param[0].buffer = const_cast<char*>(username.c_str());
  bind_param[0].buffer_length = static_cast<unsigned long>(username.size());

  if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
    error = "mysql_stmt_bind_param failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }

  if (mysql_stmt_execute(stmt) != 0) {
    error = "mysql_stmt_execute failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }

  MYSQL_BIND bind_result[1];
  std::memset(bind_result, 0, sizeof(bind_result));
  char pass_buf[256] = {0};
  unsigned long pass_len = 0;
  using BindBool = std::remove_pointer_t<decltype(bind_result[0].is_null)>;
  BindBool is_null = 0;
  BindBool error_flag = 0;
  bind_result[0].buffer_type = MYSQL_TYPE_STRING;
  bind_result[0].buffer = pass_buf;
  bind_result[0].buffer_length = sizeof(pass_buf) - 1;
  bind_result[0].length = &pass_len;
  bind_result[0].is_null = &is_null;
  bind_result[0].error = &error_flag;

  if (mysql_stmt_bind_result(stmt, bind_result) != 0) {
    error = "mysql_stmt_bind_result failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }

  if (mysql_stmt_store_result(stmt) != 0) {
    error = "mysql_stmt_store_result failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }

  int fetch_status = mysql_stmt_fetch(stmt);
  if (fetch_status == MYSQL_NO_DATA || is_null) {
    error = "user not found";
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }
  if (fetch_status != 0 && fetch_status != MYSQL_DATA_TRUNCATED) {
    error = "mysql_stmt_fetch failed";
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }

  pass_buf[pass_len] = '\0';
  const std::string stored_pass(pass_buf, pass_len);
  const bool ok = VerifyPassword(password, stored_pass);

  mysql_stmt_free_result(stmt);
  mysql_stmt_close(stmt);
  mysql_close(conn);

  if (!ok) {
    error = "invalid credentials";
    return false;
  }
  return true;
#endif
}

bool MySqlAuthProvider::UserExists(const std::string& username,
                                   std::string& error) {
#ifndef MI_E2EE_ENABLE_MYSQL
  error = "mysql provider not built (enable MI_E2EE_ENABLE_MYSQL)";
  return false;
#else
  MYSQL* conn = mysql_init(nullptr);
  if (!conn) {
    error = "mysql_init failed";
    return false;
  }

  MYSQL* res = mysql_real_connect(conn, cfg_.host.c_str(),
                                  cfg_.username.c_str(),
                                  cfg_.password.get().c_str(),
                                  cfg_.database.c_str(), cfg_.port, nullptr, 0);
  if (!res) {
    error = "mysql_connect failed";
    mysql_close(conn);
    return false;
  }

  const char* query = "SELECT 1 FROM user_auth WHERE username=? LIMIT 1";
  MYSQL_STMT* stmt = mysql_stmt_init(conn);
  if (!stmt) {
    error = "mysql_stmt_init failed";
    mysql_close(conn);
    return false;
  }
  if (mysql_stmt_prepare(stmt, query,
                         static_cast<unsigned long>(std::strlen(query))) != 0) {
    error = "mysql_stmt_prepare failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }

  MYSQL_BIND bind_param[1];
  std::memset(bind_param, 0, sizeof(bind_param));
  bind_param[0].buffer_type = MYSQL_TYPE_STRING;
  bind_param[0].buffer = const_cast<char*>(username.c_str());
  bind_param[0].buffer_length = static_cast<unsigned long>(username.size());

  if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
    error = "mysql_stmt_bind_param failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }
  if (mysql_stmt_execute(stmt) != 0) {
    error = "mysql_stmt_execute failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
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
    error = "mysql_stmt_bind_result failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }
  if (mysql_stmt_store_result(stmt) != 0) {
    error = "mysql_stmt_store_result failed";
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }

  int fetch_status = mysql_stmt_fetch(stmt);
  mysql_stmt_free_result(stmt);
  mysql_stmt_close(stmt);
  mysql_close(conn);

  if (fetch_status == MYSQL_NO_DATA || is_null) {
    error = "user not found";
    return false;
  }
  if (fetch_status != 0 && fetch_status != MYSQL_DATA_TRUNCATED) {
    error = "mysql_stmt_fetch failed";
    return false;
  }

  error.clear();
  return true;
#endif
}

std::unique_ptr<AuthProvider> MakeAuthProvider(const ServerConfig& cfg,
                                               std::string& error) {
  if (cfg.mode == AuthMode::kDemo) {
    DemoUserTable table;
    const std::string demo_path = "test_user.txt";
    if (!LoadDemoUsers(demo_path, table, error)) {
      return nullptr;
    }
    return std::make_unique<DemoAuthProvider>(std::move(table));
  }

  return std::make_unique<MySqlAuthProvider>(cfg.mysql);
}

}  // namespace mi::server
