#ifndef MI_E2EE_CLIENT_MEDIA_JITTER_BUFFER_H
#define MI_E2EE_CLIENT_MEDIA_JITTER_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../../shard/media_frame.h"

namespace mi::client::media {

struct MediaJitterStats {
  std::uint64_t pushed{0};
  std::uint64_t popped{0};
  std::uint64_t dropped{0};
  std::uint64_t late{0};
};

class MediaJitterBuffer {
 public:
  MediaJitterBuffer(std::uint64_t target_delay_ms = 60,
                    std::size_t max_frames = 256);

  void Reset();
  void Push(const mi::media::MediaFrame& frame, std::uint64_t now_ms);
  bool PopReady(std::uint64_t now_ms, mi::media::MediaFrame& out);
  std::size_t size() const { return frames_.size(); }
  const MediaJitterStats& stats() const { return stats_; }

 private:
 void DropOldest();

  struct FrameEntry {
    std::uint64_t ts{0};
    mi::media::MediaFrame frame;
  };

  std::vector<FrameEntry> frames_;
  std::uint64_t target_delay_ms_{0};
  std::size_t max_frames_{0};
  std::uint64_t base_timestamp_ms_{0};
  std::uint64_t base_local_ms_{0};
  std::uint64_t last_pop_ts_{0};
  bool has_base_{false};
  MediaJitterStats stats_{};
};

}  // namespace mi::client::media

#endif  // MI_E2EE_CLIENT_MEDIA_JITTER_BUFFER_H
