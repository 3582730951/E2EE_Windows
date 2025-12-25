#ifndef MI_E2EE_SERVER_KCP_SERVER_H
#define MI_E2EE_SERVER_KCP_SERVER_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "listener.h"
#include "network_server.h"

namespace mi::server {

struct KcpOptions {
  std::uint32_t mtu{1400};
  std::uint32_t snd_wnd{256};
  std::uint32_t rcv_wnd{256};
  std::uint32_t nodelay{1};
  std::uint32_t interval{10};
  std::uint32_t resend{2};
  std::uint32_t nc{1};
  std::uint32_t min_rto{30};
  std::uint32_t session_idle_sec{60};
};

class KcpServer {
 public:
  KcpServer(Listener* listener, std::uint16_t port, KcpOptions options,
            NetworkServerLimits limits);
  ~KcpServer();

  bool Start(std::string& error);
  void Stop();

 private:
  void Run();
  bool StartSocket(std::string& error);
  void StopSocket();
  bool TryAcquireConnectionSlot(const std::string& remote_ip);
  void ReleaseConnectionSlot(const std::string& remote_ip);

  Listener* listener_;
  std::uint16_t port_{0};
  KcpOptions options_{};
  NetworkServerLimits limits_{};
  std::atomic<bool> running_{false};
  std::thread worker_;
  std::mutex conn_mutex_;
  std::unordered_map<std::string, std::uint32_t> connections_by_ip_;
  std::uint32_t active_connections_{0};
  std::intptr_t sock_{-1};
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_KCP_SERVER_H
