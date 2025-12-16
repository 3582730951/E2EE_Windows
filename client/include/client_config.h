#ifndef MI_E2EE_CLIENT_CONFIG_H
#define MI_E2EE_CLIENT_CONFIG_H

#include <cstdint>
#include <string>

namespace mi::client {

enum class ProxyType : std::uint8_t { kNone = 0, kSocks5 = 1 };

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

struct ClientConfig {
  std::string server_ip{"127.0.0.1"};
  std::uint16_t server_port{9000};
  bool use_tls{true};
  std::string trust_store{"server_trust.ini"};
  ProxyConfig proxy;
  DeviceSyncConfig device_sync;
};

bool LoadClientConfig(const std::string& path, ClientConfig& out_cfg,
                      std::string& error);

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_CONFIG_H
