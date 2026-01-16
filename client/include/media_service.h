#ifndef MI_E2EE_CLIENT_MEDIA_SERVICE_H
#define MI_E2EE_CLIENT_MEDIA_SERVICE_H

#include <array>
#include <cstdint>
#include <string>

namespace mi::client {

class ClientCore;

class MediaService {
 public:
  bool DeriveMediaRoot(ClientCore& core, const std::string& peer_username,
                       const std::array<std::uint8_t, 16>& call_id,
                       std::array<std::uint8_t, 32>& out_media_root,
                       std::string& out_error) const;
};

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_MEDIA_SERVICE_H
