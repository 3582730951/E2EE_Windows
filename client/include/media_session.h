#ifndef MI_E2EE_CLIENT_MEDIA_SESSION_H
#define MI_E2EE_CLIENT_MEDIA_SESSION_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "client_core.h"
#include "media_crypto.h"
#include "media_jitter_buffer.h"

namespace mi::client::media {

struct MediaStreamStats {
  std::uint64_t frames_sent{0};
  std::uint64_t frames_recv{0};
  std::uint64_t frames_drop{0};
  std::uint64_t decrypt_fail{0};
};

struct MediaSessionStats {
  MediaStreamStats audio;
  MediaStreamStats video;
};

struct MediaSessionConfig {
  std::string peer_username;
  std::array<std::uint8_t, 16> call_id{};
  bool initiator{false};
  bool enable_audio{true};
  bool enable_video{true};
  std::uint64_t audio_delay_ms{60};
  std::uint64_t video_delay_ms{120};
  std::size_t audio_max_frames{256};
  std::size_t video_max_frames{256};
};

class MediaSession {
 public:
  MediaSession(mi::client::ClientCore& core, MediaSessionConfig config);

  bool Init(std::string& error);
  bool SendAudioFrame(const std::vector<std::uint8_t>& payload,
                      std::uint64_t timestamp_ms,
                      std::uint8_t flags = 0);
  bool SendVideoFrame(const std::vector<std::uint8_t>& payload,
                      std::uint64_t timestamp_ms,
                      std::uint8_t flags = 0);

  bool PollIncoming(std::uint32_t max_packets,
                    std::uint32_t wait_ms,
                    std::string& error);

  bool PopAudioFrame(std::uint64_t now_ms, mi::media::MediaFrame& out);
  bool PopVideoFrame(std::uint64_t now_ms, mi::media::MediaFrame& out);

  const MediaSessionStats& stats() const { return stats_; }
  const MediaSessionConfig& config() const { return config_; }
  const MediaJitterStats& audio_jitter_stats() const {
    return audio_jitter_.stats();
  }
  const MediaJitterStats& video_jitter_stats() const {
    return video_jitter_.stats();
  }

 private:
  bool SendFrame(mi::media::StreamKind kind,
                 const std::vector<std::uint8_t>& payload,
                 std::uint64_t timestamp_ms,
                 std::uint8_t flags);

  bool HandleIncomingPacket(const std::string& sender,
                            const std::vector<std::uint8_t>& packet,
                            std::string& error);

  mi::client::ClientCore& core_;
  MediaSessionConfig config_;
  std::array<std::uint8_t, 32> media_root_{};
  std::unique_ptr<MediaRatchet> audio_send_;
  std::unique_ptr<MediaRatchet> audio_recv_;
  std::unique_ptr<MediaRatchet> video_send_;
  std::unique_ptr<MediaRatchet> video_recv_;
  MediaJitterBuffer audio_jitter_;
  MediaJitterBuffer video_jitter_;
  MediaSessionStats stats_{};
  bool ready_{false};
};

}  // namespace mi::client::media

#endif  // MI_E2EE_CLIENT_MEDIA_SESSION_H
