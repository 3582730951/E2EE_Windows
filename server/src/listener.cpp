#include "listener.h"

namespace mi::server {

Listener::Listener(ServerApp* app) : handler_(app) {}

Listener::~Listener() = default;

bool Listener::Process(const std::vector<std::uint8_t>& frame_bytes,
                       std::vector<std::uint8_t>& out_bytes,
                       TransportKind transport) {
  return handler_.OnData(frame_bytes.data(), frame_bytes.size(), out_bytes, {},
                         transport);
}

bool Listener::Process(const std::vector<std::uint8_t>& frame_bytes,
                       std::vector<std::uint8_t>& out_bytes,
                       const std::string& remote_ip,
                       TransportKind transport) {
  return handler_.OnData(frame_bytes.data(), frame_bytes.size(), out_bytes,
                         remote_ip, transport);
}

}  // namespace mi::server
