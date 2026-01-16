#include "auth_provider.h"

#include <array>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef MI_E2EE_ENABLE_MYSQL
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#endif
#include <mysql.h>
#ifdef _WIN32
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif
#endif

#include "crypto.h"
#include "hex_utils.h"
#include "monocypher.h"
#include "opaque_pake.h"
#include "platform_time.h"

namespace mi::server {

namespace {

constexpr char kOpaquePasswordPrefix[] = "opaque1$";

bool ConstantTimeEqual(const std::vector<std::uint8_t>& a,
                       const std::vector<std::uint8_t>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  std::uint8_t diff = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    diff |= static_cast<std::uint8_t>(a[i] ^ b[i]);
  }
  return diff == 0;
}

bool ConstantTimeEqualString(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) {
    return false;
  }
  std::uint8_t diff = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    diff |= static_cast<std::uint8_t>(a[i] ^ b[i]);
  }
  return diff == 0;
}

static const std::array<std::int8_t, 256>& Base64DecodeTable() {
  static const std::array<std::int8_t, 256> table = [] {
    std::array<std::int8_t, 256> t{};
    t.fill(-1);
    for (int i = 0; i < 26; ++i) {
      t[static_cast<std::size_t>('A' + i)] = static_cast<std::int8_t>(i);
      t[static_cast<std::size_t>('a' + i)] = static_cast<std::int8_t>(26 + i);
    }
    for (int i = 0; i < 10; ++i) {
      t[static_cast<std::size_t>('0' + i)] = static_cast<std::int8_t>(52 + i);
    }
    t[static_cast<std::size_t>('+')] = 62;
    t[static_cast<std::size_t>('/')] = 63;
    return t;
  }();
  return table;
}

std::string Base64Encode(const std::vector<std::uint8_t>& in) {
  static constexpr char kB64[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((in.size() + 2) / 3) * 4);
  std::size_t i = 0;
  while (i + 3 <= in.size()) {
    const std::uint32_t v = (static_cast<std::uint32_t>(in[i]) << 16) |
                            (static_cast<std::uint32_t>(in[i + 1]) << 8) |
                            static_cast<std::uint32_t>(in[i + 2]);
    out.push_back(kB64[(v >> 18) & 0x3F]);
    out.push_back(kB64[(v >> 12) & 0x3F]);
    out.push_back(kB64[(v >> 6) & 0x3F]);
    out.push_back(kB64[v & 0x3F]);
    i += 3;
  }
  const std::size_t rem = in.size() - i;
  if (rem == 1) {
    const std::uint32_t v = (static_cast<std::uint32_t>(in[i]) << 16);
    out.push_back(kB64[(v >> 18) & 0x3F]);
    out.push_back(kB64[(v >> 12) & 0x3F]);
    out.push_back('=');
    out.push_back('=');
  } else if (rem == 2) {
    const std::uint32_t v = (static_cast<std::uint32_t>(in[i]) << 16) |
                            (static_cast<std::uint32_t>(in[i + 1]) << 8);
    out.push_back(kB64[(v >> 18) & 0x3F]);
    out.push_back(kB64[(v >> 12) & 0x3F]);
    out.push_back(kB64[(v >> 6) & 0x3F]);
    out.push_back('=');
  }
  return out;
}

bool Base64Decode(std::string_view in, std::vector<std::uint8_t>& out) {
  out.clear();
  if (in.empty()) {
    return true;
  }
  if ((in.size() % 4) != 0) {
    return false;
  }
  out.reserve((in.size() / 4) * 3);
  const auto& table = Base64DecodeTable();
  for (std::size_t i = 0; i < in.size(); i += 4) {
    const char c0 = in[i + 0];
    const char c1 = in[i + 1];
    const char c2 = in[i + 2];
    const char c3 = in[i + 3];
    const int v0 = table[static_cast<std::uint8_t>(c0)];
    const int v1 = table[static_cast<std::uint8_t>(c1)];
    if (v0 < 0 || v1 < 0) {
      out.clear();
      return false;
    }
    const bool pad2 = (c2 == '=');
    const bool pad3 = (c3 == '=');
    const int v2 = pad2 ? 0 : table[static_cast<std::uint8_t>(c2)];
    const int v3 = pad3 ? 0 : table[static_cast<std::uint8_t>(c3)];
    if (!pad2 && v2 < 0) {
      out.clear();
      return false;
    }
    if (!pad3 && v3 < 0) {
      out.clear();
      return false;
    }
    const std::uint32_t triplet =
        (static_cast<std::uint32_t>(v0) << 18) |
        (static_cast<std::uint32_t>(v1) << 12) |
        (static_cast<std::uint32_t>(v2) << 6) |
        static_cast<std::uint32_t>(v3);
    out.push_back(static_cast<std::uint8_t>((triplet >> 16) & 0xFF));
    if (!pad2) {
      out.push_back(static_cast<std::uint8_t>((triplet >> 8) & 0xFF));
    }
    if (!pad3) {
      out.push_back(static_cast<std::uint8_t>(triplet & 0xFF));
    }
    if (pad2 || pad3) {
      // Padding must only appear at the end.
      for (std::size_t j = i + 4; j < in.size(); ++j) {
        if (in[j] != '=') {
          out.clear();
          return false;
        }
      }
      break;
    }
  }
  return true;
}

bool VerifyPasswordArgon2id(const std::string& input,
                            const std::string& stored) {
  // Format: argon2id$<nb_blocks>$<nb_passes>$<salt_hex>$<hash_hex>
  static constexpr char kPrefix[] = "argon2id$";
  if (stored.rfind(kPrefix, 0) != 0) {
    return false;
  }
  std::vector<std::string> parts;
  parts.reserve(5);
  std::size_t start = 0;
  while (start <= stored.size()) {
    const auto pos = stored.find('$', start);
    if (pos == std::string::npos) {
      parts.push_back(stored.substr(start));
      break;
    }
    parts.push_back(stored.substr(start, pos - start));
    start = pos + 1;
  }
  if (parts.size() != 5) {
    return false;
  }
  if (parts[0] != "argon2id") {
    return false;
  }
  const std::uint32_t nb_blocks =
      static_cast<std::uint32_t>(std::strtoul(parts[1].c_str(), nullptr, 10));
  const std::uint32_t nb_passes =
      static_cast<std::uint32_t>(std::strtoul(parts[2].c_str(), nullptr, 10));
  if (nb_blocks < 8 || nb_passes < 1) {
    return false;
  }
  // Cap to avoid unbounded allocations (nb_blocks are 1KB blocks).
  if (nb_blocks > 262144) {  // 256MB
    return false;
  }

  std::vector<std::uint8_t> salt;
  std::vector<std::uint8_t> expected;
  if (!mi::common::HexToBytes(parts[3], salt) ||
      !mi::common::HexToBytes(parts[4], expected)) {
    return false;
  }
  if (salt.empty() || expected.empty()) {
    return false;
  }

  std::vector<std::uint8_t> work_area;
  work_area.resize(static_cast<std::size_t>(nb_blocks) * 1024);
  std::vector<std::uint8_t> computed(expected.size());

  crypto_argon2_config cfg;
  cfg.algorithm = CRYPTO_ARGON2_ID;
  cfg.nb_blocks = nb_blocks;
  cfg.nb_passes = nb_passes;
  cfg.nb_lanes = 1;

  crypto_argon2_inputs in;
  in.pass = reinterpret_cast<const std::uint8_t*>(input.data());
  in.pass_size = static_cast<std::uint32_t>(input.size());
  in.salt = salt.data();
  in.salt_size = static_cast<std::uint32_t>(salt.size());

  crypto_argon2(computed.data(), static_cast<std::uint32_t>(computed.size()),
                work_area.data(), cfg, in, crypto_argon2_no_extras);
  return ConstantTimeEqual(computed, expected);
}

bool VerifyPassword(const std::string& input, const std::string& stored) {
  if (stored.rfind(kOpaquePasswordPrefix, 0) == 0) {
    return false;
  }
  if (VerifyPasswordArgon2id(input, stored)) {
    return true;
  }
  if (stored == input) {
    return true;
  }
  //  salt:hash hash = SHA256(salt + password)
  const auto pos = stored.find(':');
  if (pos != std::string::npos) {
    const std::string salt = stored.substr(0, pos);
    const std::string hash = stored.substr(pos + 1);
    const std::string salted_input = salt + input;
    const std::string salted = mi::common::Sha256Hex(
        reinterpret_cast<const std::uint8_t*>(salted_input.data()),
        salted_input.size());
    return ConstantTimeEqualString(salted, hash);
  }
  const std::string hashed = mi::common::Sha256Hex(
      reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
  return ConstantTimeEqualString(stored, hashed);
}

struct RustBuf {
  std::uint8_t* ptr{nullptr};
  std::size_t len{0};
  ~RustBuf() {
    if (ptr && len) {
      mi_opaque_free(ptr, len);
    }
  }
};

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
  if (stored_user != username || !VerifyPassword(password, stored_pass)) {
    error = "invalid credentials";
    return false;
  }
  return true;
}

bool DemoAuthProvider::GetStoredPassword(const std::string& username,
                                         std::string& out_password,
                                         std::string& error) {
  out_password.clear();
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
  if (stored_user != username) {
    error = "user not found";
    return false;
  }
  out_password = stored_pass;
  error.clear();
  return true;
}

bool DemoAuthProvider::GetOpaqueUserRecord(const std::string& username,
                                           std::vector<std::uint8_t>& out_record,
                                           std::string& error) {
  out_record.clear();
  const auto it = users_.find(username);
  if (it == users_.end()) {
    error = "user not found";
    return false;
  }
  if (it->second.opaque_password_file.empty()) {
    error = "opaque record missing";
    return false;
  }
  out_record = it->second.opaque_password_file;
  error.clear();
  return true;
}

bool DemoAuthProvider::UpsertOpaqueUserRecord(
    const std::string& username,
    const std::vector<std::uint8_t>& record,
    std::string& error) {
  if (username.empty()) {
    error = "username empty";
    return false;
  }
  if (record.empty()) {
    error = "opaque record empty";
    return false;
  }
  auto it = users_.find(username);
  if (it == users_.end()) {
    DemoUser user;
    user.username.set(username);
    user.password.set("");
    user.username_plain = username;
    user.password_plain.clear();
    user.opaque_password_file = record;
    users_.emplace(username, std::move(user));
    error.clear();
    return true;
  }
  it->second.opaque_password_file = record;
  error.clear();
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

#ifdef MI_E2EE_ENABLE_MYSQL
namespace {

std::atomic<bool> g_mysql_user_auth_ready{false};

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
      mi::platform::SleepMs(200);
    }
  }
  return nullptr;
}

bool EnsureMySqlUserAuthTable(MYSQL* conn, std::string& error) {
  if (!conn) {
    error = "mysql connection missing";
    return false;
  }
  if (g_mysql_user_auth_ready.load(std::memory_order_acquire)) {
    return true;
  }
  const char* ddl =
      "CREATE TABLE IF NOT EXISTS user_auth ("
      "  username VARCHAR(64) NOT NULL,"
      "  password MEDIUMTEXT NOT NULL,"
      "  PRIMARY KEY (username)"
      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
  if (mysql_query(conn, ddl) != 0) {
    error = "mysql create user_auth failed";
    return false;
  }
  g_mysql_user_auth_ready.store(true, std::memory_order_release);
  error.clear();
  return true;
}

bool MySqlFetchPassword(MYSQL* conn, const std::string& username,
                        std::string& out_password, std::string& error) {
  out_password.clear();
  const char* query = "SELECT password FROM user_auth WHERE username=? LIMIT 1";
  MYSQL_STMT* stmt = mysql_stmt_init(conn);
  if (!stmt) {
    error = "mysql_stmt_init failed";
    return false;
  }
  if (mysql_stmt_prepare(stmt, query,
                         static_cast<unsigned long>(std::strlen(query))) != 0) {
    error = "mysql_stmt_prepare failed";
    mysql_stmt_close(stmt);
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
    return false;
  }
  if (mysql_stmt_execute(stmt) != 0) {
    error = "mysql_stmt_execute failed";
    mysql_stmt_close(stmt);
    return false;
  }

  std::vector<char> buf(256);
  unsigned long pass_len = 0;
  using BindBool = std::remove_pointer_t<decltype(std::declval<MYSQL_BIND>().is_null)>;
  BindBool is_null = 0;
  BindBool error_flag = 0;
  MYSQL_BIND bind_result[1];
  std::memset(bind_result, 0, sizeof(bind_result));
  bind_result[0].buffer_type = MYSQL_TYPE_STRING;
  bind_result[0].buffer = buf.data();
  bind_result[0].buffer_length = static_cast<unsigned long>(buf.size());
  bind_result[0].length = &pass_len;
  bind_result[0].is_null = &is_null;
  bind_result[0].error = &error_flag;

  if (mysql_stmt_bind_result(stmt, bind_result) != 0) {
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

  int fetch_status = mysql_stmt_fetch(stmt);
  if (fetch_status == MYSQL_NO_DATA || is_null) {
    error = "user not found";
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return false;
  }
  if (fetch_status != 0 && fetch_status != MYSQL_DATA_TRUNCATED) {
    error = "mysql_stmt_fetch failed";
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return false;
  }

  if (fetch_status == MYSQL_DATA_TRUNCATED) {
    if (pass_len == 0 || pass_len > (16U * 1024U * 1024U)) {
      error = "mysql password field too large";
      mysql_stmt_free_result(stmt);
      mysql_stmt_close(stmt);
      return false;
    }
    buf.assign(static_cast<std::size_t>(pass_len), '\0');
    MYSQL_BIND col;
    std::memset(&col, 0, sizeof(col));
    col.buffer_type = MYSQL_TYPE_STRING;
    col.buffer = buf.data();
    col.buffer_length = pass_len;
    col.length = &pass_len;
    col.is_null = &is_null;
    col.error = &error_flag;
    if (mysql_stmt_fetch_column(stmt, &col, 0, 0) != 0) {
      error = "mysql_stmt_fetch_column failed";
      mysql_stmt_free_result(stmt);
      mysql_stmt_close(stmt);
      return false;
    }
  }

  if (pass_len > buf.size()) {
    error = "mysql password length invalid";
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return false;
  }
  out_password.assign(buf.data(), static_cast<std::size_t>(pass_len));

  mysql_stmt_free_result(stmt);
  mysql_stmt_close(stmt);
  error.clear();
  return true;
}

bool MySqlStoreOpaqueRecord(MYSQL* conn, const std::string& username,
                            const std::string& opaque_value,
                            std::string& error) {
  const char* query =
      "INSERT INTO user_auth (username,password) VALUES (?,?) "
      "ON DUPLICATE KEY UPDATE password=VALUES(password)";
  MYSQL_STMT* stmt = mysql_stmt_init(conn);
  if (!stmt) {
    error = "mysql_stmt_init failed";
    return false;
  }
  if (mysql_stmt_prepare(stmt, query,
                         static_cast<unsigned long>(std::strlen(query))) != 0) {
    error = "mysql_stmt_prepare failed";
    mysql_stmt_close(stmt);
    return false;
  }

  MYSQL_BIND bind_param[2];
  std::memset(bind_param, 0, sizeof(bind_param));
  bind_param[0].buffer_type = MYSQL_TYPE_STRING;
  bind_param[0].buffer = const_cast<char*>(username.c_str());
  bind_param[0].buffer_length = static_cast<unsigned long>(username.size());
  bind_param[1].buffer_type = MYSQL_TYPE_STRING;
  bind_param[1].buffer = const_cast<char*>(opaque_value.c_str());
  bind_param[1].buffer_length = static_cast<unsigned long>(opaque_value.size());

  if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
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
  error.clear();
  return true;
}

}  // namespace
#endif

bool MySqlAuthProvider::Validate(const std::string& username,
                                 const std::string& password,
                                 std::string& error) {
#ifndef MI_E2EE_ENABLE_MYSQL
  error = "mysql provider not built (enable MI_E2EE_ENABLE_MYSQL)";
  return false;
#else
  MYSQL* conn = ConnectMysql(cfg_, error);
  if (!conn) {
    return false;
  }
  if (!EnsureMySqlUserAuthTable(conn, error)) {
    mysql_close(conn);
    return false;
  }

  std::string stored_pass;
  if (!MySqlFetchPassword(conn, username, stored_pass, error)) {
    mysql_close(conn);
    return false;
  }
  const bool ok = VerifyPassword(password, stored_pass);
  mysql_close(conn);

  if (!ok) {
    error = "invalid credentials";
    return false;
  }
  return true;
#endif
}

bool MySqlAuthProvider::GetStoredPassword(const std::string& username,
                                          std::string& out_password,
                                          std::string& error) {
  out_password.clear();
#ifndef MI_E2EE_ENABLE_MYSQL
  error = "mysql provider not built (enable MI_E2EE_ENABLE_MYSQL)";
  return false;
#else
  MYSQL* conn = ConnectMysql(cfg_, error);
  if (!conn) {
    return false;
  }
  if (!EnsureMySqlUserAuthTable(conn, error)) {
    mysql_close(conn);
    return false;
  }

  const bool ok = MySqlFetchPassword(conn, username, out_password, error);
  mysql_close(conn);
  return ok;
#endif
}

bool MySqlAuthProvider::GetOpaqueUserRecord(const std::string& username,
                                            std::vector<std::uint8_t>& out_record,
                                            std::string& error) {
  out_record.clear();
  std::string stored;
  if (!GetStoredPassword(username, stored, error)) {
    return false;
  }
  if (stored.rfind(kOpaquePasswordPrefix, 0) != 0) {
    error = "opaque record missing";
    return false;
  }
  std::string_view b64(stored);
  b64.remove_prefix(sizeof(kOpaquePasswordPrefix) - 1);
  if (!Base64Decode(b64, out_record) || out_record.empty()) {
    error = "opaque record invalid";
    out_record.clear();
    return false;
  }
  error.clear();
  return true;
}

bool MySqlAuthProvider::UpsertOpaqueUserRecord(
    const std::string& username,
    const std::vector<std::uint8_t>& record,
    std::string& error) {
  if (username.empty()) {
    error = "username empty";
    return false;
  }
  if (record.empty()) {
    error = "opaque record empty";
    return false;
  }
#ifndef MI_E2EE_ENABLE_MYSQL
  error = "mysql provider not built (enable MI_E2EE_ENABLE_MYSQL)";
  return false;
#else
  MYSQL* conn = ConnectMysql(cfg_, error);
  if (!conn) {
    return false;
  }
  if (!EnsureMySqlUserAuthTable(conn, error)) {
    mysql_close(conn);
    return false;
  }
  const std::string opaque_value =
      std::string(kOpaquePasswordPrefix) + Base64Encode(record);
  const bool ok = MySqlStoreOpaqueRecord(conn, username, opaque_value, error);
  mysql_close(conn);
  return ok;
#endif
}

bool MySqlAuthProvider::UserExists(const std::string& username,
                                   std::string& error) {
#ifndef MI_E2EE_ENABLE_MYSQL
  error = "mysql provider not built (enable MI_E2EE_ENABLE_MYSQL)";
  return false;
#else
  MYSQL* conn = ConnectMysql(cfg_, error);
  if (!conn) {
    return false;
  }
  if (!EnsureMySqlUserAuthTable(conn, error)) {
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

std::unique_ptr<AuthProvider> MakeAuthProvider(
    const ServerConfig& cfg,
    const std::vector<std::uint8_t>& opaque_server_setup,
    std::string& error) {
  if (cfg.mode == AuthMode::kDemo) {
    DemoUserTable table;
    const std::string demo_path = "test_user.txt";
    if (!LoadDemoUsers(demo_path, table, error)) {
      return nullptr;
    }
    if (!opaque_server_setup.empty()) {
      for (auto& kv : table) {
        DemoUser& user = kv.second;
        if (user.username_plain.empty() || user.password_plain.empty()) {
          continue;
        }
        RustBuf out_file;
        RustBuf out_err;
        const int rc = mi_opaque_create_user_password_file(
            opaque_server_setup.data(), opaque_server_setup.size(),
            reinterpret_cast<const std::uint8_t*>(user.username_plain.data()),
            user.username_plain.size(),
            reinterpret_cast<const std::uint8_t*>(user.password_plain.data()),
            user.password_plain.size(), &out_file.ptr, &out_file.len,
            &out_err.ptr, &out_err.len);
        if (rc != 0 || !out_file.ptr || out_file.len == 0) {
          if (out_err.ptr && out_err.len) {
            error.assign(reinterpret_cast<const char*>(out_err.ptr),
                         out_err.len);
          } else {
            error = "opaque demo provisioning failed";
          }
          return nullptr;
        }
        user.opaque_password_file.assign(out_file.ptr,
                                         out_file.ptr + out_file.len);
      }
    }
    return std::make_unique<DemoAuthProvider>(std::move(table));
  }

#ifndef MI_E2EE_ENABLE_MYSQL
  error = "mysql mode requested but mysql provider not built; rebuild with -DMI_E2EE_ENABLE_MYSQL=ON or set [mode] mode=1";
  return nullptr;
#else
  return std::make_unique<MySqlAuthProvider>(cfg.mysql);
#endif
}

}  // namespace mi::server
