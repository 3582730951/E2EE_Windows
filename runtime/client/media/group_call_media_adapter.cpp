#include "group_call_media_adapter.h"

namespace mi::client::media {

GroupCallMediaAdapter::GroupCallMediaAdapter(GroupCallSession& session)
    : session_(session) {}

void GroupCallMediaAdapter::PushIncoming(GroupMediaFrame frame) {
  const auto kind = frame.frame.kind;
  std::lock_guard<std::mutex> lock(mutex_);
  auto& queue = (kind == mi::media::StreamKind::kVideo) ? video_queue_
                                                       : audio_queue_;
  queue.push_back(std::move(frame.frame));
  while (queue.size() > max_queue_) {
    queue.pop_front();
  }
  if (kind == mi::media::StreamKind::kVideo) {
    stats_.video.frames_recv++;
  } else {
    stats_.audio.frames_recv++;
  }
}

void GroupCallMediaAdapter::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  audio_queue_.clear();
  video_queue_.clear();
}

bool GroupCallMediaAdapter::SendAudioFrame(
    const std::vector<std::uint8_t>& payload,
    std::uint64_t timestamp_ms,
    std::uint8_t flags) {
  const bool ok = session_.SendAudioFrame(payload, timestamp_ms, flags);
  if (ok) {
    stats_.audio.frames_sent++;
  }
  return ok;
}

bool GroupCallMediaAdapter::SendVideoFrame(
    const std::vector<std::uint8_t>& payload,
    std::uint64_t timestamp_ms,
    std::uint8_t flags) {
  const bool ok = session_.SendVideoFrame(payload, timestamp_ms, flags);
  if (ok) {
    stats_.video.frames_sent++;
  }
  return ok;
}

bool GroupCallMediaAdapter::PopAudioFrame(std::uint64_t /*now_ms*/,
                                          mi::media::MediaFrame& out) {
  return PopFrame(audio_queue_, out);
}

bool GroupCallMediaAdapter::PopVideoFrame(std::uint64_t /*now_ms*/,
                                          mi::media::MediaFrame& out) {
  return PopFrame(video_queue_, out);
}

bool GroupCallMediaAdapter::PopFrame(std::deque<mi::media::MediaFrame>& queue,
                                     mi::media::MediaFrame& out) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (queue.empty()) {
    return false;
  }
  out = std::move(queue.front());
  queue.pop_front();
  return true;
}

}  // namespace mi::client::media
