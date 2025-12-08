#ifndef MI_E2EE_SERVER_NETWORK_SERVER_H
#define MI_E2EE_SERVER_NETWORK_SERVER_H

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "listener.h"

namespace mi::server {

//  TCP/KCP 
class NetworkServer {
 public:
  NetworkServer(Listener* listener, std::uint16_t port);
  ~NetworkServer();

  bool Start();
  void Stop();

 private:
  void Run();
  bool StartSocket();
  void StopSocket();

  Listener* listener_;
  std::uint16_t port_{0};
  std::atomic<bool> running_{false};
  std::thread worker_;

#ifdef MI_E2EE_ENABLE_TCP_SERVER
  int listen_fd_{-1};
#endif
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_NETWORK_SERVER_H
