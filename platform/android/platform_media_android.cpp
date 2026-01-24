#include "platform_media.h"

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace mi::platform::media {

namespace {

#ifndef AMEDIAFORMAT_KEY_REQUEST_SYNC_FRAME
#define AMEDIAFORMAT_KEY_REQUEST_SYNC_FRAME "request-sync"
#endif

constexpr int kColorFormatYuv420Planar = 19;
constexpr int kColorFormatYuv420SemiPlanar = 21;
constexpr int kColorFormatYuv420Flexible = 0x7F420888;
constexpr std::size_t kOpusHeadBytes = 19;

std::array<std::uint8_t, kOpusHeadBytes> BuildOpusHead(int sample_rate,
                                                       int channels) {
  std::array<std::uint8_t, kOpusHeadBytes> head{};
  const char magic[] = "OpusHead";
  std::memcpy(head.data(), magic, sizeof(magic) - 1);
  head[8] = 1;
  head[9] = static_cast<std::uint8_t>(std::max(1, channels));
  head[10] = 0;
  head[11] = 0;
  const std::uint32_t rate =
      static_cast<std::uint32_t>(std::max(8000, sample_rate));
  head[12] = static_cast<std::uint8_t>(rate & 0xFFu);
  head[13] = static_cast<std::uint8_t>((rate >> 8) & 0xFFu);
  head[14] = static_cast<std::uint8_t>((rate >> 16) & 0xFFu);
  head[15] = static_cast<std::uint8_t>((rate >> 24) & 0xFFu);
  head[16] = 0;
  head[17] = 0;
  head[18] = 0;
  return head;
}

std::int64_t FrameDurationUs(int sample_rate, int frame_samples) {
  if (sample_rate <= 0 || frame_samples <= 0) {
    return 0;
  }
  return static_cast<std::int64_t>(
      (static_cast<std::int64_t>(frame_samples) * 1000000ll) / sample_rate);
}

bool CopyNv12ToContiguous(const std::uint8_t* src,
                          std::size_t stride,
                          std::uint32_t width,
                          std::uint32_t height,
                          std::vector<std::uint8_t>& out) {
  if (!src || width == 0 || height == 0 || stride < width) {
    return false;
  }
  const std::size_t y_bytes = static_cast<std::size_t>(width) * height;
  const std::size_t uv_bytes = y_bytes / 2;
  out.resize(y_bytes + uv_bytes);
  for (std::uint32_t row = 0; row < height; ++row) {
    std::memcpy(out.data() + row * width, src + row * stride, width);
  }
  const std::uint8_t* src_uv = src + stride * height;
  std::uint8_t* dst_uv = out.data() + y_bytes;
  const std::uint32_t uv_rows = height / 2;
  for (std::uint32_t row = 0; row < uv_rows; ++row) {
    std::memcpy(dst_uv + row * width, src_uv + row * stride, width);
  }
  return true;
}

bool ConvertOutputToNv12(const std::uint8_t* src,
                         std::size_t src_size,
                         int color_format,
                         std::uint32_t width,
                         std::uint32_t height,
                         int stride,
                         int slice_height,
                         std::vector<std::uint8_t>& out) {
  if (!src || width == 0 || height == 0 || stride <= 0 || slice_height <= 0) {
    return false;
  }
  const std::size_t y_bytes = static_cast<std::size_t>(width) * height;
  const std::size_t uv_bytes = y_bytes / 2;
  const std::size_t needed = y_bytes + uv_bytes;
  out.resize(needed);
  if (color_format == kColorFormatYuv420SemiPlanar ||
      color_format == kColorFormatYuv420Flexible) {
    const std::size_t src_y_bytes =
        static_cast<std::size_t>(stride) * slice_height;
    if (src_size < src_y_bytes + uv_bytes) {
      return false;
    }
    for (std::uint32_t row = 0; row < height; ++row) {
      std::memcpy(out.data() + row * width, src + row * stride, width);
    }
    const std::uint8_t* src_uv = src + src_y_bytes;
    std::uint8_t* dst_uv = out.data() + y_bytes;
    const std::uint32_t uv_rows = height / 2;
    for (std::uint32_t row = 0; row < uv_rows; ++row) {
      std::memcpy(dst_uv + row * width, src_uv + row * stride, width);
    }
    return true;
  }
  if (color_format == kColorFormatYuv420Planar) {
    const std::size_t src_y_bytes =
        static_cast<std::size_t>(stride) * slice_height;
    const std::size_t uv_stride = static_cast<std::size_t>(stride / 2);
    const std::size_t uv_rows = static_cast<std::size_t>(slice_height / 2);
    const std::size_t src_uv_bytes = uv_stride * uv_rows;
    if (src_size < src_y_bytes + src_uv_bytes * 2) {
      return false;
    }
    for (std::uint32_t row = 0; row < height; ++row) {
      std::memcpy(out.data() + row * width, src + row * stride, width);
    }
    const std::uint8_t* src_u = src + src_y_bytes;
    const std::uint8_t* src_v = src + src_y_bytes + src_uv_bytes;
    std::uint8_t* dst_uv = out.data() + y_bytes;
    const std::uint32_t uv_h = height / 2;
    for (std::uint32_t row = 0; row < uv_h; ++row) {
      for (std::uint32_t col = 0; col < width / 2; ++col) {
        dst_uv[row * width + col * 2] =
            src_u[row * uv_stride + col];
        dst_uv[row * width + col * 2 + 1] =
            src_v[row * uv_stride + col];
      }
    }
    return true;
  }
  return false;
}

bool LooksLikeAnnexB(const std::uint8_t* data, std::size_t len) {
  if (!data || len < 4) {
    return false;
  }
  for (std::size_t i = 0; i + 3 < len && i < 64; ++i) {
    if (data[i] == 0 && data[i + 1] == 0 &&
        ((data[i + 2] == 1) || (data[i + 2] == 0 && data[i + 3] == 1))) {
      return true;
    }
  }
  return false;
}

bool ConvertAvccToAnnexB(const std::uint8_t* data,
                         std::size_t len,
                         std::vector<std::uint8_t>& out) {
  out.clear();
  if (!data || len < 4) {
    return false;
  }
  std::size_t off = 0;
  while (off + 4 <= len) {
    const std::uint32_t n =
        (static_cast<std::uint32_t>(data[off]) << 24) |
        (static_cast<std::uint32_t>(data[off + 1]) << 16) |
        (static_cast<std::uint32_t>(data[off + 2]) << 8) |
        static_cast<std::uint32_t>(data[off + 3]);
    off += 4;
    if (n == 0 || off + n > len) {
      out.clear();
      return false;
    }
    out.push_back(0);
    out.push_back(0);
    out.push_back(0);
    out.push_back(1);
    out.insert(out.end(), data + off, data + off + n);
    off += n;
  }
  return !out.empty();
}

}  // namespace

class OpusCodecAndroid final : public OpusCodec {
 public:
  ~OpusCodecAndroid() override { Shutdown(); }

  bool Init(int sample_rate,
            int channels,
            int bitrate,
            bool /*enable_fec*/,
            bool /*enable_dtx*/,
            int /*loss_pct*/,
            std::string& error) override {
    error.clear();
    Shutdown();
    sample_rate_ = sample_rate;
    channels_ = channels > 0 ? channels : 1;
    bitrate_ = bitrate > 0 ? bitrate : 24000;

    encoder_ = AMediaCodec_createEncoderByType("audio/opus");
    if (!encoder_) {
      error = "opus encoder unavailable";
      Shutdown();
      return false;
    }
    AMediaFormat* enc_fmt = AMediaFormat_new();
    AMediaFormat_setString(enc_fmt, AMEDIAFORMAT_KEY_MIME, "audio/opus");
    AMediaFormat_setInt32(enc_fmt, AMEDIAFORMAT_KEY_SAMPLE_RATE, sample_rate_);
    AMediaFormat_setInt32(enc_fmt, AMEDIAFORMAT_KEY_CHANNEL_COUNT, channels_);
    AMediaFormat_setInt32(enc_fmt, AMEDIAFORMAT_KEY_BIT_RATE, bitrate_);
    if (AMediaCodec_configure(encoder_, enc_fmt, nullptr, nullptr,
                              AMEDIACODEC_CONFIGURE_FLAG_ENCODE) !=
        AMEDIA_OK) {
      AMediaFormat_delete(enc_fmt);
      error = "opus encoder config failed";
      Shutdown();
      return false;
    }
    AMediaFormat_delete(enc_fmt);
    if (AMediaCodec_start(encoder_) != AMEDIA_OK) {
      error = "opus encoder start failed";
      Shutdown();
      return false;
    }

    decoder_ = AMediaCodec_createDecoderByType("audio/opus");
    if (!decoder_) {
      error = "opus decoder unavailable";
      Shutdown();
      return false;
    }
    AMediaFormat* dec_fmt = AMediaFormat_new();
    AMediaFormat_setString(dec_fmt, AMEDIAFORMAT_KEY_MIME, "audio/opus");
    AMediaFormat_setInt32(dec_fmt, AMEDIAFORMAT_KEY_SAMPLE_RATE, sample_rate_);
    AMediaFormat_setInt32(dec_fmt, AMEDIAFORMAT_KEY_CHANNEL_COUNT, channels_);
    const auto head = BuildOpusHead(sample_rate_, channels_);
    AMediaFormat_setBuffer(dec_fmt, AMEDIAFORMAT_KEY_CSD_0,
                           const_cast<std::uint8_t*>(head.data()),
                           head.size());
    if (AMediaCodec_configure(decoder_, dec_fmt, nullptr, nullptr, 0) !=
        AMEDIA_OK) {
      AMediaFormat_delete(dec_fmt);
      error = "opus decoder config failed";
      Shutdown();
      return false;
    }
    AMediaFormat_delete(dec_fmt);
    if (AMediaCodec_start(decoder_) != AMEDIA_OK) {
      error = "opus decoder start failed";
      Shutdown();
      return false;
    }

    frame_pts_us_ = 0;
    return true;
  }

  void Shutdown() override {
    if (encoder_) {
      AMediaCodec_stop(encoder_);
      AMediaCodec_delete(encoder_);
      encoder_ = nullptr;
    }
    if (decoder_) {
      AMediaCodec_stop(decoder_);
      AMediaCodec_delete(decoder_);
      decoder_ = nullptr;
    }
  }

  bool EncodeInto(const std::int16_t* pcm,
                  int frame_samples,
                  std::uint8_t* out,
                  int max_len,
                  std::size_t& out_len) override {
    out_len = 0;
    if (!encoder_ || !pcm || !out || frame_samples <= 0 || max_len <= 0) {
      return false;
    }
    const std::size_t bytes =
        static_cast<std::size_t>(frame_samples) * channels_ * sizeof(std::int16_t);
    const ssize_t in_idx =
        AMediaCodec_dequeueInputBuffer(encoder_, 2000);
    if (in_idx < 0) {
      return false;
    }
    std::size_t in_size = 0;
    std::uint8_t* in_buf =
        AMediaCodec_getInputBuffer(encoder_, in_idx, &in_size);
    if (!in_buf || in_size < bytes) {
      AMediaCodec_queueInputBuffer(encoder_, in_idx, 0, 0, 0, 0);
      return false;
    }
    std::memcpy(in_buf, pcm, bytes);
    const std::int64_t pts = frame_pts_us_;
    frame_pts_us_ += FrameDurationUs(sample_rate_, frame_samples);
    if (AMediaCodec_queueInputBuffer(encoder_, in_idx, 0, bytes, pts, 0) !=
        AMEDIA_OK) {
      return false;
    }

    AMediaCodecBufferInfo info{};
    for (;;) {
      const ssize_t out_idx =
          AMediaCodec_dequeueOutputBuffer(encoder_, &info, 0);
      if (out_idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
        break;
      }
      if (out_idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
        continue;
      }
      if (out_idx < 0) {
        return false;
      }
      if (info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) {
        AMediaCodec_releaseOutputBuffer(encoder_, out_idx, false);
        continue;
      }
      std::size_t buf_size = 0;
      std::uint8_t* buf =
          AMediaCodec_getOutputBuffer(encoder_, out_idx, &buf_size);
      if (!buf || info.offset + info.size > buf_size) {
        AMediaCodec_releaseOutputBuffer(encoder_, out_idx, false);
        return false;
      }
      if (info.size > static_cast<std::size_t>(max_len)) {
        AMediaCodec_releaseOutputBuffer(encoder_, out_idx, false);
        return false;
      }
      std::memcpy(out, buf + info.offset,
                  static_cast<std::size_t>(info.size));
      out_len = static_cast<std::size_t>(info.size);
      AMediaCodec_releaseOutputBuffer(encoder_, out_idx, false);
      return out_len > 0;
    }
    return false;
  }

  bool Decode(const std::uint8_t* data,
              std::size_t len,
              int frame_samples,
              std::vector<std::int16_t>& out) override {
    if (!decoder_ || (!data && len != 0)) {
      return false;
    }
    const ssize_t in_idx =
        AMediaCodec_dequeueInputBuffer(decoder_, 2000);
    if (in_idx < 0) {
      return false;
    }
    std::size_t in_size = 0;
    std::uint8_t* in_buf =
        AMediaCodec_getInputBuffer(decoder_, in_idx, &in_size);
    if (!in_buf || in_size < len) {
      AMediaCodec_queueInputBuffer(decoder_, in_idx, 0, 0, 0, 0);
      return false;
    }
    if (len > 0) {
      std::memcpy(in_buf, data, len);
    }
    if (AMediaCodec_queueInputBuffer(decoder_, in_idx, 0, len, 0, 0) !=
        AMEDIA_OK) {
      return false;
    }

    AMediaCodecBufferInfo info{};
    for (;;) {
      const ssize_t out_idx =
          AMediaCodec_dequeueOutputBuffer(decoder_, &info, 0);
      if (out_idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
        break;
      }
      if (out_idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
        continue;
      }
      if (out_idx < 0) {
        return false;
      }
      std::size_t buf_size = 0;
      std::uint8_t* buf =
          AMediaCodec_getOutputBuffer(decoder_, out_idx, &buf_size);
      if (!buf || info.offset + info.size > buf_size) {
        AMediaCodec_releaseOutputBuffer(decoder_, out_idx, false);
        return false;
      }
      const std::size_t samples =
          static_cast<std::size_t>(info.size) / sizeof(std::int16_t);
      out.resize(samples);
      std::memcpy(out.data(), buf + info.offset, samples * sizeof(std::int16_t));
      AMediaCodec_releaseOutputBuffer(decoder_, out_idx, false);
      return !out.empty();
    }
    if (frame_samples > 0) {
      out.resize(static_cast<std::size_t>(frame_samples * channels_));
    }
    return false;
  }

  bool SetBitrate(int bitrate) override {
    if (!encoder_ || bitrate <= 0) {
      return false;
    }
    AMediaFormat* fmt = AMediaFormat_new();
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
    const media_status_t rc = AMediaCodec_setParameters(encoder_, fmt);
    AMediaFormat_delete(fmt);
    if (rc != AMEDIA_OK) {
      return false;
    }
    bitrate_ = bitrate;
    return true;
  }

 private:
  AMediaCodec* encoder_{nullptr};
  AMediaCodec* decoder_{nullptr};
  int sample_rate_{0};
  int channels_{1};
  int bitrate_{0};
  std::int64_t frame_pts_us_{0};
};

class H264CodecAndroid final : public H264Codec {
 public:
  ~H264CodecAndroid() override { Shutdown(); }

  bool Init(std::uint32_t width,
            std::uint32_t height,
            std::uint32_t fps,
            std::uint32_t bitrate,
            std::string& error) override {
    error.clear();
    Shutdown();
    width_ = width;
    height_ = height;
    fps_ = fps;
    bitrate_ = bitrate;
    if (!CreateEncoder(error)) {
      Shutdown();
      return false;
    }
    if (!CreateDecoder(error)) {
      Shutdown();
      return false;
    }
    return true;
  }

  bool Encode(const std::uint8_t* nv12,
              std::size_t stride,
              bool keyframe,
              std::vector<std::uint8_t>& out,
              std::uint64_t timestamp_ms) override {
    if (!encoder_ || !nv12 || width_ == 0 || height_ == 0) {
      return false;
    }
    if (keyframe) {
      RequestKeyframe();
    }
    const std::size_t frame_bytes =
        static_cast<std::size_t>(width_) * height_ * 3 / 2;
    const std::uint8_t* src = nv12;
    if (stride != width_) {
      if (!CopyNv12ToContiguous(nv12, stride, width_, height_, scratch_)) {
        return false;
      }
      src = scratch_.data();
    }
    const ssize_t in_idx =
        AMediaCodec_dequeueInputBuffer(encoder_, 5000);
    if (in_idx < 0) {
      return false;
    }
    std::size_t in_size = 0;
    std::uint8_t* in_buf =
        AMediaCodec_getInputBuffer(encoder_, in_idx, &in_size);
    if (!in_buf || in_size < frame_bytes) {
      AMediaCodec_queueInputBuffer(encoder_, in_idx, 0, 0, 0, 0);
      return false;
    }
    std::memcpy(in_buf, src, frame_bytes);
    const std::int64_t pts = static_cast<std::int64_t>(timestamp_ms) * 1000;
    if (AMediaCodec_queueInputBuffer(encoder_, in_idx, 0, frame_bytes, pts, 0) !=
        AMEDIA_OK) {
      return false;
    }

    const std::size_t start = out.size();
    AMediaCodecBufferInfo info{};
    for (;;) {
      const ssize_t out_idx =
          AMediaCodec_dequeueOutputBuffer(encoder_, &info, 0);
      if (out_idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
        break;
      }
      if (out_idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
        UpdateCsdFromFormat();
        continue;
      }
      if (out_idx < 0) {
        return false;
      }
      if (info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) {
        AMediaCodec_releaseOutputBuffer(encoder_, out_idx, false);
        continue;
      }
      std::size_t buf_size = 0;
      std::uint8_t* buf =
          AMediaCodec_getOutputBuffer(encoder_, out_idx, &buf_size);
      if (!buf || info.offset + info.size > buf_size) {
        AMediaCodec_releaseOutputBuffer(encoder_, out_idx, false);
        return false;
      }
      if (keyframe && !csd_.empty()) {
        out.insert(out.end(), csd_.begin(), csd_.end());
      }
      out.insert(out.end(), buf + info.offset,
                 buf + info.offset + info.size);
      AMediaCodec_releaseOutputBuffer(encoder_, out_idx, false);
      break;
    }
    return out.size() > start;
  }

  bool Decode(const std::uint8_t* data,
              std::size_t len,
              std::vector<std::uint8_t>& out,
              std::uint64_t timestamp_ms) override {
    if (!decoder_ || (!data && len != 0) || width_ == 0 || height_ == 0) {
      return false;
    }
    const std::uint8_t* src = data;
    std::vector<std::uint8_t> annexb;
    if (!LooksLikeAnnexB(data, len) && ConvertAvccToAnnexB(data, len, annexb)) {
      src = annexb.data();
      len = annexb.size();
    }

    const ssize_t in_idx =
        AMediaCodec_dequeueInputBuffer(decoder_, 5000);
    if (in_idx < 0) {
      return false;
    }
    std::size_t in_size = 0;
    std::uint8_t* in_buf =
        AMediaCodec_getInputBuffer(decoder_, in_idx, &in_size);
    if (!in_buf || in_size < len) {
      AMediaCodec_queueInputBuffer(decoder_, in_idx, 0, 0, 0, 0);
      return false;
    }
    if (len > 0) {
      std::memcpy(in_buf, src, len);
    }
    const std::int64_t pts = static_cast<std::int64_t>(timestamp_ms) * 1000;
    if (AMediaCodec_queueInputBuffer(decoder_, in_idx, 0, len, pts, 0) !=
        AMEDIA_OK) {
      return false;
    }

    AMediaCodecBufferInfo info{};
    for (;;) {
      const ssize_t out_idx =
          AMediaCodec_dequeueOutputBuffer(decoder_, &info, 0);
      if (out_idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
        break;
      }
      if (out_idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
        UpdateOutputFormat();
        continue;
      }
      if (out_idx < 0) {
        return false;
      }
      std::size_t buf_size = 0;
      std::uint8_t* buf =
          AMediaCodec_getOutputBuffer(decoder_, out_idx, &buf_size);
      if (!buf || info.offset + info.size > buf_size) {
        AMediaCodec_releaseOutputBuffer(decoder_, out_idx, false);
        return false;
      }
      const std::uint8_t* frame = buf + info.offset;
      if (!ConvertOutputToNv12(frame, info.size, out_color_format_,
                               out_width_, out_height_,
                               out_stride_, out_slice_height_, out)) {
        AMediaCodec_releaseOutputBuffer(decoder_, out_idx, false);
        return false;
      }
      AMediaCodec_releaseOutputBuffer(decoder_, out_idx, false);
      return !out.empty();
    }
    return false;
  }

  bool SetBitrate(std::uint32_t bitrate) override {
    if (!encoder_ || bitrate == 0) {
      return false;
    }
    AMediaFormat* fmt = AMediaFormat_new();
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_BIT_RATE,
                          static_cast<int32_t>(bitrate));
    const media_status_t rc = AMediaCodec_setParameters(encoder_, fmt);
    AMediaFormat_delete(fmt);
    if (rc != AMEDIA_OK) {
      return false;
    }
    bitrate_ = bitrate;
    return true;
  }

 private:
  void Shutdown() {
    if (encoder_) {
      AMediaCodec_stop(encoder_);
      AMediaCodec_delete(encoder_);
      encoder_ = nullptr;
    }
    if (decoder_) {
      AMediaCodec_stop(decoder_);
      AMediaCodec_delete(decoder_);
      decoder_ = nullptr;
    }
    csd_.clear();
    out_width_ = static_cast<int>(width_);
    out_height_ = static_cast<int>(height_);
    out_stride_ = static_cast<int>(width_);
    out_slice_height_ = static_cast<int>(height_);
    out_color_format_ = kColorFormatYuv420SemiPlanar;
  }

  bool CreateEncoder(std::string& error) {
    encoder_ = AMediaCodec_createEncoderByType("video/avc");
    if (!encoder_) {
      error = "h264 encoder unavailable";
      return false;
    }
    AMediaFormat* fmt = AMediaFormat_new();
    AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, "video/avc");
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_WIDTH,
                          static_cast<int32_t>(width_));
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT,
                          static_cast<int32_t>(height_));
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_FRAME_RATE,
                          static_cast<int32_t>(std::max(1u, fps_)));
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 2);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_BIT_RATE,
                          static_cast<int32_t>(bitrate_));
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_COLOR_FORMAT,
                          kColorFormatYuv420SemiPlanar);
    if (AMediaCodec_configure(encoder_, fmt, nullptr, nullptr,
                              AMEDIACODEC_CONFIGURE_FLAG_ENCODE) !=
        AMEDIA_OK) {
      AMediaFormat_delete(fmt);
      error = "h264 encoder config failed";
      return false;
    }
    AMediaFormat_delete(fmt);
    if (AMediaCodec_start(encoder_) != AMEDIA_OK) {
      error = "h264 encoder start failed";
      return false;
    }
    return true;
  }

  bool CreateDecoder(std::string& error) {
    decoder_ = AMediaCodec_createDecoderByType("video/avc");
    if (!decoder_) {
      error = "h264 decoder unavailable";
      return false;
    }
    AMediaFormat* fmt = AMediaFormat_new();
    AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, "video/avc");
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_WIDTH,
                          static_cast<int32_t>(width_));
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT,
                          static_cast<int32_t>(height_));
    if (AMediaCodec_configure(decoder_, fmt, nullptr, nullptr, 0) !=
        AMEDIA_OK) {
      AMediaFormat_delete(fmt);
      error = "h264 decoder config failed";
      return false;
    }
    AMediaFormat_delete(fmt);
    if (AMediaCodec_start(decoder_) != AMEDIA_OK) {
      error = "h264 decoder start failed";
      return false;
    }
    UpdateOutputFormat();
    return true;
  }

  void UpdateCsdFromFormat() {
    AMediaFormat* fmt = AMediaCodec_getOutputFormat(encoder_);
    if (!fmt) {
      return;
    }
    std::uint8_t* sps = nullptr;
    std::size_t sps_len = 0;
    std::uint8_t* pps = nullptr;
    std::size_t pps_len = 0;
    if (AMediaFormat_getBuffer(fmt, AMEDIAFORMAT_KEY_CSD_0,
                               reinterpret_cast<void**>(&sps), &sps_len) &&
        AMediaFormat_getBuffer(fmt, AMEDIAFORMAT_KEY_CSD_1,
                               reinterpret_cast<void**>(&pps), &pps_len) &&
        sps && pps && sps_len > 0 && pps_len > 0) {
      csd_.clear();
      csd_.reserve(4 + sps_len + 4 + pps_len);
      csd_.push_back(0);
      csd_.push_back(0);
      csd_.push_back(0);
      csd_.push_back(1);
      csd_.insert(csd_.end(), sps, sps + sps_len);
      csd_.push_back(0);
      csd_.push_back(0);
      csd_.push_back(0);
      csd_.push_back(1);
      csd_.insert(csd_.end(), pps, pps + pps_len);
    }
    AMediaFormat_delete(fmt);
  }

  void UpdateOutputFormat() {
    AMediaFormat* fmt = AMediaCodec_getOutputFormat(decoder_);
    if (!fmt) {
      return;
    }
    int32_t width = static_cast<int32_t>(width_);
    int32_t height = static_cast<int32_t>(height_);
    int32_t stride = static_cast<int32_t>(width_);
    int32_t slice = static_cast<int32_t>(height_);
    int32_t color = kColorFormatYuv420SemiPlanar;
    AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_WIDTH, &width);
    AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, &height);
    AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_STRIDE, &stride);
    AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_SLICE_HEIGHT, &slice);
    AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_COLOR_FORMAT, &color);
    out_width_ = width;
    out_height_ = height;
    out_stride_ = stride > 0 ? stride : width;
    out_slice_height_ = slice > 0 ? slice : height;
    out_color_format_ = color;
    AMediaFormat_delete(fmt);
  }

  void RequestKeyframe() {
    if (!encoder_) {
      return;
    }
    AMediaFormat* fmt = AMediaFormat_new();
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_REQUEST_SYNC_FRAME, 0);
    AMediaCodec_setParameters(encoder_, fmt);
    AMediaFormat_delete(fmt);
  }

  AMediaCodec* encoder_{nullptr};
  AMediaCodec* decoder_{nullptr};
  std::uint32_t width_{0};
  std::uint32_t height_{0};
  std::uint32_t fps_{0};
  std::uint32_t bitrate_{0};
  std::vector<std::uint8_t> csd_;
  std::vector<std::uint8_t> scratch_;
  int out_width_{0};
  int out_height_{0};
  int out_stride_{0};
  int out_slice_height_{0};
  int out_color_format_{kColorFormatYuv420SemiPlanar};
};

std::unique_ptr<OpusCodec> CreateOpusCodec() {
  return std::make_unique<OpusCodecAndroid>();
}

std::unique_ptr<H264Codec> CreateH264Codec() {
  return std::make_unique<H264CodecAndroid>();
}

}  // namespace mi::platform::media
