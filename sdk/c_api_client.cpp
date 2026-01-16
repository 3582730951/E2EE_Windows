#include "c_api_client.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <functional>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "client_core.h"
#include "platform_time.h"

struct Event {
  std::uint32_t type{MI_EVENT_NONE};
  std::uint64_t ts_ms{0};
  std::string peer;
  std::string sender;
  std::string group_id;
  std::string message_id;
  std::string text;
  std::string file_id;
  std::string file_name;
  std::uint64_t file_size{0};
  std::array<std::uint8_t, 32> file_key{};
  std::uint32_t file_key_len{0};
  std::string sticker_id;
  std::uint32_t notice_kind{0};
  std::string actor;
  std::string target;
  std::uint32_t role{0};
  std::uint8_t typing{0};
  std::uint8_t online{0};
  std::array<std::uint8_t, 16> call_id{};
  std::uint32_t call_key_id{0};
  std::uint32_t call_op{0};
  std::uint8_t call_media_flags{0};
  std::vector<std::uint8_t> payload;
};

struct MediaSubscription {
  std::array<std::uint8_t, 16> call_id{};
  bool is_group{false};
  std::string group_id;
};

struct mi_client_handle {
  mi::client::ClientCore* core{nullptr};
  bool owns_core{false};
  std::vector<MediaSubscription> media_subs;
  std::deque<Event> pending;
  std::vector<Event> last_events;
  std::vector<mi::client::ClientCore::FriendEntry> friend_cache;
  std::vector<mi_friend_entry_t> friend_view;
  std::vector<mi::client::ClientCore::FriendRequestEntry> friend_req_cache;
  std::vector<mi_friend_request_entry_t> friend_req_view;
  std::vector<mi::client::ClientCore::DeviceEntry> device_cache;
  std::vector<mi_device_entry_t> device_view;
  std::vector<mi::client::ClientCore::GroupMemberInfo> group_member_cache;
  std::vector<mi_group_member_entry_t> group_member_view;
  std::vector<std::string> group_call_member_cache;
  std::vector<mi_group_call_member_t> group_call_member_view;
  std::vector<mi::client::ClientCore::MediaRelayPacket> media_packet_cache;
  std::vector<mi_media_packet_t> media_packet_view;
  std::vector<mi::client::ClientCore::MediaRelayPacket> group_media_packet_cache;
  std::vector<mi_media_packet_t> group_media_packet_view;
  std::vector<mi::client::ClientCore::DevicePairingRequest> pairing_cache;
  std::vector<mi_device_pairing_request_t> pairing_view;
  std::vector<mi::client::ClientCore::HistoryEntry> history_cache;
  std::vector<mi_history_entry_t> history_view;
};

namespace {

const char* kDefaultConfigPath = "config/client_config.ini";
std::string g_last_create_error;

const char* NormalizeConfigPath(const char* config_path) {
  if (!config_path || *config_path == '\0') {
    return kDefaultConfigPath;
  }
  return config_path;
}

std::uint64_t NowMs() {
  return mi::platform::NowUnixSeconds() * 1000ULL;
}

bool ParseCallId(const std::uint8_t* call_id, std::uint32_t call_id_len,
                 std::array<std::uint8_t, 16>& out) {
  out.fill(0);
  if (!call_id || call_id_len != out.size()) {
    return false;
  }
  std::memcpy(out.data(), call_id, out.size());
  return true;
}

MediaSubscription* FindMediaSubscription(mi_client_handle* handle,
                                         const std::array<std::uint8_t, 16>& call_id,
                                         bool is_group) {
  for (auto& sub : handle->media_subs) {
    if (sub.call_id == call_id && sub.is_group == is_group) {
      return &sub;
    }
  }
  return nullptr;
}

bool CopyStringToC(const std::string& src, char** out) {
  if (!out) {
    return true;
  }
  *out = nullptr;
  if (src.empty()) {
    return true;
  }
  const std::size_t size = src.size();
  char* buf = static_cast<char*>(std::malloc(size + 1));
  if (!buf) {
    return false;
  }
  std::memcpy(buf, src.data(), size);
  buf[size] = '\0';
  *out = buf;
  return true;
}

std::filesystem::path PathFromUtf8(const char* path_utf8) {
  if (!path_utf8) {
    return {};
  }
  return std::filesystem::u8path(path_utf8);
}

bool BuildChatFileMessage(const char* file_id,
                          const std::uint8_t* file_key,
                          std::uint32_t file_key_len,
                          const char* file_name,
                          std::uint64_t file_size,
                          mi::client::ClientCore::ChatFileMessage& out) {
  out = {};
  if (!file_id || !file_key || file_key_len != out.file_key.size()) {
    return false;
  }
  out.file_id = file_id;
  if (file_name) {
    out.file_name = file_name;
  }
  out.file_size = file_size;
  std::memcpy(out.file_key.data(), file_key, out.file_key.size());
  return true;
}

void AppendFileKey(Event& ev, const std::array<std::uint8_t, 32>& key) {
  ev.file_key = key;
  ev.file_key_len = static_cast<std::uint32_t>(ev.file_key.size());
}

bool AppendChatEvents(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return false;
  }
  const auto result = handle->core->PollChat();
  const std::uint64_t now_ms = NowMs();
  bool added = false;

  for (const auto& msg : result.texts) {
    Event ev;
    ev.type = MI_EVENT_CHAT_TEXT;
    ev.ts_ms = now_ms;
    ev.peer = msg.from_username;
    ev.sender = msg.from_username;
    ev.message_id = msg.message_id_hex;
    ev.text = msg.text_utf8;
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& msg : result.files) {
    Event ev;
    ev.type = MI_EVENT_CHAT_FILE;
    ev.ts_ms = now_ms;
    ev.peer = msg.from_username;
    ev.sender = msg.from_username;
    ev.message_id = msg.message_id_hex;
    ev.file_id = msg.file_id;
    ev.file_name = msg.file_name;
    ev.file_size = msg.file_size;
    AppendFileKey(ev, msg.file_key);
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& msg : result.stickers) {
    Event ev;
    ev.type = MI_EVENT_CHAT_STICKER;
    ev.ts_ms = now_ms;
    ev.peer = msg.from_username;
    ev.sender = msg.from_username;
    ev.message_id = msg.message_id_hex;
    ev.sticker_id = msg.sticker_id;
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& msg : result.group_texts) {
    Event ev;
    ev.type = MI_EVENT_GROUP_TEXT;
    ev.ts_ms = now_ms;
    ev.group_id = msg.group_id;
    ev.sender = msg.from_username;
    ev.message_id = msg.message_id_hex;
    ev.text = msg.text_utf8;
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& msg : result.group_files) {
    Event ev;
    ev.type = MI_EVENT_GROUP_FILE;
    ev.ts_ms = now_ms;
    ev.group_id = msg.group_id;
    ev.sender = msg.from_username;
    ev.message_id = msg.message_id_hex;
    ev.file_id = msg.file_id;
    ev.file_name = msg.file_name;
    ev.file_size = msg.file_size;
    AppendFileKey(ev, msg.file_key);
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& msg : result.group_invites) {
    Event ev;
    ev.type = MI_EVENT_GROUP_INVITE;
    ev.ts_ms = now_ms;
    ev.group_id = msg.group_id;
    ev.sender = msg.from_username;
    ev.message_id = msg.message_id_hex;
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& notice : result.group_notices) {
    Event ev;
    ev.type = MI_EVENT_GROUP_NOTICE;
    ev.ts_ms = now_ms;
    ev.group_id = notice.group_id;
    ev.notice_kind = notice.kind;
    ev.actor = notice.actor_username;
    ev.target = notice.target_username;
    ev.role = static_cast<std::uint32_t>(notice.role);
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& msg : result.outgoing_texts) {
    Event ev;
    ev.type = MI_EVENT_OUTGOING_TEXT;
    ev.ts_ms = now_ms;
    ev.peer = msg.peer_username;
    ev.message_id = msg.message_id_hex;
    ev.text = msg.text_utf8;
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& msg : result.outgoing_files) {
    Event ev;
    ev.type = MI_EVENT_OUTGOING_FILE;
    ev.ts_ms = now_ms;
    ev.peer = msg.peer_username;
    ev.message_id = msg.message_id_hex;
    ev.file_id = msg.file_id;
    ev.file_name = msg.file_name;
    ev.file_size = msg.file_size;
    AppendFileKey(ev, msg.file_key);
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& msg : result.outgoing_stickers) {
    Event ev;
    ev.type = MI_EVENT_OUTGOING_STICKER;
    ev.ts_ms = now_ms;
    ev.peer = msg.peer_username;
    ev.message_id = msg.message_id_hex;
    ev.sticker_id = msg.sticker_id;
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& msg : result.outgoing_group_texts) {
    Event ev;
    ev.type = MI_EVENT_OUTGOING_GROUP_TEXT;
    ev.ts_ms = now_ms;
    ev.group_id = msg.group_id;
    ev.message_id = msg.message_id_hex;
    ev.text = msg.text_utf8;
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& msg : result.outgoing_group_files) {
    Event ev;
    ev.type = MI_EVENT_OUTGOING_GROUP_FILE;
    ev.ts_ms = now_ms;
    ev.group_id = msg.group_id;
    ev.message_id = msg.message_id_hex;
    ev.file_id = msg.file_id;
    ev.file_name = msg.file_name;
    ev.file_size = msg.file_size;
    AppendFileKey(ev, msg.file_key);
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& delivery : result.deliveries) {
    Event ev;
    ev.type = MI_EVENT_DELIVERY;
    ev.ts_ms = now_ms;
    ev.peer = delivery.from_username;
    ev.message_id = delivery.message_id_hex;
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& receipt : result.read_receipts) {
    Event ev;
    ev.type = MI_EVENT_READ_RECEIPT;
    ev.ts_ms = now_ms;
    ev.peer = receipt.from_username;
    ev.message_id = receipt.message_id_hex;
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& typing : result.typing_events) {
    Event ev;
    ev.type = MI_EVENT_TYPING;
    ev.ts_ms = now_ms;
    ev.peer = typing.from_username;
    ev.typing = typing.typing ? 1 : 0;
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  for (const auto& presence : result.presence_events) {
    Event ev;
    ev.type = MI_EVENT_PRESENCE;
    ev.ts_ms = now_ms;
    ev.peer = presence.from_username;
    ev.online = presence.online ? 1 : 0;
    handle->pending.push_back(std::move(ev));
    added = true;
  }
  return added;
}

bool AppendOfflineEvents(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return false;
  }
  const auto payloads = handle->core->PullOffline();
  if (payloads.empty()) {
    return false;
  }
  const std::uint64_t now_ms = NowMs();
  for (const auto& payload : payloads) {
    Event ev;
    ev.type = MI_EVENT_OFFLINE_PAYLOAD;
    ev.ts_ms = now_ms;
    ev.payload = payload;
    handle->pending.push_back(std::move(ev));
  }
  return true;
}

bool AppendGroupCallEvents(mi_client_handle* handle, std::uint32_t wait_ms) {
  if (!handle || !handle->core) {
    return false;
  }
  const auto events = handle->core->PullGroupCallEvents(32, wait_ms);
  bool added = false;
  for (const auto& ev : events) {
    Event out;
    out.type = MI_EVENT_GROUP_CALL;
    out.ts_ms = ev.ts_ms;
    out.group_id = ev.group_id;
    out.sender = ev.sender;
    out.call_id = ev.call_id;
    out.call_key_id = ev.key_id;
    out.call_op = ev.op;
    out.call_media_flags = ev.media_flags;
    handle->pending.push_back(std::move(out));
    added = true;
  }
  return added;
}

bool AppendMediaEvents(mi_client_handle* handle, std::uint32_t wait_ms) {
  if (!handle || !handle->core || handle->media_subs.empty()) {
    return false;
  }
  bool added = false;
  bool waited = false;
  const auto& media_cfg = handle->core->media_config();
  for (const auto& sub : handle->media_subs) {
    const std::uint32_t use_wait = waited ? 0 : wait_ms;
    waited = waited || (wait_ms > 0);
    std::uint32_t max_packets = sub.is_group
                                    ? media_cfg.group_pull_max_packets
                                    : media_cfg.pull_max_packets;
    if (max_packets == 0) {
      max_packets = sub.is_group ? 64u : 32u;
    }
    const std::uint32_t effective_wait = use_wait;
    std::vector<mi::client::ClientCore::MediaRelayPacket> packets =
        sub.is_group
            ? handle->core->PullGroupMedia(sub.call_id, max_packets,
                                           effective_wait)
            : handle->core->PullMedia(sub.call_id, max_packets, effective_wait);
    if (packets.empty()) {
      continue;
    }
    const std::uint64_t now_ms = NowMs();
    for (auto& packet : packets) {
      Event ev;
      ev.type = sub.is_group ? MI_EVENT_GROUP_MEDIA_RELAY : MI_EVENT_MEDIA_RELAY;
      ev.ts_ms = now_ms;
      ev.sender = packet.sender;
      ev.call_id = sub.call_id;
      if (sub.is_group && !sub.group_id.empty()) {
        ev.group_id = sub.group_id;
      }
      ev.payload = std::move(packet.payload);
      handle->pending.push_back(std::move(ev));
      added = true;
    }
  }
  return added;
}

void FillEventView(const Event& src, mi_event_t& dst) {
  std::memset(&dst, 0, sizeof(dst));
  dst.type = src.type;
  dst.ts_ms = src.ts_ms;
  dst.peer = src.peer.empty() ? nullptr : src.peer.c_str();
  dst.sender = src.sender.empty() ? nullptr : src.sender.c_str();
  dst.group_id = src.group_id.empty() ? nullptr : src.group_id.c_str();
  dst.message_id = src.message_id.empty() ? nullptr : src.message_id.c_str();
  dst.text = src.text.empty() ? nullptr : src.text.c_str();
  dst.file_id = src.file_id.empty() ? nullptr : src.file_id.c_str();
  dst.file_name = src.file_name.empty() ? nullptr : src.file_name.c_str();
  dst.file_size = src.file_size;
  if (src.file_key_len > 0) {
    dst.file_key = src.file_key.data();
    dst.file_key_len = src.file_key_len;
  }
  dst.sticker_id = src.sticker_id.empty() ? nullptr : src.sticker_id.c_str();
  dst.notice_kind = src.notice_kind;
  dst.actor = src.actor.empty() ? nullptr : src.actor.c_str();
  dst.target = src.target.empty() ? nullptr : src.target.c_str();
  dst.role = src.role;
  dst.typing = src.typing;
  dst.online = src.online;
  std::memcpy(dst.call_id, src.call_id.data(), src.call_id.size());
  dst.call_key_id = src.call_key_id;
  dst.call_op = src.call_op;
  dst.call_media_flags = src.call_media_flags;
  if (!src.payload.empty()) {
    dst.payload = src.payload.data();
    dst.payload_len = static_cast<std::uint32_t>(src.payload.size());
  }
}

std::uint32_t FillFriendView(
    const std::vector<mi::client::ClientCore::FriendEntry>& src,
    std::vector<mi_friend_entry_t>& view,
    mi_friend_entry_t* out_entries,
    std::uint32_t max_entries) {
  view.clear();
  view.reserve(src.size());
  for (const auto& entry : src) {
    mi_friend_entry_t v{};
    v.username = entry.username.empty() ? nullptr : entry.username.c_str();
    v.remark = entry.remark.empty() ? nullptr : entry.remark.c_str();
    view.push_back(v);
  }
  const std::uint32_t count = std::min<std::uint32_t>(
      max_entries, static_cast<std::uint32_t>(view.size()));
  for (std::uint32_t i = 0; i < count; ++i) {
    out_entries[i] = view[i];
  }
  return count;
}

std::uint32_t FillFriendRequestView(
    const std::vector<mi::client::ClientCore::FriendRequestEntry>& src,
    std::vector<mi_friend_request_entry_t>& view,
    mi_friend_request_entry_t* out_entries,
    std::uint32_t max_entries) {
  view.clear();
  view.reserve(src.size());
  for (const auto& entry : src) {
    mi_friend_request_entry_t v{};
    v.requester_username =
        entry.requester_username.empty() ? nullptr : entry.requester_username.c_str();
    v.requester_remark =
        entry.requester_remark.empty() ? nullptr : entry.requester_remark.c_str();
    view.push_back(v);
  }
  const std::uint32_t count = std::min<std::uint32_t>(
      max_entries, static_cast<std::uint32_t>(view.size()));
  for (std::uint32_t i = 0; i < count; ++i) {
    out_entries[i] = view[i];
  }
  return count;
}

std::uint32_t FillDeviceView(
    const std::vector<mi::client::ClientCore::DeviceEntry>& src,
    std::vector<mi_device_entry_t>& view,
    mi_device_entry_t* out_entries,
    std::uint32_t max_entries) {
  view.clear();
  view.reserve(src.size());
  for (const auto& entry : src) {
    mi_device_entry_t v{};
    v.device_id = entry.device_id.empty() ? nullptr : entry.device_id.c_str();
    v.last_seen_sec = entry.last_seen_sec;
    view.push_back(v);
  }
  const std::uint32_t count = std::min<std::uint32_t>(
      max_entries, static_cast<std::uint32_t>(view.size()));
  for (std::uint32_t i = 0; i < count; ++i) {
    out_entries[i] = view[i];
  }
  return count;
}

std::uint32_t FillDevicePairingView(
    const std::vector<mi::client::ClientCore::DevicePairingRequest>& src,
    std::vector<mi_device_pairing_request_t>& view,
    mi_device_pairing_request_t* out_entries,
    std::uint32_t max_entries) {
  view.clear();
  view.reserve(src.size());
  for (const auto& entry : src) {
    mi_device_pairing_request_t v{};
    v.device_id = entry.device_id.empty() ? nullptr : entry.device_id.c_str();
    v.request_id_hex =
        entry.request_id_hex.empty() ? nullptr : entry.request_id_hex.c_str();
    view.push_back(v);
  }
  const std::uint32_t count = std::min<std::uint32_t>(
      max_entries, static_cast<std::uint32_t>(view.size()));
  for (std::uint32_t i = 0; i < count; ++i) {
    out_entries[i] = view[i];
  }
  return count;
}

std::uint32_t FillGroupMemberView(
    const std::vector<mi::client::ClientCore::GroupMemberInfo>& src,
    std::vector<mi_group_member_entry_t>& view,
    mi_group_member_entry_t* out_entries,
    std::uint32_t max_entries) {
  view.clear();
  view.reserve(src.size());
  for (const auto& entry : src) {
    mi_group_member_entry_t v{};
    v.username = entry.username.empty() ? nullptr : entry.username.c_str();
    v.role = static_cast<std::uint32_t>(entry.role);
    view.push_back(v);
  }
  const std::uint32_t count = std::min<std::uint32_t>(
      max_entries, static_cast<std::uint32_t>(view.size()));
  for (std::uint32_t i = 0; i < count; ++i) {
    out_entries[i] = view[i];
  }
  return count;
}

std::uint32_t FillGroupCallMemberView(
    const std::vector<std::string>& src,
    std::vector<mi_group_call_member_t>& view,
    mi_group_call_member_t* out_entries,
    std::uint32_t max_entries) {
  view.clear();
  view.reserve(src.size());
  for (const auto& entry : src) {
    mi_group_call_member_t v{};
    v.username = entry.empty() ? nullptr : entry.c_str();
    view.push_back(v);
  }
  const std::uint32_t count = std::min<std::uint32_t>(
      max_entries, static_cast<std::uint32_t>(view.size()));
  for (std::uint32_t i = 0; i < count; ++i) {
    out_entries[i] = view[i];
  }
  return count;
}

std::uint32_t FillMediaPacketView(
    const std::vector<mi::client::ClientCore::MediaRelayPacket>& src,
    std::vector<mi_media_packet_t>& view,
    mi_media_packet_t* out_entries,
    std::uint32_t max_entries) {
  view.clear();
  view.reserve(src.size());
  for (const auto& entry : src) {
    mi_media_packet_t v{};
    v.sender = entry.sender.empty() ? nullptr : entry.sender.c_str();
    if (!entry.payload.empty()) {
      v.payload = entry.payload.data();
      v.payload_len = static_cast<std::uint32_t>(entry.payload.size());
    }
    view.push_back(v);
  }
  const std::uint32_t count = std::min<std::uint32_t>(
      max_entries, static_cast<std::uint32_t>(view.size()));
  for (std::uint32_t i = 0; i < count; ++i) {
    out_entries[i] = view[i];
  }
  return count;
}

std::uint32_t FillHistoryView(
    const std::vector<mi::client::ClientCore::HistoryEntry>& src,
    std::vector<mi_history_entry_t>& view,
    mi_history_entry_t* out_entries,
    std::uint32_t max_entries) {
  view.clear();
  view.reserve(src.size());
  for (const auto& entry : src) {
    mi_history_entry_t v{};
    v.kind = static_cast<std::uint32_t>(entry.kind);
    v.status = static_cast<std::uint32_t>(entry.status);
    v.is_group = entry.is_group ? 1 : 0;
    v.outgoing = entry.outgoing ? 1 : 0;
    v.timestamp_sec = entry.timestamp_sec;
    v.conv_id = entry.conv_id.empty() ? nullptr : entry.conv_id.c_str();
    v.sender = entry.sender.empty() ? nullptr : entry.sender.c_str();
    v.message_id =
        entry.message_id_hex.empty() ? nullptr : entry.message_id_hex.c_str();
    v.text = entry.text_utf8.empty() ? nullptr : entry.text_utf8.c_str();
    v.file_id = entry.file_id.empty() ? nullptr : entry.file_id.c_str();
    v.file_key = entry.file_key.data();
    v.file_key_len = static_cast<std::uint32_t>(entry.file_key.size());
    v.file_name = entry.file_name.empty() ? nullptr : entry.file_name.c_str();
    v.file_size = entry.file_size;
    v.sticker_id =
        entry.sticker_id.empty() ? nullptr : entry.sticker_id.c_str();
    view.push_back(v);
  }
  const std::uint32_t count = std::min<std::uint32_t>(
      max_entries, static_cast<std::uint32_t>(view.size()));
  for (std::uint32_t i = 0; i < count; ++i) {
    out_entries[i] = view[i];
  }
  return count;
}

std::vector<std::string> BuildMemberList(const char** members,
                                         std::uint32_t member_count) {
  std::vector<std::string> out;
  if (!members || member_count == 0) {
    return out;
  }
  out.reserve(member_count);
  for (std::uint32_t i = 0; i < member_count; ++i) {
    const char* value = members[i];
    if (!value || *value == '\0') {
      continue;
    }
    out.emplace_back(value);
  }
  return out;
}

}  // namespace

extern "C" {

void mi_client_get_version(mi_sdk_version* out_version) {
  if (!out_version) {
    return;
  }
  out_version->major = MI_E2EE_SDK_VERSION_MAJOR;
  out_version->minor = MI_E2EE_SDK_VERSION_MINOR;
  out_version->patch = MI_E2EE_SDK_VERSION_PATCH;
  out_version->abi = MI_E2EE_SDK_ABI_VERSION;
}

std::uint32_t mi_client_get_capabilities(void) {
  return MI_CLIENT_CAP_CHAT | MI_CLIENT_CAP_GROUP | MI_CLIENT_CAP_MEDIA |
         MI_CLIENT_CAP_GROUP_CALL | MI_CLIENT_CAP_OFFLINE |
         MI_CLIENT_CAP_DEVICE_SYNC | MI_CLIENT_CAP_KCP | MI_CLIENT_CAP_OPAQUE;
}

mi_client_handle* mi_client_create(const char* config_path) {
  g_last_create_error.clear();
  try {
    auto* handle = new (std::nothrow) mi_client_handle();
    if (!handle) {
      g_last_create_error = "client handle alloc failed";
      return nullptr;
    }
    handle->core = new (std::nothrow) mi::client::ClientCore();
    if (!handle->core) {
      g_last_create_error = "client core alloc failed";
      delete handle;
      return nullptr;
    }
    handle->owns_core = true;
    if (!handle->core->Init(NormalizeConfigPath(config_path))) {
      g_last_create_error = handle->core->last_error();
      if (g_last_create_error.empty()) {
        g_last_create_error = "client init failed";
      }
      delete handle->core;
      delete handle;
      return nullptr;
    }
    return handle;
  } catch (...) {
    g_last_create_error = "client create failed";
    return nullptr;
  }
}

const char* mi_client_last_create_error(void) {
  return g_last_create_error.c_str();
}

void mi_client_destroy(mi_client_handle* handle) {
  try {
    if (!handle) {
      return;
    }
    if (handle->owns_core && handle->core) {
      delete handle->core;
      handle->core = nullptr;
    }
    delete handle;
  } catch (...) {
  }
}

const char* mi_client_last_error(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return "";
  }
  return handle->core->last_error().c_str();
}

const char* mi_client_token(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return "";
  }
  return handle->core->token().c_str();
}

const char* mi_client_device_id(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return "";
  }
  return handle->core->device_id().c_str();
}

int mi_client_remote_ok(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return 0;
  }
  return handle->core->remote_ok() ? 1 : 0;
}

const char* mi_client_remote_error(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return "";
  }
  return handle->core->remote_error().c_str();
}

int mi_client_is_remote_mode(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return 0;
  }
  return handle->core->is_remote_mode() ? 1 : 0;
}

int mi_client_relogin(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return 0;
  }
  try {
    return handle->core->Relogin() ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_has_pending_server_trust(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return 0;
  }
  return handle->core->HasPendingServerTrust() ? 1 : 0;
}

const char* mi_client_pending_server_fingerprint(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return "";
  }
  return handle->core->pending_server_fingerprint().c_str();
}

const char* mi_client_pending_server_pin(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return "";
  }
  return handle->core->pending_server_pin().c_str();
}

int mi_client_trust_pending_server(mi_client_handle* handle, const char* pin) {
  if (!handle || !handle->core || !pin) {
    return 0;
  }
  try {
    return handle->core->TrustPendingServer(pin) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_has_pending_peer_trust(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return 0;
  }
  return handle->core->HasPendingPeerTrust() ? 1 : 0;
}

const char* mi_client_pending_peer_username(mi_client_handle* handle) {
  if (!handle || !handle->core || !handle->core->HasPendingPeerTrust()) {
    return "";
  }
  return handle->core->pending_peer_trust().peer_username.c_str();
}

const char* mi_client_pending_peer_fingerprint(mi_client_handle* handle) {
  if (!handle || !handle->core || !handle->core->HasPendingPeerTrust()) {
    return "";
  }
  return handle->core->pending_peer_trust().fingerprint_hex.c_str();
}

const char* mi_client_pending_peer_pin(mi_client_handle* handle) {
  if (!handle || !handle->core || !handle->core->HasPendingPeerTrust()) {
    return "";
  }
  return handle->core->pending_peer_trust().pin6.c_str();
}

int mi_client_trust_pending_peer(mi_client_handle* handle, const char* pin) {
  if (!handle || !handle->core || !pin) {
    return 0;
  }
  try {
    return handle->core->TrustPendingPeer(pin) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_register(mi_client_handle* handle,
                       const char* username,
                       const char* password) {
  if (!handle || !username || !password) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    return handle->core->Register(username, password) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_login(mi_client_handle* handle,
                    const char* username,
                    const char* password) {
  if (!handle || !username || !password) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    return handle->core->Login(username, password) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_logout(mi_client_handle* handle) {
  if (!handle) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    return handle->core->Logout() ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_heartbeat(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return 0;
  }
  try {
    return handle->core->Heartbeat() ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_send_private_text(mi_client_handle* handle,
                                const char* peer_username,
                                const char* text_utf8,
                                char** out_message_id_hex) {
  if (out_message_id_hex) {
    *out_message_id_hex = nullptr;
  }
  if (!handle || !peer_username || !text_utf8) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    std::string message_id;
    if (!handle->core->SendChatText(peer_username, text_utf8, message_id)) {
      return 0;
    }
    return CopyStringToC(message_id, out_message_id_hex) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_send_private_text_with_reply(mi_client_handle* handle,
                                           const char* peer_username,
                                           const char* text_utf8,
                                           const char* reply_to_message_id_hex,
                                           const char* reply_preview_utf8,
                                           char** out_message_id_hex) {
  if (out_message_id_hex) {
    *out_message_id_hex = nullptr;
  }
  if (!handle || !peer_username || !text_utf8 || !reply_to_message_id_hex) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    std::string message_id;
    const std::string preview = reply_preview_utf8 ? reply_preview_utf8 : "";
    if (!handle->core->SendChatTextWithReply(peer_username, text_utf8,
                                             reply_to_message_id_hex, preview,
                                             message_id)) {
      return 0;
    }
    return CopyStringToC(message_id, out_message_id_hex) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_resend_private_text(mi_client_handle* handle,
                                  const char* peer_username,
                                  const char* message_id_hex,
                                  const char* text_utf8) {
  if (!handle || !peer_username || !message_id_hex || !text_utf8) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    return handle->core->ResendChatText(peer_username, message_id_hex, text_utf8)
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_resend_private_text_with_reply(mi_client_handle* handle,
                                             const char* peer_username,
                                             const char* message_id_hex,
                                             const char* text_utf8,
                                             const char* reply_to_message_id_hex,
                                             const char* reply_preview_utf8) {
  if (!handle || !peer_username || !message_id_hex || !text_utf8 ||
      !reply_to_message_id_hex) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    const std::string preview = reply_preview_utf8 ? reply_preview_utf8 : "";
    return handle->core->ResendChatTextWithReply(peer_username, message_id_hex,
                                                 text_utf8,
                                                 reply_to_message_id_hex,
                                                 preview)
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_send_group_text(mi_client_handle* handle,
                              const char* group_id,
                              const char* text_utf8,
                              char** out_message_id_hex) {
  if (out_message_id_hex) {
    *out_message_id_hex = nullptr;
  }
  if (!handle || !group_id || !text_utf8) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    std::string message_id;
    if (!handle->core->SendGroupChatText(group_id, text_utf8, message_id)) {
      return 0;
    }
    return CopyStringToC(message_id, out_message_id_hex) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_resend_group_text(mi_client_handle* handle,
                                const char* group_id,
                                const char* message_id_hex,
                                const char* text_utf8) {
  if (!handle || !group_id || !message_id_hex || !text_utf8) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    return handle->core->ResendGroupChatText(group_id, message_id_hex, text_utf8)
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_send_private_file(mi_client_handle* handle,
                                const char* peer_username,
                                const char* file_path_utf8,
                                char** out_message_id_hex) {
  if (out_message_id_hex) {
    *out_message_id_hex = nullptr;
  }
  if (!handle || !peer_username || !file_path_utf8) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    std::string message_id;
    if (!handle->core->SendChatFile(peer_username,
                                    PathFromUtf8(file_path_utf8), message_id)) {
      return 0;
    }
    return CopyStringToC(message_id, out_message_id_hex) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_resend_private_file(mi_client_handle* handle,
                                  const char* peer_username,
                                  const char* message_id_hex,
                                  const char* file_path_utf8) {
  if (!handle || !peer_username || !message_id_hex || !file_path_utf8) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    return handle->core->ResendChatFile(peer_username, message_id_hex,
                                        PathFromUtf8(file_path_utf8))
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_send_group_file(mi_client_handle* handle,
                              const char* group_id,
                              const char* file_path_utf8,
                              char** out_message_id_hex) {
  if (out_message_id_hex) {
    *out_message_id_hex = nullptr;
  }
  if (!handle || !group_id || !file_path_utf8) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    std::string message_id;
    if (!handle->core->SendGroupChatFile(group_id,
                                         PathFromUtf8(file_path_utf8),
                                         message_id)) {
      return 0;
    }
    return CopyStringToC(message_id, out_message_id_hex) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_resend_group_file(mi_client_handle* handle,
                                const char* group_id,
                                const char* message_id_hex,
                                const char* file_path_utf8) {
  if (!handle || !group_id || !message_id_hex || !file_path_utf8) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    return handle->core->ResendGroupChatFile(group_id, message_id_hex,
                                             PathFromUtf8(file_path_utf8))
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_send_private_sticker(mi_client_handle* handle,
                                   const char* peer_username,
                                   const char* sticker_id,
                                   char** out_message_id_hex) {
  if (out_message_id_hex) {
    *out_message_id_hex = nullptr;
  }
  if (!handle || !peer_username || !sticker_id) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    std::string message_id;
    if (!handle->core->SendChatSticker(peer_username, sticker_id, message_id)) {
      return 0;
    }
    return CopyStringToC(message_id, out_message_id_hex) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_resend_private_sticker(mi_client_handle* handle,
                                     const char* peer_username,
                                     const char* message_id_hex,
                                     const char* sticker_id) {
  if (!handle || !peer_username || !message_id_hex || !sticker_id) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    return handle->core->ResendChatSticker(peer_username, message_id_hex,
                                           sticker_id)
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_send_private_location(mi_client_handle* handle,
                                    const char* peer_username,
                                    std::int32_t lat_e7,
                                    std::int32_t lon_e7,
                                    const char* label_utf8,
                                    char** out_message_id_hex) {
  if (out_message_id_hex) {
    *out_message_id_hex = nullptr;
  }
  if (!handle || !peer_username) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    std::string message_id;
    const std::string label = label_utf8 ? label_utf8 : "";
    if (!handle->core->SendChatLocation(peer_username, lat_e7, lon_e7, label,
                                        message_id)) {
      return 0;
    }
    return CopyStringToC(message_id, out_message_id_hex) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_resend_private_location(mi_client_handle* handle,
                                      const char* peer_username,
                                      const char* message_id_hex,
                                      std::int32_t lat_e7,
                                      std::int32_t lon_e7,
                                      const char* label_utf8) {
  if (!handle || !peer_username || !message_id_hex) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    const std::string label = label_utf8 ? label_utf8 : "";
    return handle->core->ResendChatLocation(peer_username, message_id_hex,
                                            lat_e7, lon_e7, label)
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_send_private_contact(mi_client_handle* handle,
                                   const char* peer_username,
                                   const char* card_username,
                                   const char* card_display,
                                   char** out_message_id_hex) {
  if (out_message_id_hex) {
    *out_message_id_hex = nullptr;
  }
  if (!handle || !peer_username || !card_username) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    std::string message_id;
    const std::string display = card_display ? card_display : "";
    if (!handle->core->SendChatContactCard(peer_username, card_username,
                                           display, message_id)) {
      return 0;
    }
    return CopyStringToC(message_id, out_message_id_hex) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_resend_private_contact(mi_client_handle* handle,
                                     const char* peer_username,
                                     const char* message_id_hex,
                                     const char* card_username,
                                     const char* card_display) {
  if (!handle || !peer_username || !message_id_hex || !card_username) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    const std::string display = card_display ? card_display : "";
    return handle->core->ResendChatContactCard(peer_username, message_id_hex,
                                               card_username, display)
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_send_read_receipt(mi_client_handle* handle,
                                const char* peer_username,
                                const char* message_id_hex) {
  if (!handle || !handle->core || !peer_username || !message_id_hex) {
    return 0;
  }
  try {
    return handle->core->SendChatReadReceipt(peer_username,
                                             message_id_hex)
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_send_typing(mi_client_handle* handle,
                          const char* peer_username,
                          int typing) {
  if (!handle || !handle->core || !peer_username) {
    return 0;
  }
  try {
    return handle->core->SendChatTyping(peer_username, typing != 0) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_send_presence(mi_client_handle* handle,
                            const char* peer_username,
                            int online) {
  if (!handle || !handle->core || !peer_username) {
    return 0;
  }
  try {
    return handle->core->SendChatPresence(peer_username, online != 0) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_add_friend(mi_client_handle* handle,
                         const char* friend_username,
                         const char* remark) {
  if (!handle || !friend_username) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    const std::string remark_str = remark ? remark : "";
    return handle->core->AddFriend(friend_username, remark_str) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_set_friend_remark(mi_client_handle* handle,
                                const char* friend_username,
                                const char* remark) {
  if (!handle || !friend_username) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    const std::string remark_str = remark ? remark : "";
    return handle->core->SetFriendRemark(friend_username, remark_str) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_delete_friend(mi_client_handle* handle,
                            const char* friend_username) {
  if (!handle || !friend_username) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    return handle->core->DeleteFriend(friend_username) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_set_user_blocked(mi_client_handle* handle,
                               const char* blocked_username,
                               int blocked) {
  if (!handle || !blocked_username) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    return handle->core->SetUserBlocked(blocked_username, blocked != 0) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_send_friend_request(mi_client_handle* handle,
                                  const char* target_username,
                                  const char* remark) {
  if (!handle || !target_username) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    const std::string remark_str = remark ? remark : "";
    return handle->core->SendFriendRequest(target_username, remark_str) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_respond_friend_request(mi_client_handle* handle,
                                     const char* requester_username,
                                     int accept) {
  if (!handle || !requester_username) {
    return 0;
  }
  if (!handle->core) {
    return 0;
  }
  try {
    return handle->core->RespondFriendRequest(requester_username,
                                              accept != 0)
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

std::uint32_t mi_client_list_friends(mi_client_handle* handle,
                                     mi_friend_entry_t* out_entries,
                                     std::uint32_t max_entries) {
  if (!handle || !handle->core || !out_entries || max_entries == 0) {
    return 0;
  }
  try {
    handle->friend_cache = handle->core->ListFriends();
    return FillFriendView(handle->friend_cache, handle->friend_view,
                          out_entries, max_entries);
  } catch (...) {
    return 0;
  }
}

std::uint32_t mi_client_sync_friends(mi_client_handle* handle,
                                     mi_friend_entry_t* out_entries,
                                     std::uint32_t max_entries,
                                     int* out_changed) {
  if (out_changed) {
    *out_changed = 0;
  }
  if (!handle || !handle->core || !out_entries || max_entries == 0) {
    return 0;
  }
  try {
    std::vector<mi::client::ClientCore::FriendEntry> synced;
    bool changed = false;
    if (!handle->core->SyncFriends(synced, changed)) {
      return 0;
    }
    if (out_changed) {
      *out_changed = changed ? 1 : 0;
    }
    if (changed) {
      handle->friend_cache = std::move(synced);
    } else {
      handle->friend_cache = handle->core->ListFriends();
    }
    return FillFriendView(handle->friend_cache, handle->friend_view,
                          out_entries, max_entries);
  } catch (...) {
    return 0;
  }
}

std::uint32_t mi_client_list_friend_requests(
    mi_client_handle* handle,
    mi_friend_request_entry_t* out_entries,
    std::uint32_t max_entries) {
  if (!handle || !handle->core || !out_entries || max_entries == 0) {
    return 0;
  }
  try {
    handle->friend_req_cache = handle->core->ListFriendRequests();
    return FillFriendRequestView(handle->friend_req_cache,
                                 handle->friend_req_view, out_entries,
                                 max_entries);
  } catch (...) {
    return 0;
  }
}

std::uint32_t mi_client_list_devices(mi_client_handle* handle,
                                     mi_device_entry_t* out_entries,
                                     std::uint32_t max_entries) {
  if (!handle || !handle->core || !out_entries || max_entries == 0) {
    return 0;
  }
  try {
    handle->device_cache = handle->core->ListDevices();
    return FillDeviceView(handle->device_cache, handle->device_view,
                          out_entries, max_entries);
  } catch (...) {
    return 0;
  }
}

int mi_client_kick_device(mi_client_handle* handle,
                          const char* device_id) {
  if (!handle || !handle->core || !device_id) {
    return 0;
  }
  try {
    return handle->core->KickDevice(device_id) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_join_group(mi_client_handle* handle, const char* group_id) {
  if (!handle || !handle->core || !group_id) {
    return 0;
  }
  try {
    return handle->core->JoinGroup(group_id) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_leave_group(mi_client_handle* handle, const char* group_id) {
  if (!handle || !handle->core || !group_id) {
    return 0;
  }
  try {
    return handle->core->LeaveGroup(group_id) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_create_group(mi_client_handle* handle, char** out_group_id) {
  if (out_group_id) {
    *out_group_id = nullptr;
  }
  if (!handle || !handle->core) {
    return 0;
  }
  try {
    std::string group_id;
    if (!handle->core->CreateGroup(group_id)) {
      return 0;
    }
    return CopyStringToC(group_id, out_group_id) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_send_group_invite(mi_client_handle* handle,
                                const char* group_id,
                                const char* peer_username,
                                char** out_message_id_hex) {
  if (out_message_id_hex) {
    *out_message_id_hex = nullptr;
  }
  if (!handle || !handle->core || !group_id || !peer_username) {
    return 0;
  }
  try {
    std::string message_id;
    if (!handle->core->SendGroupInvite(group_id, peer_username, message_id)) {
      return 0;
    }
    return CopyStringToC(message_id, out_message_id_hex) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

std::uint32_t mi_client_list_group_members_info(
    mi_client_handle* handle,
    const char* group_id,
    mi_group_member_entry_t* out_entries,
    std::uint32_t max_entries) {
  if (!handle || !handle->core || !group_id || !out_entries ||
      max_entries == 0) {
    return 0;
  }
  try {
    handle->group_member_cache = handle->core->ListGroupMembersInfo(group_id);
    return FillGroupMemberView(handle->group_member_cache,
                               handle->group_member_view, out_entries,
                               max_entries);
  } catch (...) {
    return 0;
  }
}

int mi_client_set_group_member_role(mi_client_handle* handle,
                                    const char* group_id,
                                    const char* peer_username,
                                    std::uint32_t role) {
  if (!handle || !handle->core || !group_id || !peer_username) {
    return 0;
  }
  mi::client::ClientCore::GroupMemberRole mapped;
  switch (role) {
    case 0:
      mapped = mi::client::ClientCore::GroupMemberRole::kOwner;
      break;
    case 1:
      mapped = mi::client::ClientCore::GroupMemberRole::kAdmin;
      break;
    case 2:
      mapped = mi::client::ClientCore::GroupMemberRole::kMember;
      break;
    default:
      return 0;
  }
  try {
    return handle->core->SetGroupMemberRole(group_id, peer_username, mapped)
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_kick_group_member(mi_client_handle* handle,
                                const char* group_id,
                                const char* peer_username) {
  if (!handle || !handle->core || !group_id || !peer_username) {
    return 0;
  }
  try {
    return handle->core->KickGroupMember(group_id, peer_username) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_start_group_call(mi_client_handle* handle,
                               const char* group_id,
                               int video,
                               std::uint8_t* out_call_id,
                               std::uint32_t out_call_id_len,
                               std::uint32_t* out_key_id) {
  if (out_call_id && out_call_id_len > 0) {
    std::memset(out_call_id, 0, out_call_id_len);
  }
  if (out_key_id) {
    *out_key_id = 0;
  }
  if (!handle || !handle->core || !group_id) {
    return 0;
  }
  if (out_call_id && out_call_id_len != 16) {
    return 0;
  }
  try {
    std::array<std::uint8_t, 16> call_id{};
    std::uint32_t key_id = 0;
    if (!handle->core->StartGroupCall(group_id, video != 0, call_id, key_id)) {
      return 0;
    }
    if (out_call_id) {
      std::memcpy(out_call_id, call_id.data(), call_id.size());
    }
    if (out_key_id) {
      *out_key_id = key_id;
    }
    return 1;
  } catch (...) {
    return 0;
  }
}

int mi_client_join_group_call(mi_client_handle* handle,
                              const char* group_id,
                              const std::uint8_t* call_id,
                              std::uint32_t call_id_len,
                              int video,
                              std::uint32_t* out_key_id) {
  if (out_key_id) {
    *out_key_id = 0;
  }
  if (!handle || !handle->core || !group_id || !call_id) {
    return 0;
  }
  std::array<std::uint8_t, 16> id{};
  if (!ParseCallId(call_id, call_id_len, id)) {
    return 0;
  }
  try {
    if (out_key_id) {
      return handle->core->JoinGroupCall(group_id, id, video != 0, *out_key_id)
                 ? 1
                 : 0;
    }
    return handle->core->JoinGroupCall(group_id, id, video != 0) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_leave_group_call(mi_client_handle* handle,
                               const char* group_id,
                               const std::uint8_t* call_id,
                               std::uint32_t call_id_len) {
  if (!handle || !handle->core || !group_id || !call_id) {
    return 0;
  }
  std::array<std::uint8_t, 16> id{};
  if (!ParseCallId(call_id, call_id_len, id)) {
    return 0;
  }
  try {
    return handle->core->LeaveGroupCall(group_id, id) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_get_group_call_key(mi_client_handle* handle,
                                 const char* group_id,
                                 const std::uint8_t* call_id,
                                 std::uint32_t call_id_len,
                                 std::uint32_t key_id,
                                 std::uint8_t* out_key,
                                 std::uint32_t out_key_len) {
  if (out_key && out_key_len > 0) {
    std::memset(out_key, 0, out_key_len);
  }
  if (!handle || !handle->core || !group_id || !call_id || !out_key ||
      out_key_len != 32) {
    return 0;
  }
  std::array<std::uint8_t, 16> id{};
  if (!ParseCallId(call_id, call_id_len, id)) {
    return 0;
  }
  try {
    std::array<std::uint8_t, 32> key{};
    if (!handle->core->GetGroupCallKey(group_id, id, key_id, key)) {
      return 0;
    }
    std::memcpy(out_key, key.data(), key.size());
    return 1;
  } catch (...) {
    return 0;
  }
}

int mi_client_rotate_group_call_key(mi_client_handle* handle,
                                    const char* group_id,
                                    const std::uint8_t* call_id,
                                    std::uint32_t call_id_len,
                                    std::uint32_t key_id,
                                    const char** members,
                                    std::uint32_t member_count) {
  if (!handle || !handle->core || !group_id || !call_id) {
    return 0;
  }
  std::array<std::uint8_t, 16> id{};
  if (!ParseCallId(call_id, call_id_len, id)) {
    return 0;
  }
  try {
    const std::vector<std::string> list =
        BuildMemberList(members, member_count);
    return handle->core->RotateGroupCallKey(group_id, id, key_id, list) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_request_group_call_key(mi_client_handle* handle,
                                     const char* group_id,
                                     const std::uint8_t* call_id,
                                     std::uint32_t call_id_len,
                                     std::uint32_t key_id,
                                     const char** members,
                                     std::uint32_t member_count) {
  if (!handle || !handle->core || !group_id || !call_id) {
    return 0;
  }
  std::array<std::uint8_t, 16> id{};
  if (!ParseCallId(call_id, call_id_len, id)) {
    return 0;
  }
  try {
    const std::vector<std::string> list =
        BuildMemberList(members, member_count);
    return handle->core->RequestGroupCallKey(group_id, id, key_id, list) ? 1
                                                                         : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_send_group_call_signal(mi_client_handle* handle,
                                     std::uint8_t op,
                                     const char* group_id,
                                     const std::uint8_t* call_id,
                                     std::uint32_t call_id_len,
                                     int video,
                                     std::uint32_t key_id,
                                     std::uint32_t seq,
                                     std::uint64_t ts_ms,
                                     const std::uint8_t* ext,
                                     std::uint32_t ext_len,
                                     std::uint8_t* out_call_id,
                                     std::uint32_t out_call_id_len,
                                     std::uint32_t* out_key_id,
                                     mi_group_call_member_t* out_members,
                                     std::uint32_t max_members,
                                     std::uint32_t* out_member_count) {
  if (out_call_id && out_call_id_len > 0) {
    std::memset(out_call_id, 0, out_call_id_len);
  }
  if (out_key_id) {
    *out_key_id = 0;
  }
  if (out_member_count) {
    *out_member_count = 0;
  }
  if (!handle || !handle->core || !group_id) {
    return 0;
  }
  if (out_call_id && out_call_id_len != 16) {
    return 0;
  }
  std::array<std::uint8_t, 16> id{};
  if (call_id && call_id_len > 0) {
    if (!ParseCallId(call_id, call_id_len, id)) {
      return 0;
    }
  }
  try {
    std::vector<std::uint8_t> ext_vec;
    if (ext && ext_len > 0) {
      ext_vec.assign(ext, ext + ext_len);
    }
    const auto resp = handle->core->SendGroupCallSignal(
        op, group_id, id, video != 0, key_id, seq, ts_ms, ext_vec);
    if (!resp.success) {
      return 0;
    }
    if (out_call_id) {
      std::memcpy(out_call_id, resp.call_id.data(), resp.call_id.size());
    }
    if (out_key_id) {
      *out_key_id = resp.key_id;
    }
    handle->group_call_member_cache = resp.members;
    const std::uint32_t available =
        static_cast<std::uint32_t>(resp.members.size());
    if (out_member_count) {
      *out_member_count = out_members
                              ? std::min<std::uint32_t>(available, max_members)
                              : available;
    }
    if (out_members && max_members > 0) {
      FillGroupCallMemberView(handle->group_call_member_cache,
                              handle->group_call_member_view, out_members,
                              max_members);
    }
    return 1;
  } catch (...) {
    return 0;
  }
}

std::uint32_t mi_client_load_chat_history(mi_client_handle* handle,
                                          const char* conv_id,
                                          int is_group,
                                          std::uint32_t limit,
                                          mi_history_entry_t* out_entries,
                                          std::uint32_t max_entries) {
  if (!handle || !handle->core || !conv_id || !out_entries || max_entries == 0) {
    return 0;
  }
  try {
    const std::size_t cap =
        limit == 0 ? static_cast<std::size_t>(max_entries) : limit;
    handle->history_cache =
        handle->core->LoadChatHistory(conv_id, is_group != 0, cap);
    return FillHistoryView(handle->history_cache, handle->history_view,
                           out_entries, max_entries);
  } catch (...) {
    return 0;
  }
}

int mi_client_delete_chat_history(mi_client_handle* handle,
                                  const char* conv_id,
                                  int is_group,
                                  int delete_attachments,
                                  int secure_wipe) {
  if (!handle || !handle->core || !conv_id) {
    return 0;
  }
  try {
    return handle->core->DeleteChatHistory(conv_id, is_group != 0,
                                           delete_attachments != 0,
                                           secure_wipe != 0)
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_set_history_enabled(mi_client_handle* handle,
                                  int enabled) {
  if (!handle || !handle->core) {
    return 0;
  }
  try {
    handle->core->SetHistoryEnabled(enabled != 0);
    return 1;
  } catch (...) {
    return 0;
  }
}

int mi_client_clear_all_history(mi_client_handle* handle,
                                int delete_attachments,
                                int secure_wipe) {
  if (!handle || !handle->core) {
    return 0;
  }
  try {
    std::string err;
    return handle->core->ClearAllHistory(delete_attachments != 0,
                                         secure_wipe != 0, err)
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_begin_device_pairing_primary(mi_client_handle* handle,
                                           char** out_pairing_code) {
  if (out_pairing_code) {
    *out_pairing_code = nullptr;
  }
  if (!handle || !handle->core) {
    return 0;
  }
  try {
    std::string code;
    if (!handle->core->BeginDevicePairingPrimary(code)) {
      return 0;
    }
    return CopyStringToC(code, out_pairing_code) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

std::uint32_t mi_client_poll_device_pairing_requests(
    mi_client_handle* handle,
    mi_device_pairing_request_t* out_entries,
    std::uint32_t max_entries) {
  if (!handle || !handle->core || !out_entries || max_entries == 0) {
    return 0;
  }
  try {
    handle->pairing_cache = handle->core->PollDevicePairingRequests();
    return FillDevicePairingView(handle->pairing_cache,
                                 handle->pairing_view, out_entries,
                                 max_entries);
  } catch (...) {
    return 0;
  }
}

int mi_client_approve_device_pairing_request(mi_client_handle* handle,
                                             const char* device_id,
                                             const char* request_id_hex) {
  if (!handle || !handle->core || !device_id || !request_id_hex) {
    return 0;
  }
  try {
    mi::client::ClientCore::DevicePairingRequest req;
    req.device_id = device_id;
    req.request_id_hex = request_id_hex;
    return handle->core->ApproveDevicePairingRequest(req) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_begin_device_pairing_linked(mi_client_handle* handle,
                                          const char* pairing_code) {
  if (!handle || !handle->core || !pairing_code) {
    return 0;
  }
  try {
    return handle->core->BeginDevicePairingLinked(pairing_code) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_poll_device_pairing_linked(mi_client_handle* handle,
                                         int* out_completed) {
  if (!handle || !handle->core || !out_completed) {
    return 0;
  }
  try {
    bool completed = false;
    const bool ok = handle->core->PollDevicePairingLinked(completed);
    *out_completed = completed ? 1 : 0;
    return ok ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

void mi_client_cancel_device_pairing(mi_client_handle* handle) {
  if (!handle || !handle->core) {
    return;
  }
  handle->core->CancelDevicePairing();
}

int mi_client_store_attachment_preview_bytes(mi_client_handle* handle,
                                             const char* file_id,
                                             const char* file_name,
                                             std::uint64_t file_size,
                                             const std::uint8_t* bytes,
                                             std::uint32_t bytes_len) {
  if (!handle || !handle->core || !file_id || !bytes || bytes_len == 0) {
    return 0;
  }
  try {
    std::vector<std::uint8_t> data(bytes, bytes + bytes_len);
    handle->core->StoreAttachmentPreviewBytes(
        file_id, file_name ? file_name : "", file_size, data);
    return 1;
  } catch (...) {
    return 0;
  }
}

int mi_client_download_chat_file_to_path(mi_client_handle* handle,
                                         const char* file_id,
                                         const std::uint8_t* file_key,
                                         std::uint32_t file_key_len,
                                         const char* file_name,
                                         std::uint64_t file_size,
                                         const char* out_path_utf8,
                                         int wipe_after_read,
                                         mi_progress_callback_t on_progress,
                                         void* user_data) {
  if (!handle || !handle->core || !file_id || !file_key || !out_path_utf8) {
    return 0;
  }
  try {
    mi::client::ClientCore::ChatFileMessage file;
    if (!BuildChatFileMessage(file_id, file_key, file_key_len, file_name,
                              file_size, file)) {
      return 0;
    }
    std::function<void(std::uint64_t, std::uint64_t)> cb;
    if (on_progress) {
      cb = [on_progress, user_data](std::uint64_t done,
                                    std::uint64_t total) {
        on_progress(done, total, user_data);
      };
    }
    return handle->core->DownloadChatFileToPath(
               file, PathFromUtf8(out_path_utf8), wipe_after_read != 0, cb)
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

int mi_client_download_chat_file_to_bytes(mi_client_handle* handle,
                                          const char* file_id,
                                          const std::uint8_t* file_key,
                                          std::uint32_t file_key_len,
                                          const char* file_name,
                                          std::uint64_t file_size,
                                          int wipe_after_read,
                                          std::uint8_t** out_bytes,
                                          std::uint64_t* out_len) {
  if (out_bytes) {
    *out_bytes = nullptr;
  }
  if (out_len) {
    *out_len = 0;
  }
  if (!handle || !handle->core || !file_id || !file_key || !out_bytes ||
      !out_len) {
    return 0;
  }
  try {
    mi::client::ClientCore::ChatFileMessage file;
    if (!BuildChatFileMessage(file_id, file_key, file_key_len, file_name,
                              file_size, file)) {
      return 0;
    }
    std::vector<std::uint8_t> plain;
    if (!handle->core->DownloadChatFileToBytes(file, plain,
                                               wipe_after_read != 0)) {
      return 0;
    }
    if (plain.empty()) {
      return 1;
    }
    std::uint8_t* buf =
        static_cast<std::uint8_t*>(std::malloc(plain.size()));
    if (!buf) {
      return 0;
    }
    std::memcpy(buf, plain.data(), plain.size());
    *out_bytes = buf;
    *out_len = static_cast<std::uint64_t>(plain.size());
    return 1;
  } catch (...) {
    return 0;
  }
}

int mi_client_get_media_config(mi_client_handle* handle,
                               mi_media_config_t* out_config) {
  if (out_config) {
    std::memset(out_config, 0, sizeof(mi_media_config_t));
  }
  if (!handle || !handle->core || !out_config) {
    return 0;
  }
  try {
    const auto& cfg = handle->core->media_config();
    out_config->audio_delay_ms = cfg.audio_delay_ms;
    out_config->video_delay_ms = cfg.video_delay_ms;
    out_config->audio_max_frames = cfg.audio_max_frames;
    out_config->video_max_frames = cfg.video_max_frames;
    out_config->pull_max_packets = cfg.pull_max_packets;
    out_config->pull_wait_ms = cfg.pull_wait_ms;
    out_config->group_pull_max_packets = cfg.group_pull_max_packets;
    out_config->group_pull_wait_ms = cfg.group_pull_wait_ms;
    handle->core->SetLastError("");
    return 1;
  } catch (...) {
    handle->core->SetLastError("media config unavailable");
    return 0;
  }
}

int mi_client_derive_media_root(mi_client_handle* handle,
                                const char* peer_username,
                                const std::uint8_t* call_id,
                                std::uint32_t call_id_len,
                                std::uint8_t* out_media_root,
                                std::uint32_t out_media_root_len) {
  if (out_media_root && out_media_root_len > 0) {
    std::memset(out_media_root, 0, out_media_root_len);
  }
  if (!handle || !handle->core || !peer_username || !out_media_root ||
      out_media_root_len != 32) {
    return 0;
  }
  std::array<std::uint8_t, 16> id{};
  if (!ParseCallId(call_id, call_id_len, id)) {
    handle->core->SetLastError("call id invalid");
    return 0;
  }
  try {
    std::array<std::uint8_t, 32> media_root{};
    std::string err;
    if (!handle->core->DeriveMediaRoot(peer_username, id, media_root, err)) {
      handle->core->SetLastError(
          err.empty() ? "media root derive failed" : err);
      return 0;
    }
    std::memcpy(out_media_root, media_root.data(), media_root.size());
    handle->core->SetLastError("");
    return 1;
  } catch (...) {
    handle->core->SetLastError("media root derive failed");
    return 0;
  }
}

int mi_client_push_media(mi_client_handle* handle,
                         const char* peer_username,
                         const std::uint8_t* call_id,
                         std::uint32_t call_id_len,
                         const std::uint8_t* packet,
                         std::uint32_t packet_len) {
  if (!handle || !handle->core || !peer_username || !call_id || !packet ||
      packet_len == 0) {
    return 0;
  }
  std::array<std::uint8_t, 16> id{};
  if (!ParseCallId(call_id, call_id_len, id)) {
    return 0;
  }
  try {
    std::vector<std::uint8_t> buf(packet, packet + packet_len);
    return handle->core->PushMedia(peer_username, id, buf) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

std::uint32_t mi_client_pull_media(mi_client_handle* handle,
                                   const std::uint8_t* call_id,
                                   std::uint32_t call_id_len,
                                   std::uint32_t max_packets,
                                   std::uint32_t wait_ms,
                                   mi_media_packet_t* out_packets) {
  if (!handle || !handle->core || !call_id || !out_packets || max_packets == 0) {
    return 0;
  }
  std::array<std::uint8_t, 16> id{};
  if (!ParseCallId(call_id, call_id_len, id)) {
    return 0;
  }
  try {
    handle->media_packet_cache =
        handle->core->PullMedia(id, max_packets, wait_ms);
    return FillMediaPacketView(handle->media_packet_cache,
                               handle->media_packet_view, out_packets,
                               max_packets);
  } catch (...) {
    return 0;
  }
}

int mi_client_push_group_media(mi_client_handle* handle,
                               const char* group_id,
                               const std::uint8_t* call_id,
                               std::uint32_t call_id_len,
                               const std::uint8_t* packet,
                               std::uint32_t packet_len) {
  if (!handle || !handle->core || !group_id || !call_id || !packet ||
      packet_len == 0) {
    return 0;
  }
  std::array<std::uint8_t, 16> id{};
  if (!ParseCallId(call_id, call_id_len, id)) {
    return 0;
  }
  try {
    std::vector<std::uint8_t> buf(packet, packet + packet_len);
    return handle->core->PushGroupMedia(group_id, id, buf) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

std::uint32_t mi_client_pull_group_media(mi_client_handle* handle,
                                         const std::uint8_t* call_id,
                                         std::uint32_t call_id_len,
                                         std::uint32_t max_packets,
                                         std::uint32_t wait_ms,
                                         mi_media_packet_t* out_packets) {
  if (!handle || !handle->core || !call_id || !out_packets || max_packets == 0) {
    return 0;
  }
  std::array<std::uint8_t, 16> id{};
  if (!ParseCallId(call_id, call_id_len, id)) {
    return 0;
  }
  try {
    handle->group_media_packet_cache =
        handle->core->PullGroupMedia(id, max_packets, wait_ms);
    return FillMediaPacketView(handle->group_media_packet_cache,
                               handle->group_media_packet_view, out_packets,
                               max_packets);
  } catch (...) {
    return 0;
  }
}

int mi_client_add_media_subscription(mi_client_handle* handle,
                                     const std::uint8_t* call_id,
                                     std::uint32_t call_id_len,
                                     int is_group,
                                     const char* group_id) {
  if (!handle) {
    return 0;
  }
  std::array<std::uint8_t, 16> id{};
  if (!ParseCallId(call_id, call_id_len, id)) {
    return 0;
  }
  const bool group = is_group != 0;
  if (auto* existing = FindMediaSubscription(handle, id, group)) {
    if (group && group_id && *group_id != '\0') {
      existing->group_id = group_id;
    }
    return 1;
  }
  MediaSubscription sub;
  sub.call_id = id;
  sub.is_group = group;
  if (group && group_id && *group_id != '\0') {
    sub.group_id = group_id;
  }
  handle->media_subs.push_back(std::move(sub));
  return 1;
}

void mi_client_clear_media_subscriptions(mi_client_handle* handle) {
  if (!handle) {
    return;
  }
  handle->media_subs.clear();
}

void mi_client_free(void* buf) {
  std::free(buf);
}

std::uint32_t mi_client_poll_event(mi_client_handle* handle,
                                   mi_event_t* out_events,
                                   std::uint32_t max_events,
                                   std::uint32_t wait_ms) {
  if (!handle || !handle->core || !out_events || max_events == 0) {
    return 0;
  }
  if (max_events > 256) {
    max_events = 256;
  }
  try {
    handle->last_events.clear();
    if (handle->pending.empty()) {
      const std::uint64_t deadline = mi::platform::NowSteadyMs() + wait_ms;
      while (handle->pending.empty()) {
        AppendChatEvents(handle);
        AppendOfflineEvents(handle);
        AppendMediaEvents(handle, 0);
        AppendGroupCallEvents(handle, 0);
        if (!handle->pending.empty() || wait_ms == 0) {
          break;
        }
        std::uint64_t now = mi::platform::NowSteadyMs();
        if (now >= deadline) {
          break;
        }
        std::uint32_t remaining =
            static_cast<std::uint32_t>(deadline - now);
        const std::uint32_t group_wait = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(remaining, 1000ULL));
        AppendGroupCallEvents(handle, group_wait);
        now = mi::platform::NowSteadyMs();
        if (now >= deadline) {
          break;
        }
        remaining = static_cast<std::uint32_t>(deadline - now);
        AppendMediaEvents(handle, remaining);
        if (!handle->pending.empty()) {
          break;
        }
        if (mi::platform::NowSteadyMs() >= deadline) {
          break;
        }
      }
    }

    const std::uint32_t count = std::min<std::uint32_t>(
        max_events, static_cast<std::uint32_t>(handle->pending.size()));
    handle->last_events.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
      handle->last_events.push_back(std::move(handle->pending.front()));
      handle->pending.pop_front();
    }
    for (std::uint32_t i = 0; i < count; ++i) {
      FillEventView(handle->last_events[i], out_events[i]);
    }
    return count;
  } catch (...) {
    return 0;
  }
}

}  // extern "C"

mi_client_handle* mi_client_wrap_cpp(mi::client::ClientCore* core) {
  if (!core) {
    return nullptr;
  }
  auto* handle = new (std::nothrow) mi_client_handle();
  if (!handle) {
    return nullptr;
  }
  handle->core = core;
  handle->owns_core = false;
  return handle;
}

void mi_client_unwrap_cpp(mi_client_handle* handle) {
  mi_client_destroy(handle);
}
