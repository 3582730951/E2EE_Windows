#ifndef MI_E2EE_CLIENT_MEDIA_PIPELINE_H
#define MI_E2EE_CLIENT_MEDIA_PIPELINE_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "media_session.h"

namespace mi::platform::media {
class OpusCodec;
class H264Codec;
}  // namespace mi::platform::media

namespace mi::client::media {

enum class AudioCodec : std::uint8_t {
  kOpus = 1,
  kPcm16 = 2,
};

enum class VideoCodec : std::uint8_t {
  kH264 = 1,
  kRawNv12 = 2,
};

struct AudioPipelineConfig {
  int sample_rate{48000};
  int channels{1};
  int frame_ms{20};
  int target_bitrate_bps{24000};
  int min_bitrate_bps{12000};
  int max_bitrate_bps{48000};
  bool enable_fec{true};
  bool enable_dtx{true};
  int max_packet_loss{10};
  bool allow_pcm_fallback{true};
  std::size_t max_decoded_frames{256};
};

struct VideoPipelineConfig {
  std::uint32_t width{640};
  std::uint32_t height{360};
  std::uint32_t fps{24};
  std::uint32_t target_bitrate_bps{600000};
  std::uint32_t min_bitrate_bps{200000};
  std::uint32_t max_bitrate_bps{1500000};
  std::uint32_t keyframe_interval_ms{2000};
  bool allow_raw_fallback{true};
  std::size_t max_decoded_frames{128};
};

struct PcmFrame {
  std::vector<std::int16_t> samples;
  std::uint64_t timestamp_ms{0};
};

struct VideoFrameData {
  std::vector<std::uint8_t> nv12;
  std::uint32_t width{0};
  std::uint32_t height{0};
  std::uint32_t stride{0};
  std::uint64_t timestamp_ms{0};
  bool keyframe{false};
};

class AudioPipeline {
 public:
  AudioPipeline(MediaSessionInterface& session, AudioPipelineConfig config);
  ~AudioPipeline();

  bool Init(std::string& error);
  bool SendPcmFrame(const std::int16_t* samples, std::size_t sample_count);
  void PumpIncoming();
  bool PopDecodedFrame(PcmFrame& out);

  bool using_opus() const { return codec_ == AudioCodec::kOpus; }
  int current_bitrate_bps() const { return current_bitrate_bps_; }
  int frame_samples() const { return frame_samples_; }

 private:
  void AdaptBitrate(std::uint64_t now_ms);

  MediaSessionInterface& session_;
  AudioPipelineConfig config_;
  AudioCodec codec_{AudioCodec::kOpus};
  int frame_samples_{0};
  int current_bitrate_bps_{0};
  std::deque<PcmFrame> decoded_;
  std::mutex mutex_;
  MediaSessionStats last_stats_{};
  MediaJitterStats last_jitter_{};
  std::uint64_t last_adapt_ms_{0};
  bool ready_{false};
  std::unique_ptr<mi::platform::media::OpusCodec> opus_;
};

class VideoPipeline {
 public:
  VideoPipeline(MediaSessionInterface& session, VideoPipelineConfig config);
  ~VideoPipeline();

  bool Init(std::string& error);
  bool SendNv12Frame(const std::uint8_t* data,
                     std::size_t stride,
                     std::uint32_t width,
                     std::uint32_t height);
  void PumpIncoming();
  bool PopDecodedFrame(VideoFrameData& out);

  bool using_h264() const { return codec_ == VideoCodec::kH264; }
  std::uint32_t current_bitrate_bps() const { return current_bitrate_bps_; }

 private:
  void AdaptBitrate(std::uint64_t now_ms);
  bool EncodeFrame(const std::uint8_t* data,
                   std::size_t stride,
                   std::uint32_t width,
                   std::uint32_t height,
                   bool keyframe,
                   std::vector<std::uint8_t>& out);
  bool DecodeFrame(const std::uint8_t* data,
                   std::size_t len,
                   std::uint32_t width,
                   std::uint32_t height,
                   std::vector<std::uint8_t>& out);

  MediaSessionInterface& session_;
  VideoPipelineConfig config_;
  VideoCodec codec_{VideoCodec::kH264};
  std::uint32_t current_bitrate_bps_{0};
  std::uint64_t last_keyframe_ms_{0};
  std::uint64_t last_send_ms_{0};
  std::deque<VideoFrameData> decoded_;
  std::mutex mutex_;
  MediaSessionStats last_stats_{};
  MediaJitterStats last_jitter_{};
  std::uint64_t last_adapt_ms_{0};
  bool ready_{false};
  std::unique_ptr<mi::platform::media::H264Codec> mf_;
  std::size_t h264_payload_hint_{0};
  std::vector<std::uint8_t> encode_scratch_;
};

}  // namespace mi::client::media

#endif  // MI_E2EE_CLIENT_MEDIA_PIPELINE_H
