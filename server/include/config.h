#ifndef MI_E2EE_SERVER_CONFIG_H
#define MI_E2EE_SERVER_CONFIG_H

#include <cstdint>
#include <string>
#include <unordered_map>

#include "secure_types.h"

namespace mi::server {

enum class AuthMode : std::uint8_t { kMySQL = 0, kDemo = 1 };

struct MySqlConfig {
  std::string host;
  std::uint16_t port{0};
  std::string database;
  std::string username;
  shard::ScrambledString password;
};

struct ServerSection {
  std::uint16_t listen_port{0};
  std::uint32_t group_rotation_threshold{10000};
  std::string offline_dir;
  bool debug_log{false};
};

struct ServerConfig {
  AuthMode mode{AuthMode::kMySQL};
  MySqlConfig mysql;
  ServerSection server;
};

struct DemoUser {
  shard::ScrambledString username;
  shard::ScrambledString password;
  std::string username_plain;
  std::string password_plain;
};

using DemoUserTable = std::unordered_map<std::string, DemoUser>;

bool LoadConfig(const std::string& path, ServerConfig& out_config,
                std::string& error);

bool LoadDemoUsers(const std::string& path, DemoUserTable& users,
                   std::string& error);

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_CONFIG_H
