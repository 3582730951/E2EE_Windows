#include "client_core.h"

#include "messaging_service.h"

namespace mi::client {

bool ClientCore::JoinGroup(const std::string& group_id) {
  return MessagingService().JoinGroup(*this, group_id);
}

bool ClientCore::LeaveGroup(const std::string& group_id) {
  return MessagingService().LeaveGroup(*this, group_id);
}

std::vector<std::string> ClientCore::ListGroupMembers(
    const std::string& group_id) {
  return MessagingService().ListGroupMembers(*this, group_id);
}

std::vector<ClientCore::GroupMemberInfo> ClientCore::ListGroupMembersInfo(
    const std::string& group_id) {
  return MessagingService().ListGroupMembersInfo(*this, group_id);
}

bool ClientCore::SetGroupMemberRole(const std::string& group_id,
                                    const std::string& target_username,
                                    GroupMemberRole role) {
  return MessagingService().SetGroupMemberRole(*this, group_id, target_username, role);
}

bool ClientCore::KickGroupMember(const std::string& group_id,
                                 const std::string& target_username) {
  return MessagingService().KickGroupMember(*this, group_id, target_username);
}

bool ClientCore::SendGroupMessage(const std::string& group_id,
                                  std::uint32_t threshold) {
  return MessagingService().SendGroupMessage(*this, group_id, threshold);
}

bool ClientCore::CreateGroup(std::string& out_group_id) {
  return MessagingService().CreateGroup(*this, out_group_id);
}

bool ClientCore::SendGroupInvite(const std::string& group_id,
                                 const std::string& peer_username,
                                 std::string& out_message_id_hex) {
  return MessagingService().SendGroupInvite(*this, group_id, peer_username, out_message_id_hex);
}

bool ClientCore::SendOffline(const std::string& recipient,
                             const std::vector<std::uint8_t>& payload) {
  return MessagingService().SendOffline(*this, recipient, payload);
}

std::vector<std::vector<std::uint8_t>> ClientCore::PullOffline() {
  return MessagingService().PullOffline(*this);
}

std::vector<ClientCore::FriendEntry> ClientCore::ListFriends() {
  return MessagingService().ListFriends(*this);
}

bool ClientCore::SyncFriends(std::vector<FriendEntry>& out, bool& changed) {
  return MessagingService().SyncFriends(*this, out, changed);
}

bool ClientCore::AddFriend(const std::string& friend_username,
                           const std::string& remark) {
  return MessagingService().AddFriend(*this, friend_username, remark);
}

bool ClientCore::SetFriendRemark(const std::string& friend_username,
                                 const std::string& remark) {
  return MessagingService().SetFriendRemark(*this, friend_username, remark);
}

bool ClientCore::SendFriendRequest(const std::string& target_username,
                                   const std::string& requester_remark) {
  return MessagingService().SendFriendRequest(*this, target_username, requester_remark);
}

std::vector<ClientCore::FriendRequestEntry> ClientCore::ListFriendRequests() {
  return MessagingService().ListFriendRequests(*this);
}

bool ClientCore::RespondFriendRequest(const std::string& requester_username,
                                      bool accept) {
  return MessagingService().RespondFriendRequest(*this, requester_username, accept);
}

bool ClientCore::DeleteFriend(const std::string& friend_username) {
  return MessagingService().DeleteFriend(*this, friend_username);
}

bool ClientCore::SetUserBlocked(const std::string& blocked_username,
                                bool blocked) {
  return MessagingService().SetUserBlocked(*this, blocked_username, blocked);
}

void ClientCore::BestEffortBroadcastDeviceSyncMessage(
    bool is_group, bool outgoing, const std::string& conv_id,
    const std::string& sender, const std::vector<std::uint8_t>& envelope) {
  MessagingService().BestEffortBroadcastDeviceSyncMessage(*this, is_group, outgoing, conv_id, sender, envelope);
}

void ClientCore::BestEffortBroadcastDeviceSyncDelivery(
    bool is_group, const std::string& conv_id,
    const std::array<std::uint8_t, 16>& msg_id, bool is_read) {
  MessagingService().BestEffortBroadcastDeviceSyncDelivery(*this, is_group, conv_id, msg_id, is_read);
}

void ClientCore::BestEffortBroadcastDeviceSyncHistorySnapshot(
    const std::string& target_device_id) {
  MessagingService().BestEffortBroadcastDeviceSyncHistorySnapshot(*this, target_device_id);
}

bool ClientCore::GetPeerIdentityCached(const std::string& peer_username,
                                       CachedPeerIdentity& out,
                                       bool require_trust) {
  return MessagingService().GetPeerIdentityCached(*this, peer_username, out, require_trust);
}

bool ClientCore::EnsureGroupSenderKeyForSend(
    const std::string& group_id, const std::vector<std::string>& members,
    GroupSenderKeyState*& out_sender_key, std::string& out_warn) {
  return MessagingService().EnsureGroupSenderKeyForSend(*this, group_id, members, out_sender_key, out_warn);
}

bool ClientCore::StoreGroupCallKey(
    const std::string& group_id, const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id, const std::array<std::uint8_t, 32>& call_key) {
  return MessagingService().StoreGroupCallKey(*this, group_id, call_id, key_id, call_key);
}

bool ClientCore::LookupGroupCallKey(
    const std::string& group_id, const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id, std::array<std::uint8_t, 32>& out_key) const {
  return MessagingService().LookupGroupCallKey(*this, group_id, call_id, key_id, out_key);
}

bool ClientCore::SendGroupCallKeyEnvelope(
    const std::string& group_id, const std::string& peer_username,
    const std::array<std::uint8_t, 16>& call_id, std::uint32_t key_id,
    const std::array<std::uint8_t, 32>& call_key) {
  return MessagingService().SendGroupCallKeyEnvelope(*this, group_id, peer_username, call_id, key_id, call_key);
}

bool ClientCore::SendGroupCallKeyRequest(
    const std::string& group_id, const std::string& peer_username,
    const std::array<std::uint8_t, 16>& call_id, std::uint32_t key_id) {
  return MessagingService().SendGroupCallKeyRequest(*this, group_id, peer_username, call_id, key_id);
}

void ClientCore::ResendPendingSenderKeyDistributions() {
  MessagingService().ResendPendingSenderKeyDistributions(*this);
}

bool ClientCore::SendGroupChatText(const std::string& group_id,
                                   const std::string& text_utf8,
                                   std::string& out_message_id_hex) {
  return MessagingService().SendGroupChatText(*this, group_id, text_utf8, out_message_id_hex);
}

bool ClientCore::ResendGroupChatText(const std::string& group_id,
                                     const std::string& message_id_hex,
                                     const std::string& text_utf8) {
  return MessagingService().ResendGroupChatText(*this, group_id, message_id_hex, text_utf8);
}

bool ClientCore::SendGroupChatFile(const std::string& group_id,
                                   const std::filesystem::path& file_path,
                                   std::string& out_message_id_hex) {
  return MessagingService().SendGroupChatFile(*this, group_id, file_path, out_message_id_hex);
}

bool ClientCore::ResendGroupChatFile(const std::string& group_id,
                                     const std::string& message_id_hex,
                                     const std::filesystem::path& file_path) {
  return MessagingService().ResendGroupChatFile(*this, group_id, message_id_hex, file_path);
}

bool ClientCore::SendPrivateE2ee(const std::string& peer_username,
                                 const std::vector<std::uint8_t>& plaintext) {
  return MessagingService().SendPrivateE2ee(*this, peer_username, plaintext);
}

std::vector<mi::client::e2ee::PrivateMessage> ClientCore::PullPrivateE2ee() {
  return MessagingService().PullPrivateE2ee(*this);
}

bool ClientCore::PushMedia(const std::string& recipient,
                           const std::array<std::uint8_t, 16>& call_id,
                           const std::vector<std::uint8_t>& packet) {
  return MessagingService().PushMedia(*this, recipient, call_id, packet);
}

std::vector<ClientCore::MediaRelayPacket> ClientCore::PullMedia(
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t max_packets,
    std::uint32_t wait_ms) {
  return MessagingService().PullMedia(*this, call_id, max_packets, wait_ms);
}

ClientCore::GroupCallSignalResult ClientCore::SendGroupCallSignal(
    std::uint8_t op, const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id, bool video,
    std::uint32_t key_id, std::uint32_t seq, std::uint64_t ts_ms,
    const std::vector<std::uint8_t>& ext) {
  return MessagingService().SendGroupCallSignal(*this, op, group_id, call_id, video, key_id, seq, ts_ms, ext);
}

bool ClientCore::StartGroupCall(const std::string& group_id,
                                bool video,
                                std::array<std::uint8_t, 16>& out_call_id,
                                std::uint32_t& out_key_id) {
  return MessagingService().StartGroupCall(*this, group_id, video, out_call_id, out_key_id);
}

bool ClientCore::JoinGroupCall(const std::string& group_id,
                               const std::array<std::uint8_t, 16>& call_id,
                               bool video) {
  return MessagingService().JoinGroupCall(*this, group_id, call_id, video);
}

bool ClientCore::JoinGroupCall(const std::string& group_id,
                               const std::array<std::uint8_t, 16>& call_id,
                               bool video,
                               std::uint32_t& out_key_id) {
  return MessagingService().JoinGroupCall(*this, group_id, call_id, video, out_key_id);
}

bool ClientCore::LeaveGroupCall(const std::string& group_id,
                                const std::array<std::uint8_t, 16>& call_id) {
  return MessagingService().LeaveGroupCall(*this, group_id, call_id);
}

bool ClientCore::RotateGroupCallKey(
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id,
    const std::vector<std::string>& members) {
  return MessagingService().RotateGroupCallKey(*this, group_id, call_id, key_id, members);
}

bool ClientCore::RequestGroupCallKey(
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id,
    const std::vector<std::string>& members) {
  return MessagingService().RequestGroupCallKey(*this, group_id, call_id, key_id, members);
}

bool ClientCore::GetGroupCallKey(const std::string& group_id,
                                 const std::array<std::uint8_t, 16>& call_id,
                                 std::uint32_t key_id,
                                 std::array<std::uint8_t, 32>& out_key) const {
  return MessagingService().GetGroupCallKey(*this, group_id, call_id, key_id, out_key);
}

std::vector<ClientCore::GroupCallEvent> ClientCore::PullGroupCallEvents(
    std::uint32_t max_events, std::uint32_t wait_ms) {
  return MessagingService().PullGroupCallEvents(*this, max_events, wait_ms);
}

bool ClientCore::PushGroupMedia(const std::string& group_id,
                                const std::array<std::uint8_t, 16>& call_id,
                                const std::vector<std::uint8_t>& packet) {
  return MessagingService().PushGroupMedia(*this, group_id, call_id, packet);
}

std::vector<ClientCore::MediaRelayPacket> ClientCore::PullGroupMedia(
    const std::array<std::uint8_t, 16>& call_id, std::uint32_t max_packets,
    std::uint32_t wait_ms) {
  return MessagingService().PullGroupMedia(*this, call_id, max_packets, wait_ms);
}

std::vector<mi::client::e2ee::PrivateMessage> ClientCore::DrainReadyPrivateE2ee() {
  return MessagingService().DrainReadyPrivateE2ee(*this);
}

bool ClientCore::SendGroupCipherMessage(const std::string& group_id,
                                        const std::vector<std::uint8_t>& payload) {
  return MessagingService().SendGroupCipherMessage(*this, group_id, payload);
}

bool ClientCore::SendGroupSenderKeyEnvelope(
    const std::string& group_id, const std::string& peer_username,
    const std::vector<std::uint8_t>& plaintext) {
  return MessagingService().SendGroupSenderKeyEnvelope(*this, group_id, peer_username, plaintext);
}

std::vector<ClientCore::PendingGroupCipher> ClientCore::PullGroupCipherMessages() {
  return MessagingService().PullGroupCipherMessages(*this);
}

std::vector<ClientCore::PendingGroupNotice> ClientCore::PullGroupNoticeMessages() {
  return MessagingService().PullGroupNoticeMessages(*this);
}

bool ClientCore::SendChatText(const std::string& peer_username,
                              const std::string& text_utf8,
                              std::string& out_message_id_hex) {
  return MessagingService().SendChatText(*this, peer_username, text_utf8, out_message_id_hex);
}

bool ClientCore::ResendChatText(const std::string& peer_username,
                                const std::string& message_id_hex,
                                const std::string& text_utf8) {
  return MessagingService().ResendChatText(*this, peer_username, message_id_hex, text_utf8);
}

bool ClientCore::SendChatTextWithReply(const std::string& peer_username,
                                      const std::string& text_utf8,
                                      const std::string& reply_to_message_id_hex,
                                      const std::string& reply_preview_utf8,
                                      std::string& out_message_id_hex) {
  return MessagingService().SendChatTextWithReply(*this, peer_username, text_utf8, reply_to_message_id_hex, reply_preview_utf8, out_message_id_hex);
}

bool ClientCore::ResendChatTextWithReply(const std::string& peer_username,
                                        const std::string& message_id_hex,
                                        const std::string& text_utf8,
                                        const std::string& reply_to_message_id_hex,
                                        const std::string& reply_preview_utf8) {
  return MessagingService().ResendChatTextWithReply(*this, peer_username, message_id_hex, text_utf8, reply_to_message_id_hex, reply_preview_utf8);
}

bool ClientCore::SendChatLocation(const std::string& peer_username,
                                  std::int32_t lat_e7, std::int32_t lon_e7,
                                  const std::string& label_utf8,
                                  std::string& out_message_id_hex) {
  return MessagingService().SendChatLocation(*this, peer_username, lat_e7, lon_e7, label_utf8, out_message_id_hex);
}

bool ClientCore::ResendChatLocation(const std::string& peer_username,
                                    const std::string& message_id_hex,
                                    std::int32_t lat_e7, std::int32_t lon_e7,
                                    const std::string& label_utf8) {
  return MessagingService().ResendChatLocation(*this, peer_username, message_id_hex, lat_e7, lon_e7, label_utf8);
}

bool ClientCore::SendChatContactCard(const std::string& peer_username,
                                     const std::string& card_username,
                                     const std::string& card_display,
                                     std::string& out_message_id_hex) {
  return MessagingService().SendChatContactCard(*this, peer_username, card_username, card_display, out_message_id_hex);
}

bool ClientCore::ResendChatContactCard(const std::string& peer_username,
                                       const std::string& message_id_hex,
                                       const std::string& card_username,
                                       const std::string& card_display) {
  return MessagingService().ResendChatContactCard(*this, peer_username, message_id_hex, card_username, card_display);
}

bool ClientCore::SendChatSticker(const std::string& peer_username,
                                 const std::string& sticker_id,
                                 std::string& out_message_id_hex) {
  return MessagingService().SendChatSticker(*this, peer_username, sticker_id, out_message_id_hex);
}

bool ClientCore::ResendChatSticker(const std::string& peer_username,
                                   const std::string& message_id_hex,
                                   const std::string& sticker_id) {
  return MessagingService().ResendChatSticker(*this, peer_username, message_id_hex, sticker_id);
}

bool ClientCore::SendChatReadReceipt(const std::string& peer_username,
                                     const std::string& message_id_hex) {
  return MessagingService().SendChatReadReceipt(*this, peer_username, message_id_hex);
}

bool ClientCore::SendChatTyping(const std::string& peer_username, bool typing) {
  return MessagingService().SendChatTyping(*this, peer_username, typing);
}

bool ClientCore::SendChatPresence(const std::string& peer_username, bool online) {
  return MessagingService().SendChatPresence(*this, peer_username, online);
}

bool ClientCore::SendChatFile(const std::string& peer_username,
                              const std::filesystem::path& file_path,
                              std::string& out_message_id_hex) {
  return MessagingService().SendChatFile(*this, peer_username, file_path, out_message_id_hex);
}

bool ClientCore::ResendChatFile(const std::string& peer_username,
                                const std::string& message_id_hex,
                                const std::filesystem::path& file_path) {
  return MessagingService().ResendChatFile(*this, peer_username, message_id_hex, file_path);
}

ClientCore::ChatPollResult ClientCore::PollChat() {
  return MessagingService().PollChat(*this);
}

}  // namespace mi::client
