#include "frame_router.h"

#include <algorithm>
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
    if (resp.kex_version == kLoginKeyExchangeV1 && !resp.kem_ct.empty()) {
      proto::WriteUint32(resp.kex_version, out);
      std::vector<std::uint8_t> pk(resp.server_dh_pk.begin(),
                                   resp.server_dh_pk.end());
      proto::WriteBytes(pk, out);
      proto::WriteBytes(resp.kem_ct, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeOpaqueRegisterStartResp(
    const OpaqueRegisterStartResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteBytes(resp.hello.registration_response, out);
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeOpaqueRegisterFinishResp(
    const OpaqueRegisterFinishResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeOpaqueLoginStartResp(
    const OpaqueLoginStartResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteString(resp.hello.login_id, out);
    proto::WriteBytes(resp.hello.credential_response, out);
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeOpaqueLoginFinishResp(
    const OpaqueLoginFinishResponse& resp) {
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

std::vector<std::uint8_t> EncodeGroupMemberListResp(
    const GroupMembersResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(static_cast<std::uint32_t>(resp.members.size()), out);
    for (const auto& m : resp.members) {
      proto::WriteString(m, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeGroupMemberInfoListResp(
    const GroupMembersInfoResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(static_cast<std::uint32_t>(resp.members.size()), out);
    for (const auto& m : resp.members) {
      proto::WriteString(m.username, out);
      out.push_back(static_cast<std::uint8_t>(m.role));
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeGroupRoleSetResp(
    const GroupRoleSetResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
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

std::vector<std::uint8_t> EncodeFriendListResp(const FriendListResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(static_cast<std::uint32_t>(resp.friends.size()), out);
    for (const auto& e : resp.friends) {
      proto::WriteString(e.username, out);
      proto::WriteString(e.remark, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeFriendAddResp(const FriendAddResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeFriendRemarkResp(
    const FriendRemarkResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeFriendRequestSendResp(
    const FriendRequestSendResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeFriendRequestListResp(
    const FriendRequestListResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(static_cast<std::uint32_t>(resp.requests.size()), out);
    for (const auto& e : resp.requests) {
      proto::WriteString(e.requester_username, out);
      proto::WriteString(e.requester_remark, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeFriendRequestRespondResp(
    const FriendRequestRespondResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeFriendDeleteResp(
    const FriendDeleteResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeUserBlockSetResp(
    const UserBlockSetResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodePreKeyPublishResp(
    const PreKeyPublishResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodePreKeyFetchResp(const PreKeyFetchResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteBytes(resp.bundle, out);
    if (resp.kt_version != 0) {
      proto::WriteUint32(resp.kt_version, out);
      proto::WriteUint64(resp.kt_tree_size, out);
      std::vector<std::uint8_t> root(resp.kt_root.begin(), resp.kt_root.end());
      proto::WriteBytes(root, out);
      proto::WriteUint64(resp.kt_leaf_index, out);
      proto::WriteUint32(static_cast<std::uint32_t>(resp.kt_audit_path.size()),
                         out);
      for (const auto& h : resp.kt_audit_path) {
        std::vector<std::uint8_t> b(h.begin(), h.end());
        proto::WriteBytes(b, out);
      }
      proto::WriteUint32(
          static_cast<std::uint32_t>(resp.kt_consistency_path.size()), out);
      for (const auto& h : resp.kt_consistency_path) {
        std::vector<std::uint8_t> b(h.begin(), h.end());
        proto::WriteBytes(b, out);
      }
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeKeyTransparencyHeadResp(
    const KeyTransparencyHeadResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint64(resp.sth.tree_size, out);
    std::vector<std::uint8_t> root(resp.sth.root.begin(), resp.sth.root.end());
    proto::WriteBytes(root, out);
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeKeyTransparencyConsistencyResp(
    const KeyTransparencyConsistencyResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint64(resp.old_size, out);
    proto::WriteUint64(resp.new_size, out);
    proto::WriteUint32(static_cast<std::uint32_t>(resp.proof.size()), out);
    for (const auto& h : resp.proof) {
      std::vector<std::uint8_t> b(h.begin(), h.end());
      proto::WriteBytes(b, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodePrivateSendResp(const PrivateSendResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodePrivatePullResp(const PrivatePullResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(static_cast<std::uint32_t>(resp.messages.size()), out);
    for (const auto& e : resp.messages) {
      proto::WriteString(e.sender, out);
      proto::WriteBytes(e.payload, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeGroupCipherSendResp(
    const GroupCipherSendResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeGroupCipherPullResp(
    const GroupCipherPullResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(static_cast<std::uint32_t>(resp.messages.size()), out);
    for (const auto& e : resp.messages) {
      proto::WriteString(e.group_id, out);
      proto::WriteString(e.sender, out);
      proto::WriteBytes(e.payload, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeGroupNoticePullResp(
    const GroupNoticePullResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(static_cast<std::uint32_t>(resp.notices.size()), out);
    for (const auto& e : resp.notices) {
      proto::WriteString(e.group_id, out);
      proto::WriteString(e.sender, out);
      proto::WriteBytes(e.payload, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeDeviceSyncPushResp(
    const DeviceSyncPushResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeDeviceSyncPullResp(
    const DeviceSyncPullResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(static_cast<std::uint32_t>(resp.messages.size()), out);
    for (const auto& msg : resp.messages) {
      proto::WriteBytes(msg, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeDeviceListResp(const DeviceListResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(static_cast<std::uint32_t>(resp.devices.size()), out);
    for (const auto& d : resp.devices) {
      proto::WriteString(d.device_id, out);
      proto::WriteUint32(d.last_seen_sec, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeDeviceKickResp(const DeviceKickResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeDevicePairingPushResp(
    const DevicePairingPushResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeDevicePairingPullResp(
    const DevicePairingPullResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(static_cast<std::uint32_t>(resp.messages.size()), out);
    for (const auto& msg : resp.messages) {
      proto::WriteBytes(msg, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeE2eeFileUploadResp(
    const FileBlobUploadResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteString(resp.file_id, out);
    proto::WriteUint64(resp.meta.size, out);
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeE2eeFileDownloadResp(
    const FileBlobDownloadResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint64(resp.meta.size, out);
    proto::WriteBytes(resp.blob, out);
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeE2eeFileUploadStartResp(
    const FileBlobUploadStartResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteString(resp.file_id, out);
    proto::WriteString(resp.upload_id, out);
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeE2eeFileUploadChunkResp(
    const FileBlobUploadChunkResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint64(resp.bytes_received, out);
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeE2eeFileUploadFinishResp(
    const FileBlobUploadFinishResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint64(resp.meta.size, out);
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeE2eeFileDownloadStartResp(
    const FileBlobDownloadStartResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteString(resp.download_id, out);
    proto::WriteUint64(resp.size, out);
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeE2eeFileDownloadChunkResp(
    const FileBlobDownloadChunkResponse& resp) {
  std::vector<std::uint8_t> out;
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint64(resp.offset, out);
    out.push_back(resp.eof ? 1 : 0);
    proto::WriteBytes(resp.chunk, out);
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
      LoginRequest req;
      req.username = s1;
      req.password = s2;
      if (offset != in.payload.size()) {
        std::uint32_t kex = 0;
        if (!proto::ReadUint32(in.payload, offset, kex)) {
          return false;
        }
        req.kex_version = kex;
        if (req.kex_version == kLoginKeyExchangeV1) {
          std::vector<std::uint8_t> dh_pk;
          std::vector<std::uint8_t> kem_pk;
          if (!proto::ReadBytes(in.payload, offset, dh_pk) ||
              !proto::ReadBytes(in.payload, offset, kem_pk)) {
            return false;
          }
          if (dh_pk.size() != kX25519PublicKeyBytes ||
              kem_pk.size() != kMlKem768PublicKeyBytes) {
            return false;
          }
          std::copy_n(dh_pk.begin(), req.client_dh_pk.size(),
                      req.client_dh_pk.begin());
          req.client_kem_pk = std::move(kem_pk);
        }
        if (offset != in.payload.size()) {
          return false;
        }
      }

      auto resp = api_->Login(req);
      out.payload = EncodeLoginResp(resp);
      return true;
    }
    case FrameType::kOpaqueRegisterStart: {
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      std::vector<std::uint8_t> reg_req;
      if (!proto::ReadBytes(in.payload, offset, reg_req) ||
          offset != in.payload.size()) {
        return false;
      }
      OpaqueRegisterStartRequest req;
      req.username = s1;
      req.registration_request = std::move(reg_req);
      auto resp = api_->OpaqueRegisterStart(req);
      out.payload = EncodeOpaqueRegisterStartResp(resp);
      return true;
    }
    case FrameType::kOpaqueRegisterFinish: {
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      std::vector<std::uint8_t> upload;
      if (!proto::ReadBytes(in.payload, offset, upload) ||
          offset != in.payload.size()) {
        return false;
      }
      OpaqueRegisterFinishRequest req;
      req.username = s1;
      req.registration_upload = std::move(upload);
      auto resp = api_->OpaqueRegisterFinish(req);
      out.payload = EncodeOpaqueRegisterFinishResp(resp);
      return true;
    }
    case FrameType::kOpaqueLoginStart: {
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      std::vector<std::uint8_t> cred_req;
      if (!proto::ReadBytes(in.payload, offset, cred_req) ||
          offset != in.payload.size()) {
        return false;
      }
      OpaqueLoginStartRequest req;
      req.username = s1;
      req.credential_request = std::move(cred_req);
      auto resp = api_->OpaqueLoginStart(req);
      out.payload = EncodeOpaqueLoginStartResp(resp);
      return true;
    }
    case FrameType::kOpaqueLoginFinish: {
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      std::vector<std::uint8_t> finalization;
      if (!proto::ReadBytes(in.payload, offset, finalization) ||
          offset != in.payload.size()) {
        return false;
      }
      OpaqueLoginFinishRequest req;
      req.login_id = s1;
      req.credential_finalization = std::move(finalization);
      auto resp = api_->OpaqueLoginFinish(req);
      out.payload = EncodeOpaqueLoginFinishResp(resp);
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
    case FrameType::kGroupMemberList: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      auto resp = api_->GroupMembers(token, s1);
      out.payload = EncodeGroupMemberListResp(resp);
      return true;
    }
    case FrameType::kGroupMemberInfoList: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      auto resp = api_->GroupMembersInfo(token, s1);
      out.payload = EncodeGroupMemberInfoListResp(resp);
      return true;
    }
    case FrameType::kGroupRoleSet: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1) ||
          !proto::ReadString(in.payload, offset, s2) ||
          offset >= in.payload.size()) {
        return false;
      }
      const auto role = static_cast<GroupRole>(in.payload[offset++]);
      if (offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->SetGroupRole(token, s1, s2, role);
      out.payload = EncodeGroupRoleSetResp(resp);
      return true;
    }
    case FrameType::kGroupKickMember: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1) ||
          !proto::ReadString(in.payload, offset, s2) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->KickGroupMember(token, s1, s2);
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
    case FrameType::kFriendList: {
      if (token.empty()) {
        return false;
      }
      auto resp = api_->ListFriends(token);
      out.payload = EncodeFriendListResp(resp);
      return true;
    }
    case FrameType::kFriendAdd: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      s2.clear();
      if (offset < in.payload.size()) {
        if (!proto::ReadString(in.payload, offset, s2)) {
          return false;
        }
      }
      auto resp = api_->AddFriend(token, s1, s2);
      out.payload = EncodeFriendAddResp(resp);
      return true;
    }
    case FrameType::kFriendRemarkSet: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1) ||
          !proto::ReadString(in.payload, offset, s2)) {
        return false;
      }
      auto resp = api_->SetFriendRemark(token, s1, s2);
      out.payload = EncodeFriendRemarkResp(resp);
      return true;
    }
    case FrameType::kFriendRequestSend: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      s2.clear();
      if (offset < in.payload.size()) {
        if (!proto::ReadString(in.payload, offset, s2)) {
          return false;
        }
      }
      auto resp = api_->SendFriendRequest(token, s1, s2);
      out.payload = EncodeFriendRequestSendResp(resp);
      return true;
    }
    case FrameType::kFriendRequestList: {
      if (token.empty()) {
        return false;
      }
      if (offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->ListFriendRequests(token);
      out.payload = EncodeFriendRequestListResp(resp);
      return true;
    }
    case FrameType::kFriendRequestRespond: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      std::uint32_t accept_u32 = 0;
      if (!proto::ReadUint32(in.payload, offset, accept_u32) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->RespondFriendRequest(token, s1, accept_u32 != 0);
      out.payload = EncodeFriendRequestRespondResp(resp);
      return true;
    }
    case FrameType::kFriendDelete: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->DeleteFriend(token, s1);
      out.payload = EncodeFriendDeleteResp(resp);
      return true;
    }
    case FrameType::kUserBlockSet: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      std::uint32_t blocked_u32 = 0;
      if (!proto::ReadUint32(in.payload, offset, blocked_u32) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->SetUserBlocked(token, s1, blocked_u32 != 0);
      out.payload = EncodeUserBlockSetResp(resp);
      return true;
    }
    case FrameType::kPreKeyPublish: {
      if (token.empty()) {
        return false;
      }
      std::vector<std::uint8_t> bundle;
      if (!proto::ReadBytes(in.payload, offset, bundle)) {
        return false;
      }
      auto resp = api_->PublishPreKeyBundle(token, std::move(bundle));
      out.payload = EncodePreKeyPublishResp(resp);
      return true;
    }
    case FrameType::kPreKeyFetch: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      std::uint64_t kt_size = 0;
      if (offset < in.payload.size()) {
        if (!proto::ReadUint64(in.payload, offset, kt_size) ||
            offset != in.payload.size()) {
          return false;
        }
      } else if (offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->FetchPreKeyBundle(token, s1, kt_size);
      out.payload = EncodePreKeyFetchResp(resp);
      return true;
    }
    case FrameType::kKeyTransparencyHead: {
      if (token.empty()) {
        return false;
      }
      if (offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->GetKeyTransparencyHead(token);
      out.payload = EncodeKeyTransparencyHeadResp(resp);
      return true;
    }
    case FrameType::kKeyTransparencyConsistency: {
      if (token.empty()) {
        return false;
      }
      std::uint64_t old_size = 0;
      std::uint64_t new_size = 0;
      if (!proto::ReadUint64(in.payload, offset, old_size) ||
          !proto::ReadUint64(in.payload, offset, new_size) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->GetKeyTransparencyConsistency(token, old_size, new_size);
      out.payload = EncodeKeyTransparencyConsistencyResp(resp);
      return true;
    }
    case FrameType::kPrivateSend: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      std::vector<std::uint8_t> payload;
      if (!proto::ReadBytes(in.payload, offset, payload)) {
        return false;
      }
      auto resp = api_->SendPrivate(token, s1, std::move(payload));
      out.payload = EncodePrivateSendResp(resp);
      return true;
    }
    case FrameType::kPrivatePull: {
      if (token.empty()) {
        return false;
      }
      auto resp = api_->PullPrivate(token);
      out.payload = EncodePrivatePullResp(resp);
      return true;
    }
    case FrameType::kGroupCipherSend: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1)) {  // group_id
        return false;
      }
      std::vector<std::uint8_t> payload;
      if (!proto::ReadBytes(in.payload, offset, payload) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->SendGroupCipher(token, s1, std::move(payload));
      out.payload = EncodeGroupCipherSendResp(resp);
      return true;
    }
    case FrameType::kGroupCipherPull: {
      if (token.empty()) {
        return false;
      }
      if (offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->PullGroupCipher(token);
      out.payload = EncodeGroupCipherPullResp(resp);
      return true;
    }
    case FrameType::kGroupNoticePull: {
      if (token.empty()) {
        return false;
      }
      if (offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->PullGroupNotices(token);
      out.payload = EncodeGroupNoticePullResp(resp);
      return true;
    }
    case FrameType::kDeviceSyncPush: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1)) {  // device_id
        return false;
      }
      std::vector<std::uint8_t> payload;
      if (!proto::ReadBytes(in.payload, offset, payload) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->PushDeviceSync(token, s1, std::move(payload));
      out.payload = EncodeDeviceSyncPushResp(resp);
      return true;
    }
    case FrameType::kDeviceSyncPull: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1) ||  // device_id
          offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->PullDeviceSync(token, s1);
      out.payload = EncodeDeviceSyncPullResp(resp);
      return true;
    }
    case FrameType::kDeviceList: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->ListDevices(token, s1);
      out.payload = EncodeDeviceListResp(resp);
      return true;
    }
    case FrameType::kDeviceKick: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1) ||
          !proto::ReadString(in.payload, offset, s2) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->KickDevice(token, s1, s2);
      out.payload = EncodeDeviceKickResp(resp);
      return true;
    }
    case FrameType::kDevicePairingRequest: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1)) {  // pairing_id_hex
        return false;
      }
      std::vector<std::uint8_t> payload;
      if (!proto::ReadBytes(in.payload, offset, payload) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->PushDevicePairingRequest(token, s1, std::move(payload));
      out.payload = EncodeDevicePairingPushResp(resp);
      return true;
    }
    case FrameType::kDevicePairingPull: {
      if (token.empty()) {
        return false;
      }
      if (offset >= in.payload.size()) {
        return false;
      }
      const std::uint8_t mode = in.payload[offset++];
      if (!proto::ReadString(in.payload, offset, s1)) {  // pairing_id_hex
        return false;
      }
      s2.clear();
      if (mode == 1) {
        if (!proto::ReadString(in.payload, offset, s2)) {  // device_id
          return false;
        }
      }
      if (offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->PullDevicePairing(token, mode, s1, s2);
      out.payload = EncodeDevicePairingPullResp(resp);
      return true;
    }
    case FrameType::kDevicePairingRespond: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1) ||  // pairing_id_hex
          !proto::ReadString(in.payload, offset, s2)) {  // target_device_id
        return false;
      }
      std::vector<std::uint8_t> payload;
      if (!proto::ReadBytes(in.payload, offset, payload) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp =
          api_->PushDevicePairingResponse(token, s1, s2, std::move(payload));
      out.payload = EncodeDevicePairingPushResp(resp);
      return true;
    }
    case FrameType::kE2eeFileUploadStart: {
      if (token.empty()) {
        return false;
      }
      std::uint64_t expected_size = 0;
      if (!proto::ReadUint64(in.payload, offset, expected_size) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->StartE2eeFileBlobUpload(token, expected_size);
      out.payload = EncodeE2eeFileUploadStartResp(resp);
      return true;
    }
    case FrameType::kE2eeFileUploadChunk: {
      if (token.empty()) {
        return false;
      }
      std::string file_id;
      std::string upload_id;
      std::uint64_t off = 0;
      std::vector<std::uint8_t> chunk;
      if (!proto::ReadString(in.payload, offset, file_id) ||
          !proto::ReadString(in.payload, offset, upload_id) ||
          !proto::ReadUint64(in.payload, offset, off) ||
          !proto::ReadBytes(in.payload, offset, chunk) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp =
          api_->UploadE2eeFileBlobChunk(token, file_id, upload_id, off, chunk);
      out.payload = EncodeE2eeFileUploadChunkResp(resp);
      return true;
    }
    case FrameType::kE2eeFileUploadFinish: {
      if (token.empty()) {
        return false;
      }
      std::string file_id;
      std::string upload_id;
      std::uint64_t total = 0;
      if (!proto::ReadString(in.payload, offset, file_id) ||
          !proto::ReadString(in.payload, offset, upload_id) ||
          !proto::ReadUint64(in.payload, offset, total) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->FinishE2eeFileBlobUpload(token, file_id, upload_id,
                                                 total);
      out.payload = EncodeE2eeFileUploadFinishResp(resp);
      return true;
    }
    case FrameType::kE2eeFileDownloadStart: {
      if (token.empty()) {
        return false;
      }
      std::string file_id;
      if (!proto::ReadString(in.payload, offset, file_id) ||
          offset >= in.payload.size()) {
        return false;
      }
      const bool wipe = in.payload[offset++] != 0;
      if (offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->StartE2eeFileBlobDownload(token, file_id, wipe);
      out.payload = EncodeE2eeFileDownloadStartResp(resp);
      return true;
    }
    case FrameType::kE2eeFileDownloadChunk: {
      if (token.empty()) {
        return false;
      }
      std::string file_id;
      std::string download_id;
      std::uint64_t off = 0;
      std::uint32_t max_len = 0;
      if (!proto::ReadString(in.payload, offset, file_id) ||
          !proto::ReadString(in.payload, offset, download_id) ||
          !proto::ReadUint64(in.payload, offset, off) ||
          !proto::ReadUint32(in.payload, offset, max_len) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp =
          api_->DownloadE2eeFileBlobChunk(token, file_id, download_id, off,
                                          max_len);
      out.payload = EncodeE2eeFileDownloadChunkResp(resp);
      return true;
    }
    case FrameType::kE2eeFileUpload: {
      if (token.empty()) {
        return false;
      }
      std::vector<std::uint8_t> blob;
      if (!proto::ReadBytes(in.payload, offset, blob) ||
          offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->StoreE2eeFileBlob(token, blob);
      out.payload = EncodeE2eeFileUploadResp(resp);
      return true;
    }
    case FrameType::kE2eeFileDownload: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadString(in.payload, offset, s1)) {
        return false;
      }
      bool wipe = true;
      if (offset < in.payload.size()) {
        wipe = in.payload[offset++] != 0;
      }
      if (offset != in.payload.size()) {
        return false;
      }
      auto resp = api_->LoadE2eeFileBlob(token, s1, wipe);
      out.payload = EncodeE2eeFileDownloadResp(resp);
      return true;
    }
    default:
      return false;
  }
}

}  // namespace mi::server
