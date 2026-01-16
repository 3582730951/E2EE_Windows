#ifndef MI_E2EE_CLIENT_MESSAGING_SERVICE_H
#define MI_E2EE_CLIENT_MESSAGING_SERVICE_H

#include "client_core.h"

namespace mi::client {

class MessagingService {
 public:
  bool JoinGroup(ClientCore& core, const std::string& group_id) const;
  bool LeaveGroup(ClientCore& core, const std::string& group_id) const;
  std::vector<std::string> ListGroupMembers(ClientCore& core, const std::string& group_id) const;
  std::vector<ClientCore::GroupMemberInfo> ListGroupMembersInfo(ClientCore& core, const std::string& group_id) const;
  bool SetGroupMemberRole(ClientCore& core, const std::string& group_id, const std::string& target_username, ClientCore::GroupMemberRole role) const;
  bool KickGroupMember(ClientCore& core, const std::string& group_id, const std::string& target_username) const;
  bool SendGroupMessage(ClientCore& core, const std::string& group_id, std::uint32_t threshold) const;
  bool CreateGroup(ClientCore& core, std::string& out_group_id) const;
  bool SendGroupInvite(ClientCore& core, const std::string& group_id, const std::string& peer_username, std::string& out_message_id_hex) const;
  bool SendOffline(ClientCore& core, const std::string& recipient, const std::vector<std::uint8_t>& payload) const;
  std::vector<std::vector<std::uint8_t>> PullOffline(ClientCore& core) const;
  std::vector<ClientCore::FriendEntry> ListFriends(ClientCore& core) const;
  bool SyncFriends(ClientCore& core, std::vector<ClientCore::FriendEntry>& out, bool& changed) const;
  bool AddFriend(ClientCore& core, const std::string& friend_username, const std::string& remark) const;
  bool SetFriendRemark(ClientCore& core, const std::string& friend_username, const std::string& remark) const;
  bool SendFriendRequest(ClientCore& core, const std::string& target_username, const std::string& requester_remark) const;
  std::vector<ClientCore::FriendRequestEntry> ListFriendRequests(ClientCore& core) const;
  bool RespondFriendRequest(ClientCore& core, const std::string& requester_username, bool accept) const;
  bool DeleteFriend(ClientCore& core, const std::string& friend_username) const;
  bool SetUserBlocked(ClientCore& core, const std::string& blocked_username, bool blocked) const;
  void BestEffortBroadcastDeviceSyncMessage(ClientCore& core, bool is_group, bool outgoing, const std::string& conv_id, const std::string& sender, const std::vector<std::uint8_t>& envelope) const;
  void BestEffortBroadcastDeviceSyncDelivery(ClientCore& core, bool is_group, const std::string& conv_id, const std::array<std::uint8_t, 16>& msg_id, bool is_read) const;
  void BestEffortBroadcastDeviceSyncHistorySnapshot(ClientCore& core, const std::string& target_device_id) const;
  bool GetPeerIdentityCached(ClientCore& core, const std::string& peer_username, ClientCore::CachedPeerIdentity& out, bool require_trust) const;
  bool EnsureGroupSenderKeyForSend(ClientCore& core, const std::string& group_id, const std::vector<std::string>& members, ClientCore::GroupSenderKeyState*& out_sender_key, std::string& out_warn) const;
  bool StoreGroupCallKey(ClientCore& core, const std::string& group_id, const std::array<std::uint8_t, 16>& call_id, std::uint32_t key_id, const std::array<std::uint8_t, 32>& call_key) const;
  bool LookupGroupCallKey(const ClientCore& core, const std::string& group_id, const std::array<std::uint8_t, 16>& call_id, std::uint32_t key_id, std::array<std::uint8_t, 32>& out_key) const;
  bool SendGroupCallKeyEnvelope(ClientCore& core, const std::string& group_id, const std::string& peer_username, const std::array<std::uint8_t, 16>& call_id, std::uint32_t key_id, const std::array<std::uint8_t, 32>& call_key) const;
  bool SendGroupCallKeyRequest(ClientCore& core, const std::string& group_id, const std::string& peer_username, const std::array<std::uint8_t, 16>& call_id, std::uint32_t key_id) const;
  void ResendPendingSenderKeyDistributions(ClientCore& core) const;
  bool SendGroupChatText(ClientCore& core, const std::string& group_id, const std::string& text_utf8, std::string& out_message_id_hex) const;
  bool ResendGroupChatText(ClientCore& core, const std::string& group_id, const std::string& message_id_hex, const std::string& text_utf8) const;
  bool SendGroupChatFile(ClientCore& core, const std::string& group_id, const std::filesystem::path& file_path, std::string& out_message_id_hex) const;
  bool ResendGroupChatFile(ClientCore& core, const std::string& group_id, const std::string& message_id_hex, const std::filesystem::path& file_path) const;
  bool SendPrivateE2ee(ClientCore& core, const std::string& peer_username, const std::vector<std::uint8_t>& plaintext) const;
  std::vector<mi::client::e2ee::PrivateMessage> PullPrivateE2ee(ClientCore& core) const;
  bool PushMedia(ClientCore& core, const std::string& recipient, const std::array<std::uint8_t, 16>& call_id, const std::vector<std::uint8_t>& packet) const;
  std::vector<ClientCore::MediaRelayPacket> PullMedia(ClientCore& core, const std::array<std::uint8_t, 16>& call_id, std::uint32_t max_packets, std::uint32_t wait_ms) const;
  ClientCore::GroupCallSignalResult SendGroupCallSignal(ClientCore& core, std::uint8_t op, const std::string& group_id, const std::array<std::uint8_t, 16>& call_id, bool video, std::uint32_t key_id, std::uint32_t seq, std::uint64_t ts_ms, const std::vector<std::uint8_t>& ext) const;
  bool StartGroupCall(ClientCore& core, const std::string& group_id, bool video, std::array<std::uint8_t, 16>& out_call_id, std::uint32_t& out_key_id) const;
  bool JoinGroupCall(ClientCore& core, const std::string& group_id, const std::array<std::uint8_t, 16>& call_id, bool video) const;
  bool JoinGroupCall(ClientCore& core, const std::string& group_id, const std::array<std::uint8_t, 16>& call_id, bool video, std::uint32_t& out_key_id) const;
  bool LeaveGroupCall(ClientCore& core, const std::string& group_id, const std::array<std::uint8_t, 16>& call_id) const;
  bool RotateGroupCallKey(ClientCore& core, const std::string& group_id, const std::array<std::uint8_t, 16>& call_id, std::uint32_t key_id, const std::vector<std::string>& members) const;
  bool RequestGroupCallKey(ClientCore& core, const std::string& group_id, const std::array<std::uint8_t, 16>& call_id, std::uint32_t key_id, const std::vector<std::string>& members) const;
  bool GetGroupCallKey(const ClientCore& core, const std::string& group_id, const std::array<std::uint8_t, 16>& call_id, std::uint32_t key_id, std::array<std::uint8_t, 32>& out_key) const;
  std::vector<ClientCore::GroupCallEvent> PullGroupCallEvents(ClientCore& core, std::uint32_t max_events, std::uint32_t wait_ms) const;
  bool PushGroupMedia(ClientCore& core, const std::string& group_id, const std::array<std::uint8_t, 16>& call_id, const std::vector<std::uint8_t>& packet) const;
  std::vector<ClientCore::MediaRelayPacket> PullGroupMedia(ClientCore& core, const std::array<std::uint8_t, 16>& call_id, std::uint32_t max_packets, std::uint32_t wait_ms) const;
  std::vector<mi::client::e2ee::PrivateMessage> DrainReadyPrivateE2ee(ClientCore& core) const;
  bool SendGroupCipherMessage(ClientCore& core, const std::string& group_id, const std::vector<std::uint8_t>& payload) const;
  bool SendGroupSenderKeyEnvelope(ClientCore& core, const std::string& group_id, const std::string& peer_username, const std::vector<std::uint8_t>& plaintext) const;
  std::vector<ClientCore::PendingGroupCipher> PullGroupCipherMessages(ClientCore& core) const;
  std::vector<ClientCore::PendingGroupNotice> PullGroupNoticeMessages(ClientCore& core) const;
  bool SendChatText(ClientCore& core, const std::string& peer_username, const std::string& text_utf8, std::string& out_message_id_hex) const;
  bool ResendChatText(ClientCore& core, const std::string& peer_username, const std::string& message_id_hex, const std::string& text_utf8) const;
  bool SendChatTextWithReply(ClientCore& core, const std::string& peer_username, const std::string& text_utf8, const std::string& reply_to_message_id_hex, const std::string& reply_preview_utf8, std::string& out_message_id_hex) const;
  bool ResendChatTextWithReply(ClientCore& core, const std::string& peer_username, const std::string& message_id_hex, const std::string& text_utf8, const std::string& reply_to_message_id_hex, const std::string& reply_preview_utf8) const;
  bool SendChatLocation(ClientCore& core, const std::string& peer_username, std::int32_t lat_e7, std::int32_t lon_e7, const std::string& label_utf8, std::string& out_message_id_hex) const;
  bool ResendChatLocation(ClientCore& core, const std::string& peer_username, const std::string& message_id_hex, std::int32_t lat_e7, std::int32_t lon_e7, const std::string& label_utf8) const;
  bool SendChatContactCard(ClientCore& core, const std::string& peer_username, const std::string& card_username, const std::string& card_display, std::string& out_message_id_hex) const;
  bool ResendChatContactCard(ClientCore& core, const std::string& peer_username, const std::string& message_id_hex, const std::string& card_username, const std::string& card_display) const;
  bool SendChatSticker(ClientCore& core, const std::string& peer_username, const std::string& sticker_id, std::string& out_message_id_hex) const;
  bool ResendChatSticker(ClientCore& core, const std::string& peer_username, const std::string& message_id_hex, const std::string& sticker_id) const;
  bool SendChatReadReceipt(ClientCore& core, const std::string& peer_username, const std::string& message_id_hex) const;
  bool SendChatTyping(ClientCore& core, const std::string& peer_username, bool typing) const;
  bool SendChatPresence(ClientCore& core, const std::string& peer_username, bool online) const;
  bool SendChatFile(ClientCore& core, const std::string& peer_username, const std::filesystem::path& file_path, std::string& out_message_id_hex) const;
  bool ResendChatFile(ClientCore& core, const std::string& peer_username, const std::string& message_id_hex, const std::filesystem::path& file_path) const;
  ClientCore::ChatPollResult PollChat(ClientCore& core) const;
};

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_MESSAGING_SERVICE_H
