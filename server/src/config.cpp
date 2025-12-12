#include "config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

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
  if (end_ptr == text.c_str()) {
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

struct IniState {
  std::string section;
  ServerConfig* cfg{nullptr};
};

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
  if (!ParseIni(path, out_config, error)) {
    return false;
  }
  if (out_config.server.group_rotation_threshold == 0) {
    out_config.server.group_rotation_threshold = 10000;
  }
  if (out_config.mode == AuthMode::kMySQL) {
    const bool ok = !out_config.mysql.host.empty() &&
                    out_config.mysql.port != 0 &&
                    !out_config.mysql.database.empty() &&
                    !out_config.mysql.username.empty() &&
                    out_config.mysql.password.size() > 0;
    if (!ok) {
      error = "mysql config incomplete";
      return false;
    }
  }
  if (out_config.server.listen_port == 0) {
    error = "server listen port missing";
    return false;
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
