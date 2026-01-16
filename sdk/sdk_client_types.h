#ifndef MI_E2EE_SDK_CLIENT_TYPES_H
#define MI_E2EE_SDK_CLIENT_TYPES_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mi::sdk {

enum class GroupMemberRole : std::uint32_t { kOwner = 0, kAdmin = 1, kMember = 2 };

struct FriendEntry {
  std::string username;
  std::string remark;
};

struct FriendRequestEntry {
  std::string requester_username;
  std::string requester_remark;
};

struct ChatTextMessage {
  std::string from_username;
  std::string message_id_hex;
  std::string text_utf8;
};

struct ChatFileMessage {
  std::string from_username;
  std::string message_id_hex;
  std::string file_id;
  std::array<std::uint8_t, 32> file_key{};
  std::string file_name;
  std::uint64_t file_size{0};
};

struct ChatStickerMessage {
  std::string from_username;
  std::string message_id_hex;
  std::string sticker_id;
};

struct GroupChatTextMessage {
  std::string group_id;
  std::string from_username;
  std::string message_id_hex;
  std::string text_utf8;
};

struct GroupChatFileMessage {
  std::string group_id;
  std::string from_username;
  std::string message_id_hex;
  std::string file_id;
  std::array<std::uint8_t, 32> file_key{};
  std::string file_name;
  std::uint64_t file_size{0};
};

struct GroupInviteMessage {
  std::string group_id;
  std::string from_username;
  std::string message_id_hex;
};

struct GroupNotice {
  std::string group_id;
  std::uint8_t kind{0};
  std::string actor_username;
  std::string target_username;
  GroupMemberRole role{GroupMemberRole::kMember};
};

struct GroupCallEvent {
  std::uint8_t op{0};
  std::string group_id;
  std::array<std::uint8_t, 16> call_id{};
  std::uint32_t key_id{0};
  std::string sender;
  std::uint8_t media_flags{0};
  std::uint64_t ts_ms{0};
};

struct OutgoingChatTextMessage {
  std::string peer_username;
  std::string message_id_hex;
  std::string text_utf8;
};

struct OutgoingChatFileMessage {
  std::string peer_username;
  std::string message_id_hex;
  std::string file_id;
  std::array<std::uint8_t, 32> file_key{};
  std::string file_name;
  std::uint64_t file_size{0};
};

struct OutgoingChatStickerMessage {
  std::string peer_username;
  std::string message_id_hex;
  std::string sticker_id;
};

struct OutgoingGroupChatTextMessage {
  std::string group_id;
  std::string message_id_hex;
  std::string text_utf8;
};

struct OutgoingGroupChatFileMessage {
  std::string group_id;
  std::string message_id_hex;
  std::string file_id;
  std::array<std::uint8_t, 32> file_key{};
  std::string file_name;
  std::uint64_t file_size{0};
};

struct ChatDelivery {
  std::string from_username;
  std::string message_id_hex;
};

struct ChatReadReceipt {
  std::string from_username;
  std::string message_id_hex;
};

struct ChatTypingEvent {
  std::string from_username;
  bool typing{false};
};

struct ChatPresenceEvent {
  std::string from_username;
  bool online{false};
};

struct ChatPollResult {
  std::vector<ChatTextMessage> texts;
  std::vector<ChatFileMessage> files;
  std::vector<ChatStickerMessage> stickers;
  std::vector<GroupChatTextMessage> group_texts;
  std::vector<GroupChatFileMessage> group_files;
  std::vector<GroupInviteMessage> group_invites;
  std::vector<GroupNotice> group_notices;
  std::vector<OutgoingChatTextMessage> outgoing_texts;
  std::vector<OutgoingChatFileMessage> outgoing_files;
  std::vector<OutgoingChatStickerMessage> outgoing_stickers;
  std::vector<OutgoingGroupChatTextMessage> outgoing_group_texts;
  std::vector<OutgoingGroupChatFileMessage> outgoing_group_files;
  std::vector<ChatDelivery> deliveries;
  std::vector<ChatReadReceipt> read_receipts;
  std::vector<ChatTypingEvent> typing_events;
  std::vector<ChatPresenceEvent> presence_events;
};

enum class HistoryKind : std::uint8_t { kText = 1, kFile = 2, kSticker = 3, kSystem = 4 };
enum class HistoryStatus : std::uint8_t { kSent = 0, kDelivered = 1, kRead = 2, kFailed = 3 };

struct PollResult {
  ChatPollResult chat;
  std::vector<GroupCallEvent> group_calls;
  std::vector<std::vector<std::uint8_t>> offline_payloads;
};

}  // namespace mi::sdk

#endif  // MI_E2EE_SDK_CLIENT_TYPES_H
