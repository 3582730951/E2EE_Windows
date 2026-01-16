#include "media_pipeline.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

#include "buffer_pool.h"
#include "platform_media.h"
#include "platform_time.h"

namespace mi::client::media {

namespace {
constexpr std::uint8_t kAudioPayloadVersion = 1;
constexpr std::uint8_t kVideoPayloadVersion = 1;
constexpr std::uint8_t kVideoFlagKeyframe = 0x01;
constexpr std::size_t kVideoHeaderSize = 8;
constexpr std::size_t kOpusMaxPacketBytes = 4000;

std::uint64_t NowMs() {
  return mi::platform::NowSteadyMs();
}

void WriteUint16Le(std::uint16_t v, std::uint8_t* out) {
  out[0] = static_cast<std::uint8_t>(v & 0xFFu);
  out[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
}

std::uint16_t ReadUint16Le(const std::uint8_t* in) {
  return static_cast<std::uint16_t>(static_cast<std::uint16_t>(in[0]) |
                                    (static_cast<std::uint16_t>(in[1]) << 8));
}

void WriteAudioPayloadHeader(AudioCodec codec,
                             std::vector<std::uint8_t>& out) {
  if (out.size() < 2) {
    out.resize(2);
  }
  out[0] = kAudioPayloadVersion;
  out[1] = static_cast<std::uint8_t>(codec);
}

bool DecodeAudioPayload(const std::vector<std::uint8_t>& payload,
                        AudioCodec& codec,
                        const std::uint8_t*& data,
                        std::size_t& len) {
  if (payload.size() < 2) {
    return false;
  }
  if (payload[0] != kAudioPayloadVersion) {
    return false;
  }
  codec = static_cast<AudioCodec>(payload[1]);
  data = payload.data() + 2;
  len = payload.size() - 2;
  return true;
}

void WriteVideoPayloadHeader(VideoCodec codec,
                             bool keyframe,
                             std::uint32_t width,
                             std::uint32_t height,
                             std::vector<std::uint8_t>& out) {
  if (out.size() < kVideoHeaderSize) {
    out.resize(kVideoHeaderSize);
  }
  out[0] = kVideoPayloadVersion;
  out[1] = static_cast<std::uint8_t>(codec);
  out[2] = keyframe ? kVideoFlagKeyframe : 0;
  out[3] = 0;
  WriteUint16Le(static_cast<std::uint16_t>(width), out.data() + 4);
  WriteUint16Le(static_cast<std::uint16_t>(height), out.data() + 6);
}

bool DecodeVideoPayload(const std::vector<std::uint8_t>& payload,
                        VideoCodec& codec,
                        bool& keyframe,
                        std::uint32_t& width,
                        std::uint32_t& height,
                        const std::uint8_t*& data,
                        std::size_t& len) {
  if (payload.size() < kVideoHeaderSize) {
    return false;
  }
  if (payload[0] != kVideoPayloadVersion) {
    return false;
  }
  codec = static_cast<VideoCodec>(payload[1]);
  keyframe = (payload[2] & kVideoFlagKeyframe) != 0;
  width = ReadUint16Le(payload.data() + 4);
  height = ReadUint16Le(payload.data() + 6);
  data = payload.data() + kVideoHeaderSize;
  len = payload.size() - kVideoHeaderSize;
  return true;
}

int ClampInt(int v, int lo, int hi) {
  return std::max(lo, std::min(hi, v));
}

std::size_t EstimateH264PayloadCapacity(const VideoPipelineConfig& config) {
  constexpr std::size_t kFallbackPayload = kVideoHeaderSize + 32u * 1024u;
  if (config.fps == 0 || config.max_bitrate_bps == 0) {
    return kFallbackPayload;
  }
  const std::uint64_t denom =
      static_cast<std::uint64_t>(config.fps) * 8u;
  if (denom == 0) {
    return kFallbackPayload;
  }
  const std::uint64_t per_frame =
      (static_cast<std::uint64_t>(config.max_bitrate_bps) + denom - 1) / denom;
  const std::uint64_t with_headroom =
      per_frame + (per_frame / 2) + 1024u;
  const std::uint64_t total =
      static_cast<std::uint64_t>(kVideoHeaderSize) + with_headroom;
  if (total >
      static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
    return kFallbackPayload;
  }
  return static_cast<std::size_t>(total);
}
}  // namespace

AudioPipeline::AudioPipeline(MediaSessionInterface& session,
                             AudioPipelineConfig config)
    : session_(session), config_(std::move(config)) {}

AudioPipeline::~AudioPipeline() = default;

bool AudioPipeline::Init(std::string& error) {
  error.clear();
  ready_ = false;
  if (config_.sample_rate <= 0 || config_.channels <= 0 ||
      config_.frame_ms <= 0) {
    error = "audio config invalid";
    return false;
  }
  frame_samples_ =
      config_.sample_rate * config_.frame_ms / 1000 * config_.channels;
  if (frame_samples_ <= 0) {
    error = "audio frame samples invalid";
    return false;
  }
  current_bitrate_bps_ =
      ClampInt(config_.target_bitrate_bps, config_.min_bitrate_bps,
               config_.max_bitrate_bps);
  opus_ = mi::platform::media::CreateOpusCodec();
  if (opus_ && opus_->Init(config_.sample_rate, config_.channels,
                           current_bitrate_bps_, config_.enable_fec,
                           config_.enable_dtx, config_.max_packet_loss, error)) {
    codec_ = AudioCodec::kOpus;
    ready_ = true;
    return true;
  }
  if (!config_.allow_pcm_fallback) {
    return false;
  }
  opus_.reset();
  codec_ = AudioCodec::kPcm16;
  ready_ = true;
  return true;
}

bool AudioPipeline::SendPcmFrame(const std::int16_t* samples,
                                 std::size_t sample_count) {
  if (!ready_ || !samples) {
    return false;
  }
  if (sample_count != static_cast<std::size_t>(frame_samples_)) {
    return false;
  }
  const std::size_t pcm_bytes = sample_count * sizeof(std::int16_t);
  const std::size_t max_encoded =
      (codec_ == AudioCodec::kOpus) ? kOpusMaxPacketBytes : pcm_bytes;
  mi::common::ScopedBuffer payload_buf(mi::common::GlobalByteBufferPool(),
                                      max_encoded + 2, false);
  auto& payload = payload_buf.get();
  payload.resize(max_encoded + 2);
  WriteAudioPayloadHeader(codec_, payload);
  std::size_t encoded_len = 0;
  if (codec_ == AudioCodec::kOpus) {
    if (!opus_ ||
        !opus_->EncodeInto(samples, frame_samples_, payload.data() + 2,
                           static_cast<int>(max_encoded), encoded_len)) {
      return false;
    }
  } else {
    if (pcm_bytes > 0) {
      std::memcpy(payload.data() + 2, samples, pcm_bytes);
    }
    encoded_len = pcm_bytes;
  }
  payload.resize(encoded_len + 2);
  return session_.SendAudioFrame(payload, NowMs(), 0);
}

void AudioPipeline::PumpIncoming() {
  if (!ready_) {
    return;
  }
  const auto now_ms = NowMs();
  mi::media::MediaFrame frame;
  while (session_.PopAudioFrame(now_ms, frame)) {
    AudioCodec codec;
    const std::uint8_t* data = nullptr;
    std::size_t len = 0;
    if (!DecodeAudioPayload(frame.payload, codec, data, len)) {
      continue;
    }
    PcmFrame decoded;
    decoded.timestamp_ms = frame.timestamp_ms;
    if (codec == AudioCodec::kOpus) {
      if (!opus_ ||
          !opus_->Decode(data, len, frame_samples_, decoded.samples)) {
        continue;
      }
    } else if (codec == AudioCodec::kPcm16) {
      const std::size_t samples = len / sizeof(std::int16_t);
      decoded.samples.resize(samples);
      if (!decoded.samples.empty()) {
        std::memcpy(decoded.samples.data(), data,
                    samples * sizeof(std::int16_t));
      }
    } else {
      continue;
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      decoded_.push_back(std::move(decoded));
      while (decoded_.size() > config_.max_decoded_frames) {
        decoded_.pop_front();
      }
    }
  }
  AdaptBitrate(now_ms);
}

bool AudioPipeline::PopDecodedFrame(PcmFrame& out) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (decoded_.empty()) {
    return false;
  }
  out = std::move(decoded_.front());
  decoded_.pop_front();
  return true;
}

void AudioPipeline::AdaptBitrate(std::uint64_t now_ms) {
  if (codec_ != AudioCodec::kOpus || !opus_) {
    return;
  }
  if (now_ms - last_adapt_ms_ < 1000) {
    return;
  }
  const auto stats = session_.stats();
  const auto jitter = session_.audio_jitter_stats();
  const auto recv_delta =
      stats.audio.frames_recv - last_stats_.audio.frames_recv;
  const auto drop_delta =
      stats.audio.frames_drop - last_stats_.audio.frames_drop +
      jitter.dropped - last_jitter_.dropped +
      jitter.late - last_jitter_.late;
  double drop_ratio = 0.0;
  if (recv_delta > 0) {
    drop_ratio = static_cast<double>(drop_delta) /
                 static_cast<double>(recv_delta);
  }
  int bitrate = current_bitrate_bps_;
  if (drop_ratio > 0.10) {
    bitrate = ClampInt(bitrate * 8 / 10, config_.min_bitrate_bps,
                       config_.max_bitrate_bps);
  } else if (drop_ratio < 0.02 && recv_delta >= 30) {
    bitrate = ClampInt(bitrate * 11 / 10, config_.min_bitrate_bps,
                       config_.max_bitrate_bps);
  }
  if (bitrate != current_bitrate_bps_) {
    if (opus_->SetBitrate(bitrate)) {
      current_bitrate_bps_ = bitrate;
    }
  }
  last_stats_ = stats;
  last_jitter_ = jitter;
  last_adapt_ms_ = now_ms;
}

VideoPipeline::VideoPipeline(MediaSessionInterface& session,
                             VideoPipelineConfig config)
    : session_(session), config_(std::move(config)) {}

VideoPipeline::~VideoPipeline() = default;

bool VideoPipeline::Init(std::string& error) {
  error.clear();
  ready_ = false;
  if (config_.width == 0 || config_.height == 0 || config_.fps == 0) {
    error = "video config invalid";
    return false;
  }
  current_bitrate_bps_ =
      std::max(config_.min_bitrate_bps,
               std::min(config_.target_bitrate_bps,
                        config_.max_bitrate_bps));
  mf_ = mi::platform::media::CreateH264Codec();
  if (mf_ && mf_->Init(config_.width, config_.height, config_.fps,
                       current_bitrate_bps_, error)) {
    codec_ = VideoCodec::kH264;
    h264_payload_hint_ = EstimateH264PayloadCapacity(config_);
    ready_ = true;
    return true;
  }
  if (!config_.allow_raw_fallback) {
    return false;
  }
  mf_.reset();
  codec_ = VideoCodec::kRawNv12;
  ready_ = true;
  return true;
}

bool VideoPipeline::SendNv12Frame(const std::uint8_t* data,
                                  std::size_t stride,
                                  std::uint32_t width,
                                  std::uint32_t height) {
  if (!ready_ || !data || width == 0 || height == 0) {
    return false;
  }
  if (stride == 0) {
    stride = width;
  }
  if (stride < width) {
    return false;
  }
  const auto now_ms = NowMs();
  const std::uint64_t interval_ms =
      config_.fps == 0 ? 0 : (1000ull / config_.fps);
  if (interval_ms > 0 && now_ms - last_send_ms_ < interval_ms) {
    return false;
  }
  last_send_ms_ = now_ms;
  const bool keyframe =
      config_.keyframe_interval_ms > 0 &&
      (now_ms - last_keyframe_ms_ >= config_.keyframe_interval_ms);
  if (keyframe) {
    last_keyframe_ms_ = now_ms;
  }
  const std::uint8_t* src = data;
  std::size_t src_stride = stride;
  if (stride != width) {
    const std::size_t y_bytes = static_cast<std::size_t>(width) * height;
    const std::size_t uv_bytes = y_bytes / 2;
    encode_scratch_.resize(y_bytes + uv_bytes);
    for (std::uint32_t row = 0; row < height; ++row) {
      std::memcpy(encode_scratch_.data() + row * width,
                  data + row * stride, width);
    }
    const std::uint8_t* uv_src = data + stride * height;
    std::uint8_t* uv_dst = encode_scratch_.data() + y_bytes;
    const std::uint32_t uv_height = height / 2;
    for (std::uint32_t row = 0; row < uv_height; ++row) {
      std::memcpy(uv_dst + row * width, uv_src + row * stride, width);
    }
    src = encode_scratch_.data();
    src_stride = width;
  }
  const std::size_t y_bytes = src_stride * height;
  const std::size_t uv_bytes = y_bytes / 2;
  const std::size_t raw_bytes = y_bytes + uv_bytes;
  std::size_t min_payload = kVideoHeaderSize;
  if (codec_ == VideoCodec::kRawNv12) {
    min_payload += raw_bytes;
  } else if (codec_ == VideoCodec::kH264 &&
             h264_payload_hint_ > min_payload) {
    min_payload = h264_payload_hint_;
  }
  mi::common::ScopedBuffer payload_buf(mi::common::GlobalByteBufferPool(),
                                      min_payload, false);
  auto& payload = payload_buf.get();
  payload.clear();
  payload.resize(kVideoHeaderSize);
  if (codec_ == VideoCodec::kH264) {
    if (!EncodeFrame(src, src_stride, width, height, keyframe, payload)) {
      return false;
    }
    if (payload.size() > h264_payload_hint_) {
      h264_payload_hint_ = payload.size();
    }
  } else {
    payload.insert(payload.end(), src, src + raw_bytes);
  }
  WriteVideoPayloadHeader(codec_, keyframe, width, height, payload);
  const std::uint8_t flags = keyframe ? mi::media::kFrameKey : 0;
  return session_.SendVideoFrame(payload, now_ms, flags);
}

void VideoPipeline::PumpIncoming() {
  if (!ready_) {
    return;
  }
  const auto now_ms = NowMs();
  mi::media::MediaFrame frame;
  while (session_.PopVideoFrame(now_ms, frame)) {
    VideoCodec codec;
    bool keyframe = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    const std::uint8_t* data = nullptr;
    std::size_t len = 0;
    if (!DecodeVideoPayload(frame.payload, codec, keyframe, width, height, data,
                            len)) {
      continue;
    }
    VideoFrameData decoded;
    decoded.timestamp_ms = frame.timestamp_ms;
    decoded.keyframe = keyframe;
    decoded.width = width;
    decoded.height = height;
    if (codec == VideoCodec::kH264) {
      if (!DecodeFrame(data, len, width, height, decoded.nv12)) {
        continue;
      }
    } else if (codec == VideoCodec::kRawNv12) {
      decoded.nv12.assign(data, data + len);
    } else {
      continue;
    }
    if (width > 0 && height > 0 && !decoded.nv12.empty()) {
      const std::size_t expected =
          static_cast<std::size_t>(width) * height * 3 / 2;
      if (decoded.nv12.size() == expected) {
        decoded.stride = width;
      } else {
        const std::size_t denom = static_cast<std::size_t>(height) * 3;
        const std::size_t maybe_stride =
            denom == 0 ? 0 : decoded.nv12.size() * 2 / denom;
        decoded.stride = maybe_stride >= width
                             ? static_cast<std::uint32_t>(maybe_stride)
                             : width;
      }
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      decoded_.push_back(std::move(decoded));
      while (decoded_.size() > config_.max_decoded_frames) {
        decoded_.pop_front();
      }
    }
  }
  AdaptBitrate(now_ms);
}

bool VideoPipeline::PopDecodedFrame(VideoFrameData& out) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (decoded_.empty()) {
    return false;
  }
  out = std::move(decoded_.front());
  decoded_.pop_front();
  return true;
}

void VideoPipeline::AdaptBitrate(std::uint64_t now_ms) {
  if (codec_ != VideoCodec::kH264 || !mf_) {
    return;
  }
  if (now_ms - last_adapt_ms_ < 1000) {
    return;
  }
  const auto stats = session_.stats();
  const auto jitter = session_.video_jitter_stats();
  const auto recv_delta =
      stats.video.frames_recv - last_stats_.video.frames_recv;
  const auto drop_delta =
      stats.video.frames_drop - last_stats_.video.frames_drop +
      jitter.dropped - last_jitter_.dropped +
      jitter.late - last_jitter_.late;
  double drop_ratio = 0.0;
  if (recv_delta > 0) {
    drop_ratio = static_cast<double>(drop_delta) /
                 static_cast<double>(recv_delta);
  }
  std::uint32_t bitrate = current_bitrate_bps_;
  if (drop_ratio > 0.10) {
    bitrate = std::max(config_.min_bitrate_bps, bitrate * 8 / 10);
  } else if (drop_ratio < 0.02 && recv_delta >= 10) {
    bitrate = std::min(config_.max_bitrate_bps, bitrate * 11 / 10);
  }
  if (bitrate != current_bitrate_bps_) {
    if (mf_->SetBitrate(bitrate)) {
      current_bitrate_bps_ = bitrate;
    }
  }
  last_stats_ = stats;
  last_jitter_ = jitter;
  last_adapt_ms_ = now_ms;
}

bool VideoPipeline::EncodeFrame(const std::uint8_t* data,
                                std::size_t stride,
                                std::uint32_t width,
                                std::uint32_t height,
                                bool keyframe,
                                std::vector<std::uint8_t>& out) {
  if (!mf_) {
    return false;
  }
  if (width != config_.width || height != config_.height) {
    std::string err;
    mf_->Init(width, height, config_.fps, current_bitrate_bps_, err);
    config_.width = width;
    config_.height = height;
  }
  return mf_->Encode(data, stride, keyframe, out, NowMs());
}

bool VideoPipeline::DecodeFrame(const std::uint8_t* data,
                                std::size_t len,
                                std::uint32_t width,
                                std::uint32_t height,
                                std::vector<std::uint8_t>& out) {
  if (!mf_) {
    return false;
  }
  if (width != config_.width || height != config_.height) {
    std::string err;
    mf_->Init(width, height, config_.fps, current_bitrate_bps_, err);
    config_.width = width;
    config_.height = height;
  }
  return mf_->Decode(data, len, out, NowMs());
}

}  // namespace mi::client::media
