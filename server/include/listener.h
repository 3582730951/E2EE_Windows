#ifndef MI_E2EE_SERVER_LISTENER_H
#define MI_E2EE_SERVER_LISTENER_H

#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "connection_handler.h"

namespace mi::server {

//  ConnectionHandler 
class Listener {
 public:
  explicit Listener(ServerApp* app);
  ~Listener();

  //  KCP/TCP 
  bool Process(const std::vector<std::uint8_t>& frame_bytes,
               std::vector<std::uint8_t>& out_bytes);

 private:
  ConnectionHandler handler_;
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_LISTENER_H
