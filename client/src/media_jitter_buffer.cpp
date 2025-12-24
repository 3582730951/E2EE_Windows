#include "media_jitter_buffer.h"

namespace mi::client::media {

MediaJitterBuffer::MediaJitterBuffer(std::uint64_t target_delay_ms,
                                     std::size_t max_frames)
    : target_delay_ms_(target_delay_ms), max_frames_(max_frames) {}

void MediaJitterBuffer::Reset() {
  frames_.clear();
  target_delay_ms_ = target_delay_ms_ == 0 ? 1 : target_delay_ms_;
  max_frames_ = max_frames_ == 0 ? 1 : max_frames_;
  base_timestamp_ms_ = 0;
  base_local_ms_ = 0;
  last_pop_ts_ = 0;
  has_base_ = false;
  stats_ = MediaJitterStats{};
}

void MediaJitterBuffer::DropOldest() {
  if (frames_.empty()) {
    return;
  }
  frames_.erase(frames_.begin());
  stats_.dropped++;
}

void MediaJitterBuffer::Push(const mi::media::MediaFrame& frame,
                             std::uint64_t now_ms) {
  if (!has_base_) {
    has_base_ = true;
    base_timestamp_ms_ = frame.timestamp_ms;
    base_local_ms_ = now_ms;
  }
  if (frame.timestamp_ms <= last_pop_ts_) {
    stats_.late++;
    return;
  }
  frames_.emplace(frame.timestamp_ms, frame);
  stats_.pushed++;
  while (frames_.size() > max_frames_) {
    DropOldest();
  }
}

bool MediaJitterBuffer::PopReady(std::uint64_t now_ms,
                                 mi::media::MediaFrame& out) {
  if (frames_.empty() || !has_base_) {
    return false;
  }
  auto it = frames_.begin();
  const std::uint64_t ts = it->first;
  std::uint64_t expected = base_local_ms_ + target_delay_ms_;
  if (ts >= base_timestamp_ms_) {
    expected += (ts - base_timestamp_ms_);
  }
  if (now_ms < expected) {
    return false;
  }
  out = std::move(it->second);
  frames_.erase(it);
  last_pop_ts_ = ts;
  stats_.popped++;
  return true;
}

}  // namespace mi::client::media
