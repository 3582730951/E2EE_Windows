#include "listener.h"

namespace mi::server {

Listener::Listener(ServerApp* app) : handler_(app) {}

Listener::~Listener() = default;

bool Listener::Process(const std::vector<std::uint8_t>& frame_bytes,
                       std::vector<std::uint8_t>& out_bytes) {
  return handler_.OnData(frame_bytes.data(), frame_bytes.size(), out_bytes, {});
}

bool Listener::Process(const std::vector<std::uint8_t>& frame_bytes,
                       std::vector<std::uint8_t>& out_bytes,
                       const std::string& remote_ip) {
  return handler_.OnData(frame_bytes.data(), frame_bytes.size(), out_bytes,
                         remote_ip);
}

}  // namespace mi::server
