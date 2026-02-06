#include "c_api_client.h"

#include <chrono>
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
#include "path_security.h"
#include "platform_net.h"
#include "platform_time.h"
#include "server_app.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

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

bool IsTruthyEnv(const std::string& value) {
  if (value.empty()) {
    return false;
  }
  if (value == "1" || value == "true" || value == "TRUE" || value == "True" ||
      value == "yes" || value == "YES" || value == "Yes" ||
      value == "on" || value == "ON" || value == "On") {
    return true;
  }
  return false;
}

bool IsCi() {
  return IsTruthyEnv(GetEnv("GITHUB_ACTIONS")) ||
         IsTruthyEnv(GetEnv("CI"));
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

bool ParseEnvPort(const std::string& text, std::uint16_t& out) {
  out = 0;
  if (text.empty()) {
    return false;
  }
  char* end_ptr = nullptr;
  const long v = std::strtol(text.c_str(), &end_ptr, 10);
  if (end_ptr == text.c_str() || v <= 0 || v > 65535) {
    return false;
  }
  out = static_cast<std::uint16_t>(v);
  return true;
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

bool HardenDirForTest(const std::filesystem::path& dir, std::string& error) {
  error.clear();
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    error = "dir create failed";
    return false;
  }
#ifdef _WIN32
  std::string perm_err;
  if (!mi::shard::security::HardenPathAcl(dir, perm_err)) {
    error = perm_err.empty() ? "dir acl harden failed" : perm_err;
    return false;
  }
#else
  std::filesystem::permissions(
      dir,
      std::filesystem::perms::owner_read |
          std::filesystem::perms::owner_write |
          std::filesystem::perms::owner_exec,
      std::filesystem::perm_options::replace, ec);
  if (ec) {
    error = "dir perms set failed";
    return false;
  }
#endif
  return true;
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

constexpr std::uint32_t kFriendTimeoutMs = 10000;
constexpr std::uint32_t kPairingTimeoutMs = 15000;
constexpr std::uint32_t kChatTimeoutMs = 15000;
constexpr std::uint32_t kGroupInviteTimeoutMs = 8000;
constexpr std::uint32_t kGroupTextTimeoutMs = 10000;
constexpr std::uint32_t kPostLoginDelayMs = 300;
constexpr char kTestMetadataKeyHex[] =
    "00112233445566778899aabbccddeeff"
    "fedcba98765432100123456789abcdef";

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
  std::error_code ec;
  auto base = std::filesystem::current_path(ec);
  if (ec || base.empty()) {
    ec.clear();
    base = std::filesystem::temp_directory_path(ec);
  }
  if (ec || base.empty()) {
    base = std::filesystem::path(".");
  }

  for (int attempt = 0; attempt < 32; ++attempt) {
    const auto stamp = std::chrono::high_resolution_clock::now()
                           .time_since_epoch()
                           .count();
    const std::filesystem::path dir =
        base / (prefix + "_" + std::to_string(stamp) + "_" +
                std::to_string(attempt));
    ec.clear();
    if (std::filesystem::create_directories(dir, ec)) {
      return dir;
    }
  }

  const std::filesystem::path fallback = base / (prefix + "_fallback");
  std::filesystem::create_directories(fallback, ec);
  return fallback;
}

#ifdef _WIN32
#ifndef MI_E2EE_SERVER_EXE_PATH
#define MI_E2EE_SERVER_EXE_PATH ""
#endif

PROCESS_INFORMATION g_server_process{};

void CloseLaunchedServerHandles() {
  if (g_server_process.hThread != nullptr) {
    CloseHandle(g_server_process.hThread);
  }
  if (g_server_process.hProcess != nullptr) {
    CloseHandle(g_server_process.hProcess);
  }
  g_server_process = PROCESS_INFORMATION{};
}

void StopLaunchedServer() {
  if (g_server_process.hProcess == nullptr) {
    return;
  }
  DWORD exit_code = 0;
  if (GetExitCodeProcess(g_server_process.hProcess, &exit_code) &&
      exit_code == STILL_ACTIVE) {
    (void)TerminateProcess(g_server_process.hProcess, 0);
    (void)WaitForSingleObject(g_server_process.hProcess, 3000);
  }
  CloseLaunchedServerHandles();
}

std::filesystem::path FindServerExecutable() {
  std::error_code ec;
  const std::string configured = MI_E2EE_SERVER_EXE_PATH;
  if (!configured.empty()) {
    const std::filesystem::path path(configured);
    if (std::filesystem::exists(path, ec) && !ec) {
      return path;
    }
  }

  auto cursor = std::filesystem::current_path(ec);
  if (ec || cursor.empty()) {
    return {};
  }
  for (int depth = 0; depth < 8 && !cursor.empty(); ++depth) {
    const std::filesystem::path candidate1 =
        cursor / "build" / "server" / "Release" / "mi_e2ee_server.exe";
    ec.clear();
    if (std::filesystem::exists(candidate1, ec) && !ec) {
      return candidate1;
    }

    const std::filesystem::path candidate2 =
        cursor / "server_build" / "Release" / "mi_e2ee_server.exe";
    ec.clear();
    if (std::filesystem::exists(candidate2, ec) && !ec) {
      return candidate2;
    }

    const std::filesystem::path candidate3 =
        cursor / "server" / "Release" / "mi_e2ee_server.exe";
    ec.clear();
    if (std::filesystem::exists(candidate3, ec) && !ec) {
      return candidate3;
    }

    if (!cursor.has_parent_path()) {
      break;
    }
    cursor = cursor.parent_path();
  }
  return {};
}

bool LaunchServerProcess(const std::filesystem::path& exe_path,
                         const std::string& cfg_path,
                         std::uint16_t port,
                         std::string& error) {
  error.clear();
  StopLaunchedServer();
  if (exe_path.empty()) {
    error = "server executable missing";
    return false;
  }

  const std::wstring exe = exe_path.wstring();
  const std::wstring cfg = std::filesystem::path(cfg_path).wstring();
  std::wstring cmdline = L"\"" + exe + L"\" \"" + cfg + L"\"";
  std::vector<wchar_t> cmdline_buf(cmdline.begin(), cmdline.end());
  cmdline_buf.push_back(L'\0');

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(exe.c_str(), cmdline_buf.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    error = "CreateProcess failed: " + std::to_string(GetLastError());
    return false;
  }
  g_server_process = pi;

  const auto deadline = mi::platform::NowSteadyMs() + 10000;
  while (mi::platform::NowSteadyMs() < deadline) {
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
      error = "GetExitCodeProcess failed";
      StopLaunchedServer();
      return false;
    }
    if (exit_code != STILL_ACTIVE) {
      error = "server exited early: " + std::to_string(exit_code);
      StopLaunchedServer();
      return false;
    }
    mi::platform::net::Socket probe = mi::platform::net::kInvalidSocket;
    std::string connect_error;
    if (mi::platform::net::ConnectTcp("127.0.0.1", port, probe,
                                      connect_error)) {
      mi::platform::net::CloseSocket(probe);
      return true;
    }
    mi::platform::SleepMs(100);
  }

  error = "server startup timeout";
  StopLaunchedServer();
  return false;
}
#endif

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
  out << "metadata_key_hex=" << kTestMetadataKeyHex << "\n";
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
                              const std::string& host,
                              std::uint16_t port,
                              bool device_sync,
                              bool primary) {
  const auto path = dir / "client_config.ini";
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << "[client]\n";
  out << "server_ip=" << (host.empty() ? "127.0.0.1" : host) << "\n";
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
    // Isolate state for each port probe to avoid stale files poisoning retries.
    const auto run_dir = dir / ("port_" + std::to_string(port));
    std::error_code ec;
    std::filesystem::remove_all(run_dir, ec);
    ec.clear();
    std::filesystem::create_directories(run_dir, ec);
    if (ec) {
      continue;
    }
    const std::string cfg_path = WriteServerConfig(run_dir, port);
    {
      const std::string msg = "server init " + std::to_string(port);
      LogStep(msg.c_str());
    }
#ifdef _WIN32
    const auto server_exe = FindServerExecutable();
    if (server_exe.empty()) {
      error = "server executable not found";
      return false;
    }
    std::string launch_err;
    if (!LaunchServerProcess(server_exe, cfg_path, port, launch_err)) {
      std::cerr << "[sdk_c_api_e2e_test] external server start failed";
      if (!launch_err.empty()) {
        std::cerr << ": " << launch_err;
      }
      std::cerr << "\n";
      continue;
    }
    out_port = port;
    mi::platform::SleepMs(300);
    return true;
#else
    auto app_try = std::make_unique<mi::server::ServerApp>();
    std::string init_err;
    if (!app_try->Init(cfg_path, init_err)) {
      std::cerr << "[sdk_c_api_e2e_test] server init failed";
      if (!init_err.empty()) {
        std::cerr << ": " << init_err;
      }
      std::cerr << "\n";
      continue;
    }
    LogStep("server app ok");
    auto listener_try = std::make_unique<mi::server::Listener>(app_try.get());
    mi::server::NetworkServerLimits limits;
    limits.max_worker_threads = 2;
    limits.max_io_threads = 2;
    limits.max_pending_tasks = 256;
    auto net_try = std::make_unique<mi::server::NetworkServer>(
        listener_try.get(), port, false, "", true, limits);
    LogStep("server net object ok");
    LogStep("server net start begin");
    std::string net_err;
    if (!net_try->Start(net_err)) {
      std::cerr << "[sdk_c_api_e2e_test] server net start failed";
      if (!net_err.empty()) {
        std::cerr << ": " << net_err;
      }
      std::cerr << "\n";
      if (net_err.find("tcp server not built") != std::string::npos) {
        error = net_err;
        return false;
      }
      continue;
    }
    LogStep("server net start ok");
    app = std::move(app_try);
    listener = std::move(listener_try);
    net = std::move(net_try);
    out_port = port;
    mi::platform::SleepMs(500);
    return true;
#endif
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

bool PublishPrekeysWithRetry(mi_client_handle* handle,
                             const char* label,
                             int attempts,
                             std::uint32_t delay_ms) {
  if (!handle) {
    return false;
  }
  const int max_attempts = attempts <= 0 ? 1 : attempts;
  for (int i = 0; i < max_attempts; ++i) {
    if (mi_client_publish_prekeys(handle) == 1) {
      return true;
    }
    if (label) {
      LogClientError(label, handle);
    }
    (void)mi_client_heartbeat(handle);
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
#ifdef _WIN32
    StopLaunchedServer();
#endif
    SetEnv("MI_E2EE_DATA_DIR", prev_data_dir);
    SetEnv("MI_E2EE_HARDENING", prev_hardening);
    RestoreTestUsers(backup);
    std::error_code ec;
    if (!base_dir.empty()) {
      std::filesystem::remove_all(base_dir, ec);
    }
  };

  auto fail = [&](const char* msg, mi_client_handle* handle) -> int {
    if (IsCi()) {
      FailNow(msg, handle);
    }
    if (msg) {
      std::cerr << msg << "\n";
    }
    if (handle) {
      LogClientError("client", handle);
    }
    cleanup();
    return 1;
  };

  if (!WriteTestUsers()) {
    return fail("write test users failed", nullptr);
  }

  LogStep("init");
  base_dir = MakeUniqueDir("test_e2e");
  LogStep("base dir ok");
  const auto server_dir = base_dir / "server";
  std::error_code ec;
  std::filesystem::create_directories(server_dir, ec);
  if (ec) {
    return fail("create server dir failed", nullptr);
  }
  std::string harden_err;
  if (!HardenDirForTest(server_dir, harden_err)) {
    const std::string err = "harden server dir failed: " + harden_err;
    return fail(err.c_str(), nullptr);
  }
  LogStep("server dir ok");

  const std::string ci_host_env = GetEnv("MI_E2EE_CI_SERVER_HOST");
  const std::string ci_port_env = GetEnv("MI_E2EE_CI_SERVER_PORT");
  std::string server_host = ci_host_env.empty() ? "127.0.0.1" : ci_host_env;
  const bool use_external_server =
      !ci_host_env.empty() && ci_host_env != "127.0.0.1" &&
      ci_host_env != "localhost";

  std::uint16_t port = 0;
  if (use_external_server) {
    if (!ParseEnvPort(ci_port_env, port)) {
      port = 31000;
    }
    LogStep("use external server");
  } else {
    if (server_host == "localhost") {
      server_host = "127.0.0.1";
    }
    std::string server_err;
    LogStep("start server");
    if (!StartServer(app, listener, net, server_dir, port, server_err)) {
      if (server_err.find("tcp server not built") != std::string::npos) {
        cleanup();
        return 0;
      }
      const std::string err = "start server failed: " + server_err;
      return fail(err.c_str(), nullptr);
    }
    LogStep("server started");
  }
  {
    const bool runtime_external = use_external_server || (!app && !net);
    const std::string msg = "server ready " + server_host + ":" +
                            std::to_string(port) +
                            (runtime_external ? " (external)" : " (embedded)");
    LogStep(msg.c_str());
  }

  const auto alice_primary_dir = base_dir / "alice_primary";
  const auto alice_linked_dir = base_dir / "alice_linked";
  const auto bob_dir = base_dir / "bob";
  std::filesystem::create_directories(alice_primary_dir, ec);
  std::filesystem::create_directories(alice_linked_dir, ec);
  std::filesystem::create_directories(bob_dir, ec);
  if (ec) {
    return fail("create client dirs failed", nullptr);
  }
  if (!HardenDirForTest(alice_primary_dir, harden_err)) {
    const std::string err = "harden alice primary dir failed: " + harden_err;
    return fail(err.c_str(), nullptr);
  }
  if (!HardenDirForTest(alice_linked_dir, harden_err)) {
    const std::string err = "harden alice linked dir failed: " + harden_err;
    return fail(err.c_str(), nullptr);
  }
  if (!HardenDirForTest(bob_dir, harden_err)) {
    const std::string err = "harden bob dir failed: " + harden_err;
    return fail(err.c_str(), nullptr);
  }

  const std::string alice_primary_cfg =
      WriteClientConfig(alice_primary_dir, server_host, port, true, true);
  const std::string alice_linked_cfg =
      WriteClientConfig(alice_linked_dir, server_host, port, true, false);
  const std::string bob_cfg =
      WriteClientConfig(bob_dir, server_host, port, false, false);

  alice = mi_client_create(alice_primary_cfg.c_str());
  if (!alice) {
    const char* create_err = mi_client_last_create_error();
    const std::string err = (create_err && *create_err)
                                ? std::string("create alice failed: ") + create_err
                                : "create alice failed";
    return fail(err.c_str(), nullptr);
  }
  if (!LoginWithRetry(alice, "alice", "alice123", "alice", 5, 500)) {
    return fail("alice login failed", alice);
  }
  LogStep("alice login ok");
  (void)mi_client_heartbeat(alice);
  mi::platform::SleepMs(kPostLoginDelayMs);

  bob = mi_client_create(bob_cfg.c_str());
  if (!bob) {
    const char* create_err = mi_client_last_create_error();
    const std::string err = (create_err && *create_err)
                                ? std::string("create bob failed: ") + create_err
                                : "create bob failed";
    return fail(err.c_str(), alice);
  }
  if (!LoginWithRetry(bob, "bob", "bob123", "bob", 5, 500)) {
    return fail("bob login failed", bob);
  }
  LogStep("bob login ok");
  (void)mi_client_heartbeat(bob);
  if (!PublishPrekeysWithRetry(alice, "alice", 3, 300)) {
    return fail("alice prekey publish failed", alice);
  }
  if (!PublishPrekeysWithRetry(bob, "bob", 3, 300)) {
    return fail("bob prekey publish failed", bob);
  }

  if (mi_client_send_friend_request(alice, "bob", "hi") != 1) {
    return fail("send friend request failed", alice);
  }
  if (!WaitForFriendRequest(bob, "alice", kFriendTimeoutMs)) {
    return fail("friend request timeout", bob);
  }
  if (mi_client_respond_friend_request(bob, "alice", 1) != 1) {
    return fail("accept friend request failed", bob);
  }
  if (!WaitForFriend(alice, "bob", kFriendTimeoutMs)) {
    return fail("friend sync timeout", alice);
  }
  if (!WaitForFriend(bob, "alice", kFriendTimeoutMs)) {
    return fail("friend sync timeout", bob);
  }
  LogStep("friend ok");
  if (!EnsurePeerTrusted(alice, bob, "bob", "alice")) {
    return fail("peer trust failed", alice);
  }
  LogStep("peer trust ok");

  alice_linked = mi_client_create(alice_linked_cfg.c_str());
  if (!alice_linked) {
    const char* create_err = mi_client_last_create_error();
    const std::string err = (create_err && *create_err)
                                ? std::string("create linked alice failed: ") +
                                      create_err
                                : "create linked alice failed";
    return fail(err.c_str(), alice);
  }
  if (!LoginWithRetry(alice_linked, "alice", "alice123", "linked", 5, 500)) {
    return fail("linked alice login failed", alice_linked);
  }
  LogStep("linked login ok");
  (void)mi_client_heartbeat(alice_linked);

  if (mi_client_begin_device_pairing_primary(alice, &pairing_code) != 1 ||
      !pairing_code) {
    return fail("begin pairing primary failed", alice);
  }
  if (mi_client_begin_device_pairing_linked(alice_linked, pairing_code) != 1) {
    return fail("begin pairing linked failed", alice_linked);
  }
  mi_client_free(pairing_code);
  pairing_code = nullptr;

  mi_device_pairing_request_t req{};
  if (!WaitForPairingRequest(alice, &req, kPairingTimeoutMs)) {
    return fail("pairing request timeout", alice);
  }
  const std::string req_device = req.device_id ? req.device_id : "";
  const std::string req_request = req.request_id_hex ? req.request_id_hex : "";
  if (req_device.empty() || req_request.empty()) {
    return fail("pairing request data missing", alice);
  }
  if (mi_client_approve_device_pairing_request(
          alice, req_device.c_str(), req_request.c_str()) != 1) {
    return fail("approve pairing request failed", alice);
  }
  if (!WaitForPairingComplete(alice_linked, kPairingTimeoutMs)) {
    return fail("pairing completion timeout", alice_linked);
  }
  LogStep("pairing ok");
  if (!EnsurePeerTrusted(alice, bob, "bob", "alice")) {
    return fail("peer trust refresh failed", alice);
  }

  DrainEvents(bob);
  bool private_ok = false;
  for (int attempt = 0; attempt < 2 && !private_ok; ++attempt) {
    if (mi_client_send_private_text(alice, "bob", "hello", &msg_id) != 1 ||
        !msg_id) {
      LogClientError("alice", alice);
    }
    if (msg_id) {
      mi_client_free(msg_id);
      msg_id = nullptr;
    }
    if (WaitForChatOrOffline(bob, "alice", kChatTimeoutMs)) {
      private_ok = true;
      break;
    }
    LogClientError("bob", bob);
    (void)mi_client_heartbeat(alice);
    (void)mi_client_heartbeat(bob);
    mi::platform::SleepMs(300);
  }
  if (!private_ok) {
    return fail("private chat event timeout", bob);
  }
  LogStep("private msg ok");

  if (mi_client_create_group(alice, &group_id) != 1 || !group_id) {
    return fail("create group failed", alice);
  }
  if (mi_client_send_group_invite(alice, group_id, "bob", nullptr) != 1) {
    return fail("send group invite failed", alice);
  }
  if (!WaitForEvent(bob, MI_EVENT_GROUP_INVITE, "alice", "",
                    kGroupInviteTimeoutMs)) {
    return fail("group invite event timeout", bob);
  }
  if (mi_client_join_group(bob, group_id) != 1) {
    return fail("join group failed", bob);
  }

  bool group_ok = false;
  for (int attempt = 0; attempt < 2 && !group_ok; ++attempt) {
    if (mi_client_send_group_text(alice, group_id, "group hi", &group_msg_id) !=
            1 ||
        !group_msg_id) {
      LogClientError("alice", alice);
    }
    if (group_msg_id) {
      mi_client_free(group_msg_id);
      group_msg_id = nullptr;
    }
    if (WaitForEvent(bob, MI_EVENT_GROUP_TEXT, "alice", group_id,
                     kGroupTextTimeoutMs)) {
      group_ok = true;
      break;
    }
    LogClientError("bob", bob);
    (void)mi_client_heartbeat(alice);
    (void)mi_client_heartbeat(bob);
    mi::platform::SleepMs(300);
  }
  if (!group_ok) {
    return fail("group text event timeout", bob);
  }
  LogStep("group msg ok");
  mi_client_free(group_id);
  group_id = nullptr;

  LogStep("cleanup");
  cleanup();
  return 0;
}
