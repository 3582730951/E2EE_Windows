#include "platform_media.h"

namespace mi::platform::media {

class OpusCodecStub final : public OpusCodec {
 public:
  bool Init(int, int, int, bool, bool, int, std::string& error) override {
    error = "opus not available";
    return false;
  }
  void Shutdown() override {}
  bool EncodeInto(const std::int16_t*, int, std::uint8_t*, int,
                  std::size_t& out_len) override {
    out_len = 0;
    return false;
  }
  bool Decode(const std::uint8_t*, std::size_t, int,
              std::vector<std::int16_t>&) override {
    return false;
  }
  bool SetBitrate(int) override { return false; }
};

class H264CodecStub final : public H264Codec {
 public:
  bool Init(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
            std::string& error) override {
    error = "media foundation not available";
    return false;
  }
  bool Encode(const std::uint8_t*, std::size_t, bool,
              std::vector<std::uint8_t>&, std::uint64_t) override {
    return false;
  }
  bool Decode(const std::uint8_t*, std::size_t,
              std::vector<std::uint8_t>&, std::uint64_t) override {
    return false;
  }
  bool SetBitrate(std::uint32_t) override { return false; }
};

std::unique_ptr<OpusCodec> CreateOpusCodec() {
  return std::make_unique<OpusCodecStub>();
}

std::unique_ptr<H264Codec> CreateH264Codec() {
  return std::make_unique<H264CodecStub>();
}

}  // namespace mi::platform::media
