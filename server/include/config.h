#ifndef MI_E2EE_SERVER_CONFIG_H
#define MI_E2EE_SERVER_CONFIG_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "secure_types.h"

namespace mi::server {

enum class AuthMode : std::uint8_t { kMySQL = 0, kDemo = 1 };
enum class KeyProtectionMode : std::uint8_t {
  kNone = 0,
  kDpapiUser = 1,
  kDpapiMachine = 2
};

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
  std::uint32_t session_ttl_sec{0};
  std::uint32_t max_connections{256};
  std::uint32_t max_connections_per_ip{64};
  std::uint32_t max_connection_bytes{512u * 1024u * 1024u};
  std::uint32_t max_worker_threads{0};
  std::uint32_t max_io_threads{0};
  std::uint32_t max_pending_tasks{1024};
#ifdef _WIN32
  bool iocp_enable{true};
#endif
#ifdef _WIN32
  bool tls_enable{true};
#else
  bool tls_enable{false};
#endif
  bool require_tls{false};
  bool require_tls_set{false};
  std::string tls_cert{"mi_e2ee_server.pfx"};
  std::string kt_signing_key;
  KeyProtectionMode key_protection{
#ifdef _WIN32
      KeyProtectionMode::kDpapiMachine
#else
      KeyProtectionMode::kNone
#endif
  };
  bool allow_legacy_login{false};
  bool secure_delete_enabled{false};
  bool secure_delete_required{false};
  std::string secure_delete_plugin;
  std::string secure_delete_plugin_sha256;
  bool kcp_enable{false};
  std::uint16_t kcp_port{0};
  std::uint32_t kcp_mtu{1400};
  std::uint32_t kcp_snd_wnd{256};
  std::uint32_t kcp_rcv_wnd{256};
  std::uint32_t kcp_nodelay{1};
  std::uint32_t kcp_interval{10};
  std::uint32_t kcp_resend{2};
  std::uint32_t kcp_nc{1};
  std::uint32_t kcp_min_rto{30};
  std::uint32_t kcp_session_idle_sec{60};
  bool ops_enable{false};
  bool ops_allow_remote{false};
  shard::ScrambledString ops_token;
};

struct CallSection {
  bool enable_group_call{false};
  std::uint32_t max_room_size{1000};
  std::uint32_t idle_timeout_sec{60};
  std::uint32_t call_timeout_sec{3600};
  std::uint32_t media_ttl_ms{5000};
  std::uint32_t max_subscriptions{0};
};

struct ServerConfig {
  AuthMode mode{AuthMode::kMySQL};
  MySqlConfig mysql;
  ServerSection server;
  CallSection call;
};

struct DemoUser {
  shard::ScrambledString username;
  shard::ScrambledString password;
  std::string username_plain;
  std::string password_plain;
  std::vector<std::uint8_t> opaque_password_file;
};

using DemoUserTable = std::unordered_map<std::string, DemoUser>;

bool LoadConfig(const std::string& path, ServerConfig& out_config,
                std::string& error);

bool LoadDemoUsers(const std::string& path, DemoUserTable& users,
                   std::string& error);

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_CONFIG_H
