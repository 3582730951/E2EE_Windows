#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <iostream>

#include "server_app.h"
#include "network_server.h"

namespace {

void LogError(const std::string& msg) {
  std::cerr << "[mi_e2ee_server] " << msg << "\n";
}

void LogInfo(bool enabled, const std::string& msg) {
  if (!enabled) {
    return;
  }
  std::cout << "[mi_e2ee_server] " << msg << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  const std::string config_path = (argc > 1) ? argv[1] : "config.ini";

  std::string error;
  mi::server::ServerApp app;
  if (!app.Init(config_path, error)) {
    LogError(error);
    return 1;
  }

  const auto& cfg = app.config();
  const bool verbose = cfg.server.debug_log;
  LogInfo(verbose, std::string("server config loaded. mode=") +
                        (cfg.mode == mi::server::AuthMode::kDemo ? "demo"
                                                                : "mysql") +
                        " listen_port=" +
                        std::to_string(cfg.server.listen_port));

  mi::server::Listener listener(&app);
  mi::server::NetworkServerLimits limits;
  limits.max_connections = cfg.server.max_connections;
  limits.max_connections_per_ip = cfg.server.max_connections_per_ip;
  limits.max_connection_bytes = cfg.server.max_connection_bytes;
  mi::server::NetworkServer net(&listener, cfg.server.listen_port,
                                cfg.server.tls_enable, cfg.server.tls_cert,
                                limits);
  std::string net_error;
  if (!net.Start(net_error)) {
    LogError(net_error.empty() ? "network server start failed" : net_error);
    return 1;
  }
  LogInfo(verbose, "server initialized");
  while (true) {
    std::string tick_error;
    if (!app.RunOnce(tick_error) && !tick_error.empty()) {
      LogError(tick_error);
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return 0;
}
