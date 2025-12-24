#ifndef MI_E2EE_MEDIA_FRAME_H
#define MI_E2EE_MEDIA_FRAME_H

#include <array>
#include <cstdint>
#include <vector>

namespace mi::media {

constexpr std::uint8_t kMediaFrameVersion = 1;

enum class StreamKind : std::uint8_t {
  kAudio = 1,
  kVideo = 2,
};

enum MediaFrameFlags : std::uint8_t {
  kFrameKey = 0x01,
  kFrameEnd = 0x02,
};

struct MediaFrame {
  std::array<std::uint8_t, 16> call_id{};
  StreamKind kind{StreamKind::kAudio};
  std::uint8_t flags{0};
  std::uint64_t timestamp_ms{0};
  std::vector<std::uint8_t> payload;
};

bool EncodeMediaFrame(const MediaFrame& frame, std::vector<std::uint8_t>& out);
bool DecodeMediaFrame(const std::vector<std::uint8_t>& data, MediaFrame& out);

}  // namespace mi::media

#endif  // MI_E2EE_MEDIA_FRAME_H
