#ifndef MI_E2EE_SERVER_NETWORK_SERVER_H
#define MI_E2EE_SERVER_NETWORK_SERVER_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "listener.h"

namespace mi::server {

struct NetworkServerLimits {
  std::uint32_t max_connections{256};
  std::uint32_t max_connections_per_ip{64};
  std::uint32_t max_connection_bytes{256u * 1024u * 1024u};
};

//  TCP/KCP 
class NetworkServer {
 public:
  NetworkServer(Listener* listener, std::uint16_t port, bool tls_enable = false,
                std::string tls_cert = "mi_e2ee_server.pfx",
                NetworkServerLimits limits = NetworkServerLimits{});
  ~NetworkServer();

  bool Start();
  void Stop();

 private:
  void Run();
  bool StartSocket();
  void StopSocket();
  bool TryAcquireConnectionSlot(const std::string& remote_ip);
  void ReleaseConnectionSlot(const std::string& remote_ip);

  Listener* listener_;
  std::uint16_t port_{0};
  bool tls_enable_{false};
 std::string tls_cert_;
  NetworkServerLimits limits_;
  std::atomic<bool> running_{false};
  std::thread worker_;
  std::atomic<std::uint32_t> active_connections_{0};
  std::mutex conn_mutex_;
  std::unordered_map<std::string, std::uint32_t> connections_by_ip_;

#ifdef MI_E2EE_ENABLE_TCP_SERVER
  int listen_fd_{-1};
#endif

#ifdef _WIN32
  struct TlsServer;
  struct TlsServerDeleter {
    void operator()(TlsServer* p) const;
  };
  std::unique_ptr<TlsServer, TlsServerDeleter> tls_{nullptr};
#endif
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_NETWORK_SERVER_H
