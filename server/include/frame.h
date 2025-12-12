#ifndef MI_E2EE_SERVER_FRAME_H
#define MI_E2EE_SERVER_FRAME_H

#include <cstdint>
#include <vector>

namespace mi::server {

enum class FrameType : std::uint16_t {
  kLogin = 1,
  kLogout = 2,
  kMessage = 3,
  kGroupEvent = 4,
  kHeartbeat = 5,
  kOfflinePush = 6,
  kOfflinePull = 7,
  kFriendList = 8,
  kFriendAdd = 9
};

struct Frame {
  FrameType type{FrameType::kHeartbeat};
  std::vector<std::uint8_t> payload;  // 
};

std::vector<std::uint8_t> EncodeFrame(const Frame& frame);

bool DecodeFrame(const std::uint8_t* data, std::size_t len, Frame& out);

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_FRAME_H
