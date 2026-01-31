#include "c_api_client.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "listener.h"
#include "key_transparency.h"
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

void LogStep(const char* msg) {
  if (!msg) {
    return;
  }
  std::cerr << "[sdk_c_api_e2e_test] " << msg << "\n";
  std::cerr.flush();
}

void LogClientError(const char* label, mi_client_handle* handle) {
  if (!label || !handle) {
    return;
  }
  const char* last = mi_client_last_error(handle);
  if (last && *last) {
    std::cerr << "[sdk_c_api_e2e_test] " << label
              << " last_error=" << last << "\n";
  }
  const char* remote = mi_client_remote_error(handle);
  if (remote && *remote) {
    std::cerr << "[sdk_c_api_e2e_test] " << label
              << " remote_error=" << remote << "\n";
  }
  std::cerr.flush();
}

[[noreturn]] void FailNow(const char* msg, mi_client_handle* handle) {
  if (msg) {
    std::cerr << "[sdk_c_api_e2e_test] " << msg << "\n";
  }
  LogClientError("client", handle);
  std::cerr.flush();
  std::fflush(nullptr);
  std::_Exit(1);
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
  {
    const auto key_path = dir / "kt_signing_key.bin";
    std::vector<std::uint8_t> key(mi::server::kKtSthSigSecretKeyBytes, 0x42);
    std::ofstream key_out(key_path, std::ios::binary | std::ios::trunc);
    if (key_out) {
      key_out.write(reinterpret_cast<const char*>(key.data()),
                    static_cast<std::streamsize>(key.size()));
    } else {
      std::cerr << "write kt_signing_key failed\n";
    }
  }
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
    {
      const std::string msg = "server init " + std::to_string(port);
      LogStep(msg.c_str());
    }
    auto app_try = std::make_unique<mi::server::ServerApp>();
    std::string init_err;
    if (!app_try->Init(cfg_path, init_err)) {
      continue;
    }
    LogStep("server app ok");
    auto listener_try = std::make_unique<mi::server::Listener>(app_try.get());
    mi::server::NetworkServerLimits limits;
    limits.max_worker_threads = 1;
    limits.max_io_threads = 1;
    limits.max_pending_tasks = 256;
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
    mi::platform::SleepMs(500);
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

bool WaitForChatOrOffline(mi_client_handle* handle,
                          const std::string& match_sender,
                          std::uint32_t timeout_ms) {
  const auto deadline = mi::platform::NowSteadyMs() + timeout_ms;
  mi_event_t events[8]{};
  while (mi::platform::NowSteadyMs() < deadline) {
    const std::uint32_t count = mi_client_poll_event(handle, events, 8, 200);
    for (std::uint32_t i = 0; i < count; ++i) {
      const mi_event_t& ev = events[i];
      if (ev.type == MI_EVENT_CHAT_TEXT) {
        if (match_sender.empty()) {
          return true;
        }
        if (ev.sender && match_sender == ev.sender) {
          return true;
        }
      } else if (ev.type == MI_EVENT_OFFLINE_PAYLOAD) {
        if (ev.payload && ev.payload_len > 0) {
          return true;
        }
      }
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

bool WaitForFriendRequest(mi_client_handle* handle,
                          const std::string& requester,
                          std::uint32_t timeout_ms) {
  if (!handle || requester.empty()) {
    return false;
  }
  const auto deadline = mi::platform::NowSteadyMs() + timeout_ms;
  mi_friend_request_entry_t entries[4]{};
  while (mi::platform::NowSteadyMs() < deadline) {
    const std::uint32_t count =
        mi_client_list_friend_requests(handle, entries, 4);
    for (std::uint32_t i = 0; i < count; ++i) {
      const char* name = entries[i].requester_username;
      if (name && requester == name) {
        return true;
      }
    }
    mi::platform::SleepMs(100);
  }
  return false;
}

bool WaitForFriend(mi_client_handle* handle,
                   const std::string& friend_username,
                   std::uint32_t timeout_ms) {
  if (!handle || friend_username.empty()) {
    return false;
  }
  const auto deadline = mi::platform::NowSteadyMs() + timeout_ms;
  mi_friend_entry_t entries[8]{};
  while (mi::platform::NowSteadyMs() < deadline) {
    int changed = 0;
    const std::uint32_t count =
        mi_client_sync_friends(handle, entries, 8, &changed);
    for (std::uint32_t i = 0; i < count; ++i) {
      const char* name = entries[i].username;
      if (name && friend_username == name) {
        return true;
      }
    }
    mi::platform::SleepMs(100);
  }
  return false;
}

void DrainEvents(mi_client_handle* handle) {
  if (!handle) {
    return;
  }
  mi_event_t events[8]{};
  while (mi_client_poll_event(handle, events, 8, 0) > 0) {
  }
}

bool WaitForPendingPeerTrust(mi_client_handle* handle,
                             const std::string& expected_peer,
                             std::string& out_pin,
                             std::uint32_t timeout_ms) {
  out_pin.clear();
  if (!handle) {
    return false;
  }
  const auto deadline = mi::platform::NowSteadyMs() + timeout_ms;
  while (mi::platform::NowSteadyMs() < deadline) {
    if (mi_client_has_pending_peer_trust(handle) == 1) {
      const char* peer = mi_client_pending_peer_username(handle);
      const char* pin = mi_client_pending_peer_pin(handle);
      if (!peer || !pin || *pin == '\0') {
        return false;
      }
      if (!expected_peer.empty() && expected_peer != peer) {
        return false;
      }
      out_pin.assign(pin);
      return true;
    }
    mi::platform::SleepMs(100);
  }
  return false;
}

bool TrustPendingPeerIfReady(mi_client_handle* handle,
                             const std::string& expected_peer,
                             std::uint32_t timeout_ms) {
  std::string pin;
  if (!WaitForPendingPeerTrust(handle, expected_peer, pin, timeout_ms)) {
    return mi_client_has_pending_peer_trust(handle) == 0;
  }
  if (mi_client_trust_pending_peer(handle, pin.c_str()) != 1) {
    return false;
  }
  return true;
}

bool EnsurePeerTrusted(mi_client_handle* sender,
                       mi_client_handle* receiver,
                       const std::string& receiver_name,
                       const std::string& sender_name) {
  if (!sender || !receiver) {
    return false;
  }
  char* msg_id = nullptr;
  if (mi_client_send_private_text(sender, receiver_name.c_str(), "trust ping",
                                  &msg_id) != 1 ||
      !msg_id) {
    const char* last_err = mi_client_last_error(sender);
    const std::string err = last_err ? last_err : "";
    if (err != "peer not trusted" && err != "peer fingerprint changed") {
      LogClientError("sender", sender);
      return false;
    }
    if (!TrustPendingPeerIfReady(sender, receiver_name, 5000)) {
      LogClientError("sender", sender);
      return false;
    }
    if (mi_client_send_private_text(sender, receiver_name.c_str(), "trust ping",
                                    &msg_id) != 1 ||
        !msg_id) {
      LogClientError("sender", sender);
      return false;
    }
  }
  if (msg_id) {
    mi_client_free(msg_id);
    msg_id = nullptr;
  }
  if (!TrustPendingPeerIfReady(receiver, sender_name, 5000)) {
    LogClientError("receiver", receiver);
    return false;
  }
  DrainEvents(receiver);
  return true;
}

bool LoginWithRetry(mi_client_handle* handle,
                    const char* username,
                    const char* password,
                    const char* label,
                    int attempts,
                    std::uint32_t delay_ms) {
  if (!handle || !username || !password) {
    return false;
  }
  const int max_attempts = attempts <= 0 ? 1 : attempts;
  for (int i = 0; i < max_attempts; ++i) {
    if (mi_client_login(handle, username, password) == 1) {
      return true;
    }
    if (label) {
      LogClientError(label, handle);
    }
    if (i + 1 < max_attempts) {
      mi::platform::SleepMs(delay_ms);
    }
  }
  return false;
}

}  // namespace

int main() {
  const std::string prev_data_dir = GetEnv("MI_E2EE_DATA_DIR");
  const std::string prev_hardening = GetEnv("MI_E2EE_HARDENING");
  SetEnv("MI_E2EE_DATA_DIR", "");
  SetEnv("MI_E2EE_HARDENING", "off");

  const UserFileBackup backup = BackupTestUsers();
  std::filesystem::path base_dir;
  std::unique_ptr<mi::server::ServerApp> app;
  std::unique_ptr<mi::server::Listener> listener;
  std::unique_ptr<mi::server::NetworkServer> net;
  mi_client_handle* alice = nullptr;
  mi_client_handle* bob = nullptr;
  mi_client_handle* alice_linked = nullptr;
  char* pairing_code = nullptr;
  char* msg_id = nullptr;
  char* group_id = nullptr;
  char* group_msg_id = nullptr;

  auto cleanup = [&]() {
    if (pairing_code) {
      mi_client_free(pairing_code);
      pairing_code = nullptr;
    }
    if (msg_id) {
      mi_client_free(msg_id);
      msg_id = nullptr;
    }
    if (group_msg_id) {
      mi_client_free(group_msg_id);
      group_msg_id = nullptr;
    }
    if (group_id) {
      mi_client_free(group_id);
      group_id = nullptr;
    }
    if (alice_linked) {
      mi_client_destroy(alice_linked);
      alice_linked = nullptr;
    }
    if (bob) {
      mi_client_destroy(bob);
      bob = nullptr;
    }
    if (alice) {
      mi_client_destroy(alice);
      alice = nullptr;
    }
    if (net) {
      net->Stop();
    }
    net.reset();
    listener.reset();
    app.reset();
    SetEnv("MI_E2EE_DATA_DIR", prev_data_dir);
    SetEnv("MI_E2EE_HARDENING", prev_hardening);
    RestoreTestUsers(backup);
    std::error_code ec;
    if (!base_dir.empty()) {
      std::filesystem::remove_all(base_dir, ec);
    }
  };

  if (!WriteTestUsers()) {
    std::cerr << "write test users failed\n";
    cleanup();
    return 1;
  }

  LogStep("init");
  base_dir = MakeUniqueDir("test_e2e");
  LogStep("base dir ok");
  const auto server_dir = base_dir / "server";
  std::error_code ec;
  std::filesystem::create_directories(server_dir, ec);
  if (ec) {
    std::cerr << "create server dir failed\n";
    cleanup();
    return 1;
  }
  LogStep("server dir ok");

  std::uint16_t port = 0;
  std::string server_err;
  LogStep("start server");
  if (!StartServer(app, listener, net, server_dir, port, server_err)) {
    if (server_err.find("tcp server not built") != std::string::npos) {
      cleanup();
      return 0;
    }
    std::cerr << "start server failed: " << server_err << "\n";
    cleanup();
    return 1;
  }
  LogStep("server started");

  const auto alice_primary_dir = base_dir / "alice_primary";
  const auto alice_linked_dir = base_dir / "alice_linked";
  const auto bob_dir = base_dir / "bob";
  std::filesystem::create_directories(alice_primary_dir, ec);
  std::filesystem::create_directories(alice_linked_dir, ec);
  std::filesystem::create_directories(bob_dir, ec);
  if (ec) {
    std::cerr << "create client dirs failed\n";
    cleanup();
    return 1;
  }

  const std::string alice_primary_cfg =
      WriteClientConfig(alice_primary_dir, port, true, true);
  const std::string alice_linked_cfg =
      WriteClientConfig(alice_linked_dir, port, true, false);
  const std::string bob_cfg =
      WriteClientConfig(bob_dir, port, false, false);

  alice = mi_client_create(alice_primary_cfg.c_str());
  if (!alice) {
    std::cerr << "create alice failed\n";
    cleanup();
    return 1;
  }
  if (!LoginWithRetry(alice, "alice", "alice123", "alice", 5, 500)) {
    std::cerr << "alice login failed\n";
    cleanup();
    return 1;
  }
  LogStep("alice login ok");
  mi::platform::SleepMs(200);

  bob = mi_client_create(bob_cfg.c_str());
  if (!bob) {
    std::cerr << "create bob failed\n";
    cleanup();
    return 1;
  }
  if (!LoginWithRetry(bob, "bob", "bob123", "bob", 5, 500)) {
    std::cerr << "bob login failed\n";
    cleanup();
    return 1;
  }
  LogStep("bob login ok");
  if (mi_client_publish_prekeys(alice) != 1) {
    FailNow("alice prekey publish failed", alice);
  }
  if (mi_client_publish_prekeys(bob) != 1) {
    FailNow("bob prekey publish failed", bob);
  }

  if (mi_client_send_friend_request(alice, "bob", "hi") != 1) {
    FailNow("send friend request failed", alice);
  }
  if (!WaitForFriendRequest(bob, "alice", 5000)) {
    FailNow("friend request timeout", bob);
  }
  if (mi_client_respond_friend_request(bob, "alice", 1) != 1) {
    FailNow("accept friend request failed", bob);
  }
  if (!WaitForFriend(alice, "bob", 5000)) {
    FailNow("friend sync timeout", alice);
  }
  if (!WaitForFriend(bob, "alice", 5000)) {
    FailNow("friend sync timeout", bob);
  }
  LogStep("friend ok");
  if (!EnsurePeerTrusted(alice, bob, "bob", "alice")) {
    FailNow("peer trust failed", alice);
  }
  LogStep("peer trust ok");

  alice_linked = mi_client_create(alice_linked_cfg.c_str());
  if (!alice_linked) {
    std::cerr << "create linked alice failed\n";
    cleanup();
    return 1;
  }
  if (!LoginWithRetry(alice_linked, "alice", "alice123", "linked", 5, 500)) {
    std::cerr << "linked alice login failed\n";
    cleanup();
    return 1;
  }
  LogStep("linked login ok");

  if (mi_client_begin_device_pairing_primary(alice, &pairing_code) != 1 ||
      !pairing_code) {
    std::cerr << "begin pairing primary failed\n";
    cleanup();
    return 1;
  }
  if (mi_client_begin_device_pairing_linked(alice_linked, pairing_code) != 1) {
    std::cerr << "begin pairing linked failed\n";
    cleanup();
    return 1;
  }
  mi_client_free(pairing_code);
  pairing_code = nullptr;

  mi_device_pairing_request_t req{};
  if (!WaitForPairingRequest(alice, &req, 5000)) {
    std::cerr << "pairing request timeout\n";
    cleanup();
    return 1;
  }
  const std::string req_device = req.device_id ? req.device_id : "";
  const std::string req_request = req.request_id_hex ? req.request_id_hex : "";
  if (req_device.empty() || req_request.empty()) {
    std::cerr << "pairing request data missing\n";
    cleanup();
    return 1;
  }
  if (mi_client_approve_device_pairing_request(
          alice, req_device.c_str(), req_request.c_str()) != 1) {
    std::cerr << "approve pairing request failed\n";
    cleanup();
    return 1;
  }
  if (!WaitForPairingComplete(alice_linked, 5000)) {
    std::cerr << "pairing completion timeout\n";
    cleanup();
    return 1;
  }
  LogStep("pairing ok");

  DrainEvents(bob);
  if (mi_client_send_private_text(alice, "bob", "hello", &msg_id) != 1 ||
      !msg_id) {
    FailNow("send private text failed", alice);
  }
  mi_client_free(msg_id);
  msg_id = nullptr;

  if (!WaitForChatOrOffline(bob, "alice", 8000)) {
    std::cerr << "private chat event timeout\n";
    cleanup();
    return 1;
  }
  LogStep("private msg ok");

  if (mi_client_create_group(alice, &group_id) != 1 || !group_id) {
    std::cerr << "create group failed\n";
    cleanup();
    return 1;
  }
  if (mi_client_send_group_invite(alice, group_id, "bob", nullptr) != 1) {
    std::cerr << "send group invite failed\n";
    cleanup();
    return 1;
  }
  if (!WaitForEvent(bob, MI_EVENT_GROUP_INVITE, "alice", "", 3000)) {
    std::cerr << "group invite event timeout\n";
    cleanup();
    return 1;
  }
  if (mi_client_join_group(bob, group_id) != 1) {
    std::cerr << "join group failed\n";
    cleanup();
    return 1;
  }

  if (mi_client_send_group_text(alice, group_id, "group hi", &group_msg_id) !=
          1 ||
      !group_msg_id) {
    std::cerr << "send group text failed\n";
    cleanup();
    return 1;
  }
  mi_client_free(group_msg_id);
  group_msg_id = nullptr;
  if (!WaitForEvent(bob, MI_EVENT_GROUP_TEXT, "alice", group_id, 5000)) {
    std::cerr << "group text event timeout\n";
    cleanup();
    return 1;
  }
  LogStep("group msg ok");
  mi_client_free(group_id);
  group_id = nullptr;

  LogStep("cleanup");
  cleanup();
  return 0;
}
