#include "client_core.h"

#include "c_api.h"
#include "transport_service.h"

namespace mi::client {

void ClientCore::ResetRemoteStream() {
  TransportService().ResetRemoteStream(*this);
}

bool ClientCore::EnsureChannel() {
  return TransportService().EnsureChannel(*this);
}

bool ClientCore::ProcessRaw(const std::vector<std::uint8_t>& in_bytes,
                            std::vector<std::uint8_t>& out_bytes) {
  return TransportService().ProcessRaw(*this, in_bytes, out_bytes);
}

}  // namespace mi::client
