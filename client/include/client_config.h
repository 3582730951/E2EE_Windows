#ifndef MI_E2EE_CLIENT_CONFIG_H
#define MI_E2EE_CLIENT_CONFIG_H

#include <cstdint>
#include <string>

namespace mi::client {

struct ClientConfig {
  std::string server_ip{"127.0.0.1"};
  std::uint16_t server_port{9000};
};

bool LoadClientConfig(const std::string& path, ClientConfig& out_cfg,
                      std::string& error);

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_CONFIG_H
