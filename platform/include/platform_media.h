#ifndef MI_E2EE_PLATFORM_MEDIA_H
#define MI_E2EE_PLATFORM_MEDIA_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mi::platform::media {

class OpusCodec {
 public:
  virtual ~OpusCodec() = default;

  virtual bool Init(int sample_rate,
                    int channels,
                    int bitrate,
                    bool enable_fec,
                    bool enable_dtx,
                    int loss_pct,
                    std::string& error) = 0;
  virtual void Shutdown() = 0;
  virtual bool EncodeInto(const std::int16_t* pcm,
                          int frame_samples,
                          std::uint8_t* out,
                          int max_len,
                          std::size_t& out_len) = 0;
  virtual bool Decode(const std::uint8_t* data,
                      std::size_t len,
                      int frame_samples,
                      std::vector<std::int16_t>& out) = 0;
  virtual bool SetBitrate(int bitrate) = 0;
};

class H264Codec {
 public:
  virtual ~H264Codec() = default;

  virtual bool Init(std::uint32_t width,
                    std::uint32_t height,
                    std::uint32_t fps,
                    std::uint32_t bitrate,
                    std::string& error) = 0;
  virtual bool Encode(const std::uint8_t* nv12,
                      std::size_t stride,
                      bool keyframe,
                      std::vector<std::uint8_t>& out,
                      std::uint64_t timestamp_ms) = 0;
  virtual bool Decode(const std::uint8_t* data,
                      std::size_t len,
                      std::vector<std::uint8_t>& out,
                      std::uint64_t timestamp_ms) = 0;
  virtual bool SetBitrate(std::uint32_t bitrate) = 0;
};

std::unique_ptr<OpusCodec> CreateOpusCodec();
std::unique_ptr<H264Codec> CreateH264Codec();

}  // namespace mi::platform::media

#endif  // MI_E2EE_PLATFORM_MEDIA_H
