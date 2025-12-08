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

bool ParseUint16(const std::string& text, std::uint16_t& out) {
  if (text.empty()) return false;
  char* end_ptr = nullptr;
  const long v = std::strtol(text.c_str(), &end_ptr, 10);
  if (end_ptr == text.c_str() || v < 0 || v > 65535) return false;
  out = static_cast<std::uint16_t>(v);
  return true;
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
  std::string line;
  std::size_t line_no = 0;
  while (std::getline(f, line)) {
    ++line_no;
    std::string t = Trim(line);
    if (t.empty() || t[0] == '#') continue;
    if (t.front() == '[' && t.back() == ']') {
      section = t.substr(1, t.size() - 2);
      continue;
    }
    const auto pos = t.find('=');
    if (pos == std::string::npos) {
      error = "invalid line " + std::to_string(line_no);
      return false;
    }
    std::string key = Trim(t.substr(0, pos));
    std::string val = Trim(t.substr(pos + 1));
    if (section == "client") {
      if (key == "server_ip") {
        out_cfg.server_ip = val;
      } else if (key == "server_port") {
        ParseUint16(val, out_cfg.server_port);
      }
    }
  }
  if (out_cfg.server_port == 0) {
    error = "server_port missing";
    return false;
  }
  return true;
}

}  // namespace mi::client
