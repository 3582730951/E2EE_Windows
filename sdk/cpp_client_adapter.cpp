#include "cpp_client_adapter.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace {

std::string ReadEventString(const char* value) {
  return value ? std::string(value) : std::string();
}

void CopyEventFileKey(const mi_event_t& ev,
                      std::array<std::uint8_t, 32>& out) {
  out.fill(0);
  if (ev.file_key && ev.file_key_len == out.size()) {
    std::copy_n(ev.file_key, out.size(), out.begin());
  }
}

void AppendEventToPollResult(const mi_event_t& ev, mi::sdk::PollResult& out) {
  switch (ev.type) {
    case MI_EVENT_CHAT_TEXT: {
      mi::sdk::ChatTextMessage msg;
      msg.from_username = ReadEventString(ev.sender);
      msg.message_id_hex = ReadEventString(ev.message_id);
      msg.text_utf8 = ReadEventString(ev.text);
      out.chat.texts.push_back(std::move(msg));
      break;
    }
    case MI_EVENT_CHAT_FILE: {
      mi::sdk::ChatFileMessage msg;
      msg.from_username = ReadEventString(ev.sender);
      msg.message_id_hex = ReadEventString(ev.message_id);
      msg.file_id = ReadEventString(ev.file_id);
      msg.file_name = ReadEventString(ev.file_name);
      msg.file_size = ev.file_size;
      CopyEventFileKey(ev, msg.file_key);
      out.chat.files.push_back(std::move(msg));
      break;
    }
    case MI_EVENT_CHAT_STICKER: {
      mi::sdk::ChatStickerMessage msg;
      msg.from_username = ReadEventString(ev.sender);
      msg.message_id_hex = ReadEventString(ev.message_id);
      msg.sticker_id = ReadEventString(ev.sticker_id);
      out.chat.stickers.push_back(std::move(msg));
      break;
    }
    case MI_EVENT_GROUP_TEXT: {
      mi::sdk::GroupChatTextMessage msg;
      msg.group_id = ReadEventString(ev.group_id);
      msg.from_username = ReadEventString(ev.sender);
      msg.message_id_hex = ReadEventString(ev.message_id);
      msg.text_utf8 = ReadEventString(ev.text);
      out.chat.group_texts.push_back(std::move(msg));
      break;
    }
    case MI_EVENT_GROUP_FILE: {
      mi::sdk::GroupChatFileMessage msg;
      msg.group_id = ReadEventString(ev.group_id);
      msg.from_username = ReadEventString(ev.sender);
      msg.message_id_hex = ReadEventString(ev.message_id);
      msg.file_id = ReadEventString(ev.file_id);
      msg.file_name = ReadEventString(ev.file_name);
      msg.file_size = ev.file_size;
      CopyEventFileKey(ev, msg.file_key);
      out.chat.group_files.push_back(std::move(msg));
      break;
    }
    case MI_EVENT_GROUP_INVITE: {
      mi::sdk::GroupInviteMessage msg;
      msg.group_id = ReadEventString(ev.group_id);
      msg.from_username = ReadEventString(ev.sender);
      msg.message_id_hex = ReadEventString(ev.message_id);
      out.chat.group_invites.push_back(std::move(msg));
      break;
    }
    case MI_EVENT_GROUP_NOTICE: {
      mi::sdk::GroupNotice notice;
      notice.group_id = ReadEventString(ev.group_id);
      notice.kind = static_cast<std::uint8_t>(ev.notice_kind);
      notice.actor_username = ReadEventString(ev.actor);
      notice.target_username = ReadEventString(ev.target);
      notice.role = static_cast<mi::sdk::GroupMemberRole>(ev.role);
      out.chat.group_notices.push_back(std::move(notice));
      break;
    }
    case MI_EVENT_OUTGOING_TEXT: {
      mi::sdk::OutgoingChatTextMessage msg;
      msg.peer_username = ReadEventString(ev.peer);
      msg.message_id_hex = ReadEventString(ev.message_id);
      msg.text_utf8 = ReadEventString(ev.text);
      out.chat.outgoing_texts.push_back(std::move(msg));
      break;
    }
    case MI_EVENT_OUTGOING_FILE: {
      mi::sdk::OutgoingChatFileMessage msg;
      msg.peer_username = ReadEventString(ev.peer);
      msg.message_id_hex = ReadEventString(ev.message_id);
      msg.file_id = ReadEventString(ev.file_id);
      msg.file_name = ReadEventString(ev.file_name);
      msg.file_size = ev.file_size;
      CopyEventFileKey(ev, msg.file_key);
      out.chat.outgoing_files.push_back(std::move(msg));
      break;
    }
    case MI_EVENT_OUTGOING_STICKER: {
      mi::sdk::OutgoingChatStickerMessage msg;
      msg.peer_username = ReadEventString(ev.peer);
      msg.message_id_hex = ReadEventString(ev.message_id);
      msg.sticker_id = ReadEventString(ev.sticker_id);
      out.chat.outgoing_stickers.push_back(std::move(msg));
      break;
    }
    case MI_EVENT_OUTGOING_GROUP_TEXT: {
      mi::sdk::OutgoingGroupChatTextMessage msg;
      msg.group_id = ReadEventString(ev.group_id);
      msg.message_id_hex = ReadEventString(ev.message_id);
      msg.text_utf8 = ReadEventString(ev.text);
      out.chat.outgoing_group_texts.push_back(std::move(msg));
      break;
    }
    case MI_EVENT_OUTGOING_GROUP_FILE: {
      mi::sdk::OutgoingGroupChatFileMessage msg;
      msg.group_id = ReadEventString(ev.group_id);
      msg.message_id_hex = ReadEventString(ev.message_id);
      msg.file_id = ReadEventString(ev.file_id);
      msg.file_name = ReadEventString(ev.file_name);
      msg.file_size = ev.file_size;
      CopyEventFileKey(ev, msg.file_key);
      out.chat.outgoing_group_files.push_back(std::move(msg));
      break;
    }
    case MI_EVENT_DELIVERY: {
      mi::sdk::ChatDelivery d;
      d.from_username = ReadEventString(ev.peer);
      d.message_id_hex = ReadEventString(ev.message_id);
      out.chat.deliveries.push_back(std::move(d));
      break;
    }
    case MI_EVENT_READ_RECEIPT: {
      mi::sdk::ChatReadReceipt r;
      r.from_username = ReadEventString(ev.peer);
      r.message_id_hex = ReadEventString(ev.message_id);
      out.chat.read_receipts.push_back(std::move(r));
      break;
    }
    case MI_EVENT_TYPING: {
      mi::sdk::ChatTypingEvent t;
      t.from_username = ReadEventString(ev.peer);
      t.typing = ev.typing != 0;
      out.chat.typing_events.push_back(std::move(t));
      break;
    }
    case MI_EVENT_PRESENCE: {
      mi::sdk::ChatPresenceEvent p;
      p.from_username = ReadEventString(ev.peer);
      p.online = ev.online != 0;
      out.chat.presence_events.push_back(std::move(p));
      break;
    }
    case MI_EVENT_GROUP_CALL: {
      mi::sdk::GroupCallEvent gc;
      gc.op = static_cast<std::uint8_t>(ev.call_op);
      gc.group_id = ReadEventString(ev.group_id);
      std::copy_n(ev.call_id, gc.call_id.size(), gc.call_id.begin());
      gc.key_id = ev.call_key_id;
      gc.sender = ReadEventString(ev.sender);
      gc.media_flags = ev.call_media_flags;
      gc.ts_ms = ev.ts_ms;
      out.group_calls.push_back(std::move(gc));
      break;
    }
    case MI_EVENT_OFFLINE_PAYLOAD: {
      if (ev.payload && ev.payload_len > 0) {
        const auto* ptr = ev.payload;
        out.offline_payloads.emplace_back(ptr, ptr + ev.payload_len);
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace

namespace mi::sdk {

bool PollEvents(mi_client_handle* handle,
                std::uint32_t max_events,
                std::uint32_t wait_ms,
                PollResult& out,
                std::string& error) {
  error.clear();
  out = PollResult{};
  if (!handle) {
    error = "client handle null";
    return false;
  }
  if (max_events == 0) {
    return true;
  }
  if (max_events > 256) {
    max_events = 256;
  }
  std::vector<mi_event_t> buffer(max_events);
  const std::uint32_t count =
      mi_client_poll_event(handle, buffer.data(), max_events, wait_ms);
  const char* last_err = mi_client_last_error(handle);
  if (last_err && *last_err != '\0') {
    error = last_err;
  }
  for (std::uint32_t i = 0; i < count; ++i) {
    AppendEventToPollResult(buffer[i], out);
  }
  return true;
}

}  // namespace mi::sdk
