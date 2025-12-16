#include "client_config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>

namespace mi::client {

namespace {

std::string Trim(const std::string& s) {
  const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
  auto b = std::find_if_not(s.begin(), s.end(), is_space);
  auto e = std::find_if_not(s.rbegin(), s.rend(), is_space).base();
  if (b >= e) return {};
  return std::string(b, e);
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
  if (text.empty()) return false;
  char* end_ptr = nullptr;
  const long v = std::strtol(text.c_str(), &end_ptr, 10);
  if (end_ptr == text.c_str() || v < 0 || v > 65535) return false;
  out = static_cast<std::uint16_t>(v);
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

std::string ToLower(std::string s) {
  for (auto& ch : s) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return s;
}

bool ParseProxyType(const std::string& text, ProxyType& out) {
  const std::string t = ToLower(Trim(text));
  if (t.empty() || t == "none" || t == "off" || t == "0") {
    out = ProxyType::kNone;
    return true;
  }
  if (t == "socks5" || t == "socks") {
    out = ProxyType::kSocks5;
    return true;
  }
  return false;
}

bool ParseDeviceSyncRole(const std::string& text, DeviceSyncRole& out) {
  const std::string t = ToLower(Trim(text));
  if (t.empty() || t == "primary" || t == "0") {
    out = DeviceSyncRole::kPrimary;
    return true;
  }
  if (t == "linked" || t == "secondary" || t == "1") {
    out = DeviceSyncRole::kLinked;
    return true;
  }
  return false;
}

}  // namespace

bool LoadClientConfig(const std::string& path, ClientConfig& out_cfg,
                      std::string& error) {
  out_cfg = ClientConfig{};
  std::ifstream f(path);
  if (!f.is_open()) {
    error = "client_config not found: " + path;
    return false;
  }
  std::string section;
  bool saw_client_section = false;
  std::string line;
  std::size_t line_no = 0;
  while (std::getline(f, line)) {
    ++line_no;
    std::string t = StripInlineComment(Trim(line));
    if (t.empty()) continue;
    if (t.front() == '[' && t.back() == ']') {
      section = t.substr(1, t.size() - 2);
      if (section == "client") {
        saw_client_section = true;
      }
      continue;
    }
    const auto pos = t.find('=');
    if (pos == std::string::npos) {
      error = "invalid line " + std::to_string(line_no);
      return false;
    }
    std::string key = Trim(t.substr(0, pos));
    std::string val = StripInlineComment(Trim(t.substr(pos + 1)));
    if (section == "client") {
      if (key == "server_ip") {
        out_cfg.server_ip = val;
      } else if (key == "server_port") {
        ParseUint16(val, out_cfg.server_port);
      } else if (key == "use_tls") {
        ParseBool(val, out_cfg.use_tls);
      } else if (key == "trust_store") {
        out_cfg.trust_store = val;
      }
    } else if (section == "proxy") {
      if (key == "type") {
        ParseProxyType(val, out_cfg.proxy.type);
      } else if (key == "host") {
        out_cfg.proxy.host = val;
      } else if (key == "port") {
        ParseUint16(val, out_cfg.proxy.port);
      } else if (key == "username") {
        out_cfg.proxy.username = val;
      } else if (key == "password") {
        out_cfg.proxy.password = val;
      }
    } else if (section == "device_sync") {
      if (key == "enabled") {
        ParseBool(val, out_cfg.device_sync.enabled);
      } else if (key == "role") {
        ParseDeviceSyncRole(val, out_cfg.device_sync.role);
      } else if (key == "key_path") {
        out_cfg.device_sync.key_path = val;
      }
    }
  }
  if (!saw_client_section) {
    error = "client section missing";
    return false;
  }
  if (out_cfg.server_port == 0) {
    error = "server_port missing";
    return false;
  }
  if (out_cfg.proxy.type == ProxyType::kSocks5 &&
      (out_cfg.proxy.host.empty() || out_cfg.proxy.port == 0)) {
    error = "proxy config incomplete";
    return false;
  }
  return true;
}

}  // namespace mi::client
