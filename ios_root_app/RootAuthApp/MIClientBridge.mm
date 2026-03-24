#import "MIClientBridge.h"

#import <algorithm>
#import <cstdint>
#import <vector>

#import "mi_client_ios_bridge.h"

namespace {

constexpr std::uint32_t kDefaultListCapacity = 16;
constexpr std::uint32_t kMaxListCapacity = 512;

NSString* MIStringOrEmpty(const char* value) {
  if (!value) {
    return @"";
  }
  NSString* string = [NSString stringWithUTF8String:value];
  return string ?: @"";
}

NSString* _Nullable MIStringOrNil(const char* value) {
  if (!value || value[0] == '\0') {
    return nil;
  }
  return [NSString stringWithUTF8String:value];
}

void SetIfPresent(NSMutableDictionary<NSString*, id>* dict,
                  NSString* key,
                  NSString* _Nullable value) {
  if (value.length > 0) {
    dict[key] = value;
  }
}

bool EventIsGroup(const mi_event_t& event) {
  return event.group_id && event.group_id[0] != '\0';
}

bool EventIsOutgoing(const mi_event_t& event) {
  switch (event.type) {
    case MI_EVENT_OUTGOING_TEXT:
    case MI_EVENT_OUTGOING_FILE:
    case MI_EVENT_OUTGOING_STICKER:
    case MI_EVENT_OUTGOING_GROUP_TEXT:
    case MI_EVENT_OUTGOING_GROUP_FILE:
      return true;
    default:
      return false;
  }
}

NSString* ConversationIDForEvent(const mi_event_t& event) {
  if (NSString* groupID = MIStringOrNil(event.group_id)) {
    return groupID;
  }
  if (NSString* peer = MIStringOrNil(event.peer)) {
    return peer;
  }
  if (NSString* sender = MIStringOrNil(event.sender)) {
    return sender;
  }
  return @"";
}

NSDictionary<NSString*, id>* DictionaryFromEvent(const mi_event_t& event) {
  NSMutableDictionary<NSString*, id>* dict = [NSMutableDictionary dictionary];
  dict[@"type"] = @(event.type);
  dict[@"timestampMS"] = @(event.ts_ms);
  dict[@"fileSize"] = @(event.file_size);
  dict[@"noticeKind"] = @(event.notice_kind);
  dict[@"role"] = @(event.role);
  dict[@"typing"] = @((event.typing != 0));
  dict[@"online"] = @((event.online != 0));
  dict[@"isGroup"] = @(EventIsGroup(event));
  dict[@"outgoing"] = @(EventIsOutgoing(event));
  dict[@"conversationID"] = ConversationIDForEvent(event);
  SetIfPresent(dict, @"peer", MIStringOrNil(event.peer));
  SetIfPresent(dict, @"sender", MIStringOrNil(event.sender));
  SetIfPresent(dict, @"groupID", MIStringOrNil(event.group_id));
  SetIfPresent(dict, @"messageID", MIStringOrNil(event.message_id));
  SetIfPresent(dict, @"text", MIStringOrNil(event.text));
  SetIfPresent(dict, @"fileID", MIStringOrNil(event.file_id));
  SetIfPresent(dict, @"fileName", MIStringOrNil(event.file_name));
  SetIfPresent(dict, @"stickerID", MIStringOrNil(event.sticker_id));
  SetIfPresent(dict, @"actor", MIStringOrNil(event.actor));
  SetIfPresent(dict, @"target", MIStringOrNil(event.target));
  return dict;
}

NSDictionary<NSString*, id>* DictionaryFromHistory(const mi_history_entry_t& entry) {
  NSMutableDictionary<NSString*, id>* dict = [NSMutableDictionary dictionary];
  dict[@"kind"] = @(entry.kind);
  dict[@"status"] = @(entry.status);
  dict[@"timestampMS"] = @(entry.timestamp_sec * 1000ULL);
  dict[@"fileSize"] = @(entry.file_size);
  dict[@"isGroup"] = @((entry.is_group != 0));
  dict[@"outgoing"] = @((entry.outgoing != 0));
  if (NSString* convID = MIStringOrNil(entry.conv_id)) {
    dict[@"conversationID"] = convID;
  } else {
    dict[@"conversationID"] = @"";
  }
  SetIfPresent(dict, @"sender", MIStringOrNil(entry.sender));
  SetIfPresent(dict, @"messageID", MIStringOrNil(entry.message_id));
  SetIfPresent(dict, @"text", MIStringOrNil(entry.text));
  SetIfPresent(dict, @"fileID", MIStringOrNil(entry.file_id));
  SetIfPresent(dict, @"fileName", MIStringOrNil(entry.file_name));
  SetIfPresent(dict, @"stickerID", MIStringOrNil(entry.sticker_id));
  return dict;
}

NSDictionary<NSString*, id>* DictionaryFromFriend(const mi_friend_entry_t& entry) {
  NSMutableDictionary<NSString*, id>* dict = [NSMutableDictionary dictionary];
  dict[@"username"] = MIStringOrEmpty(entry.username);
  dict[@"remark"] = MIStringOrEmpty(entry.remark);
  return dict;
}

NSDictionary<NSString*, id>* DictionaryFromDevice(const mi_device_entry_t& entry) {
  NSMutableDictionary<NSString*, id>* dict = [NSMutableDictionary dictionary];
  dict[@"deviceID"] = MIStringOrEmpty(entry.device_id);
  dict[@"displayID"] = MIStringOrEmpty(entry.display_id);
  dict[@"lastSeenSec"] = @(entry.last_seen_sec);
  return dict;
}

NSDictionary<NSString*, id>* DictionaryFromGroupMember(const mi_group_member_entry_t& entry) {
  NSMutableDictionary<NSString*, id>* dict = [NSMutableDictionary dictionary];
  dict[@"username"] = MIStringOrEmpty(entry.username);
  dict[@"role"] = @(entry.role);
  return dict;
}

template <typename Entry, typename Func>
std::vector<Entry> FetchList(Func&& func) {
  std::vector<Entry> entries;
  std::uint32_t capacity = kDefaultListCapacity;
  std::uint32_t count = 0;
  while (true) {
    entries.assign(capacity, Entry{});
    count = func(entries.data(), capacity);
    if (count < capacity || capacity >= kMaxListCapacity) {
      break;
    }
    capacity = std::min<std::uint32_t>(capacity * 2, kMaxListCapacity);
  }
  if (count > entries.size()) {
    count = static_cast<std::uint32_t>(entries.size());
  }
  entries.resize(count);
  return entries;
}

NSArray<NSDictionary<NSString*, id>*>* FriendArray(mi_client_handle* handle) {
  if (!handle) {
    return @[];
  }
  const auto entries = FetchList<mi_friend_entry_t>(
      [&](mi_friend_entry_t* buffer, std::uint32_t maxEntries) {
        return mi_client_list_friends(handle, buffer, maxEntries);
      });
  NSMutableArray<NSDictionary<NSString*, id>*>* array =
      [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(entries.size())];
  for (const auto& entry : entries) {
    [array addObject:DictionaryFromFriend(entry)];
  }
  return array;
}

NSArray<NSDictionary<NSString*, id>*>* DeviceArray(mi_client_handle* handle) {
  if (!handle) {
    return @[];
  }
  const auto entries = FetchList<mi_device_entry_t>(
      [&](mi_device_entry_t* buffer, std::uint32_t maxEntries) {
        return mi_client_list_devices(handle, buffer, maxEntries);
      });
  NSMutableArray<NSDictionary<NSString*, id>*>* array =
      [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(entries.size())];
  for (const auto& entry : entries) {
    [array addObject:DictionaryFromDevice(entry)];
  }
  return array;
}

NSArray<NSDictionary<NSString*, id>*>* GroupMemberArray(mi_client_handle* handle,
                                                        NSString* groupID) {
  if (!handle || groupID.length == 0) {
    return @[];
  }
  const auto entries = FetchList<mi_group_member_entry_t>(
      [&](mi_group_member_entry_t* buffer, std::uint32_t maxEntries) {
        return mi_client_list_group_members_info(handle,
                                                 groupID.UTF8String,
                                                 buffer,
                                                 maxEntries);
      });
  NSMutableArray<NSDictionary<NSString*, id>*>* array =
      [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(entries.size())];
  for (const auto& entry : entries) {
    [array addObject:DictionaryFromGroupMember(entry)];
  }
  return array;
}

}  // namespace

@implementation MIClientBridge {
  mi_client_handle* _handle;
  NSString* _configPath;
  NSString* _lastCreateError;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _handle = nullptr;
    _configPath = @"";
    _lastCreateError = @"";
  }
  return self;
}

- (void)dealloc {
  [self close];
}

- (BOOL)isReady {
  return _handle != nullptr;
}

- (NSString*)configPath {
  return _configPath ?: @"";
}

- (NSString*)lastCreateError {
  return _lastCreateError ?: @"";
}

- (NSString*)lastError {
  if (_handle) {
    return MIStringOrEmpty(mi_client_last_error(_handle));
  }
  return self.lastCreateError;
}

- (NSString*)token {
  return _handle ? MIStringOrEmpty(mi_client_token(_handle)) : @"";
}

- (NSString*)deviceID {
  return _handle ? MIStringOrEmpty(mi_client_device_id(_handle)) : @"";
}

- (NSString*)deviceDisplayID {
  return _handle ? MIStringOrEmpty(mi_client_device_display_id(_handle)) : @"";
}

- (BOOL)remoteOK {
  return _handle ? (mi_client_remote_ok(_handle) != 0) : NO;
}

- (NSString*)remoteError {
  return _handle ? MIStringOrEmpty(mi_client_remote_error(_handle)) : @"";
}

- (BOOL)openWithConfigPath:(NSString *)configPath {
  [self close];
  _configPath = [configPath copy] ?: @"";
  const char* rawPath = _configPath.length > 0 ? _configPath.fileSystemRepresentation : nullptr;
  _handle = mi_client_create(rawPath);
  _lastCreateError = MIStringOrEmpty(mi_client_last_create_error());
  if (!_handle && _lastCreateError.length == 0) {
    _lastCreateError = @"mi_client_create failed";
  }
  return _handle != nullptr;
}

- (void)close {
  if (_handle) {
    mi_client_destroy(_handle);
    _handle = nullptr;
  }
}

- (BOOL)createAccountWithUsername:(NSString *)username
                         password:(NSString *)password {
  return _handle &&
      mi_client_register(_handle, username.UTF8String, password.UTF8String) != 0;
}

- (BOOL)loginWithUsername:(NSString *)username
                 password:(NSString *)password {
  return _handle &&
      mi_client_login(_handle, username.UTF8String, password.UTF8String) != 0;
}

- (BOOL)loginWithUsername:(NSString *)username
                 password:(NSString *)password
                 rootCode:(NSString *)rootCode {
  if (!_handle) {
    return NO;
  }
  if (rootCode.length == 0) {
    return [self loginWithUsername:username password:password];
  }
  return mi_client_login_with_root_code(_handle,
                                        username.UTF8String,
                                        password.UTF8String,
                                        rootCode.UTF8String) != 0;
}

- (BOOL)bootstrapRootAuthWithPublicKeyHex:(NSString *)publicKeyHex {
  return _handle &&
      mi_client_root_auth_init(_handle, publicKeyHex.UTF8String) != 0;
}

- (BOOL)publishPrekeys {
  return _handle && mi_client_publish_prekeys(_handle) != 0;
}

- (BOOL)heartbeat {
  return _handle && mi_client_heartbeat(_handle) != 0;
}

- (NSArray<NSDictionary<NSString *, id> *> *)pollEventsWithMaxCount:(NSInteger)maxCount
                                                             waitMS:(NSInteger)waitMS {
  if (!_handle || maxCount <= 0) {
    return @[];
  }
  std::uint32_t eventCount = static_cast<std::uint32_t>(std::max<NSInteger>(1, maxCount));
  std::vector<mi_event_t> events(eventCount);
  std::uint32_t count = mi_client_poll_event(_handle,
                                             events.data(),
                                             eventCount,
                                             static_cast<std::uint32_t>(std::max<NSInteger>(0, waitMS)));
  if (count > events.size()) {
    count = static_cast<std::uint32_t>(events.size());
  }
  NSMutableArray<NSDictionary<NSString*, id>*>* array =
      [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(count)];
  for (std::uint32_t index = 0; index < count; ++index) {
    [array addObject:DictionaryFromEvent(events[index])];
  }
  return array;
}

- (NSArray<NSDictionary<NSString *, id> *> *)loadHistoryForConversationID:(NSString *)conversationID
                                                                   isGroup:(BOOL)isGroup
                                                                     limit:(NSInteger)limit {
  if (!_handle || conversationID.length == 0 || limit <= 0) {
    return @[];
  }
  std::uint32_t entryLimit = static_cast<std::uint32_t>(limit);
  std::vector<mi_history_entry_t> entries(entryLimit);
  std::uint32_t count = mi_client_load_chat_history(_handle,
                                                    conversationID.UTF8String,
                                                    isGroup ? 1 : 0,
                                                    entryLimit,
                                                    entries.data(),
                                                    entryLimit);
  if (count > entries.size()) {
    count = static_cast<std::uint32_t>(entries.size());
  }
  NSMutableArray<NSDictionary<NSString*, id>*>* array =
      [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(count)];
  for (std::uint32_t index = 0; index < count; ++index) {
    [array addObject:DictionaryFromHistory(entries[index])];
  }
  return array;
}

- (NSString* _Nullable)sendPrivateText:(NSString *)text
                        toPeerUsername:(NSString *)peerUsername {
  if (!_handle || peerUsername.length == 0 || text.length == 0) {
    return nil;
  }
  char* outMessageID = nullptr;
  const BOOL ok = mi_client_send_private_text(_handle,
                                              peerUsername.UTF8String,
                                              text.UTF8String,
                                              &outMessageID) != 0;
  NSString* messageID = MIStringOrNil(outMessageID);
  if (outMessageID) {
    mi_client_free(outMessageID);
  }
  return ok ? messageID : nil;
}

- (NSString* _Nullable)sendGroupText:(NSString *)text
                              groupID:(NSString *)groupID {
  if (!_handle || groupID.length == 0 || text.length == 0) {
    return nil;
  }
  char* outMessageID = nullptr;
  const BOOL ok = mi_client_send_group_text(_handle,
                                            groupID.UTF8String,
                                            text.UTF8String,
                                            &outMessageID) != 0;
  NSString* messageID = MIStringOrNil(outMessageID);
  if (outMessageID) {
    mi_client_free(outMessageID);
  }
  return ok ? messageID : nil;
}

- (NSArray<NSDictionary<NSString *, id> *> *)listFriends {
  return FriendArray(_handle);
}

- (NSArray<NSDictionary<NSString *, id> *> *)listDevices {
  return DeviceArray(_handle);
}

- (NSString* _Nullable)createGroup {
  if (!_handle) {
    return nil;
  }
  char* outGroupID = nullptr;
  const BOOL ok = mi_client_create_group(_handle, &outGroupID) != 0;
  NSString* groupID = MIStringOrNil(outGroupID);
  if (outGroupID) {
    mi_client_free(outGroupID);
  }
  return ok ? groupID : nil;
}

- (BOOL)joinGroupWithID:(NSString *)groupID {
  return _handle &&
      groupID.length > 0 &&
      mi_client_join_group(_handle, groupID.UTF8String) != 0;
}

- (BOOL)leaveGroupWithID:(NSString *)groupID {
  return _handle &&
      groupID.length > 0 &&
      mi_client_leave_group(_handle, groupID.UTF8String) != 0;
}

- (NSString* _Nullable)sendGroupInviteToPeerUsername:(NSString *)peerUsername
                                             groupID:(NSString *)groupID {
  if (!_handle || groupID.length == 0 || peerUsername.length == 0) {
    return nil;
  }
  char* outMessageID = nullptr;
  const BOOL ok = mi_client_send_group_invite(_handle,
                                              groupID.UTF8String,
                                              peerUsername.UTF8String,
                                              &outMessageID) != 0;
  NSString* messageID = MIStringOrNil(outMessageID);
  if (outMessageID) {
    mi_client_free(outMessageID);
  }
  return ok ? messageID : nil;
}

- (NSArray<NSDictionary<NSString *, id> *> *)listGroupMembersForGroupID:(NSString *)groupID {
  return GroupMemberArray(_handle, groupID);
}

@end
