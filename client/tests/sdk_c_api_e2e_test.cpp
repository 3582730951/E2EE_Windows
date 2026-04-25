#include "c_api_client.h"

#include <chrono>
#include <algorithm>
#include <array>
#include <cstring>
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
#include "test_permissions.h"

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

bool IsTcpPortAvailable(std::uint16_t port) {
  mi::platform::net::Socket sock = mi::platform::net::kInvalidSocket;
  std::string error;
  if (!mi::platform::net::CreateTcpListener(port, sock, error)) {
    return false;
  }
  mi::platform::net::CloseSocket(sock);
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
  out.close();
  mi::client::test::SetOwnerOnlyFile(path);
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

bool WriteBytesToFile(const std::filesystem::path& path,
                      const std::vector<std::uint8_t>& bytes) {
  std::error_code ec;
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      return false;
    }
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  if (!bytes.empty()) {
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }
  out.flush();
  return static_cast<bool>(out);
}

void WriteLe16(std::uint16_t value, std::vector<std::uint8_t>& out) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void WriteLe32(std::uint32_t value, std::vector<std::uint8_t>& out) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

bool WriteProtoString(const std::string& value, std::vector<std::uint8_t>& out) {
  if (value.size() > 0xFFFFu) {
    return false;
  }
  WriteLe16(static_cast<std::uint16_t>(value.size()), out);
  out.insert(out.end(), value.begin(), value.end());
  return true;
}

std::vector<std::uint8_t> BuildGroupCallSubscriptionPayload(
    const std::string& sender,
    std::uint8_t flags) {
  std::vector<std::uint8_t> out;
  WriteLe32(1, out);
  (void)WriteProtoString(sender, out);
  out.push_back(flags);
  return out;
}

std::vector<std::uint8_t> BuildSyntheticAudioPacket() {
  std::vector<std::uint8_t> packet(22, 0);
  packet[0] = 2;  // MediaPacket v2.
  packet[1] = 1;  // audio stream.
  for (std::size_t i = 6; i < packet.size(); ++i) {
    packet[i] = static_cast<std::uint8_t>(0xA0u + (i & 0x0Fu));
  }
  return packet;
}

std::array<std::uint8_t, 16> FixedCallId(std::uint8_t seed) {
  std::array<std::uint8_t, 16> out{};
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<std::uint8_t>(seed + i);
  }
  return out;
}

constexpr std::uint32_t kFriendTimeoutMs = 10000;
constexpr std::uint32_t kPairingTimeoutMs = 15000;
constexpr std::uint32_t kChatTimeoutMs = 15000;
constexpr std::uint32_t kGroupInviteTimeoutMs = 8000;
constexpr std::uint32_t kGroupTextTimeoutMs = 10000;
constexpr std::uint32_t kFileTimeoutMs = 15000;
constexpr std::uint32_t kMediaTimeoutMs = 10000;
constexpr std::uint32_t kDeviceTimeoutMs = 10000;
constexpr std::uint32_t kPostLoginDelayMs = 300;
constexpr char kTestMetadataKeyHex[] =
    "00112233445566778899aabbccddeeff"
    "fedcba98765432100123456789abcdef";

struct FileEventSnapshot {
  std::string sender;
  std::string group_id;
  std::string message_id;
  std::string file_id;
  std::string file_name;
  std::uint64_t file_size{0};
  std::array<std::uint8_t, 32> file_key{};
  std::uint32_t file_key_len{0};
};

struct MediaEventSnapshot {
  std::string sender;
  std::string group_id;
  std::array<std::uint8_t, 16> call_id{};
  std::vector<std::uint8_t> payload;
};

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
  std::filesystem::path base;
  const std::string runtime_root = GetEnv("MI_E2EE_E2E_RUNTIME_ROOT");
  if (!runtime_root.empty()) {
    base = runtime_root;
    std::filesystem::create_directories(base, ec);
  }
  if (runtime_root.empty() || ec || base.empty()) {
    ec.clear();
    base = std::filesystem::temp_directory_path(ec);
  }
  if (ec || base.empty()) {
    ec.clear();
    base = std::filesystem::current_path(ec);
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
  out << "max_io_threads=1\n";
  out << "[call]\n";
  out << "enable_group_call=1\n";
  out << "[kcp]\n";
  out << "enable=0\n";
  out.flush();
  out.close();
  mi::client::test::SetOwnerOnlyFile(path);
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
  out.close();
  mi::client::test::SetOwnerOnlyFile(path);
  return path.string();
}

bool StartServer(std::unique_ptr<mi::server::ServerApp>& app,
                 std::unique_ptr<mi::server::Listener>& listener,
                 std::unique_ptr<mi::server::NetworkServer>& net,
                 const std::filesystem::path& dir,
                 std::uint16_t& out_port,
                 std::string& error) {
  error.clear();
#ifdef _WIN32
  const auto server_exe = FindServerExecutable();
  if (server_exe.empty()) {
    error = "server executable not found";
    return false;
  }
#endif
  for (std::uint16_t port = 31000; port < 31100; ++port) {
    if (!IsTcpPortAvailable(port)) {
      continue;
    }
    // Isolate state for each port probe to avoid stale files poisoning retries.
    const auto run_dir = dir / ("port_" + std::to_string(port));
    std::error_code ec;
    std::filesystem::remove_all(run_dir, ec);
    ec.clear();
    if (!HardenDirForTest(run_dir, error)) {
      continue;
    }
    const std::string cfg_path = WriteServerConfig(run_dir, port);
    {
      const std::string msg = "server init " + std::to_string(port);
      LogStep(msg.c_str());
    }
#ifdef _WIN32
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
    limits.max_io_threads = 1;
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

bool WaitForPeerEvent(mi_client_handle* handle,
                      std::uint32_t type,
                      const std::string& match_peer,
                      const std::string& match_message_id,
                      std::uint32_t timeout_ms) {
  const auto deadline = mi::platform::NowSteadyMs() + timeout_ms;
  mi_event_t events[8]{};
  while (mi::platform::NowSteadyMs() < deadline) {
    const std::uint32_t count = mi_client_poll_event(handle, events, 8, 200);
    for (std::uint32_t i = 0; i < count; ++i) {
      const mi_event_t& ev = events[i];
      if (ev.type != type) {
        continue;
      }
      if (!match_peer.empty() && (!ev.peer || match_peer != ev.peer)) {
        continue;
      }
      if (!match_message_id.empty() &&
          (!ev.message_id || match_message_id != ev.message_id)) {
        continue;
      }
      return true;
    }
  }
  return false;
}

bool WaitForFileEvent(mi_client_handle* handle,
                      std::uint32_t type,
                      const std::string& match_sender,
                      const std::string& match_group,
                      std::uint32_t timeout_ms,
                      FileEventSnapshot& out) {
  out = {};
  const auto deadline = mi::platform::NowSteadyMs() + timeout_ms;
  mi_event_t events[8]{};
  while (mi::platform::NowSteadyMs() < deadline) {
    const std::uint32_t count = mi_client_poll_event(handle, events, 8, 200);
    for (std::uint32_t i = 0; i < count; ++i) {
      const mi_event_t& ev = events[i];
      if (ev.type != type) {
        continue;
      }
      if (!match_sender.empty() && (!ev.sender || match_sender != ev.sender)) {
        continue;
      }
      if (!match_group.empty() && (!ev.group_id || match_group != ev.group_id)) {
        continue;
      }
      if (!ev.file_id || !ev.file_key || ev.file_key_len != out.file_key.size()) {
        continue;
      }
      out.sender = ev.sender ? ev.sender : "";
      out.group_id = ev.group_id ? ev.group_id : "";
      out.message_id = ev.message_id ? ev.message_id : "";
      out.file_id = ev.file_id;
      out.file_name = ev.file_name ? ev.file_name : "";
      out.file_size = ev.file_size;
      out.file_key_len = ev.file_key_len;
      std::memcpy(out.file_key.data(), ev.file_key, out.file_key.size());
      return true;
    }
  }
  return false;
}

bool DownloadFileMatches(mi_client_handle* handle,
                         const FileEventSnapshot& file,
                         const std::vector<std::uint8_t>& expected) {
  if (!handle || file.file_id.empty() || file.file_key_len != file.file_key.size()) {
    return false;
  }
  std::uint8_t* bytes = nullptr;
  std::uint64_t len = 0;
  const int ok = mi_client_download_chat_file_to_bytes(
      handle, file.file_id.c_str(), file.file_key.data(), file.file_key_len,
      file.file_name.c_str(), file.file_size, 0, &bytes, &len);
  if (ok != 1) {
    return false;
  }
  const bool match =
      len == expected.size() &&
      (expected.empty() ||
       std::memcmp(bytes, expected.data(), expected.size()) == 0);
  mi_client_free(bytes);
  return match;
}

bool WaitForMediaEvent(mi_client_handle* handle,
                       std::uint32_t type,
                       const std::string& match_sender,
                       const std::string& match_group,
                       const std::vector<std::uint8_t>& expected_payload,
                       std::uint32_t timeout_ms,
                       MediaEventSnapshot& out) {
  out = {};
  const auto deadline = mi::platform::NowSteadyMs() + timeout_ms;
  mi_event_t events[8]{};
  while (mi::platform::NowSteadyMs() < deadline) {
    const std::uint32_t count = mi_client_poll_event(handle, events, 8, 200);
    for (std::uint32_t i = 0; i < count; ++i) {
      const mi_event_t& ev = events[i];
      if (ev.type != type) {
        continue;
      }
      if (!match_sender.empty() && (!ev.sender || match_sender != ev.sender)) {
        continue;
      }
      if (!match_group.empty() && (!ev.group_id || match_group != ev.group_id)) {
        continue;
      }
      if (!ev.payload || ev.payload_len != expected_payload.size()) {
        continue;
      }
      if (!expected_payload.empty() &&
          std::memcmp(ev.payload, expected_payload.data(),
                      expected_payload.size()) != 0) {
        continue;
      }
      out.sender = ev.sender ? ev.sender : "";
      out.group_id = ev.group_id ? ev.group_id : "";
      std::memcpy(out.call_id.data(), ev.call_id, out.call_id.size());
      out.payload.assign(ev.payload, ev.payload + ev.payload_len);
      return true;
    }
  }
  return false;
}

bool PullMediaMatches(mi_client_handle* handle,
                      const std::array<std::uint8_t, 16>& call_id,
                      bool group,
                      const std::string& expected_sender,
                      const std::vector<std::uint8_t>& expected_payload) {
  mi_media_packet_t packets[8]{};
  const std::uint32_t count =
      group ? mi_client_pull_group_media(handle, call_id.data(),
                                         static_cast<std::uint32_t>(call_id.size()),
                                         8, 1000, packets)
            : mi_client_pull_media(handle, call_id.data(),
                                   static_cast<std::uint32_t>(call_id.size()),
                                   8, 1000, packets);
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto& p = packets[i];
    if (!p.sender || expected_sender != p.sender || !p.payload ||
        p.payload_len != expected_payload.size()) {
      continue;
    }
    if (expected_payload.empty() ||
        std::memcmp(p.payload, expected_payload.data(),
                    expected_payload.size()) == 0) {
      return true;
    }
  }
  return false;
}

bool WaitForGroupCallEvent(mi_client_handle* handle,
                           std::uint32_t op,
                           const std::string& match_sender,
                           const std::string& match_group,
                           const std::array<std::uint8_t, 16>& match_call_id,
                           std::uint32_t timeout_ms) {
  const auto deadline = mi::platform::NowSteadyMs() + timeout_ms;
  mi_event_t events[8]{};
  while (mi::platform::NowSteadyMs() < deadline) {
    const std::uint32_t count = mi_client_poll_event(handle, events, 8, 200);
    for (std::uint32_t i = 0; i < count; ++i) {
      const mi_event_t& ev = events[i];
      if (ev.type != MI_EVENT_GROUP_CALL || ev.call_op != op) {
        continue;
      }
      if (!match_sender.empty() && (!ev.sender || match_sender != ev.sender)) {
        continue;
      }
      if (!match_group.empty() && (!ev.group_id || match_group != ev.group_id)) {
        continue;
      }
      if (std::memcmp(ev.call_id, match_call_id.data(),
                      match_call_id.size()) != 0) {
        continue;
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

bool WaitForNoFriend(mi_client_handle* handle,
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
    bool found = false;
    for (std::uint32_t i = 0; i < count; ++i) {
      const char* name = entries[i].username;
      if (name && friend_username == name) {
        found = true;
        break;
      }
    }
    if (!found) {
      return true;
    }
    mi::platform::SleepMs(100);
  }
  return false;
}

bool WaitForGroupMemberRole(mi_client_handle* handle,
                            const std::string& group_id,
                            const std::string& username,
                            std::uint32_t role,
                            std::uint32_t timeout_ms) {
  if (!handle || group_id.empty() || username.empty()) {
    return false;
  }
  const auto deadline = mi::platform::NowSteadyMs() + timeout_ms;
  mi_group_member_entry_t entries[8]{};
  while (mi::platform::NowSteadyMs() < deadline) {
    const std::uint32_t count = mi_client_list_group_members_info(
        handle, group_id.c_str(), entries, 8);
    for (std::uint32_t i = 0; i < count; ++i) {
      const char* name = entries[i].username;
      if (name && username == name && entries[i].role == role) {
        return true;
      }
    }
    mi::platform::SleepMs(100);
  }
  return false;
}

bool DeviceListContains(mi_client_handle* handle,
                        const std::string& target_device_id,
                        std::uint32_t min_count,
                        std::uint32_t timeout_ms) {
  if (!handle || target_device_id.empty()) {
    return false;
  }
  const auto deadline = mi::platform::NowSteadyMs() + timeout_ms;
  mi_device_entry_t entries[8]{};
  while (mi::platform::NowSteadyMs() < deadline) {
    const std::uint32_t count = mi_client_list_devices(handle, entries, 8);
    bool found = false;
    for (std::uint32_t i = 0; i < count; ++i) {
      const char* id = entries[i].device_id;
      if (id && target_device_id == id) {
        found = true;
        break;
      }
    }
    if (found && count >= min_count) {
      return true;
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
  std::string private_msg_id;

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
    if (net) {
      net->Stop();
    }
    net.reset();
    listener.reset();
    app.reset();
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
#ifdef _WIN32
    StopLaunchedServer();
#endif
    SetEnv("MI_E2EE_DATA_DIR", prev_data_dir);
    SetEnv("MI_E2EE_HARDENING", prev_hardening);
    RestoreTestUsers(backup);
    std::error_code ec;
    if (!base_dir.empty() &&
        !IsTruthyEnv(GetEnv("MI_E2EE_E2E_KEEP_RUNTIME_DIR"))) {
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
      private_msg_id = msg_id;
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

  if (!private_msg_id.empty()) {
    if (mi_client_send_read_receipt(bob, "alice", private_msg_id.c_str()) != 1) {
      return fail("read receipt send failed", bob);
    }
    if (!WaitForPeerEvent(alice, MI_EVENT_READ_RECEIPT, "bob", private_msg_id,
                          kChatTimeoutMs)) {
      return fail("read receipt event timeout", alice);
    }
  }
  if (mi_client_send_typing(alice, "bob", 1) != 1) {
    return fail("typing send failed", alice);
  }
  if (!WaitForPeerEvent(bob, MI_EVENT_TYPING, "alice", "", kChatTimeoutMs)) {
    return fail("typing event timeout", bob);
  }
  if (mi_client_send_presence(alice, "bob", 1) != 1) {
    return fail("presence send failed", alice);
  }
  if (!WaitForPeerEvent(bob, MI_EVENT_PRESENCE, "alice", "", kChatTimeoutMs)) {
    return fail("presence event timeout", bob);
  }
  LogStep("chat receipts ok");

  const std::vector<std::uint8_t> private_file_bytes = {
      0x6d, 0x69, 0x2d, 0x65, 0x32, 0x65, 0x65, 0x2d,
      0x70, 0x72, 0x69, 0x76, 0x61, 0x74, 0x65, 0x2d,
      0x66, 0x69, 0x6c, 0x65, 0x0a, 0x01, 0x02, 0x03};
  const auto private_file_path = base_dir / "payloads" / "private.bin";
  if (!WriteBytesToFile(private_file_path, private_file_bytes)) {
    return fail("write private file payload failed", nullptr);
  }
  if (mi_client_send_private_file(alice, "bob",
                                  private_file_path.u8string().c_str(),
                                  &msg_id) != 1 ||
      !msg_id) {
    return fail("private file send failed", alice);
  }
  mi_client_free(msg_id);
  msg_id = nullptr;
  FileEventSnapshot private_file;
  if (!WaitForFileEvent(bob, MI_EVENT_CHAT_FILE, "alice", "",
                        kFileTimeoutMs, private_file)) {
    return fail("private file event timeout", bob);
  }
  if (!DownloadFileMatches(bob, private_file, private_file_bytes)) {
    return fail("private file download mismatch", bob);
  }
  LogStep("private file ok");

  const auto private_media_call_id = FixedCallId(0x30);
  const std::vector<std::uint8_t> private_media_packet = {
      0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00,
      0x00, 0x00, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
      0x10, 0x20, 0x30, 0x40, 0x51, 0x52, 0x53, 0x54,
      0x55, 0x56, 0x57, 0x58};
  if (mi_client_add_media_subscription(
          bob, private_media_call_id.data(),
          static_cast<std::uint32_t>(private_media_call_id.size()), 0,
          nullptr) != 1) {
    return fail("private media subscription failed", bob);
  }
  if (mi_client_push_media(alice, "bob", private_media_call_id.data(),
                           static_cast<std::uint32_t>(private_media_call_id.size()),
                           private_media_packet.data(),
                           static_cast<std::uint32_t>(private_media_packet.size())) !=
      1) {
    return fail("private media push failed", alice);
  }
  MediaEventSnapshot private_media;
  if (!WaitForMediaEvent(bob, MI_EVENT_MEDIA_RELAY, "alice", "",
                         private_media_packet, kMediaTimeoutMs,
                         private_media)) {
    return fail("private media event timeout", bob);
  }
  LogStep("private media ok");

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

  if (!WaitForGroupMemberRole(alice, group_id, "alice", 0, kChatTimeoutMs) ||
      !WaitForGroupMemberRole(alice, group_id, "bob", 2, kChatTimeoutMs)) {
    return fail("group member list failed", alice);
  }
  if (mi_client_set_group_member_role(alice, group_id, "bob", 1) != 1) {
    return fail("group role set failed", alice);
  }
  if (!WaitForGroupMemberRole(alice, group_id, "bob", 1, kChatTimeoutMs)) {
    return fail("group role sync timeout", alice);
  }
  LogStep("group members ok");

  const std::vector<std::uint8_t> group_file_bytes = {
      0x67, 0x72, 0x6f, 0x75, 0x70, 0x2d, 0x66, 0x69,
      0x6c, 0x65, 0x2d, 0x65, 0x32, 0x65, 0x65, 0x0a,
      0x90, 0x91, 0x92, 0x93, 0x94};
  const auto group_file_path = base_dir / "payloads" / "group.bin";
  if (!WriteBytesToFile(group_file_path, group_file_bytes)) {
    return fail("write group file payload failed", nullptr);
  }
  if (mi_client_send_group_file(alice, group_id,
                                group_file_path.u8string().c_str(),
                                &msg_id) != 1 ||
      !msg_id) {
    return fail("group file send failed", alice);
  }
  mi_client_free(msg_id);
  msg_id = nullptr;
  FileEventSnapshot group_file;
  if (!WaitForFileEvent(bob, MI_EVENT_GROUP_FILE, "alice", group_id,
                        kFileTimeoutMs, group_file)) {
    return fail("group file event timeout", bob);
  }
  if (!DownloadFileMatches(bob, group_file, group_file_bytes)) {
    return fail("group file download mismatch", bob);
  }
  LogStep("group file ok");

  std::uint8_t call_id_buf[16]{};
  std::uint32_t call_key_id = 0;
  if (mi_client_start_group_call(alice, group_id, 1, call_id_buf,
                                 sizeof(call_id_buf), &call_key_id) != 1 ||
      call_key_id == 0) {
    return fail("group call start failed", alice);
  }
  std::array<std::uint8_t, 16> group_call_id{};
  std::memcpy(group_call_id.data(), call_id_buf, group_call_id.size());
  if (!WaitForGroupCallEvent(bob, 1, "alice", group_id, group_call_id,
                             kMediaTimeoutMs)) {
    return fail("group call create event timeout", bob);
  }
  std::uint32_t bob_call_key_id = 0;
  if (mi_client_join_group_call(bob, group_id, group_call_id.data(),
                                static_cast<std::uint32_t>(group_call_id.size()),
                                1, &bob_call_key_id) != 1 ||
      bob_call_key_id == 0) {
    return fail("group call join failed", bob);
  }
  if (!WaitForGroupCallEvent(alice, 2, "bob", group_id, group_call_id,
                             kMediaTimeoutMs)) {
    return fail("group call join event timeout", alice);
  }

  const std::vector<std::uint8_t> subscription =
      BuildGroupCallSubscriptionPayload("alice", 0x01);
  std::uint32_t member_count = 0;
  if (mi_client_send_group_call_signal(
          bob, 5, group_id, group_call_id.data(),
          static_cast<std::uint32_t>(group_call_id.size()), 1,
          bob_call_key_id, 1, 0, subscription.data(),
          static_cast<std::uint32_t>(subscription.size()), nullptr, 0,
          nullptr, nullptr, 0, &member_count) != 1) {
    return fail("group call subscription update failed", bob);
  }
  const std::vector<std::uint8_t> group_media_packet =
      BuildSyntheticAudioPacket();
  if (mi_client_push_group_media(
          alice, group_id, group_call_id.data(),
          static_cast<std::uint32_t>(group_call_id.size()),
          group_media_packet.data(),
          static_cast<std::uint32_t>(group_media_packet.size())) != 1) {
    return fail("group media push failed", alice);
  }
  if (!PullMediaMatches(bob, group_call_id, true, "alice",
                        group_media_packet)) {
    return fail("group media pull mismatch", bob);
  }
  if (mi_client_leave_group_call(bob, group_id, group_call_id.data(),
                                 static_cast<std::uint32_t>(
                                     group_call_id.size())) != 1) {
    return fail("group call leave failed", bob);
  }
  if (!WaitForGroupCallEvent(alice, 3, "bob", group_id, group_call_id,
                             kMediaTimeoutMs)) {
    return fail("group call leave event timeout", alice);
  }
  (void)mi_client_leave_group_call(alice, group_id, group_call_id.data(),
                                   static_cast<std::uint32_t>(
                                       group_call_id.size()));
  LogStep("group call media ok");

  if (mi_client_kick_group_member(alice, group_id, "bob") != 1) {
    return fail("group member kick failed", alice);
  }
  if (mi_client_send_group_text(bob, group_id, "after kick", &group_msg_id) ==
      1) {
    if (group_msg_id) {
      mi_client_free(group_msg_id);
      group_msg_id = nullptr;
    }
    return fail("kicked member could still send group text", bob);
  }
  LogStep("group kick ok");

  mi_history_entry_t history[16]{};
  const std::uint32_t history_count =
      mi_client_export_recent_history_snapshot(alice, 8, 8, history, 16);
  bool saw_private_history = false;
  bool saw_group_history = false;
  for (std::uint32_t i = 0; i < history_count; ++i) {
    const mi_history_entry_t& entry = history[i];
    const std::string conv = entry.conv_id ? entry.conv_id : "";
    if (!entry.is_group && conv == "bob") {
      saw_private_history = true;
    }
    if (entry.is_group && conv == group_id) {
      saw_group_history = true;
    }
  }
  if (!saw_private_history || !saw_group_history) {
    return fail("recent history snapshot missing conversations", alice);
  }
  LogStep("history snapshot ok");

  if (mi_client_delete_friend(alice, "bob") != 1) {
    return fail("friend delete failed", alice);
  }
  if (!WaitForNoFriend(alice, "bob", kFriendTimeoutMs) ||
      !WaitForNoFriend(bob, "alice", kFriendTimeoutMs)) {
    return fail("friend delete sync timeout", alice);
  }
  if (mi_client_send_private_text(alice, "bob", "after delete", &msg_id) == 1) {
    if (msg_id) {
      mi_client_free(msg_id);
      msg_id = nullptr;
    }
    return fail("deleted friend could still receive private text", alice);
  }
  if (mi_client_add_friend(alice, "bob", "restored") != 1) {
    return fail("friend direct add failed", alice);
  }
  if (!WaitForFriend(alice, "bob", kFriendTimeoutMs) ||
      !WaitForFriend(bob, "alice", kFriendTimeoutMs)) {
    return fail("friend direct add sync timeout", alice);
  }
  if (mi_client_set_user_blocked(alice, "bob", 1) != 1 ||
      !WaitForNoFriend(alice, "bob", kFriendTimeoutMs) ||
      !WaitForNoFriend(bob, "alice", kFriendTimeoutMs)) {
    return fail("friend block failed", alice);
  }
  if (mi_client_set_user_blocked(alice, "bob", 0) != 1) {
    return fail("friend unblock failed", alice);
  }
  LogStep("friend delete block ok");

  const char* linked_device_raw = mi_client_device_id(alice_linked);
  const std::string linked_device_id =
      linked_device_raw ? linked_device_raw : "";
  if (linked_device_id.empty()) {
    return fail("linked device id missing", alice_linked);
  }
  mi_device_entry_t linked_entries[8]{};
  (void)mi_client_list_devices(alice_linked, linked_entries, 8);
  if (!DeviceListContains(alice, linked_device_id, 2, kDeviceTimeoutMs)) {
    return fail("linked device missing from device list", alice);
  }
  if (mi_client_kick_device(alice, linked_device_id.c_str()) != 1) {
    return fail("device kick failed", alice);
  }
  if (mi_client_heartbeat(alice_linked) == 1) {
    return fail("kicked linked device heartbeat still succeeded",
                alice_linked);
  }
  LogStep("device kick ok");

  mi_client_free(group_id);
  group_id = nullptr;

  cleanup();
  return 0;
}
