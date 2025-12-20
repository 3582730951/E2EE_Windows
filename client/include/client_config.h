#ifndef MI_E2EE_CLIENT_CONFIG_H
#define MI_E2EE_CLIENT_CONFIG_H

#include <cstdint>
#include <string>

namespace mi::client {

enum class ProxyType : std::uint8_t { kNone = 0, kSocks5 = 1 };

// Login/authentication handshake selection.
// - kLegacy: FrameType::kLogin (password verified by server; derive channel keys from credentials)
// - kOpaque: OPAQUE (PAKE) register/login (server stores opaque password file; derive keys from session key)
enum class AuthMode : std::uint8_t { kLegacy = 0, kOpaque = 1 };

enum class DeviceSyncRole : std::uint8_t { kPrimary = 0, kLinked = 1 };

struct ProxyConfig {
  ProxyType type{ProxyType::kNone};
  std::string host;
  std::uint16_t port{0};
  std::string username;
  std::string password;

  bool enabled() const {
    return type != ProxyType::kNone && !host.empty() && port != 0;
  }
};

struct DeviceSyncConfig {
  bool enabled{false};
  DeviceSyncRole role{DeviceSyncRole::kPrimary};
  std::string key_path;
};

struct IdentityConfig {
  std::uint32_t rotation_days{90};
  std::uint32_t legacy_retention_days{180};
  bool tpm_enable{true};
  bool tpm_require{false};
};

struct TrafficConfig {
  bool cover_traffic_enabled{true};
  std::uint32_t cover_traffic_interval_sec{30};
};

struct KtConfig {
  bool require_signature{true};
  std::uint32_t gossip_alert_threshold{3};
  std::string root_pubkey_hex;
  std::string root_pubkey_path;
};

struct ClientConfig {
  std::string server_ip{"127.0.0.1"};
  std::uint16_t server_port{9000};
  bool use_tls{true};
  bool require_tls{true};
  std::string trust_store{"server_trust.ini"};
  bool require_pinned_fingerprint{true};
  std::string pinned_fingerprint;
  AuthMode auth_mode{AuthMode::kOpaque};
  ProxyConfig proxy;
  DeviceSyncConfig device_sync;
  IdentityConfig identity;
  TrafficConfig traffic;
  KtConfig kt;
};

bool LoadClientConfig(const std::string& path, ClientConfig& out_cfg,
                      std::string& error);

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_CONFIG_H
