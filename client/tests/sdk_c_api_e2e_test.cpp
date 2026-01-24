#include "c_api_client.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "listener.h"
#include "network_server.h"
#include "platform_time.h"
#include "server_app.h"

namespace {

std::string GetEnv(const char* name) {
#ifdef _WIN32
  if (!name || *name == '\0') {
    return {};
  }
  size_t len = 0;
  (void)getenv_s(&len, nullptr, 0, name);
  if (len == 0) {
    return {};
  }
  std::string out(len - 1, '\0');
  (void)getenv_s(&len, out.data(), out.size() + 1, name);
  return out;
#else
  const char* value = std::getenv(name);
  return value ? std::string(value) : std::string();
#endif
}

bool SetEnv(const char* name, const std::string& value) {
#ifdef _WIN32
  return _putenv_s(name, value.c_str()) == 0;
#else
  if (value.empty()) {
    return ::unsetenv(name) == 0;
  }
  return ::setenv(name, value.c_str(), 1) == 0;
#endif
}

struct UserFileBackup {
  bool existed{false};
  std::string content;
};

UserFileBackup BackupTestUsers() {
  UserFileBackup backup;
  std::error_code ec;
  const auto path = std::filesystem::current_path(ec) / "test_user.txt";
  if (ec || !std::filesystem::exists(path, ec) || ec) {
    return backup;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return backup;
  }
  backup.existed = true;
  backup.content.assign(std::istreambuf_iterator<char>(in),
                        std::istreambuf_iterator<char>());
  return backup;
}

void RestoreTestUsers(const UserFileBackup& backup) {
  std::error_code ec;
  const auto path = std::filesystem::current_path(ec) / "test_user.txt";
  if (backup.existed) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << backup.content;
    out.flush();
    return;
  }
  std::filesystem::remove(path, ec);
}

bool WriteTestUsers() {
  std::error_code ec;
  const auto path = std::filesystem::current_path(ec) / "test_user.txt";
  if (ec) {
    return false;
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out << "alice:alice123\n";
  out << "bob:bob123\n";
  out.flush();
  return static_cast<bool>(out);
}

std::filesystem::path MakeUniqueDir(const std::string& prefix) {
  const auto now = static_cast<unsigned long long>(
      mi::platform::NowSteadyMs());
  std::filesystem::path dir =
      std::filesystem::current_path() / (prefix + "_" + std::to_string(now));
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir;
}

std::string WriteServerConfig(const std::filesystem::path& dir,
                              std::uint16_t port) {
  const auto path = dir / "server_config.ini";
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << "[mode]\n";
  out << "mode=1\n";
  out << "[server]\n";
  out << "list_port=" << port << "\n";
  out << "offline_dir=" << (dir / "offline_store").string() << "\n";
  out << "debug_log=0\n";
  out << "tls_enable=0\n";
  out << "require_tls=0\n";
  out << "key_protection=none\n";
  out << "kt_signing_key=" << (dir / "kt_signing_key.bin").string() << "\n";
  out << "allow_legacy_login=0\n";
  out << "[call]\n";
  out << "enable_group_call=0\n";
  out << "[kcp]\n";
  out << "enable=0\n";
  out.flush();
  return path.string();
}

std::string WriteClientConfig(const std::filesystem::path& dir,
                              std::uint16_t port,
                              bool device_sync,
                              bool primary) {
  const auto path = dir / "client_config.ini";
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << "[client]\n";
  out << "server_ip=127.0.0.1\n";
  out << "server_port=" << port << "\n";
  out << "use_tls=0\n";
  out << "require_tls=0\n";
  out << "tls_verify_mode=ca\n";
  out << "auth_mode=opaque\n";
  out << "allow_legacy_login=0\n";
  out << "\n[traffic]\n";
  out << "cover_traffic_enabled=0\n";
  out << "\n[kt]\n";
  out << "require_signature=0\n";
  out << "\n[device_sync]\n";
  out << "enabled=" << (device_sync ? 1 : 0) << "\n";
  out << "role=" << (primary ? "primary" : "linked") << "\n";
  out << "ratchet_enable=1\n";
  out.flush();
  return path.string();
}

bool StartServer(std::unique_ptr<mi::server::ServerApp>& app,
                 std::unique_ptr<mi::server::Listener>& listener,
                 std::unique_ptr<mi::server::NetworkServer>& net,
                 const std::filesystem::path& dir,
                 std::uint16_t& out_port,
                 std::string& error) {
  error.clear();
  for (std::uint16_t port = 31000; port < 31100; ++port) {
    const std::string cfg_path = WriteServerConfig(dir, port);
    auto app_try = std::make_unique<mi::server::ServerApp>();
    std::string init_err;
    if (!app_try->Init(cfg_path, init_err)) {
      continue;
    }
    auto listener_try = std::make_unique<mi::server::Listener>(app_try.get());
    mi::server::NetworkServerLimits limits;
    auto net_try = std::make_unique<mi::server::NetworkServer>(
        listener_try.get(), port, false, "", false, limits);
    std::string net_err;
    if (!net_try->Start(net_err)) {
      if (net_err.find("tcp server not built") != std::string::npos) {
        error = net_err;
        return false;
      }
      continue;
    }
    app = std::move(app_try);
    listener = std::move(listener_try);
    net = std::move(net_try);
    out_port = port;
    mi::platform::SleepMs(50);
    return true;
  }
  error = "network server start failed";
  return false;
}

bool WaitForEvent(mi_client_handle* handle,
                  std::uint32_t type,
                  const std::string& match_sender,
                  const std::string& match_group,
                  std::uint32_t timeout_ms) {
  const auto deadline = mi::platform::NowSteadyMs() + timeout_ms;
  mi_event_t events[8]{};
  while (mi::platform::NowSteadyMs() < deadline) {
    const std::uint32_t count =
        mi_client_poll_event(handle, events, 8, 100);
    for (std::uint32_t i = 0; i < count; ++i) {
      const mi_event_t& ev = events[i];
      if (ev.type != type) {
        continue;
      }
      if (!match_sender.empty()) {
        if (!ev.sender || match_sender != ev.sender) {
          continue;
        }
      }
      if (!match_group.empty()) {
        if (!ev.group_id || match_group != ev.group_id) {
          continue;
        }
      }
      return true;
    }
  }
  return false;
}

bool WaitForPairingRequest(mi_client_handle* handle,
                           mi_device_pairing_request_t* out_request,
                           std::uint32_t timeout_ms) {
  const auto deadline = mi::platform::NowSteadyMs() + timeout_ms;
  mi_device_pairing_request_t entries[4]{};
  while (mi::platform::NowSteadyMs() < deadline) {
    const std::uint32_t count =
        mi_client_poll_device_pairing_requests(handle, entries, 4);
    if (count > 0) {
      if (out_request) {
        *out_request = entries[0];
      }
      return true;
    }
    mi::platform::SleepMs(100);
  }
  return false;
}

bool WaitForPairingComplete(mi_client_handle* handle,
                            std::uint32_t timeout_ms) {
  const auto deadline = mi::platform::NowSteadyMs() + timeout_ms;
  while (mi::platform::NowSteadyMs() < deadline) {
    int completed = 0;
    if (mi_client_poll_device_pairing_linked(handle, &completed) == 1 &&
        completed != 0) {
      return true;
    }
    mi::platform::SleepMs(100);
  }
  return false;
}

}  // namespace

int main() {
  const std::string prev_data_dir = GetEnv("MI_E2EE_DATA_DIR");
  const std::string prev_hardening = GetEnv("MI_E2EE_HARDENING");
  SetEnv("MI_E2EE_HARDENING", "off");

  const UserFileBackup backup = BackupTestUsers();
  if (!WriteTestUsers()) {
    RestoreTestUsers(backup);
    return 1;
  }

  const auto base_dir = MakeUniqueDir("test_e2e");
  const auto server_dir = base_dir / "server";
  std::error_code ec;
  std::filesystem::create_directories(server_dir, ec);
  if (ec) {
    RestoreTestUsers(backup);
    return 1;
  }

  std::unique_ptr<mi::server::ServerApp> app;
  std::unique_ptr<mi::server::Listener> listener;
  std::unique_ptr<mi::server::NetworkServer> net;
  std::uint16_t port = 0;
  std::string server_err;
  if (!StartServer(app, listener, net, server_dir, port, server_err)) {
    if (server_err.find("tcp server not built") != std::string::npos) {
      RestoreTestUsers(backup);
      SetEnv("MI_E2EE_DATA_DIR", prev_data_dir);
      SetEnv("MI_E2EE_HARDENING", prev_hardening);
      return 0;
    }
    RestoreTestUsers(backup);
    return 1;
  }

  const auto alice_primary_dir = base_dir / "alice_primary";
  const auto alice_linked_dir = base_dir / "alice_linked";
  const auto bob_dir = base_dir / "bob";
  std::filesystem::create_directories(alice_primary_dir, ec);
  std::filesystem::create_directories(alice_linked_dir, ec);
  std::filesystem::create_directories(bob_dir, ec);

  const std::string alice_primary_cfg =
      WriteClientConfig(alice_primary_dir, port, true, true);
  const std::string alice_linked_cfg =
      WriteClientConfig(alice_linked_dir, port, true, false);
  const std::string bob_cfg =
      WriteClientConfig(bob_dir, port, false, false);

  SetEnv("MI_E2EE_DATA_DIR", alice_primary_dir.string());
  mi_client_handle* alice = mi_client_create(alice_primary_cfg.c_str());
  assert(alice != nullptr);
  assert(mi_client_login(alice, "alice", "alice123") == 1);

  SetEnv("MI_E2EE_DATA_DIR", bob_dir.string());
  mi_client_handle* bob = mi_client_create(bob_cfg.c_str());
  assert(bob != nullptr);
  assert(mi_client_login(bob, "bob", "bob123") == 1);

  SetEnv("MI_E2EE_DATA_DIR", alice_linked_dir.string());
  mi_client_handle* alice_linked = mi_client_create(alice_linked_cfg.c_str());
  assert(alice_linked != nullptr);
  assert(mi_client_login(alice_linked, "alice", "alice123") == 1);

  char* pairing_code = nullptr;
  assert(mi_client_begin_device_pairing_primary(alice, &pairing_code) == 1);
  assert(pairing_code != nullptr);
  assert(mi_client_begin_device_pairing_linked(alice_linked, pairing_code) == 1);
  mi_client_free(pairing_code);

  mi_device_pairing_request_t req{};
  assert(WaitForPairingRequest(alice, &req, 5000));
  assert(mi_client_approve_device_pairing_request(
             alice, req.device_id, req.request_id_hex) == 1);
  assert(WaitForPairingComplete(alice_linked, 5000));

  assert(mi_client_logout(bob) == 1);
  char* msg_id = nullptr;
  assert(mi_client_send_private_text(alice, "bob", "hello", &msg_id) == 1);
  mi_client_free(msg_id);

  assert(mi_client_login(bob, "bob", "bob123") == 1);
  assert(WaitForEvent(bob, MI_EVENT_CHAT_TEXT, "alice", "", 5000));

  char* group_id = nullptr;
  assert(mi_client_create_group(alice, &group_id) == 1);
  assert(group_id != nullptr);
  assert(mi_client_send_group_invite(alice, group_id, "bob", nullptr) == 1);
  (void)WaitForEvent(bob, MI_EVENT_GROUP_INVITE, "alice", "", 3000);
  assert(mi_client_join_group(bob, group_id) == 1);

  char* group_msg_id = nullptr;
  assert(mi_client_send_group_text(alice, group_id, "group hi", &group_msg_id) == 1);
  mi_client_free(group_msg_id);
  assert(WaitForEvent(bob, MI_EVENT_GROUP_TEXT, "alice", group_id, 5000));
  mi_client_free(group_id);

  mi_client_destroy(alice_linked);
  mi_client_destroy(bob);
  mi_client_destroy(alice);

  if (net) {
    net->Stop();
  }
  net.reset();
  listener.reset();
  app.reset();

  SetEnv("MI_E2EE_DATA_DIR", prev_data_dir);
  SetEnv("MI_E2EE_HARDENING", prev_hardening);
  RestoreTestUsers(backup);
  std::filesystem::remove_all(base_dir, ec);
  return 0;
}
