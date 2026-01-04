#include "frame_router.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "protocol.h"

namespace mi::server {

namespace {

struct PayloadView {
  proto::ByteView view;
  const std::uint8_t* data() const { return view.data; }
  std::size_t size() const { return view.size; }
  const std::uint8_t& operator[](std::size_t idx) const {
    return view.data[idx];
  }
};

PayloadView MakePayloadView(const FrameView& frame) {
  return PayloadView{proto::ByteView{frame.payload, frame.payload_len}};
}

bool ReadFixed16(proto::ByteView data, std::size_t& offset,
                 std::array<std::uint8_t, 16>& out) {
  if (!data.data || offset + out.size() > data.size) {
    return false;
  }
  std::memcpy(out.data(), data.data + offset, out.size());
  offset += out.size();
  return true;
}

void WriteFixed16(const std::array<std::uint8_t, 16>& data,
                  std::vector<std::uint8_t>& out) {
  out.insert(out.end(), data.begin(), data.end());
}

void AssignString(std::string& out, std::string_view in) {
  out.assign(in.data(), in.size());
}

constexpr std::size_t kStringSizeOverhead = 2;
constexpr std::size_t kBytesSizeOverhead = 4;

std::size_t EncodedStringSize(const std::string& s) {
  return kStringSizeOverhead + s.size();
}

std::size_t EncodedBytesSize(std::size_t len) {
  return kBytesSizeOverhead + len;
}

std::size_t EncodedBytesSize(const std::vector<std::uint8_t>& data) {
  return EncodedBytesSize(data.size());
}

template <std::size_t N>
std::size_t EncodedBytesSize(const std::array<std::uint8_t, N>&) {
  return EncodedBytesSize(N);
}

std::vector<std::uint8_t> EncodeLoginResp(const LoginResponse& resp) {
  std::vector<std::uint8_t> out;
  std::size_t reserve = 1;
  if (resp.success) {
    reserve += EncodedStringSize(resp.token);
    if (resp.kex_version == kLoginKeyExchangeV1 && !resp.kem_ct.empty()) {
      reserve += 4;
      reserve += EncodedBytesSize(resp.server_dh_pk);
      reserve += EncodedBytesSize(resp.kem_ct);
    }
  } else {
    reserve += EncodedStringSize(resp.error);
  }
  out.reserve(reserve);
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteString(resp.token, out);
    if (resp.kex_version == kLoginKeyExchangeV1 && !resp.kem_ct.empty()) {
      proto::WriteUint32(resp.kex_version, out);
      proto::WriteBytes(resp.server_dh_pk.data(), resp.server_dh_pk.size(), out);
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
  std::size_t reserve = 1;
  if (resp.success) {
    reserve += EncodedBytesSize(resp.hello.registration_response);
  } else {
    reserve += EncodedStringSize(resp.error);
  }
  out.reserve(reserve);
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
  std::size_t reserve = 1;
  if (!resp.success) {
    reserve += EncodedStringSize(resp.error);
  }
  out.reserve(reserve);
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeOpaqueLoginStartResp(
    const OpaqueLoginStartResponse& resp) {
  std::vector<std::uint8_t> out;
  std::size_t reserve = 1;
  if (resp.success) {
    reserve += EncodedStringSize(resp.hello.login_id);
    reserve += EncodedBytesSize(resp.hello.credential_response);
  } else {
    reserve += EncodedStringSize(resp.error);
  }
  out.reserve(reserve);
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
  std::size_t reserve = 1;
  if (resp.success) {
    reserve += EncodedStringSize(resp.token);
  } else {
    reserve += EncodedStringSize(resp.error);
  }
  out.reserve(reserve);
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
  std::size_t reserve = 1;
  if (!resp.success) {
    reserve += EncodedStringSize(resp.error);
  }
  out.reserve(reserve);
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeGroupEventResp(
    const GroupEventResponse& resp) {
  std::vector<std::uint8_t> out;
  std::size_t reserve = resp.success ? (1 + 4 + 1)
                                     : (1 + EncodedStringSize(resp.error));
  out.reserve(reserve);
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
  std::size_t reserve = 1 + 1;
  if (resp.success && resp.rotated.has_value()) {
    reserve += 4 + 1;
  }
  if (!resp.success) {
    reserve += EncodedStringSize(resp.error);
  }
  out.reserve(reserve);
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
  if (resp.success) {
    std::size_t reserve = 1 + 4;
    for (const auto& m : resp.members) {
      reserve += EncodedStringSize(m);
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (resp.success) {
    std::size_t reserve = 1 + 4;
    for (const auto& m : resp.members) {
      reserve += EncodedStringSize(m.username) + 1;
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeOfflinePushResp(
    const OfflinePushResponse& resp) {
  std::vector<std::uint8_t> out;
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeOfflinePullResp(
    const OfflinePullResponse& resp) {
  std::vector<std::uint8_t> out;
  if (resp.success) {
    std::size_t reserve = 1 + 4;
    for (const auto& msg : resp.messages) {
      reserve += EncodedBytesSize(msg);
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (resp.success) {
    std::size_t reserve = 1 + 4;
    for (const auto& e : resp.friends) {
      reserve += EncodedStringSize(e.username);
      reserve += EncodedStringSize(e.remark);
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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

std::vector<std::uint8_t> EncodeFriendSyncResp(const FriendSyncResponse& resp) {
  std::vector<std::uint8_t> out;
  if (resp.success) {
    std::size_t reserve = 1 + 4 + 1;
    if (resp.changed) {
      reserve += 4;
      for (const auto& e : resp.friends) {
        reserve += EncodedStringSize(e.username);
        reserve += EncodedStringSize(e.remark);
      }
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(resp.version, out);
    out.push_back(resp.changed ? 1 : 0);
    if (resp.changed) {
      proto::WriteUint32(static_cast<std::uint32_t>(resp.friends.size()), out);
      for (const auto& e : resp.friends) {
        proto::WriteString(e.username, out);
        proto::WriteString(e.remark, out);
      }
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeFriendAddResp(const FriendAddResponse& resp) {
  std::vector<std::uint8_t> out;
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeFriendRemarkResp(
    const FriendRemarkResponse& resp) {
  std::vector<std::uint8_t> out;
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeFriendRequestSendResp(
    const FriendRequestSendResponse& resp) {
  std::vector<std::uint8_t> out;
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeFriendRequestListResp(
    const FriendRequestListResponse& resp) {
  std::vector<std::uint8_t> out;
  if (resp.success) {
    std::size_t reserve = 1 + 4;
    for (const auto& e : resp.requests) {
      reserve += EncodedStringSize(e.requester_username);
      reserve += EncodedStringSize(e.requester_remark);
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeFriendDeleteResp(
    const FriendDeleteResponse& resp) {
  std::vector<std::uint8_t> out;
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeUserBlockSetResp(
    const UserBlockSetResponse& resp) {
  std::vector<std::uint8_t> out;
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodePreKeyPublishResp(
    const PreKeyPublishResponse& resp) {
  std::vector<std::uint8_t> out;
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodePreKeyFetchResp(const PreKeyFetchResponse& resp) {
  std::vector<std::uint8_t> out;
  if (resp.success) {
    std::size_t reserve = 1 + EncodedBytesSize(resp.bundle);
    if (resp.kt_version != 0) {
      reserve += 4 + 8;
      reserve += EncodedBytesSize(resp.kt_root);
      reserve += 8 + 4;
      reserve += resp.kt_audit_path.size() * EncodedBytesSize(resp.kt_root);
      reserve += 4;
      reserve += resp.kt_consistency_path.size() * EncodedBytesSize(resp.kt_root);
      reserve += EncodedBytesSize(resp.kt_signature);
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteBytes(resp.bundle, out);
    if (resp.kt_version != 0) {
      proto::WriteUint32(resp.kt_version, out);
      proto::WriteUint64(resp.kt_tree_size, out);
      proto::WriteBytes(resp.kt_root.data(), resp.kt_root.size(), out);
      proto::WriteUint64(resp.kt_leaf_index, out);
      proto::WriteUint32(static_cast<std::uint32_t>(resp.kt_audit_path.size()),
                         out);
      for (const auto& h : resp.kt_audit_path) {
        proto::WriteBytes(h.data(), h.size(), out);
      }
      proto::WriteUint32(
          static_cast<std::uint32_t>(resp.kt_consistency_path.size()), out);
      for (const auto& h : resp.kt_consistency_path) {
        proto::WriteBytes(h.data(), h.size(), out);
      }
      proto::WriteBytes(resp.kt_signature, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeKeyTransparencyHeadResp(
    const KeyTransparencyHeadResponse& resp) {
  std::vector<std::uint8_t> out;
  if (resp.success) {
    std::size_t reserve = 1 + 8;
    reserve += EncodedBytesSize(resp.sth.root);
    reserve += EncodedBytesSize(resp.sth.signature);
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint64(resp.sth.tree_size, out);
    proto::WriteBytes(resp.sth.root.data(), resp.sth.root.size(), out);
    proto::WriteBytes(resp.sth.signature, out);
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeKeyTransparencyConsistencyResp(
    const KeyTransparencyConsistencyResponse& resp) {
  std::vector<std::uint8_t> out;
  if (resp.success) {
    std::size_t reserve = 1 + 8 + 8 + 4;
    if (!resp.proof.empty()) {
      reserve += resp.proof.size() * EncodedBytesSize(resp.proof.front());
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint64(resp.old_size, out);
    proto::WriteUint64(resp.new_size, out);
    proto::WriteUint32(static_cast<std::uint32_t>(resp.proof.size()), out);
    for (const auto& h : resp.proof) {
      proto::WriteBytes(h.data(), h.size(), out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodePrivateSendResp(const PrivateSendResponse& resp) {
  std::vector<std::uint8_t> out;
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeGroupSenderKeySendResp(
    const GroupSenderKeySendResponse& resp) {
  std::vector<std::uint8_t> out;
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodePrivatePullResp(const PrivatePullResponse& resp) {
  std::vector<std::uint8_t> out;
  if (resp.success) {
    std::size_t reserve = 1 + 4;
    for (const auto& e : resp.messages) {
      reserve += EncodedStringSize(e.sender);
      reserve += EncodedBytesSize(e.payload);
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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

std::vector<std::uint8_t> EncodeMediaPushResp(const MediaPushResponse& resp) {
  std::vector<std::uint8_t> out;
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeMediaPullResp(const MediaPullResponse& resp) {
  std::vector<std::uint8_t> out;
  if (resp.success) {
    std::size_t reserve = 1 + 4;
    for (const auto& e : resp.packets) {
      reserve += EncodedStringSize(e.sender);
      reserve += EncodedBytesSize(e.payload);
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(static_cast<std::uint32_t>(resp.packets.size()), out);
    for (const auto& e : resp.packets) {
      proto::WriteString(e.sender, out);
      proto::WriteBytes(e.payload, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeGroupCallSignalResp(
    const GroupCallSignalResponse& resp) {
  std::vector<std::uint8_t> out;
  if (resp.success) {
    std::size_t reserve = 1 + 16 + 4 + 4;
    for (const auto& member : resp.members) {
      reserve += EncodedStringSize(member);
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    WriteFixed16(resp.call_id, out);
    proto::WriteUint32(resp.key_id, out);
    proto::WriteUint32(static_cast<std::uint32_t>(resp.members.size()), out);
    for (const auto& member : resp.members) {
      proto::WriteString(member, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeGroupCallSignalPullResp(
    const GroupCallSignalPullResponse& resp) {
  std::vector<std::uint8_t> out;
  if (resp.success) {
    std::size_t reserve = 1 + 4;
    for (const auto& e : resp.events) {
      reserve += 1;
      reserve += EncodedStringSize(e.group_id);
      reserve += 16;
      reserve += 4;
      reserve += EncodedStringSize(e.sender);
      reserve += 1;
      reserve += 8;
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
  out.push_back(resp.success ? 1 : 0);
  if (resp.success) {
    proto::WriteUint32(static_cast<std::uint32_t>(resp.events.size()), out);
    for (const auto& e : resp.events) {
      out.push_back(e.op);
      proto::WriteString(e.group_id, out);
      WriteFixed16(e.call_id, out);
      proto::WriteUint32(e.key_id, out);
      proto::WriteString(e.sender, out);
      out.push_back(e.media_flags);
      proto::WriteUint64(e.ts_ms, out);
    }
  } else {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeGroupCipherSendResp(
    const GroupCipherSendResponse& resp) {
  std::vector<std::uint8_t> out;
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeGroupCipherPullResp(
    const GroupCipherPullResponse& resp) {
  std::vector<std::uint8_t> out;
  if (resp.success) {
    std::size_t reserve = 1 + 4;
    for (const auto& e : resp.messages) {
      reserve += EncodedStringSize(e.group_id);
      reserve += EncodedStringSize(e.sender);
      reserve += EncodedBytesSize(e.payload);
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (resp.success) {
    std::size_t reserve = 1 + 4;
    for (const auto& e : resp.notices) {
      reserve += EncodedStringSize(e.group_id);
      reserve += EncodedStringSize(e.sender);
      reserve += EncodedBytesSize(e.payload);
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeDeviceSyncPullResp(
    const DeviceSyncPullResponse& resp) {
  std::vector<std::uint8_t> out;
  if (resp.success) {
    std::size_t reserve = 1 + 4;
    for (const auto& msg : resp.messages) {
      reserve += EncodedBytesSize(msg);
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (resp.success) {
    std::size_t reserve = 1 + 4;
    for (const auto& d : resp.devices) {
      reserve += EncodedStringSize(d.device_id);
      reserve += 4;
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeDevicePairingPushResp(
    const DevicePairingPushResponse& resp) {
  std::vector<std::uint8_t> out;
  if (!resp.success) {
    out.reserve(1 + EncodedStringSize(resp.error));
  } else {
    out.reserve(1);
  }
  out.push_back(resp.success ? 1 : 0);
  if (!resp.success) {
    proto::WriteString(resp.error, out);
  }
  return out;
}

std::vector<std::uint8_t> EncodeDevicePairingPullResp(
    const DevicePairingPullResponse& resp) {
  std::vector<std::uint8_t> out;
  if (resp.success) {
    std::size_t reserve = 1 + 4;
    for (const auto& msg : resp.messages) {
      reserve += EncodedBytesSize(msg);
    }
    out.reserve(reserve);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (resp.success) {
    out.reserve(1 + EncodedStringSize(resp.file_id) + 8);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (resp.success) {
    out.reserve(1 + 8 + EncodedBytesSize(resp.blob));
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (resp.success) {
    out.reserve(1 + EncodedStringSize(resp.file_id) +
                EncodedStringSize(resp.upload_id));
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (resp.success) {
    out.reserve(1 + 8);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (resp.success) {
    out.reserve(1 + 8);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (resp.success) {
    out.reserve(1 + EncodedStringSize(resp.download_id) + 8);
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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
  if (resp.success) {
    out.reserve(1 + 8 + 1 + EncodedBytesSize(resp.chunk));
  } else {
    out.reserve(1 + EncodedStringSize(resp.error));
  }
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

bool FrameRouter::Handle(const Frame& in, Frame& out, const std::string& token,
                         TransportKind transport) {
  FrameView view{in.type, in.payload.data(), in.payload.size()};
  return HandleView(view, out, token, transport);
}

bool FrameRouter::HandleView(const FrameView& in, Frame& out,
                             const std::string& token,
                             TransportKind transport) {
  if (!api_) {
    return false;
  }
  out.type = in.type;
  const PayloadView payload_bytes = MakePayloadView(in);
  const proto::ByteView payload_view = payload_bytes.view;
  std::size_t offset = 0;
  std::string s1, s2;
  std::string_view s1_view;
  std::string_view s2_view;
  switch (in.type) {
    case FrameType::kLogin: {
      if (!proto::ReadStringView(payload_view, offset, s1_view) ||
          !proto::ReadStringView(payload_view, offset, s2_view)) {
        return false;
      }
      AssignString(s1, s1_view);
      AssignString(s2, s2_view);
      LoginRequest req;
      req.username = s1;
      req.password = s2;
      if (offset != payload_bytes.size()) {
        std::uint32_t kex = 0;
        if (!proto::ReadUint32(payload_view, offset, kex)) {
          return false;
        }
        req.kex_version = kex;
        if (req.kex_version == kLoginKeyExchangeV1) {
          std::vector<std::uint8_t> dh_pk;
          std::vector<std::uint8_t> kem_pk;
          if (!proto::ReadBytes(payload_view, offset, dh_pk) ||
              !proto::ReadBytes(payload_view, offset, kem_pk)) {
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
        if (offset != payload_bytes.size()) {
          return false;
        }
      }

      auto resp = api_->Login(req, transport);
      out.payload = EncodeLoginResp(resp);
      return true;
    }
    case FrameType::kOpaqueRegisterStart: {
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      std::vector<std::uint8_t> reg_req;
      if (!proto::ReadBytes(payload_view, offset, reg_req) ||
          offset != payload_bytes.size()) {
        return false;
      }
      AssignString(s1, s1_view);
      OpaqueRegisterStartRequest req;
      req.username = s1;
      req.registration_request = std::move(reg_req);
      auto resp = api_->OpaqueRegisterStart(req);
      out.payload = EncodeOpaqueRegisterStartResp(resp);
      return true;
    }
    case FrameType::kOpaqueRegisterFinish: {
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      std::vector<std::uint8_t> upload;
      if (!proto::ReadBytes(payload_view, offset, upload) ||
          offset != payload_bytes.size()) {
        return false;
      }
      AssignString(s1, s1_view);
      OpaqueRegisterFinishRequest req;
      req.username = s1;
      req.registration_upload = std::move(upload);
      auto resp = api_->OpaqueRegisterFinish(req);
      out.payload = EncodeOpaqueRegisterFinishResp(resp);
      return true;
    }
    case FrameType::kOpaqueLoginStart: {
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      std::vector<std::uint8_t> cred_req;
      if (!proto::ReadBytes(payload_view, offset, cred_req) ||
          offset != payload_bytes.size()) {
        return false;
      }
      AssignString(s1, s1_view);
      OpaqueLoginStartRequest req;
      req.username = s1;
      req.credential_request = std::move(cred_req);
      auto resp = api_->OpaqueLoginStart(req);
      out.payload = EncodeOpaqueLoginStartResp(resp);
      return true;
    }
    case FrameType::kOpaqueLoginFinish: {
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      std::vector<std::uint8_t> finalization;
      if (!proto::ReadBytes(payload_view, offset, finalization) ||
          offset != payload_bytes.size()) {
        return false;
      }
      AssignString(s1, s1_view);
      OpaqueLoginFinishRequest req;
      req.login_id = s1;
      req.credential_finalization = std::move(finalization);
      auto resp = api_->OpaqueLoginFinish(req, transport);
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
      if (offset >= payload_bytes.size()) {
        return false;
      }
      const std::uint8_t action = payload_bytes[offset++];
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      AssignString(s1, s1_view);
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
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      AssignString(s1, s1_view);
      auto resp = api_->GroupMembers(token, s1);
      out.payload = EncodeGroupMemberListResp(resp);
      return true;
    }
    case FrameType::kGroupMemberInfoList: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      AssignString(s1, s1_view);
      auto resp = api_->GroupMembersInfo(token, s1);
      out.payload = EncodeGroupMemberInfoListResp(resp);
      return true;
    }
    case FrameType::kGroupRoleSet: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view) ||
          !proto::ReadStringView(payload_view, offset, s2_view) ||
          offset >= payload_bytes.size()) {
        return false;
      }
      const auto role = static_cast<GroupRole>(payload_bytes[offset++]);
      if (offset != payload_bytes.size()) {
        return false;
      }
      AssignString(s1, s1_view);
      AssignString(s2, s2_view);
      auto resp = api_->SetGroupRole(token, s1, s2, role);
      out.payload = EncodeGroupRoleSetResp(resp);
      return true;
    }
    case FrameType::kGroupKickMember: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view) ||
          !proto::ReadStringView(payload_view, offset, s2_view) ||
          offset != payload_bytes.size()) {
        return false;
      }
      AssignString(s1, s1_view);
      AssignString(s2, s2_view);
      auto resp = api_->KickGroupMember(token, s1, s2);
      out.payload = EncodeGroupEventResp(resp);
      return true;
    }
    case FrameType::kMessage: {
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      AssignString(s1, s1_view);
      std::uint32_t threshold = api_->default_group_threshold();
      proto::ReadUint32(payload_view, offset, threshold);
      auto resp = api_->OnGroupMessage(token, s1, threshold);
      out.payload = EncodeGroupMessageResp(resp);
      return true;
    }
    case FrameType::kHeartbeat: {
      out.payload.clear();
      return true;
    }
    case FrameType::kOfflinePush: {
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {  // recipient
        return false;
      }
      AssignString(s1, s1_view);
      std::vector<std::uint8_t> msg;
      if (!proto::ReadBytes(payload_view, offset, msg)) {
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
    case FrameType::kFriendSync: {
      if (token.empty()) {
        return false;
      }
      std::uint32_t last_version = 0;
      if (!proto::ReadUint32(payload_view, offset, last_version) ||
          offset != payload_bytes.size()) {
        return false;
      }
      auto resp = api_->SyncFriends(token, last_version);
      out.payload = EncodeFriendSyncResp(resp);
      return true;
    }
    case FrameType::kFriendAdd: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      AssignString(s1, s1_view);
      s2.clear();
      if (offset < payload_bytes.size()) {
        if (!proto::ReadStringView(payload_view, offset, s2_view)) {
          return false;
        }
        AssignString(s2, s2_view);
      }
      auto resp = api_->AddFriend(token, s1, s2);
      out.payload = EncodeFriendAddResp(resp);
      return true;
    }
    case FrameType::kFriendRemarkSet: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view) ||
          !proto::ReadStringView(payload_view, offset, s2_view)) {
        return false;
      }
      AssignString(s1, s1_view);
      AssignString(s2, s2_view);
      auto resp = api_->SetFriendRemark(token, s1, s2);
      out.payload = EncodeFriendRemarkResp(resp);
      return true;
    }
    case FrameType::kFriendRequestSend: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      AssignString(s1, s1_view);
      s2.clear();
      if (offset < payload_bytes.size()) {
        if (!proto::ReadStringView(payload_view, offset, s2_view)) {
          return false;
        }
        AssignString(s2, s2_view);
      }
      auto resp = api_->SendFriendRequest(token, s1, s2);
      out.payload = EncodeFriendRequestSendResp(resp);
      return true;
    }
    case FrameType::kFriendRequestList: {
      if (token.empty()) {
        return false;
      }
      if (offset != payload_bytes.size()) {
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
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      AssignString(s1, s1_view);
      std::uint32_t accept_u32 = 0;
      if (!proto::ReadUint32(payload_view, offset, accept_u32) ||
          offset != payload_bytes.size()) {
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
      if (!proto::ReadStringView(payload_view, offset, s1_view) ||
          offset != payload_bytes.size()) {
        return false;
      }
      AssignString(s1, s1_view);
      auto resp = api_->DeleteFriend(token, s1);
      out.payload = EncodeFriendDeleteResp(resp);
      return true;
    }
    case FrameType::kUserBlockSet: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      AssignString(s1, s1_view);
      std::uint32_t blocked_u32 = 0;
      if (!proto::ReadUint32(payload_view, offset, blocked_u32) ||
          offset != payload_bytes.size()) {
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
      if (!proto::ReadBytes(payload_view, offset, bundle)) {
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
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      AssignString(s1, s1_view);
      std::uint64_t kt_size = 0;
      if (offset < payload_bytes.size()) {
        if (!proto::ReadUint64(payload_view, offset, kt_size) ||
            offset != payload_bytes.size()) {
          return false;
        }
      } else if (offset != payload_bytes.size()) {
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
      if (offset != payload_bytes.size()) {
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
      if (!proto::ReadUint64(payload_view, offset, old_size) ||
          !proto::ReadUint64(payload_view, offset, new_size) ||
          offset != payload_bytes.size()) {
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
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      AssignString(s1, s1_view);
      std::vector<std::uint8_t> payload;
      if (!proto::ReadBytes(payload_view, offset, payload)) {
        return false;
      }
      auto resp = api_->SendPrivate(token, s1, std::move(payload));
      out.payload = EncodePrivateSendResp(resp);
      return true;
    }
    case FrameType::kGroupSenderKeySend: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view) ||
          !proto::ReadStringView(payload_view, offset, s2_view)) {
        return false;
      }
      AssignString(s1, s1_view);
      AssignString(s2, s2_view);
      std::vector<std::uint8_t> payload;
      if (!proto::ReadBytes(payload_view, offset, payload) ||
          offset != payload_bytes.size()) {
        return false;
      }
      auto resp = api_->SendGroupSenderKey(token, s1, s2, std::move(payload));
      out.payload = EncodeGroupSenderKeySendResp(resp);
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
    case FrameType::kMediaPush: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      AssignString(s1, s1_view);
      std::array<std::uint8_t, 16> call_id{};
      if (!ReadFixed16(payload_view, offset, call_id)) {
        return false;
      }
      std::vector<std::uint8_t> payload;
      if (!proto::ReadBytes(payload_view, offset, payload) ||
          offset != payload_bytes.size()) {
        return false;
      }
      auto resp = api_->PushMedia(token, s1, call_id, std::move(payload));
      out.payload = EncodeMediaPushResp(resp);
      return true;
    }
    case FrameType::kMediaPull: {
      if (token.empty()) {
        return false;
      }
      std::array<std::uint8_t, 16> call_id{};
      std::uint32_t max_packets = 0;
      std::uint32_t wait_ms = 0;
      if (!ReadFixed16(payload_view, offset, call_id) ||
          !proto::ReadUint32(payload_view, offset, max_packets) ||
          !proto::ReadUint32(payload_view, offset, wait_ms) ||
          offset != payload_bytes.size()) {
        return false;
      }
      auto resp = api_->PullMedia(token, call_id, max_packets, wait_ms);
      out.payload = EncodeMediaPullResp(resp);
      return true;
    }
    case FrameType::kGroupCallSignal: {
      if (token.empty()) {
        return false;
      }
      if (offset >= payload_bytes.size()) {
        return false;
      }
      const std::uint8_t op = payload_view.data[offset++];
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      AssignString(s1, s1_view);  // group_id
      std::array<std::uint8_t, 16> call_id{};
      if (!ReadFixed16(payload_view, offset, call_id)) {
        return false;
      }
      if (offset >= payload_bytes.size()) {
        return false;
      }
      const std::uint8_t media_flags = payload_view.data[offset++];
      std::uint32_t key_id = 0;
      std::uint32_t seq = 0;
      std::uint64_t ts_ms = 0;
      if (!proto::ReadUint32(payload_view, offset, key_id) ||
          !proto::ReadUint32(payload_view, offset, seq) ||
          !proto::ReadUint64(payload_view, offset, ts_ms)) {
        return false;
      }
      std::vector<std::uint8_t> ext;
      if (!proto::ReadBytes(payload_view, offset, ext) ||
          offset != payload_bytes.size()) {
        return false;
      }
      auto resp =
          api_->GroupCallSignal(token, op, s1, call_id, media_flags, key_id,
                                seq, ts_ms, std::move(ext));
      out.payload = EncodeGroupCallSignalResp(resp);
      return true;
    }
    case FrameType::kGroupCallSignalPull: {
      if (token.empty()) {
        return false;
      }
      std::uint32_t max_events = 0;
      std::uint32_t wait_ms = 0;
      if (!proto::ReadUint32(payload_view, offset, max_events) ||
          !proto::ReadUint32(payload_view, offset, wait_ms) ||
          offset != payload_bytes.size()) {
        return false;
      }
      auto resp = api_->PullGroupCallSignals(token, max_events, wait_ms);
      out.payload = EncodeGroupCallSignalPullResp(resp);
      return true;
    }
    case FrameType::kGroupMediaPush: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      AssignString(s1, s1_view);  // group_id
      std::array<std::uint8_t, 16> call_id{};
      if (!ReadFixed16(payload_view, offset, call_id)) {
        return false;
      }
      std::vector<std::uint8_t> payload;
      if (!proto::ReadBytes(payload_view, offset, payload) ||
          offset != payload_bytes.size()) {
        return false;
      }
      auto resp =
          api_->PushGroupMedia(token, s1, call_id, std::move(payload));
      out.payload = EncodeMediaPushResp(resp);
      return true;
    }
    case FrameType::kGroupMediaPull: {
      if (token.empty()) {
        return false;
      }
      std::array<std::uint8_t, 16> call_id{};
      std::uint32_t max_packets = 0;
      std::uint32_t wait_ms = 0;
      if (!ReadFixed16(payload_view, offset, call_id) ||
          !proto::ReadUint32(payload_view, offset, max_packets) ||
          !proto::ReadUint32(payload_view, offset, wait_ms) ||
          offset != payload_bytes.size()) {
        return false;
      }
      auto resp = api_->PullGroupMedia(token, call_id, max_packets, wait_ms);
      out.payload = EncodeMediaPullResp(resp);
      return true;
    }
    case FrameType::kGroupCipherSend: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {  // group_id
        return false;
      }
      AssignString(s1, s1_view);
      std::vector<std::uint8_t> payload;
      if (!proto::ReadBytes(payload_view, offset, payload) ||
          offset != payload_bytes.size()) {
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
      if (offset != payload_bytes.size()) {
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
      if (offset != payload_bytes.size()) {
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
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {  // device_id
        return false;
      }
      AssignString(s1, s1_view);
      std::vector<std::uint8_t> payload;
      if (!proto::ReadBytes(payload_view, offset, payload) ||
          offset != payload_bytes.size()) {
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
      if (!proto::ReadStringView(payload_view, offset, s1_view) ||  // device_id
          offset != payload_bytes.size()) {
        return false;
      }
      AssignString(s1, s1_view);
      auto resp = api_->PullDeviceSync(token, s1);
      out.payload = EncodeDeviceSyncPullResp(resp);
      return true;
    }
    case FrameType::kDeviceList: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view) ||
          offset != payload_bytes.size()) {
        return false;
      }
      AssignString(s1, s1_view);
      auto resp = api_->ListDevices(token, s1);
      out.payload = EncodeDeviceListResp(resp);
      return true;
    }
    case FrameType::kDeviceKick: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view) ||
          !proto::ReadStringView(payload_view, offset, s2_view) ||
          offset != payload_bytes.size()) {
        return false;
      }
      AssignString(s1, s1_view);
      AssignString(s2, s2_view);
      auto resp = api_->KickDevice(token, s1, s2);
      out.payload = EncodeDeviceKickResp(resp);
      return true;
    }
    case FrameType::kDevicePairingRequest: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {  // pairing_id_hex
        return false;
      }
      AssignString(s1, s1_view);
      std::vector<std::uint8_t> payload;
      if (!proto::ReadBytes(payload_view, offset, payload) ||
          offset != payload_bytes.size()) {
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
      if (offset >= payload_bytes.size()) {
        return false;
      }
      const std::uint8_t mode = payload_bytes[offset++];
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {  // pairing_id_hex
        return false;
      }
      AssignString(s1, s1_view);
      s2.clear();
      if (mode == 1) {
        if (!proto::ReadStringView(payload_view, offset, s2_view)) {  // device_id
          return false;
        }
        AssignString(s2, s2_view);
      }
      if (offset != payload_bytes.size()) {
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
      if (!proto::ReadStringView(payload_view, offset, s1_view) ||  // pairing_id_hex
          !proto::ReadStringView(payload_view, offset, s2_view)) {  // target_device_id
        return false;
      }
      AssignString(s1, s1_view);
      AssignString(s2, s2_view);
      std::vector<std::uint8_t> payload;
      if (!proto::ReadBytes(payload_view, offset, payload) ||
          offset != payload_bytes.size()) {
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
      if (!proto::ReadUint64(payload_view, offset, expected_size) ||
          offset != payload_bytes.size()) {
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
      std::uint64_t off = 0;
      std::vector<std::uint8_t> chunk;
      if (!proto::ReadStringView(payload_view, offset, s1_view) ||
          !proto::ReadStringView(payload_view, offset, s2_view) ||
          !proto::ReadUint64(payload_view, offset, off) ||
          !proto::ReadBytes(payload_view, offset, chunk) ||
          offset != payload_bytes.size()) {
        return false;
      }
      AssignString(s1, s1_view);
      AssignString(s2, s2_view);
      auto resp =
          api_->UploadE2eeFileBlobChunk(token, s1, s2, off, chunk);
      out.payload = EncodeE2eeFileUploadChunkResp(resp);
      return true;
    }
    case FrameType::kE2eeFileUploadFinish: {
      if (token.empty()) {
        return false;
      }
      std::uint64_t total = 0;
      if (!proto::ReadStringView(payload_view, offset, s1_view) ||
          !proto::ReadStringView(payload_view, offset, s2_view) ||
          !proto::ReadUint64(payload_view, offset, total) ||
          offset != payload_bytes.size()) {
        return false;
      }
      AssignString(s1, s1_view);
      AssignString(s2, s2_view);
      auto resp = api_->FinishE2eeFileBlobUpload(token, s1, s2, total);
      out.payload = EncodeE2eeFileUploadFinishResp(resp);
      return true;
    }
    case FrameType::kE2eeFileDownloadStart: {
      if (token.empty()) {
        return false;
      }
      if (!proto::ReadStringView(payload_view, offset, s1_view) ||
          offset >= payload_bytes.size()) {
        return false;
      }
      AssignString(s1, s1_view);
      const bool wipe = payload_bytes[offset++] != 0;
      if (offset != payload_bytes.size()) {
        return false;
      }
      auto resp = api_->StartE2eeFileBlobDownload(token, s1, wipe);
      out.payload = EncodeE2eeFileDownloadStartResp(resp);
      return true;
    }
    case FrameType::kE2eeFileDownloadChunk: {
      if (token.empty()) {
        return false;
      }
      std::uint64_t off = 0;
      std::uint32_t max_len = 0;
      if (!proto::ReadStringView(payload_view, offset, s1_view) ||
          !proto::ReadStringView(payload_view, offset, s2_view) ||
          !proto::ReadUint64(payload_view, offset, off) ||
          !proto::ReadUint32(payload_view, offset, max_len) ||
          offset != payload_bytes.size()) {
        return false;
      }
      AssignString(s1, s1_view);
      AssignString(s2, s2_view);
      auto resp =
          api_->DownloadE2eeFileBlobChunk(token, s1, s2, off, max_len);
      out.payload = EncodeE2eeFileDownloadChunkResp(resp);
      return true;
    }
    case FrameType::kE2eeFileUpload: {
      if (token.empty()) {
        return false;
      }
      std::vector<std::uint8_t> blob;
      if (!proto::ReadBytes(payload_view, offset, blob) ||
          offset != payload_bytes.size()) {
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
      if (!proto::ReadStringView(payload_view, offset, s1_view)) {
        return false;
      }
      AssignString(s1, s1_view);
      bool wipe = true;
      if (offset < payload_bytes.size()) {
        wipe = payload_bytes[offset++] != 0;
      }
      if (offset != payload_bytes.size()) {
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


