#include <memory>
#include <string>
#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "server_app.h"
#include "network_server.h"
#include "kcp_server.h"
#include "platform_log.h"
#include "platform_time.h"

namespace {

void LogError(const std::string& msg) {
  mi::platform::log::Log(mi::platform::log::Level::kError, "server", msg);
}

void LogInfo(bool enabled, const std::string& msg) {
  if (!enabled) {
    return;
  }
  mi::platform::log::Log(mi::platform::log::Level::kInfo, "server", msg);
}

#ifdef _WIN32
#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif
#ifndef LOAD_LIBRARY_SEARCH_USER_DIRS
#define LOAD_LIBRARY_SEARCH_USER_DIRS 0x00000400
#endif
#ifndef DLL_DIRECTORY_COOKIE
using DLL_DIRECTORY_COOKIE = void*;
#endif

using SetDefaultDllDirectoriesFn = BOOL(WINAPI*)(DWORD);
using AddDllDirectoryFn = DLL_DIRECTORY_COOKIE(WINAPI*)(PCWSTR);

std::wstring GetModuleDir() {
  wchar_t path[MAX_PATH] = {};
  const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return {};
  }
  std::wstring full(path, len);
  const size_t pos = full.find_last_of(L"\\/");
  if (pos == std::wstring::npos) {
    return {};
  }
  return full.substr(0, pos);
}

bool DirExists(const std::wstring& path) {
  const DWORD attr = GetFileAttributesW(path.c_str());
  return attr != INVALID_FILE_ATTRIBUTES &&
         (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

void ApplyDllSearchHardening() {
  SetDllDirectoryW(L"");
  const auto kernel32 = GetModuleHandleW(L"kernel32.dll");
  if (!kernel32) {
    return;
  }
  const auto setDefault =
      reinterpret_cast<SetDefaultDllDirectoriesFn>(
          GetProcAddress(kernel32, "SetDefaultDllDirectories"));
  if (setDefault) {
    setDefault(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS);
  }
  const auto addDir =
      reinterpret_cast<AddDllDirectoryFn>(
          GetProcAddress(kernel32, "AddDllDirectory"));
  if (!addDir) {
    return;
  }
  const std::wstring exe_dir = GetModuleDir();
  if (exe_dir.empty()) {
    return;
  }
  const std::wstring dll_dir = exe_dir + L"\\dll";
  if (DirExists(dll_dir)) {
    addDir(dll_dir.c_str());
  }
}
#endif

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  ApplyDllSearchHardening();
#endif
  std::string config_path;
  if (argc > 1) {
    config_path = argv[1];
  } else {
    std::error_code ec;
    const bool has_config_dir = std::filesystem::exists("config/config.ini", ec);
    config_path = has_config_dir ? "config/config.ini" : "config.ini";
  }

  std::string error;
  mi::server::ServerApp app;
  if (!app.Init(config_path, error)) {
    LogError(error);
    return 1;
  }

  const auto& cfg = app.config();
  const bool verbose = cfg.server.debug_log;
  if (verbose) {
    const std::string mode =
        cfg.mode == mi::server::AuthMode::kDemo ? "demo" : "mysql";
    const std::string port = std::to_string(cfg.server.listen_port);
    mi::platform::log::Log(mi::platform::log::Level::kInfo, "server",
                           "server config loaded",
                           {{"mode", mode}, {"listen_port", port}});
  }

  mi::server::Listener listener(&app);
  mi::server::NetworkServerLimits limits;
  limits.max_connections = cfg.server.max_connections;
  limits.max_connections_per_ip = cfg.server.max_connections_per_ip;
  limits.max_connection_bytes = cfg.server.max_connection_bytes;
  limits.max_worker_threads = cfg.server.max_worker_threads;
  limits.max_io_threads = cfg.server.max_io_threads;
  limits.max_pending_tasks = cfg.server.max_pending_tasks;
#ifdef _WIN32
  const bool iocp_enable = cfg.server.iocp_enable;
#else
  const bool iocp_enable = false;
#endif
  mi::server::NetworkServer net(&listener, cfg.server.listen_port,
                                cfg.server.tls_enable, cfg.server.tls_cert,
                                iocp_enable,
                                limits);
  std::string net_error;
  if (!net.Start(net_error)) {
    LogError(net_error.empty() ? "network server start failed" : net_error);
    return 1;
  }

  std::unique_ptr<mi::server::KcpServer> kcp;
  if (cfg.server.kcp_enable) {
    if (cfg.server.require_tls) {
      LogError("kcp disabled because require_tls=1");
    } else {
      const std::uint16_t kcp_port =
          cfg.server.kcp_port == 0 ? cfg.server.listen_port
                                   : cfg.server.kcp_port;
      mi::server::KcpOptions kcp_opts;
      kcp_opts.mtu = cfg.server.kcp_mtu;
      kcp_opts.snd_wnd = cfg.server.kcp_snd_wnd;
      kcp_opts.rcv_wnd = cfg.server.kcp_rcv_wnd;
      kcp_opts.nodelay = cfg.server.kcp_nodelay;
      kcp_opts.interval = cfg.server.kcp_interval;
      kcp_opts.resend = cfg.server.kcp_resend;
      kcp_opts.nc = cfg.server.kcp_nc;
      kcp_opts.min_rto = cfg.server.kcp_min_rto;
      kcp_opts.session_idle_sec = cfg.server.kcp_session_idle_sec;
      kcp = std::make_unique<mi::server::KcpServer>(&listener, kcp_port,
                                                    kcp_opts, limits);
      std::string kcp_error;
      if (!kcp->Start(kcp_error)) {
        LogError(kcp_error.empty() ? "kcp server start failed" : kcp_error);
        return 1;
      }
      LogInfo(verbose, "kcp server initialized");
    }
  }

  LogInfo(verbose, "server initialized");
  while (true) {
    std::string tick_error;
    if (!app.RunOnce(tick_error) && !tick_error.empty()) {
      LogError(tick_error);
    }
    mi::platform::SleepMs(1000);
  }
  return 0;
}
