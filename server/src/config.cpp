#include "config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include "path_security.h"
#include "platform_secure_store.h"

namespace mi::server {

namespace {

std::string Trim(const std::string& input) {
  const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  auto begin = std::find_if_not(input.begin(), input.end(), is_space);
  auto end = std::find_if_not(input.rbegin(), input.rend(), is_space).base();
  if (begin >= end) {
    return {};
  }
  return std::string(begin, end);
}

std::string ToLower(std::string s) {
  for (auto& ch : s) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return s;
}

std::string StripInlineComment(const std::string& input) {
  for (std::size_t i = 0; i < input.size(); ++i) {
    const char ch = input[i];
    if ((ch == '#' || ch == ';') &&
        (i == 0 ||
         std::isspace(static_cast<unsigned char>(input[i - 1])) != 0)) {
      return Trim(input.substr(0, i));
    }
  }
  return input;
}

bool ParseUint16(const std::string& text, std::uint16_t& out) {
  if (text.empty()) {
    return false;
  }
  char* end_ptr = nullptr;
  const long value = std::strtol(text.c_str(), &end_ptr, 10);
  if (end_ptr == text.c_str() || value < 0 || value > 65535) {
    return false;
  }
  out = static_cast<std::uint16_t>(value);
  return true;
}

bool ParseUint32(const std::string& text, std::uint32_t& out) {
  if (text.empty()) {
    return false;
  }
  char* end_ptr = nullptr;
  const unsigned long value = std::strtoul(text.c_str(), &end_ptr, 10);
  if (end_ptr == text.c_str() || *end_ptr != '\0' ||
      value > (std::numeric_limits<std::uint32_t>::max)()) {
    return false;
  }
  out = static_cast<std::uint32_t>(value);
  return true;
}

bool ParseBool(const std::string& text, bool& out) {
  if (text == "1" || text == "true" || text == "on") {
    out = true;
    return true;
  }
  if (text == "0" || text == "false" || text == "off") {
    out = false;
    return true;
  }
  return false;
}

bool ParseKeyProtection(const std::string& text, KeyProtectionMode& out) {
  const std::string t = ToLower(Trim(text));
  if (t.empty() || t == "none" || t == "off" || t == "0") {
    out = KeyProtectionMode::kNone;
    return true;
  }
  if (t == "dpapi" || t == "dpapi_user" || t == "user") {
    out = KeyProtectionMode::kDpapiUser;
    return true;
  }
  if (t == "dpapi_machine" || t == "machine") {
    out = KeyProtectionMode::kDpapiMachine;
    return true;
  }
  return false;
}

struct IniState {
  std::string section;
  ServerConfig* cfg{nullptr};
};

bool CheckPathPermissions(const std::string& path, std::string& error) {
#ifdef _WIN32
  if (mi::shard::security::CheckPathNotWorldWritable(path, error)) {
    return true;
  }
  const std::string kInsecureAclPrefix = "insecure acl (world-writable)";
  if (error.compare(0, kInsecureAclPrefix.size(), kInsecureAclPrefix) != 0) {
    return false;
  }
  std::string fix_error;
  if (!mi::shard::security::HardenPathAcl(path, fix_error)) {
    if (!fix_error.empty()) {
      error = fix_error;
    }
    return false;
  }
  error.clear();
  return mi::shard::security::CheckPathNotWorldWritable(path, error);
#else
  std::error_code ec;
  const auto perms = std::filesystem::status(path, ec).permissions();
  if (ec || perms == std::filesystem::perms::unknown) {
    return true;  // best-effort on platforms that don't expose perms
  }
  const auto writable =
      std::filesystem::perms::group_write | std::filesystem::perms::others_write;
  if ((perms & writable) != std::filesystem::perms::none) {
    error = "config file permissions too permissive: " + path +
            "; fix: chmod 600 and remove group/world write";
    return false;
  }
#endif
  (void)path;
  (void)error;
  return true;
}

void ApplyKV(IniState& state, const std::string& key,
             const std::string& value) {
  if (state.section == "mode" && key == "mode") {
    state.cfg->mode = (value == "1") ? AuthMode::kDemo : AuthMode::kMySQL;
    return;
  }
  if (state.section == "mysql") {
    if (key == "mysql_ip") {
      state.cfg->mysql.host = value;
    } else if (key == "mysql_port") {
      ParseUint16(value, state.cfg->mysql.port);
    } else if (key == "mysql_database") {
      state.cfg->mysql.database = value;
    } else if (key == "mysql_username") {
      state.cfg->mysql.username = value;
    } else if (key == "mysql_password") {
      state.cfg->mysql.password.set(value);
    }
    return;
  }
  if (state.section == "server") {
    if (key == "list_port") {
      ParseUint16(value, state.cfg->server.listen_port);
    } else if (key == "rotation_threshold") {
      ParseUint32(value, state.cfg->server.group_rotation_threshold);
    } else if (key == "offline_dir") {
      state.cfg->server.offline_dir = value;
    } else if (key == "debug_log") {
      ParseBool(value, state.cfg->server.debug_log);
    } else if (key == "session_ttl_sec") {
      ParseUint32(value, state.cfg->server.session_ttl_sec);
    } else if (key == "max_connections") {
      ParseUint32(value, state.cfg->server.max_connections);
    } else if (key == "max_connections_per_ip") {
      ParseUint32(value, state.cfg->server.max_connections_per_ip);
    } else if (key == "max_connection_bytes") {
      ParseUint32(value, state.cfg->server.max_connection_bytes);
    } else if (key == "max_worker_threads") {
      ParseUint32(value, state.cfg->server.max_worker_threads);
    } else if (key == "max_io_threads") {
      ParseUint32(value, state.cfg->server.max_io_threads);
    } else if (key == "max_pending_tasks") {
      ParseUint32(value, state.cfg->server.max_pending_tasks);
#ifdef _WIN32
    } else if (key == "iocp_enable") {
      ParseBool(value, state.cfg->server.iocp_enable);
#endif
    } else if (key == "tls_enable") {
      ParseBool(value, state.cfg->server.tls_enable);
    } else if (key == "require_tls") {
      if (ParseBool(value, state.cfg->server.require_tls)) {
        state.cfg->server.require_tls_set = true;
      }
    } else if (key == "tls_cert") {
      state.cfg->server.tls_cert = value;
    } else if (key == "kt_signing_key") {
      state.cfg->server.kt_signing_key = value;
    } else if (key == "key_protection") {
      ParseKeyProtection(value, state.cfg->server.key_protection);
    } else if (key == "allow_legacy_login") {
      ParseBool(value, state.cfg->server.allow_legacy_login);
    } else if (key == "secure_delete_enabled") {
      ParseBool(value, state.cfg->server.secure_delete_enabled);
    } else if (key == "secure_delete_required") {
      ParseBool(value, state.cfg->server.secure_delete_required);
    } else if (key == "secure_delete_plugin") {
      state.cfg->server.secure_delete_plugin = value;
    } else if (key == "secure_delete_plugin_sha256") {
      state.cfg->server.secure_delete_plugin_sha256 = value;
    } else if (key == "ops_enable") {
      ParseBool(value, state.cfg->server.ops_enable);
    } else if (key == "ops_allow_remote") {
      ParseBool(value, state.cfg->server.ops_allow_remote);
    } else if (key == "ops_token") {
      state.cfg->server.ops_token.set(value);
    }
    return;
  }
  if (state.section == "kcp") {
    if (key == "enable") {
      ParseBool(value, state.cfg->server.kcp_enable);
    } else if (key == "listen_port") {
      ParseUint16(value, state.cfg->server.kcp_port);
    } else if (key == "mtu") {
      ParseUint32(value, state.cfg->server.kcp_mtu);
    } else if (key == "snd_wnd") {
      ParseUint32(value, state.cfg->server.kcp_snd_wnd);
    } else if (key == "rcv_wnd") {
      ParseUint32(value, state.cfg->server.kcp_rcv_wnd);
    } else if (key == "nodelay") {
      ParseUint32(value, state.cfg->server.kcp_nodelay);
    } else if (key == "interval") {
      ParseUint32(value, state.cfg->server.kcp_interval);
    } else if (key == "resend") {
      ParseUint32(value, state.cfg->server.kcp_resend);
    } else if (key == "nc") {
      ParseUint32(value, state.cfg->server.kcp_nc);
    } else if (key == "min_rto") {
      ParseUint32(value, state.cfg->server.kcp_min_rto);
    } else if (key == "session_idle_sec") {
      ParseUint32(value, state.cfg->server.kcp_session_idle_sec);
    }
    return;
  }
  if (state.section == "call") {
    if (key == "enable_group_call") {
      ParseBool(value, state.cfg->call.enable_group_call);
    } else if (key == "max_room_size") {
      ParseUint32(value, state.cfg->call.max_room_size);
    } else if (key == "idle_timeout_sec") {
      ParseUint32(value, state.cfg->call.idle_timeout_sec);
    } else if (key == "call_timeout_sec") {
      ParseUint32(value, state.cfg->call.call_timeout_sec);
    } else if (key == "media_ttl_ms") {
      ParseUint32(value, state.cfg->call.media_ttl_ms);
    } else if (key == "max_subscriptions") {
      ParseUint32(value, state.cfg->call.max_subscriptions);
    }
    return;
  }
}

bool ParseIni(const std::string& path, ServerConfig& out, std::string& error) {
  std::ifstream file(path);
  if (!file.is_open()) {
    error = "config file not found: " + path;
    return false;
  }

  IniState state;
  state.cfg = &out;

  std::string line;
  std::size_t line_no = 0;
  while (std::getline(file, line)) {
    ++line_no;
    const std::string trimmed = StripInlineComment(Trim(line));
    if (trimmed.empty()) {
      continue;
    }
    if (trimmed.front() == '[' && trimmed.back() == ']') {
      state.section = trimmed.substr(1, trimmed.size() - 2);
      continue;
    }
    const auto pos = trimmed.find('=');
    if (pos == std::string::npos) {
      std::ostringstream oss;
      oss << "invalid line " << line_no;
      error = oss.str();
      return false;
    }
    std::string key = Trim(trimmed.substr(0, pos));
    std::string value = Trim(trimmed.substr(pos + 1));
    ApplyKV(state, key, value);
  }
  return true;
}

}  // namespace

bool LoadConfig(const std::string& path, ServerConfig& out_config,
                std::string& error) {
  out_config = ServerConfig{};
  if (!CheckPathPermissions(path, error)) {
    return false;
  }
  if (!ParseIni(path, out_config, error)) {
    return false;
  }
  if (out_config.server.group_rotation_threshold == 0) {
    out_config.server.group_rotation_threshold = 10000;
  }
  if (out_config.call.max_room_size == 0) {
    out_config.call.max_room_size = 1000;
  }
  if (out_config.call.idle_timeout_sec == 0) {
    out_config.call.idle_timeout_sec = 60;
  }
  if (out_config.call.call_timeout_sec == 0) {
    out_config.call.call_timeout_sec = 3600;
  }
  if (out_config.call.media_ttl_ms == 0) {
    out_config.call.media_ttl_ms = 5000;
  }
  if (out_config.call.max_subscriptions == 0) {
    out_config.call.max_subscriptions = out_config.call.max_room_size;
  }
  if (out_config.server.tls_enable && !out_config.server.require_tls_set) {
    out_config.server.require_tls = true;
  }
  if (out_config.mode == AuthMode::kMySQL) {
    const bool ok = !out_config.mysql.host.empty() && out_config.mysql.port != 0 &&
                    !out_config.mysql.database.empty() &&
                    !out_config.mysql.username.empty() &&
                    out_config.mysql.password.size() > 0;
    if (!ok) {
      std::ostringstream oss;
      oss << "mysql config incomplete (missing:";
      bool first = true;
      const auto add = [&](const char* key) {
        oss << (first ? " " : ", ") << key;
        first = false;
      };
      if (out_config.mysql.host.empty()) {
        add("mysql_ip");
      }
      if (out_config.mysql.port == 0) {
        add("mysql_port");
      }
      if (out_config.mysql.database.empty()) {
        add("mysql_database");
      }
      if (out_config.mysql.username.empty()) {
        add("mysql_username");
      }
      if (out_config.mysql.password.size() == 0) {
        add("mysql_password");
      }
      oss << ")";
      error = oss.str();
      return false;
    }
  }
  if (out_config.server.listen_port == 0) {
    error = "server listen port missing";
    return false;
  }
  if (out_config.server.max_connections == 0) {
    out_config.server.max_connections = 256;
  }
  if (out_config.server.max_connections_per_ip == 0) {
    out_config.server.max_connections_per_ip = 64;
  }
  if (out_config.server.max_pending_tasks == 0) {
    out_config.server.max_pending_tasks = 1024;
  }
  if (out_config.server.max_connection_bytes < 4096) {
    error = "max_connection_bytes too small";
    return false;
  }
  if (out_config.server.key_protection != KeyProtectionMode::kNone &&
      !mi::platform::SecureStoreSupported()) {
    error = "key_protection not supported on this platform";
    return false;
  }
  if (out_config.server.require_tls && !out_config.server.tls_enable) {
    error = "require_tls=1 but tls_enable=0";
    return false;
  }
  if (out_config.server.allow_legacy_login &&
      !out_config.server.require_tls) {
    error = "legacy login requires TLS";
    return false;
  }
  if (out_config.server.tls_enable && out_config.server.tls_cert.empty()) {
    error = "tls_cert empty";
    return false;
  }
  if (out_config.server.kt_signing_key.empty()) {
    error = "kt_signing_key missing";
    return false;
  }
  if (out_config.server.secure_delete_enabled &&
      out_config.server.secure_delete_plugin.empty()) {
    error = "secure_delete_plugin missing";
    return false;
  }
  if (out_config.server.secure_delete_required &&
      !out_config.server.secure_delete_enabled) {
    error = "secure_delete_required=1 but secure_delete_enabled=0";
    return false;
  }
  if (out_config.server.secure_delete_enabled &&
      out_config.server.secure_delete_plugin_sha256.empty()) {
    error = "secure_delete_plugin_sha256 missing";
    return false;
  }
  if (out_config.server.ops_enable && out_config.server.ops_token.size() < 16) {
    error = "ops_token missing or too short (>=16 chars)";
    return false;
  }
  if (out_config.server.ops_allow_remote &&
      !out_config.server.require_tls) {
    error = "ops_allow_remote requires require_tls=1";
    return false;
  }
  if (out_config.server.kcp_enable) {
    if (out_config.server.kcp_port == 0) {
      out_config.server.kcp_port = out_config.server.listen_port;
    }
    if (out_config.server.kcp_mtu == 0) {
      out_config.server.kcp_mtu = 1400;
    }
    if (out_config.server.kcp_snd_wnd == 0) {
      out_config.server.kcp_snd_wnd = 256;
    }
    if (out_config.server.kcp_rcv_wnd == 0) {
      out_config.server.kcp_rcv_wnd = 256;
    }
    if (out_config.server.kcp_interval == 0) {
      out_config.server.kcp_interval = 10;
    }
    if (out_config.server.kcp_min_rto == 0) {
      out_config.server.kcp_min_rto = 30;
    }
    if (out_config.server.kcp_session_idle_sec == 0) {
      out_config.server.kcp_session_idle_sec = 60;
    }
  }
  return true;
}

bool LoadDemoUsers(const std::string& path, DemoUserTable& users,
                   std::string& error) {
  users.clear();
  std::ifstream file(path);
  if (!file.is_open()) {
    error = "test_user.txt not found: " + path;
    return false;
  }
  std::string line;
  std::size_t line_no = 0;
  while (std::getline(file, line)) {
    ++line_no;
    const std::string trimmed = StripInlineComment(Trim(line));
    if (trimmed.empty()) {
      continue;
    }
    const auto pos = trimmed.find(':');
    if (pos == std::string::npos || pos == 0 || pos == trimmed.size() - 1) {
      std::ostringstream oss;
      oss << "invalid test user line " << line_no;
      error = oss.str();
      return false;
    }
    const std::string username = trimmed.substr(0, pos);
    const std::string password = trimmed.substr(pos + 1);
    DemoUser user;
    user.username.set(username);
    user.password.set(password);
    user.username_plain = username;
    user.password_plain = password;
    users.emplace(username, std::move(user));
  }
  return true;
}

}  // namespace mi::server
