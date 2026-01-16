#ifndef MI_E2EE_SERVER_NETWORK_SERVER_H
#define MI_E2EE_SERVER_NETWORK_SERVER_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <deque>
#include <functional>
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
  std::uint32_t max_worker_threads{0};
  std::uint32_t max_io_threads{0};
  std::uint32_t max_pending_tasks{1024};
};

//  TCP/KCP 
class NetworkServer {
  struct Connection;
  class Reactor;
  class IocpEngine;

 public:
  NetworkServer(Listener* listener, std::uint16_t port, bool tls_enable = false,
                std::string tls_cert = "mi_e2ee_server.pfx",
                bool iocp_enable = false,
                NetworkServerLimits limits = NetworkServerLimits{});
  ~NetworkServer();

  bool Start(std::string& error);
  void Stop();

 private:
  void Run();
  bool StartSocket(std::string& error);
  void StopSocket();
  void StartWorkers();
  void StopWorkers();
  void StartReactors();
  void StopReactors();
  bool StartIocp(std::string& error);
  void StopIocp();
  void AssignConnection(std::shared_ptr<Connection> conn);
  void WorkerLoop();
  bool EnqueueTask(std::function<void()> task);
  bool TryAcquireConnectionSlot(const std::string& remote_ip);
  void ReleaseConnectionSlot(const std::string& remote_ip);

  Listener* listener_;
  std::uint16_t port_{0};
  bool tls_enable_{false};
  std::string tls_cert_;
  bool iocp_enable_{false};
  bool use_iocp_{false};
  NetworkServerLimits limits_;
  std::atomic<bool> running_{false};
  std::thread worker_;
  std::atomic<std::uint32_t> active_connections_{0};
  std::mutex conn_mutex_;
  std::unordered_map<std::string, std::uint32_t> connections_by_ip_;
  std::atomic<bool> pool_running_{false};
  std::vector<std::thread> worker_threads_;
  std::mutex work_mutex_;
  std::condition_variable work_cv_;
  std::deque<std::function<void()>> work_queue_;
  std::vector<std::unique_ptr<Reactor>> reactors_;
#ifdef _WIN32
  std::unique_ptr<IocpEngine> iocp_;
#endif
  std::atomic<std::uint32_t> next_reactor_{0};

#ifdef MI_E2EE_ENABLE_TCP_SERVER
  std::intptr_t listen_fd_{-1};
#endif

#ifdef MI_E2EE_ENABLE_TCP_SERVER
  struct TlsServer;
  struct TlsServerDeleter {
    void operator()(TlsServer* p) const;
  };
  std::unique_ptr<TlsServer, TlsServerDeleter> tls_{nullptr};
#endif
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_NETWORK_SERVER_H
