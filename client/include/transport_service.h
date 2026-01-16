#ifndef MI_E2EE_CLIENT_TRANSPORT_SERVICE_H
#define MI_E2EE_CLIENT_TRANSPORT_SERVICE_H

#include <cstdint>
#include <vector>

#include "frame.h"

namespace mi::client {

class ClientCore;

class TransportService {
 public:
  void ResetRemoteStream(ClientCore& core) const;
  bool EnsureChannel(ClientCore& core) const;
  bool ProcessRaw(ClientCore& core,
                  const std::vector<std::uint8_t>& in_bytes,
                  std::vector<std::uint8_t>& out_bytes) const;
  bool ProcessEncrypted(ClientCore& core,
                        mi::server::FrameType type,
                        const std::vector<std::uint8_t>& plain,
                        std::vector<std::uint8_t>& out_plain) const;
};

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_TRANSPORT_SERVICE_H
