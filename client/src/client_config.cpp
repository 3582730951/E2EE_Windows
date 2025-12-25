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

bool ParseUint32(const std::string& text, std::uint32_t& out) {
  if (text.empty()) return false;
  char* end_ptr = nullptr;
  const unsigned long v = std::strtoul(text.c_str(), &end_ptr, 10);
  if (end_ptr == text.c_str() || v > 0xFFFFFFFFu) return false;
  out = static_cast<std::uint32_t>(v);
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

bool ParseAuthMode(const std::string& text, AuthMode& out) {
  const std::string t = ToLower(Trim(text));
  if (t.empty() || t == "legacy" || t == "plain" || t == "password" || t == "0") {
    out = AuthMode::kLegacy;
    return true;
  }
  if (t == "opaque" || t == "pake" || t == "1") {
    out = AuthMode::kOpaque;
    return true;
  }
  return false;
}

bool ParseCoverTrafficMode(const std::string& text, CoverTrafficMode& out) {
  const std::string t = ToLower(Trim(text));
  if (t.empty() || t == "auto" || t == "adaptive" || t == "2") {
    out = CoverTrafficMode::kAuto;
    return true;
  }
  if (t == "on" || t == "enable" || t == "enabled" || t == "1") {
    out = CoverTrafficMode::kOn;
    return true;
  }
  if (t == "off" || t == "disable" || t == "disabled" || t == "0") {
    out = CoverTrafficMode::kOff;
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
  bool cover_traffic_mode_set = false;
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
      } else if (key == "require_tls") {
        ParseBool(val, out_cfg.require_tls);
      } else if (key == "trust_store") {
        out_cfg.trust_store = val;
      } else if (key == "require_pinned_fingerprint") {
        ParseBool(val, out_cfg.require_pinned_fingerprint);
      } else if (key == "pinned_fingerprint") {
        out_cfg.pinned_fingerprint = val;
      } else if (key == "auth_mode") {
        if (!ParseAuthMode(val, out_cfg.auth_mode)) {
          error = "invalid auth_mode at line " + std::to_string(line_no);
          return false;
        }
      } else if (key == "allow_legacy_login") {
        ParseBool(val, out_cfg.allow_legacy_login);
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
    } else if (section == "identity") {
      if (key == "rotation_days") {
        ParseUint32(val, out_cfg.identity.rotation_days);
      } else if (key == "legacy_retention_days") {
        ParseUint32(val, out_cfg.identity.legacy_retention_days);
      } else if (key == "tpm_enable") {
        ParseBool(val, out_cfg.identity.tpm_enable);
      } else if (key == "tpm_require") {
        ParseBool(val, out_cfg.identity.tpm_require);
      }
    } else if (section == "traffic") {
      if (key == "cover_traffic_mode") {
        if (!ParseCoverTrafficMode(val, out_cfg.traffic.cover_traffic_mode)) {
          error = "invalid cover_traffic_mode at line " +
                  std::to_string(line_no);
          return false;
        }
        cover_traffic_mode_set = true;
      } else if (key == "cover_traffic_enabled") {
        bool enabled = false;
        if (!ParseBool(val, enabled)) {
          error = "invalid cover_traffic_enabled at line " +
                  std::to_string(line_no);
          return false;
        }
        if (!cover_traffic_mode_set) {
          out_cfg.traffic.cover_traffic_mode =
              enabled ? CoverTrafficMode::kOn : CoverTrafficMode::kOff;
        }
      } else if (key == "cover_traffic_interval_sec") {
        ParseUint32(val, out_cfg.traffic.cover_traffic_interval_sec);
      }
    } else if (section == "performance") {
      if (key == "pqc_precompute_pool") {
        ParseUint32(val, out_cfg.perf.pqc_precompute_pool);
      }
    } else if (section == "kt") {
      if (key == "require_signature") {
        ParseBool(val, out_cfg.kt.require_signature);
      } else if (key == "gossip_alert_threshold") {
        ParseUint32(val, out_cfg.kt.gossip_alert_threshold);
      } else if (key == "root_pubkey_hex") {
        out_cfg.kt.root_pubkey_hex = val;
      } else if (key == "root_pubkey_path") {
        out_cfg.kt.root_pubkey_path = val;
      }
    } else if (section == "kcp") {
      if (key == "enable") {
        ParseBool(val, out_cfg.kcp.enable);
      } else if (key == "server_port") {
        ParseUint16(val, out_cfg.kcp.server_port);
      } else if (key == "mtu") {
        ParseUint32(val, out_cfg.kcp.mtu);
      } else if (key == "snd_wnd") {
        ParseUint32(val, out_cfg.kcp.snd_wnd);
      } else if (key == "rcv_wnd") {
        ParseUint32(val, out_cfg.kcp.rcv_wnd);
      } else if (key == "nodelay") {
        ParseUint32(val, out_cfg.kcp.nodelay);
      } else if (key == "interval") {
        ParseUint32(val, out_cfg.kcp.interval);
      } else if (key == "resend") {
        ParseUint32(val, out_cfg.kcp.resend);
      } else if (key == "nc") {
        ParseUint32(val, out_cfg.kcp.nc);
      } else if (key == "min_rto") {
        ParseUint32(val, out_cfg.kcp.min_rto);
      } else if (key == "request_timeout_ms") {
        ParseUint32(val, out_cfg.kcp.request_timeout_ms);
      } else if (key == "session_idle_sec") {
        ParseUint32(val, out_cfg.kcp.session_idle_sec);
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
  if (out_cfg.require_tls && !out_cfg.use_tls) {
    error = "require_tls=1 but use_tls=0";
    return false;
  }
  if (out_cfg.auth_mode == AuthMode::kLegacy &&
      !out_cfg.allow_legacy_login) {
    error = "legacy auth disabled (set allow_legacy_login=1 to override)";
    return false;
  }
  if (out_cfg.auth_mode == AuthMode::kLegacy &&
      (!out_cfg.use_tls || !out_cfg.require_tls)) {
    error = "legacy auth requires TLS (use_tls=1, require_tls=1)";
    return false;
  }
  if (!out_cfg.require_pinned_fingerprint) {
    if (!out_cfg.kcp.enable) {
      error = "require_pinned_fingerprint must be enabled";
      return false;
    }
  }
  if (out_cfg.identity.tpm_require && !out_cfg.identity.tpm_enable) {
    error = "tpm_require=1 but tpm_enable=0";
    return false;
  }
  if (out_cfg.traffic.cover_traffic_interval_sec == 0) {
    out_cfg.traffic.cover_traffic_interval_sec = 30;
  }
  if (out_cfg.perf.pqc_precompute_pool > 64) {
    out_cfg.perf.pqc_precompute_pool = 64;
  }
  if (out_cfg.kt.gossip_alert_threshold == 0) {
    out_cfg.kt.gossip_alert_threshold = 3;
  }
  if (out_cfg.kt.require_signature &&
      out_cfg.kt.root_pubkey_hex.empty() &&
      out_cfg.kt.root_pubkey_path.empty()) {
    // Allow bootstrap; ClientCore will resolve or report a detailed error.
  }
  if (out_cfg.proxy.type == ProxyType::kSocks5 &&
      (out_cfg.proxy.host.empty() || out_cfg.proxy.port == 0)) {
    error = "proxy config incomplete";
    return false;
  }
  if (out_cfg.kcp.enable) {
    if (out_cfg.use_tls || out_cfg.require_tls) {
      error = "kcp enabled but use_tls/require_tls enabled";
      return false;
    }
    if (out_cfg.proxy.enabled()) {
      error = "kcp does not support proxy";
      return false;
    }
    if (out_cfg.kcp.server_port == 0) {
      out_cfg.kcp.server_port = out_cfg.server_port;
    }
    if (out_cfg.kcp.mtu == 0) {
      out_cfg.kcp.mtu = 1400;
    }
    if (out_cfg.kcp.snd_wnd == 0) {
      out_cfg.kcp.snd_wnd = 256;
    }
    if (out_cfg.kcp.rcv_wnd == 0) {
      out_cfg.kcp.rcv_wnd = 256;
    }
    if (out_cfg.kcp.interval == 0) {
      out_cfg.kcp.interval = 10;
    }
    if (out_cfg.kcp.min_rto == 0) {
      out_cfg.kcp.min_rto = 30;
    }
    if (out_cfg.kcp.request_timeout_ms == 0) {
      out_cfg.kcp.request_timeout_ms = 5000;
    }
    if (out_cfg.kcp.session_idle_sec == 0) {
      out_cfg.kcp.session_idle_sec = 60;
    }
  }
  return true;
}

}  // namespace mi::client
