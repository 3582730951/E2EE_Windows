#include "mi_client_ios_bridge.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct StubFriend {
  std::string username;
  std::string remark;
};

struct StubDevice {
  std::string deviceID;
  std::string displayID;
  std::uint32_t lastSeenSec;
};

struct StubMessage {
  std::string id;
  std::string conversationID;
  std::string sender;
  std::string text;
  bool isGroup = false;
  bool outgoing = false;
  std::uint64_t timestampMS = 0;
};

struct StubGroup {
  std::string id;
  std::vector<std::string> members;
};

struct mi_client_handle {
  std::string configPath;
  std::string lastError;
  std::string token;
  std::string deviceID = "ios-sim-01";
  std::string deviceDisplayID = "Primary iPhone";
  std::string remoteError;
  std::string username;
  std::string rootAuthKey;
  bool remoteOK = false;
  bool seeded = false;
  std::uint32_t nextMessage = 1;
  std::uint32_t nextGroup = 1;
  std::vector<StubFriend> friends;
  std::vector<StubDevice> devices;
  std::unordered_map<std::string, std::vector<StubMessage>> history;
  std::unordered_map<std::string, StubGroup> groups;
  std::deque<StubMessage> pendingEvents;
  std::vector<StubMessage> lastPolledEvents;
};

namespace {

std::string gLastCreateError;

std::uint64_t NowMS() {
  using namespace std::chrono;
  return static_cast<std::uint64_t>(
      duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

std::uint32_t NowSec() {
  return static_cast<std::uint32_t>(NowMS() / 1000ULL);
}

const char* CStringOrNull(const std::string& value) {
  return value.empty() ? nullptr : value.c_str();
}

char* CopyCString(const std::string& value) {
  char* copy = static_cast<char*>(std::malloc(value.size() + 1));
  if (!copy) {
    return nullptr;
  }
  std::memcpy(copy, value.c_str(), value.size() + 1);
  return copy;
}

void SetError(mi_client_handle* handle, const std::string& error) {
  if (handle) {
    handle->lastError = error;
  }
}

bool EnsureHandle(mi_client_handle* handle) {
  return handle != nullptr;
}

bool EnsureAuthenticated(mi_client_handle* handle) {
  if (!handle) {
    return false;
  }
  if (!handle->token.empty()) {
    return true;
  }
  SetError(handle, "Sign in to continue.");
  return false;
}

std::string NextMessageID(mi_client_handle* handle) {
  return "ios-msg-" + std::to_string(handle->nextMessage++);
}

std::string NextGroupID(mi_client_handle* handle) {
  return "ops-room-" + std::to_string(handle->nextGroup++);
}

void AppendHistory(mi_client_handle* handle,
                   const std::string& conversationID,
                   const std::string& sender,
                   const std::string& text,
                   bool isGroup,
                   bool outgoing,
                   std::uint64_t timestampMS,
                   const std::string& messageID = std::string()) {
  if (!handle) {
    return;
  }
  StubMessage message;
  message.id = messageID.empty() ? NextMessageID(handle) : messageID;
  message.conversationID = conversationID;
  message.sender = sender;
  message.text = text;
  message.isGroup = isGroup;
  message.outgoing = outgoing;
  message.timestampMS = timestampMS;
  handle->history[conversationID].push_back(std::move(message));
}

void QueueEvent(mi_client_handle* handle,
                const std::string& conversationID,
                const std::string& sender,
                const std::string& text,
                bool isGroup,
                bool outgoing) {
  if (!handle) {
    return;
  }
  StubMessage event;
  event.id = NextMessageID(handle);
  event.conversationID = conversationID;
  event.sender = sender;
  event.text = text;
  event.isGroup = isGroup;
  event.outgoing = outgoing;
  event.timestampMS = NowMS();
  handle->pendingEvents.push_back(std::move(event));
}

void SeedDemoData(mi_client_handle* handle) {
  if (!handle || handle->seeded) {
    return;
  }

  const std::uint64_t now = NowMS();
  handle->friends = {
      {"alice", "Alice Chen"},
      {"bob", "Bob Rivera"},
      {"nina", "Nina Park"},
  };

  handle->devices = {
      {"ios-sim-01", "Primary iPhone", NowSec()},
      {"mac-linked-02", "Mac Companion", NowSec() - 240},
  };

  handle->groups["ops-room"] = StubGroup{
      "ops-room",
      {handle->username.empty() ? "owner" : handle->username, "maya", "nina"}};

  AppendHistory(handle,
                "alice",
                "alice",
                "Morning. The secure shell looks much better now.",
                false,
                false,
                now - 22ULL * 60ULL * 1000ULL);
  AppendHistory(handle,
                "alice",
                handle->username.empty() ? "me" : handle->username,
                "Windows, Android, and iOS are finally aligned.",
                false,
                true,
                now - 18ULL * 60ULL * 1000ULL);
  AppendHistory(handle,
                "alice",
                "alice",
                "Keep the input bar dense and readable like Telegram.",
                false,
                false,
                now - 12ULL * 60ULL * 1000ULL);

  AppendHistory(handle,
                "bob",
                "bob",
                "Desktop smoke passed. Packaging is running.",
                false,
                false,
                now - 16ULL * 60ULL * 1000ULL);
  AppendHistory(handle,
                "bob",
                handle->username.empty() ? "me" : handle->username,
                "Good. I am waiting on the iOS linker fix.",
                false,
                true,
                now - 14ULL * 60ULL * 1000ULL);

  AppendHistory(handle,
                "ops-room",
                "maya",
                "Security Center copy is ready for review.",
                true,
                false,
                now - 10ULL * 60ULL * 1000ULL);
  AppendHistory(handle,
                "ops-room",
                handle->username.empty() ? "me" : handle->username,
                "Ship the acceptance bundle after screenshots.",
                true,
                true,
                now - 7ULL * 60ULL * 1000ULL);

  QueueEvent(handle,
             "alice",
             "alice",
             "Android emulator screenshots are queued.",
             false,
             false);
  QueueEvent(handle,
             "ops-room",
             "maya",
             "Windows smoke artifacts uploaded.",
             true,
             false);

  handle->seeded = true;
}

bool Authenticate(mi_client_handle* handle,
                  const char* username,
                  const char* password,
                  const char* rootCode) {
  if (!EnsureHandle(handle)) {
    return false;
  }
  const std::string user = username ? username : "";
  const std::string pass = password ? password : "";
  if (user.empty() || pass.empty()) {
    SetError(handle, "Username and password are required.");
    return false;
  }

  handle->username = user;
  handle->token = "stub-token-" + user;
  handle->remoteOK = true;
  handle->remoteError.clear();
  handle->lastError.clear();
  if (rootCode && rootCode[0] != '\0') {
    handle->rootAuthKey = rootCode;
  }
  SeedDemoData(handle);
  return true;
}

StubGroup* FindGroup(mi_client_handle* handle, const char* groupID) {
  if (!handle || !groupID || groupID[0] == '\0') {
    return nullptr;
  }
  auto it = handle->groups.find(groupID);
  if (it == handle->groups.end()) {
    return nullptr;
  }
  return &it->second;
}

}  // namespace

extern "C" {

MI_E2EE_SDK_API mi_client_handle* mi_client_create(const char* config_path) {
  gLastCreateError.clear();
  mi_client_handle* handle = new mi_client_handle();
  handle->configPath = (config_path && config_path[0] != '\0')
      ? config_path
      : "config/client_config.ini";
  return handle;
}

MI_E2EE_SDK_API const char* mi_client_last_create_error(void) {
  return gLastCreateError.c_str();
}

MI_E2EE_SDK_API void mi_client_destroy(mi_client_handle* handle) {
  delete handle;
}

MI_E2EE_SDK_API const char* mi_client_last_error(mi_client_handle* handle) {
  if (!handle) {
    return "";
  }
  return handle->lastError.c_str();
}

MI_E2EE_SDK_API const char* mi_client_token(mi_client_handle* handle) {
  return handle ? handle->token.c_str() : "";
}

MI_E2EE_SDK_API const char* mi_client_device_id(mi_client_handle* handle) {
  return handle ? handle->deviceID.c_str() : "";
}

MI_E2EE_SDK_API const char* mi_client_device_display_id(mi_client_handle* handle) {
  return handle ? handle->deviceDisplayID.c_str() : "";
}

MI_E2EE_SDK_API int mi_client_remote_ok(mi_client_handle* handle) {
  return (handle && handle->remoteOK) ? 1 : 0;
}

MI_E2EE_SDK_API const char* mi_client_remote_error(mi_client_handle* handle) {
  return handle ? handle->remoteError.c_str() : "";
}

MI_E2EE_SDK_API int mi_client_register(mi_client_handle* handle,
                                       const char* username,
                                       const char* password) {
  return Authenticate(handle, username, password, nullptr) ? 1 : 0;
}

MI_E2EE_SDK_API int mi_client_login(mi_client_handle* handle,
                                    const char* username,
                                    const char* password) {
  return Authenticate(handle, username, password, nullptr) ? 1 : 0;
}

MI_E2EE_SDK_API int mi_client_login_with_root_code(mi_client_handle* handle,
                                                   const char* username,
                                                   const char* password,
                                                   const char* root_code) {
  return Authenticate(handle, username, password, root_code) ? 1 : 0;
}

MI_E2EE_SDK_API int mi_client_root_auth_init(mi_client_handle* handle,
                                             const char* pubkey_hex) {
  if (!EnsureHandle(handle)) {
    return 0;
  }
  handle->rootAuthKey = pubkey_hex ? pubkey_hex : "";
  handle->lastError.clear();
  return 1;
}

MI_E2EE_SDK_API int mi_client_publish_prekeys(mi_client_handle* handle) {
  if (!EnsureAuthenticated(handle)) {
    return 0;
  }
  handle->lastError.clear();
  return 1;
}

MI_E2EE_SDK_API int mi_client_heartbeat(mi_client_handle* handle) {
  if (!EnsureAuthenticated(handle)) {
    return 0;
  }
  handle->lastError.clear();
  return 1;
}

MI_E2EE_SDK_API int mi_client_send_private_text(mi_client_handle* handle,
                                                const char* peer_username,
                                                const char* text_utf8,
                                                char** out_message_id_hex) {
  if (!EnsureAuthenticated(handle) || !peer_username || !text_utf8 || text_utf8[0] == '\0') {
    return 0;
  }

  const std::string conversationID = peer_username;
  const std::string text = text_utf8;
  const std::string messageID = NextMessageID(handle);
  AppendHistory(handle,
                conversationID,
                handle->username,
                text,
                false,
                true,
                NowMS(),
                messageID);
  QueueEvent(handle,
             conversationID,
             conversationID,
             "Acknowledged. The mobile shell looks consistent now.",
             false,
             false);
  if (out_message_id_hex) {
    *out_message_id_hex = CopyCString(messageID);
  }
  handle->lastError.clear();
  return 1;
}

MI_E2EE_SDK_API int mi_client_send_group_text(mi_client_handle* handle,
                                              const char* group_id,
                                              const char* text_utf8,
                                              char** out_message_id_hex) {
  if (!EnsureAuthenticated(handle) || !group_id || !text_utf8 || text_utf8[0] == '\0') {
    return 0;
  }
  StubGroup* group = FindGroup(handle, group_id);
  if (!group) {
    SetError(handle, "Unknown group.");
    return 0;
  }

  const std::string messageID = NextMessageID(handle);
  AppendHistory(handle,
                group->id,
                handle->username,
                text_utf8,
                true,
                true,
                NowMS(),
                messageID);
  QueueEvent(handle,
             group->id,
             group->members.size() > 1 ? group->members[1] : "maya",
             "Accepted. I will keep the screenshots attached to the release.",
             true,
             false);
  if (out_message_id_hex) {
    *out_message_id_hex = CopyCString(messageID);
  }
  handle->lastError.clear();
  return 1;
}

MI_E2EE_SDK_API std::uint32_t mi_client_list_friends(mi_client_handle* handle,
                                                     mi_friend_entry_t* out_entries,
                                                     std::uint32_t max_entries) {
  if (!EnsureAuthenticated(handle) || !out_entries || max_entries == 0) {
    return 0;
  }
  const std::uint32_t count =
      std::min<std::uint32_t>(static_cast<std::uint32_t>(handle->friends.size()), max_entries);
  for (std::uint32_t index = 0; index < count; ++index) {
    out_entries[index].username = CStringOrNull(handle->friends[index].username);
    out_entries[index].remark = CStringOrNull(handle->friends[index].remark);
  }
  return count;
}

MI_E2EE_SDK_API std::uint32_t mi_client_list_devices(mi_client_handle* handle,
                                                     mi_device_entry_t* out_entries,
                                                     std::uint32_t max_entries) {
  if (!EnsureAuthenticated(handle) || !out_entries || max_entries == 0) {
    return 0;
  }
  const std::uint32_t count =
      std::min<std::uint32_t>(static_cast<std::uint32_t>(handle->devices.size()), max_entries);
  for (std::uint32_t index = 0; index < count; ++index) {
    out_entries[index].device_id = CStringOrNull(handle->devices[index].deviceID);
    out_entries[index].display_id = CStringOrNull(handle->devices[index].displayID);
    out_entries[index].last_seen_sec = handle->devices[index].lastSeenSec;
  }
  return count;
}

MI_E2EE_SDK_API int mi_client_join_group(mi_client_handle* handle, const char* group_id) {
  if (!EnsureAuthenticated(handle) || !group_id || group_id[0] == '\0') {
    return 0;
  }
  auto& group = handle->groups[group_id];
  group.id = group_id;
  if (std::find(group.members.begin(), group.members.end(), handle->username) == group.members.end()) {
    group.members.push_back(handle->username);
  }
  handle->lastError.clear();
  return 1;
}

MI_E2EE_SDK_API int mi_client_leave_group(mi_client_handle* handle, const char* group_id) {
  if (!EnsureAuthenticated(handle) || !group_id || group_id[0] == '\0') {
    return 0;
  }
  StubGroup* group = FindGroup(handle, group_id);
  if (!group) {
    SetError(handle, "Unknown group.");
    return 0;
  }
  group->members.erase(std::remove(group->members.begin(), group->members.end(), handle->username),
                       group->members.end());
  handle->lastError.clear();
  return 1;
}

MI_E2EE_SDK_API int mi_client_create_group(mi_client_handle* handle, char** out_group_id) {
  if (!EnsureAuthenticated(handle)) {
    return 0;
  }
  const std::string groupID = NextGroupID(handle);
  handle->groups[groupID] = StubGroup{groupID, {handle->username, "nina", "maya"}};
  AppendHistory(handle,
                groupID,
                "maya",
                "New secure group created for release coordination.",
                true,
                false,
                NowMS());
  if (out_group_id) {
    *out_group_id = CopyCString(groupID);
  }
  handle->lastError.clear();
  return 1;
}

MI_E2EE_SDK_API int mi_client_send_group_invite(mi_client_handle* handle,
                                                const char* group_id,
                                                const char* peer_username,
                                                char** out_message_id_hex) {
  if (!EnsureAuthenticated(handle) || !group_id || !peer_username || peer_username[0] == '\0') {
    return 0;
  }
  StubGroup* group = FindGroup(handle, group_id);
  if (!group) {
    SetError(handle, "Unknown group.");
    return 0;
  }
  if (std::find(group->members.begin(), group->members.end(), peer_username) == group->members.end()) {
    group->members.push_back(peer_username);
  }
  const std::string messageID = NextMessageID(handle);
  if (out_message_id_hex) {
    *out_message_id_hex = CopyCString(messageID);
  }
  handle->lastError.clear();
  return 1;
}

MI_E2EE_SDK_API std::uint32_t mi_client_list_group_members_info(
    mi_client_handle* handle,
    const char* group_id,
    mi_group_member_entry_t* out_entries,
    std::uint32_t max_entries) {
  if (!EnsureAuthenticated(handle) || !out_entries || max_entries == 0) {
    return 0;
  }
  StubGroup* group = FindGroup(handle, group_id);
  if (!group) {
    return 0;
  }
  const std::uint32_t count =
      std::min<std::uint32_t>(static_cast<std::uint32_t>(group->members.size()), max_entries);
  for (std::uint32_t index = 0; index < count; ++index) {
    out_entries[index].username = CStringOrNull(group->members[index]);
    out_entries[index].role = (group->members[index] == handle->username) ? 2u : 1u;
  }
  return count;
}

MI_E2EE_SDK_API std::uint32_t mi_client_load_chat_history(mi_client_handle* handle,
                                                          const char* conv_id,
                                                          int is_group,
                                                          std::uint32_t limit,
                                                          mi_history_entry_t* out_entries,
                                                          std::uint32_t max_entries) {
  (void)is_group;
  if (!EnsureAuthenticated(handle) || !conv_id || !out_entries || max_entries == 0 || limit == 0) {
    return 0;
  }
  auto it = handle->history.find(conv_id);
  if (it == handle->history.end()) {
    return 0;
  }

  const auto& messages = it->second;
  const std::uint32_t available = static_cast<std::uint32_t>(messages.size());
  const std::uint32_t count = std::min<std::uint32_t>(available, std::min(limit, max_entries));
  const std::uint32_t start = available > count ? (available - count) : 0;
  for (std::uint32_t index = 0; index < count; ++index) {
    const StubMessage& message = messages[start + index];
    mi_history_entry_t entry{};
    entry.kind = message.isGroup ? MI_EVENT_GROUP_TEXT : MI_EVENT_CHAT_TEXT;
    entry.status = 1;
    entry.is_group = message.isGroup ? 1 : 0;
    entry.outgoing = message.outgoing ? 1 : 0;
    entry.timestamp_sec = message.timestampMS / 1000ULL;
    entry.conv_id = CStringOrNull(message.conversationID);
    entry.sender = CStringOrNull(message.sender);
    entry.message_id = CStringOrNull(message.id);
    entry.text = CStringOrNull(message.text);
    entry.canonical_envelope =
        reinterpret_cast<const std::uint8_t*>(message.text.data());
    entry.canonical_envelope_len =
        static_cast<std::uint32_t>(message.text.size());
    entry.message_type = message.isGroup ? 4u : 1u;
    out_entries[index] = entry;
  }
  return count;
}

MI_E2EE_SDK_API std::uint32_t mi_client_poll_event(mi_client_handle* handle,
                                                   mi_event_t* out_events,
                                                   std::uint32_t max_events,
                                                   std::uint32_t wait_ms) {
  (void)wait_ms;
  if (!EnsureAuthenticated(handle) || !out_events || max_events == 0) {
    return 0;
  }

  const std::uint32_t count =
      std::min<std::uint32_t>(static_cast<std::uint32_t>(handle->pendingEvents.size()), max_events);
  handle->lastPolledEvents.clear();
  handle->lastPolledEvents.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    handle->lastPolledEvents.push_back(handle->pendingEvents.front());
    handle->pendingEvents.pop_front();
    const StubMessage& message = handle->lastPolledEvents.back();
    mi_event_t event{};
    event.type = message.isGroup
        ? (message.outgoing ? MI_EVENT_OUTGOING_GROUP_TEXT : MI_EVENT_GROUP_TEXT)
        : (message.outgoing ? MI_EVENT_OUTGOING_TEXT : MI_EVENT_CHAT_TEXT);
    event.ts_ms = message.timestampMS;
    event.peer = message.isGroup ? nullptr : CStringOrNull(message.conversationID);
    event.sender = CStringOrNull(message.sender);
    event.group_id = message.isGroup ? CStringOrNull(message.conversationID) : nullptr;
    event.message_id = CStringOrNull(message.id);
    event.text = CStringOrNull(message.text);
    out_events[index] = event;
  }
  handle->lastError.clear();
  return count;
}

MI_E2EE_SDK_API void mi_client_free(void* buf) {
  std::free(buf);
}

}  // extern "C"
