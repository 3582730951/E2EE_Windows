#ifndef MI_E2EE_CLIENT_GROUP_CALL_SESSION_H
#define MI_E2EE_CLIENT_GROUP_CALL_SESSION_H

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "media_crypto.h"
#include "media_jitter_buffer.h"
#include "media_transport.h"

namespace mi::client::media {

struct GroupCallSessionConfig {
  std::string group_id;
  std::array<std::uint8_t, 16> call_id{};
  std::uint32_t key_id{1};
  bool enable_audio{true};
  bool enable_video{true};
  std::uint64_t audio_delay_ms{60};
  std::uint64_t video_delay_ms{120};
  std::size_t audio_max_frames{256};
  std::size_t video_max_frames{256};
};

struct GroupMediaFrame {
  std::string sender;
  mi::media::MediaFrame frame;
};

class GroupCallSession {
 public:
  GroupCallSession(MediaTransport& transport, GroupCallSessionConfig config);

  bool Init(std::string& error);
  bool SetActiveKey(std::uint32_t key_id, std::string& error);

  bool SendAudioFrame(const std::vector<std::uint8_t>& payload,
                      std::uint64_t timestamp_ms,
                      std::uint8_t flags = 0);
  bool SendVideoFrame(const std::vector<std::uint8_t>& payload,
                      std::uint64_t timestamp_ms,
                      std::uint8_t flags = 0);

  bool PollIncoming(std::uint32_t max_packets,
                    std::uint32_t wait_ms,
                    std::string& error);

  bool PopAudioFrame(std::uint64_t now_ms, GroupMediaFrame& out);
  bool PopVideoFrame(std::uint64_t now_ms, GroupMediaFrame& out);

  const GroupCallSessionConfig& config() const { return config_; }

 private:
  struct SenderState {
    std::uint32_t key_id{0};
    std::unique_ptr<MediaRatchet> audio_recv;
    std::unique_ptr<MediaRatchet> video_recv;
    MediaJitterBuffer audio_jitter;
    MediaJitterBuffer video_jitter;
  };

  bool SendFrame(mi::media::StreamKind kind,
                 const std::vector<std::uint8_t>& payload,
                 std::uint64_t timestamp_ms,
                 std::uint8_t flags);
  SenderState* EnsureSenderState(const std::string& sender,
                                 std::uint32_t key_id,
                                 std::string& error);
  bool HandleIncomingPacket(const std::string& sender,
                            const std::vector<std::uint8_t>& packet,
                            std::string& error);

  MediaTransport& transport_;
  GroupCallSessionConfig config_;
  std::uint32_t active_key_id_{0};
  std::unique_ptr<MediaRatchet> audio_send_;
  std::unique_ptr<MediaRatchet> video_send_;
  std::unordered_map<std::string, SenderState> senders_;
  std::vector<std::uint8_t> audio_packet_buf_;
  std::vector<std::uint8_t> video_packet_buf_;
  std::vector<MediaRelayPacket> pull_packets_;
  bool ready_{false};
};

}  // namespace mi::client::media

#endif  // MI_E2EE_CLIENT_GROUP_CALL_SESSION_H
