#ifndef MI_E2EE_CLIENT_MEDIA_TRANSPORT_H
#define MI_E2EE_CLIENT_MEDIA_TRANSPORT_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mi::client::media {

struct MediaRelayPacket {
  std::string sender;
  std::vector<std::uint8_t> payload;
};

class MediaTransport {
 public:
  virtual ~MediaTransport() = default;

  virtual bool DeriveMediaRoot(const std::string& peer_username,
                               const std::array<std::uint8_t, 16>& call_id,
                               std::array<std::uint8_t, 32>& out_media_root,
                               std::string& out_error) = 0;
  virtual bool PushMedia(const std::string& peer_username,
                         const std::array<std::uint8_t, 16>& call_id,
                         const std::vector<std::uint8_t>& packet,
                         std::string& out_error) = 0;
  virtual bool PullMedia(const std::array<std::uint8_t, 16>& call_id,
                         std::uint32_t max_packets,
                         std::uint32_t wait_ms,
                         std::vector<MediaRelayPacket>& out_packets,
                         std::string& out_error) = 0;
  virtual bool PushGroupMedia(const std::string& group_id,
                              const std::array<std::uint8_t, 16>& call_id,
                              const std::vector<std::uint8_t>& packet,
                              std::string& out_error) = 0;
  virtual bool PullGroupMedia(const std::array<std::uint8_t, 16>& call_id,
                              std::uint32_t max_packets,
                              std::uint32_t wait_ms,
                              std::vector<MediaRelayPacket>& out_packets,
                              std::string& out_error) = 0;
  virtual bool GetGroupCallKey(const std::string& group_id,
                               const std::array<std::uint8_t, 16>& call_id,
                               std::uint32_t key_id,
                               std::array<std::uint8_t, 32>& out_key,
                               std::string& out_error) = 0;
};

}  // namespace mi::client::media

#endif  // MI_E2EE_CLIENT_MEDIA_TRANSPORT_H
