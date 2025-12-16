#include <memory>
#include <string>
#include <thread>
#include <chrono>

#include "server_app.h"
#include "network_server.h"

namespace {

void LogError(const std::string&) {}
void LogInfo(const std::string&) {}

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
  LogInfo(std::string("server config loaded. mode=") +
          (cfg.mode == mi::server::AuthMode::kDemo ? "demo" : "mysql") +
          " listen_port=" + std::to_string(cfg.server.listen_port));

  mi::server::Listener listener(&app);
  mi::server::NetworkServerLimits limits;
  limits.max_connections = cfg.server.max_connections;
  limits.max_connections_per_ip = cfg.server.max_connections_per_ip;
  limits.max_connection_bytes = cfg.server.max_connection_bytes;
  mi::server::NetworkServer net(&listener, cfg.server.listen_port,
                                cfg.server.tls_enable, cfg.server.tls_cert,
                                limits);
  if (!net.Start()) {
    LogError("network server start failed (placeholder)");
    return 1;
  }
  LogInfo("server initialized (network placeholder running)");
  // 占位运行，保持主线程。
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
  return 0;
}
