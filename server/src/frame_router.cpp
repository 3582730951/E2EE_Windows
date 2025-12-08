#include "frame_router.h"

#include <cstdint>
#include <vector>

#include "protocol.h"

namespace mi::server {

namespace {

std::vector<std::uint8_t> EncodeLoginResp(const LoginResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteString(resp.token, out);
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeLogoutResp(const LogoutResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeGroupEventResp(
    const GroupEventResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(resp.version, out);
    out.push_back(static_cast<std::uint8_t>(resp.reason));
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeGroupMessageResp(
    const GroupMessageResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success && resp.rotated.has_value()) {
    out.push_back(1);
    proto::WriteUint32(resp.rotated->version, out);
    out.push_back(static_cast<std::uint8_t>(resp.rotated->reason));
  } else {
    out.push_back(0);
  }
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeOfflinePushResp(
    const OfflinePushResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeOfflinePullResp(
    const OfflinePullResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(static_cast<std::uint32_t>(resp.messages.size()), out);
    for (const auto& m : resp.messages) {
      proto::WriteBytes(m, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

}  // namespace

FrameRouter::FrameRouter(ApiService* api) : api_(api) {}

bool FrameRouter::Handle(const Frame& in, Frame& out, const std::string& token) {
  if (!api_) {
    return false;
  }
  out.type = in.type;
  std::size_t offset = 0;
  std::string s1, s2;
  switch (in.type) {
    case FrameType::kLogin: {
      if (!proto::ReadString(in.payload, offset, s1) ||
          !proto::ReadString(in.payload, offset, s2)) {
        return false;
      }
      auto resp = api_->Login(LoginRequest{s1, s2});
      out.payload = EncodeLoginResp(resp);
      return true;
    }
    case FrameType::kLogout: {
      if (token.empty()) {
        return false;
      }
      auto resp = api_->Logout(LogoutRequest{token});
      out.payload = EncodeLogoutResp(resp);
      return true;
    }
    case FrameType::kGroupEvent: {
      if (offset >= in.payload.size()) {
        return false;
      }
      const std::uint8_t action = in.payload[offset++];
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      GroupEventResponse resp;
      if (action == 0) {
        resp = api_->JoinGroup(token, s1);
      } else if (action == 1) {
        resp = api_->LeaveGroup(token, s1);
      } else if (action == 2) {
        resp = api_->KickGroup(token, s1);
      } else {
        resp.success = false;
        resp.error = "invalid group action";
      }
      out.payload = EncodeGroupEventResp(resp);
      return true;
    }
    case FrameType::kMessage: {
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      std::uint32_t threshold = api_->default_group_threshold();
      proto::ReadUint32(in.payload, offset, threshold);
      auto resp = api_->OnGroupMessage(token, s1, threshold);
      out.payload = EncodeGroupMessageResp(resp);
      return true;
    }
    case FrameType::kHeartbeat: {
      out.payload.clear();
      return true;
    }
    case FrameType::kOfflinePush: {
      if (!proto::ReadString(in.payload, offset, s1)) {  // recipient
        return false;
      }
      std::vector<std::uint8_t> msg;
      if (!proto::ReadBytes(in.payload, offset, msg)) {
        return false;
      }
      auto resp = api_->EnqueueOffline(token, s1, std::move(msg));
      out.payload = EncodeOfflinePushResp(resp);
      return true;
    }
    case FrameType::kOfflinePull: {
      auto resp = api_->PullOffline(token);
      out.payload = EncodeOfflinePullResp(resp);
      return true;
    }
    default:
      return false;
  }
}

}  // namespace mi::server
