#ifndef MI_E2EE_SERVER_CONNECTION_HANDLER_H
#define MI_E2EE_SERVER_CONNECTION_HANDLER_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>

#include "frame.h"
#include "server_app.h"
#include "secure_channel.h"

namespace mi::server {

class ConnectionHandler {
 public:
  explicit ConnectionHandler(ServerApp* app);

  // 
  bool OnData(const std::uint8_t* data, std::size_t len,
              std::vector<std::uint8_t>& out_bytes);

 private:
  struct ChannelState {
    SecureChannel channel;
    std::uint64_t send_seq{0};
    std::uint64_t recv_seq{0};
  };

  ServerApp* app_;
  std::unordered_map<std::string, ChannelState> channel_states_;
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_CONNECTION_HANDLER_H
