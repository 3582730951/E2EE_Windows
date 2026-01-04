#ifndef MI_E2EE_SERVER_FRAME_H
#define MI_E2EE_SERVER_FRAME_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mi::server {

inline constexpr std::uint32_t kFrameMagic = 0x4D495746;  // 'MIWF'
inline constexpr std::uint16_t kFrameVersion = 1;
inline constexpr std::size_t kFrameHeaderSize = 12;
inline constexpr std::size_t kMaxFramePayloadBytes = 16u * 1024u * 1024u;

enum class FrameType : std::uint16_t {
  kLogin = 1,
  kLogout = 2,
  kMessage = 3,
  kGroupEvent = 4,
  kHeartbeat = 5,
  kOfflinePush = 6,
  kOfflinePull = 7,
  kFriendList = 8,
  kFriendAdd = 9,
  kFriendRemarkSet = 10,
  kPreKeyPublish = 11,
  kPreKeyFetch = 12,
  kPrivateSend = 13,
  kPrivatePull = 14,
  kOpaqueLoginStart = 15,
  kOpaqueLoginFinish = 16,
  kKeyTransparencyHead = 17,
  kKeyTransparencyConsistency = 18,
  kOpaqueRegisterStart = 19,
  kOpaqueRegisterFinish = 20,
  kE2eeFileUpload = 21,
  kE2eeFileDownload = 22,
  kFriendRequestSend = 23,
  kFriendRequestList = 24,
  kFriendRequestRespond = 25,
  kFriendDelete = 26,
  kUserBlockSet = 27,
  kGroupMemberList = 28,
  kGroupCipherSend = 29,
  kGroupCipherPull = 30,
  kDeviceSyncPush = 31,
  kDeviceSyncPull = 32,
  kGroupMemberInfoList = 33,
  kGroupRoleSet = 34,
  kGroupKickMember = 35,
  kDeviceList = 36,
  kDeviceKick = 37,
  kGroupNoticePull = 38,
  kDevicePairingRequest = 39,
  kDevicePairingPull = 40,
  kDevicePairingRespond = 41,
  kHealthCheck = 42,
  kE2eeFileUploadStart = 43,
  kE2eeFileUploadChunk = 44,
  kE2eeFileUploadFinish = 45,
  kE2eeFileDownloadStart = 46,
  kE2eeFileDownloadChunk = 47,
  kFriendSync = 48,
  kGroupSenderKeySend = 49,
  kMediaPush = 50,
  kMediaPull = 51,
  kGroupCallSignal = 52,
  kGroupCallSignalPull = 53,
  kGroupMediaPush = 54,
  kGroupMediaPull = 55
};

struct Frame {
  FrameType type{FrameType::kHeartbeat};
  std::vector<std::uint8_t> payload;
};

struct FrameView {
  FrameType type{FrameType::kHeartbeat};
  const std::uint8_t* payload{nullptr};
  std::size_t payload_len{0};
};

std::vector<std::uint8_t> EncodeFrame(const Frame& frame);
void EncodeFrame(const Frame& frame, std::vector<std::uint8_t>& out);
void EncodeFrame(const FrameView& frame, std::vector<std::uint8_t>& out);

bool DecodeFrameHeader(const std::uint8_t* data, std::size_t len,
                       FrameType& out_type, std::uint32_t& out_payload_len);

bool DecodeFrame(const std::uint8_t* data, std::size_t len, Frame& out);
bool DecodeFrameView(const std::uint8_t* data, std::size_t len,
                     FrameView& out);

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_FRAME_H
