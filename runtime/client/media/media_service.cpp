#include "media_service.h"

#include "client_core.h"

namespace mi::client {

bool MediaService::DeriveMediaRoot(
    ClientCore& core, const std::string& peer_username,
    const std::array<std::uint8_t, 16>& call_id,
    std::array<std::uint8_t, 32>& out_media_root,
    std::string& out_error) const {
  out_error.clear();
  core.last_error_.clear();
  out_media_root.fill(0);
  if (!core.EnsureE2ee()) {
    out_error = core.last_error_.empty() ? "e2ee not ready" : core.last_error_;
    return false;
  }
  if (peer_username.empty()) {
    out_error = "peer username empty";
    core.last_error_ = out_error;
    return false;
  }
  if (!core.e2ee_.DeriveMediaRoot(peer_username, call_id, out_media_root,
                                  out_error)) {
    if (out_error.empty()) {
      out_error = "media root derive failed";
    }
    core.last_error_ = out_error;
    return false;
  }
  return true;
}

}  // namespace mi::client
