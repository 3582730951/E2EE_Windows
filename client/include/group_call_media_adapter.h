#ifndef MI_E2EE_CLIENT_GROUP_CALL_MEDIA_ADAPTER_H
#define MI_E2EE_CLIENT_GROUP_CALL_MEDIA_ADAPTER_H

#include <deque>
#include <mutex>
#include <vector>

#include "group_call_session.h"
#include "media_session.h"

namespace mi::client::media {

class GroupCallMediaAdapter : public MediaSessionInterface {
 public:
  explicit GroupCallMediaAdapter(GroupCallSession& session);

  void PushIncoming(GroupMediaFrame frame);
  void Clear();

  bool SendAudioFrame(const std::vector<std::uint8_t>& payload,
                      std::uint64_t timestamp_ms,
                      std::uint8_t flags) override;
  bool SendVideoFrame(const std::vector<std::uint8_t>& payload,
                      std::uint64_t timestamp_ms,
                      std::uint8_t flags) override;
  bool PopAudioFrame(std::uint64_t now_ms, mi::media::MediaFrame& out) override;
  bool PopVideoFrame(std::uint64_t now_ms, mi::media::MediaFrame& out) override;

  const MediaSessionStats& stats() const override { return stats_; }
  const MediaJitterStats& audio_jitter_stats() const override {
    return audio_jitter_stats_;
  }
  const MediaJitterStats& video_jitter_stats() const override {
    return video_jitter_stats_;
  }

 private:
  bool PopFrame(std::deque<mi::media::MediaFrame>& queue,
                mi::media::MediaFrame& out);

  GroupCallSession& session_;
  mutable std::mutex mutex_;
  std::deque<mi::media::MediaFrame> audio_queue_;
  std::deque<mi::media::MediaFrame> video_queue_;
  MediaSessionStats stats_{};
  MediaJitterStats audio_jitter_stats_{};
  MediaJitterStats video_jitter_stats_{};
  std::size_t max_queue_{256};
};

}  // namespace mi::client::media

#endif  // MI_E2EE_CLIENT_GROUP_CALL_MEDIA_ADAPTER_H
