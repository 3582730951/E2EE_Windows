#include <jni.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "c_api_client.h"

namespace {

constexpr std::uint32_t kCallIdLen = 16;
constexpr std::uint32_t kMediaRootLen = 32;
constexpr std::uint32_t kGroupCallKeyLen = 32;
constexpr std::uint32_t kDefaultListCapacity = 32;
constexpr std::uint32_t kMaxListCapacity = 4096;
constexpr std::uint32_t kGroupCallMaxMembers = 256;

struct JniCache {
  jclass clsSdkVersion = nullptr;
  jmethodID ctorSdkVersion = nullptr;
  jclass clsFriendEntry = nullptr;
  jmethodID ctorFriendEntry = nullptr;
  jclass clsFriendRequestEntry = nullptr;
  jmethodID ctorFriendRequestEntry = nullptr;
  jclass clsDeviceEntry = nullptr;
  jmethodID ctorDeviceEntry = nullptr;
  jclass clsGroupMemberEntry = nullptr;
  jmethodID ctorGroupMemberEntry = nullptr;
  jclass clsGroupCallMember = nullptr;
  jmethodID ctorGroupCallMember = nullptr;
  jclass clsDevicePairingRequest = nullptr;
  jmethodID ctorDevicePairingRequest = nullptr;
  jclass clsMediaPacket = nullptr;
  jmethodID ctorMediaPacket = nullptr;
  jclass clsMediaConfig = nullptr;
  jmethodID ctorMediaConfig = nullptr;
  jclass clsHistoryEntry = nullptr;
  jmethodID ctorHistoryEntry = nullptr;
  jclass clsGroupCallInfo = nullptr;
  jmethodID ctorGroupCallInfo = nullptr;
  jclass clsGroupCallSignalResult = nullptr;
  jmethodID ctorGroupCallSignalResult = nullptr;
  jclass clsSyncFriendsResult = nullptr;
  jmethodID ctorSyncFriendsResult = nullptr;
  jclass clsSdkEvent = nullptr;
  jmethodID ctorSdkEvent = nullptr;
};

static JniCache g_cache;
static bool g_cache_ready = false;

static jstring NewJString(JNIEnv* env, const char* utf8) {
  return env->NewStringUTF(utf8 ? utf8 : "");
}

static std::string JStringToString(JNIEnv* env, jstring input) {
  if (!input) {
    return std::string();
  }
  const char* utf8 = env->GetStringUTFChars(input, nullptr);
  std::string out = utf8 ? utf8 : "";
  if (utf8) {
    env->ReleaseStringUTFChars(input, utf8);
  }
  return out;
}

static jbyteArray ToJByteArray(JNIEnv* env, const std::uint8_t* data, std::size_t len) {
  jbyteArray arr = env->NewByteArray(static_cast<jsize>(len));
  if (arr && data && len > 0) {
    env->SetByteArrayRegion(arr, 0, static_cast<jsize>(len),
                            reinterpret_cast<const jbyte*>(data));
  }
  return arr;
}

static std::vector<std::uint8_t> JByteArrayToVector(JNIEnv* env, jbyteArray input) {
  std::vector<std::uint8_t> out;
  if (!input) {
    return out;
  }
  jsize len = env->GetArrayLength(input);
  if (len <= 0) {
    return out;
  }
  out.resize(static_cast<std::size_t>(len));
  env->GetByteArrayRegion(input, 0, len, reinterpret_cast<jbyte*>(out.data()));
  return out;
}

static std::vector<std::string> JStringArrayToVector(JNIEnv* env, jobjectArray input) {
  std::vector<std::string> out;
  if (!input) {
    return out;
  }
  jsize len = env->GetArrayLength(input);
  out.reserve(static_cast<std::size_t>(len));
  for (jsize i = 0; i < len; ++i) {
    auto* item = static_cast<jstring>(env->GetObjectArrayElement(input, i));
    out.push_back(JStringToString(env, item));
    env->DeleteLocalRef(item);
  }
  return out;
}

template <typename Entry, typename Func>
static std::uint32_t FetchList(Func func, std::vector<Entry>& out) {
  std::uint32_t max_entries = kDefaultListCapacity;
  std::uint32_t count = 0;
  while (true) {
    out.assign(max_entries, Entry{});
    count = func(out.data(), max_entries);
    if (count < max_entries || max_entries >= kMaxListCapacity) {
      break;
    }
    max_entries = std::min<std::uint32_t>(max_entries * 2, kMaxListCapacity);
  }
  if (count > out.size()) {
    count = static_cast<std::uint32_t>(out.size());
  }
  out.resize(count);
  return count;
}
static bool InitCache(JNIEnv* env) {
  jclass local = env->FindClass("mi/e2ee/android/sdk/SdkVersion");
  if (!local) return false;
  g_cache.clsSdkVersion = reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctorSdkVersion = env->GetMethodID(g_cache.clsSdkVersion, "<init>", "(IIII)V");
  if (!g_cache.ctorSdkVersion) return false;

  local = env->FindClass("mi/e2ee/android/sdk/FriendEntry");
  if (!local) return false;
  g_cache.clsFriendEntry = reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctorFriendEntry = env->GetMethodID(
      g_cache.clsFriendEntry, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");
  if (!g_cache.ctorFriendEntry) return false;

  local = env->FindClass("mi/e2ee/android/sdk/FriendRequestEntry");
  if (!local) return false;
  g_cache.clsFriendRequestEntry =
      reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctorFriendRequestEntry = env->GetMethodID(
      g_cache.clsFriendRequestEntry, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");
  if (!g_cache.ctorFriendRequestEntry) return false;

  local = env->FindClass("mi/e2ee/android/sdk/DeviceEntry");
  if (!local) return false;
  g_cache.clsDeviceEntry = reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctorDeviceEntry = env->GetMethodID(
      g_cache.clsDeviceEntry, "<init>", "(Ljava/lang/String;I)V");
  if (!g_cache.ctorDeviceEntry) return false;

  local = env->FindClass("mi/e2ee/android/sdk/GroupMemberEntry");
  if (!local) return false;
  g_cache.clsGroupMemberEntry =
      reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctorGroupMemberEntry = env->GetMethodID(
      g_cache.clsGroupMemberEntry, "<init>", "(Ljava/lang/String;I)V");
  if (!g_cache.ctorGroupMemberEntry) return false;

  local = env->FindClass("mi/e2ee/android/sdk/GroupCallMember");
  if (!local) return false;
  g_cache.clsGroupCallMember =
      reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctorGroupCallMember = env->GetMethodID(
      g_cache.clsGroupCallMember, "<init>", "(Ljava/lang/String;)V");
  if (!g_cache.ctorGroupCallMember) return false;

  local = env->FindClass("mi/e2ee/android/sdk/DevicePairingRequest");
  if (!local) return false;
  g_cache.clsDevicePairingRequest =
      reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctorDevicePairingRequest = env->GetMethodID(
      g_cache.clsDevicePairingRequest, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");
  if (!g_cache.ctorDevicePairingRequest) return false;

  local = env->FindClass("mi/e2ee/android/sdk/MediaPacket");
  if (!local) return false;
  g_cache.clsMediaPacket = reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctorMediaPacket = env->GetMethodID(
      g_cache.clsMediaPacket, "<init>", "(Ljava/lang/String;[B)V");
  if (!g_cache.ctorMediaPacket) return false;

  local = env->FindClass("mi/e2ee/android/sdk/MediaConfig");
  if (!local) return false;
  g_cache.clsMediaConfig = reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctorMediaConfig = env->GetMethodID(
      g_cache.clsMediaConfig, "<init>", "(IIIIIIII)V");
  if (!g_cache.ctorMediaConfig) return false;

  local = env->FindClass("mi/e2ee/android/sdk/HistoryEntry");
  if (!local) return false;
  g_cache.clsHistoryEntry = reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctorHistoryEntry = env->GetMethodID(
      g_cache.clsHistoryEntry,
      "<init>",
      "(IIZZJLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
      "Ljava/lang/String;[BLjava/lang/String;JLjava/lang/String;)V");
  if (!g_cache.ctorHistoryEntry) return false;

  local = env->FindClass("mi/e2ee/android/sdk/GroupCallInfo");
  if (!local) return false;
  g_cache.clsGroupCallInfo = reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctorGroupCallInfo = env->GetMethodID(
      g_cache.clsGroupCallInfo, "<init>", "([BI)V");
  if (!g_cache.ctorGroupCallInfo) return false;

  local = env->FindClass("mi/e2ee/android/sdk/GroupCallSignalResult");
  if (!local) return false;
  g_cache.clsGroupCallSignalResult =
      reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctorGroupCallSignalResult = env->GetMethodID(
      g_cache.clsGroupCallSignalResult,
      "<init>",
      "([BI[Lmi/e2ee/android/sdk/GroupCallMember;)V");
  if (!g_cache.ctorGroupCallSignalResult) return false;

  local = env->FindClass("mi/e2ee/android/sdk/SyncFriendsResult");
  if (!local) return false;
  g_cache.clsSyncFriendsResult =
      reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctorSyncFriendsResult = env->GetMethodID(
      g_cache.clsSyncFriendsResult,
      "<init>",
      "(Z[Lmi/e2ee/android/sdk/FriendEntry;)V");
  if (!g_cache.ctorSyncFriendsResult) return false;

  local = env->FindClass("mi/e2ee/android/sdk/SdkEvent");
  if (!local) return false;
  g_cache.clsSdkEvent = reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctorSdkEvent = env->GetMethodID(
      g_cache.clsSdkEvent,
      "<init>",
      "(IJLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
      "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;J[BLjava/lang/String;"
      "ILjava/lang/String;Ljava/lang/String;IZZ[BIII[B)V");
  if (!g_cache.ctorSdkEvent) return false;

  return true;
}

static bool EnsureCache(JNIEnv* env) {
  if (g_cache_ready) {
    return true;
  }
  g_cache_ready = InitCache(env);
  return g_cache_ready;
}

static mi_client_handle* FromHandle(jlong handle) {
  return reinterpret_cast<mi_client_handle*>(handle);
}
static jobject NewSdkVersion(JNIEnv* env, const mi_sdk_version& version) {
  return env->NewObject(
      g_cache.clsSdkVersion,
      g_cache.ctorSdkVersion,
      static_cast<jint>(version.major),
      static_cast<jint>(version.minor),
      static_cast<jint>(version.patch),
      static_cast<jint>(version.abi));
}

static jobject NewFriendEntry(JNIEnv* env, const mi_friend_entry_t& entry) {
  jstring username = NewJString(env, entry.username);
  jstring remark = NewJString(env, entry.remark);
  jobject obj = env->NewObject(
      g_cache.clsFriendEntry, g_cache.ctorFriendEntry, username, remark);
  env->DeleteLocalRef(username);
  env->DeleteLocalRef(remark);
  return obj;
}

static jobject NewFriendRequestEntry(JNIEnv* env,
                                     const mi_friend_request_entry_t& entry) {
  jstring username = NewJString(env, entry.requester_username);
  jstring remark = NewJString(env, entry.requester_remark);
  jobject obj = env->NewObject(
      g_cache.clsFriendRequestEntry, g_cache.ctorFriendRequestEntry, username, remark);
  env->DeleteLocalRef(username);
  env->DeleteLocalRef(remark);
  return obj;
}

static jobject NewDeviceEntry(JNIEnv* env, const mi_device_entry_t& entry) {
  jstring device_id = NewJString(env, entry.device_id);
  jobject obj = env->NewObject(
      g_cache.clsDeviceEntry,
      g_cache.ctorDeviceEntry,
      device_id,
      static_cast<jint>(entry.last_seen_sec));
  env->DeleteLocalRef(device_id);
  return obj;
}

static jobject NewGroupMemberEntry(JNIEnv* env, const mi_group_member_entry_t& entry) {
  jstring username = NewJString(env, entry.username);
  jobject obj = env->NewObject(
      g_cache.clsGroupMemberEntry,
      g_cache.ctorGroupMemberEntry,
      username,
      static_cast<jint>(entry.role));
  env->DeleteLocalRef(username);
  return obj;
}

static jobject NewGroupCallMember(JNIEnv* env, const mi_group_call_member_t& entry) {
  jstring username = NewJString(env, entry.username);
  jobject obj = env->NewObject(
      g_cache.clsGroupCallMember,
      g_cache.ctorGroupCallMember,
      username);
  env->DeleteLocalRef(username);
  return obj;
}

static jobject NewDevicePairingRequest(JNIEnv* env,
                                       const mi_device_pairing_request_t& entry) {
  jstring device_id = NewJString(env, entry.device_id);
  jstring request_id = NewJString(env, entry.request_id_hex);
  jobject obj = env->NewObject(
      g_cache.clsDevicePairingRequest,
      g_cache.ctorDevicePairingRequest,
      device_id,
      request_id);
  env->DeleteLocalRef(device_id);
  env->DeleteLocalRef(request_id);
  return obj;
}

static jobject NewMediaPacket(JNIEnv* env, const mi_media_packet_t& packet) {
  jstring sender = NewJString(env, packet.sender);
  jbyteArray payload = ToJByteArray(env, packet.payload, packet.payload_len);
  jobject obj = env->NewObject(
      g_cache.clsMediaPacket, g_cache.ctorMediaPacket, sender, payload);
  env->DeleteLocalRef(sender);
  env->DeleteLocalRef(payload);
  return obj;
}

static jobject NewMediaConfig(JNIEnv* env, const mi_media_config_t& config) {
  return env->NewObject(
      g_cache.clsMediaConfig,
      g_cache.ctorMediaConfig,
      static_cast<jint>(config.audio_delay_ms),
      static_cast<jint>(config.video_delay_ms),
      static_cast<jint>(config.audio_max_frames),
      static_cast<jint>(config.video_max_frames),
      static_cast<jint>(config.pull_max_packets),
      static_cast<jint>(config.pull_wait_ms),
      static_cast<jint>(config.group_pull_max_packets),
      static_cast<jint>(config.group_pull_wait_ms));
}
static jobject NewHistoryEntry(JNIEnv* env, const mi_history_entry_t& entry) {
  jstring conv_id = NewJString(env, entry.conv_id);
  jstring sender = NewJString(env, entry.sender);
  jstring message_id = NewJString(env, entry.message_id);
  jstring text = NewJString(env, entry.text);
  jstring file_id = NewJString(env, entry.file_id);
  jbyteArray file_key = ToJByteArray(env, entry.file_key, entry.file_key_len);
  jstring file_name = NewJString(env, entry.file_name);
  jstring sticker_id = NewJString(env, entry.sticker_id);
  jobject obj = env->NewObject(
      g_cache.clsHistoryEntry,
      g_cache.ctorHistoryEntry,
      static_cast<jint>(entry.kind),
      static_cast<jint>(entry.status),
      static_cast<jboolean>(entry.is_group != 0),
      static_cast<jboolean>(entry.outgoing != 0),
      static_cast<jlong>(entry.timestamp_sec),
      conv_id,
      sender,
      message_id,
      text,
      file_id,
      file_key,
      file_name,
      static_cast<jlong>(entry.file_size),
      sticker_id);
  env->DeleteLocalRef(conv_id);
  env->DeleteLocalRef(sender);
  env->DeleteLocalRef(message_id);
  env->DeleteLocalRef(text);
  env->DeleteLocalRef(file_id);
  env->DeleteLocalRef(file_key);
  env->DeleteLocalRef(file_name);
  env->DeleteLocalRef(sticker_id);
  return obj;
}

static jobjectArray BuildGroupCallMemberArray(JNIEnv* env,
                                              const mi_group_call_member_t* members,
                                              std::uint32_t count) {
  jobjectArray arr = env->NewObjectArray(
      static_cast<jsize>(count), g_cache.clsGroupCallMember, nullptr);
  if (!members || count == 0) {
    return arr;
  }
  for (std::uint32_t i = 0; i < count; ++i) {
    jobject member = NewGroupCallMember(env, members[i]);
    env->SetObjectArrayElement(arr, static_cast<jsize>(i), member);
    env->DeleteLocalRef(member);
  }
  return arr;
}

static jobject NewGroupCallInfo(JNIEnv* env,
                                const std::uint8_t* call_id,
                                std::uint32_t key_id) {
  jbyteArray call = ToJByteArray(env, call_id, kCallIdLen);
  jobject obj = env->NewObject(
      g_cache.clsGroupCallInfo,
      g_cache.ctorGroupCallInfo,
      call,
      static_cast<jint>(key_id));
  env->DeleteLocalRef(call);
  return obj;
}

static jobject NewSyncFriendsResult(JNIEnv* env,
                                    jboolean changed,
                                    jobjectArray entries) {
  return env->NewObject(
      g_cache.clsSyncFriendsResult,
      g_cache.ctorSyncFriendsResult,
      changed,
      entries);
}

static jobject NewSdkEvent(JNIEnv* env, const mi_event_t& event) {
  jstring peer = NewJString(env, event.peer);
  jstring sender = NewJString(env, event.sender);
  jstring group_id = NewJString(env, event.group_id);
  jstring message_id = NewJString(env, event.message_id);
  jstring text = NewJString(env, event.text);
  jstring file_id = NewJString(env, event.file_id);
  jstring file_name = NewJString(env, event.file_name);
  jbyteArray file_key = ToJByteArray(env, event.file_key, event.file_key_len);
  jstring sticker_id = NewJString(env, event.sticker_id);
  jstring actor = NewJString(env, event.actor);
  jstring target = NewJString(env, event.target);
  jbyteArray call_id = ToJByteArray(env, event.call_id, kCallIdLen);
  jbyteArray payload = ToJByteArray(env, event.payload, event.payload_len);
  jobject obj = env->NewObject(
      g_cache.clsSdkEvent,
      g_cache.ctorSdkEvent,
      static_cast<jint>(event.type),
      static_cast<jlong>(event.ts_ms),
      peer,
      sender,
      group_id,
      message_id,
      text,
      file_id,
      file_name,
      static_cast<jlong>(event.file_size),
      file_key,
      sticker_id,
      static_cast<jint>(event.notice_kind),
      actor,
      target,
      static_cast<jint>(event.role),
      static_cast<jboolean>(event.typing != 0),
      static_cast<jboolean>(event.online != 0),
      call_id,
      static_cast<jint>(event.call_key_id),
      static_cast<jint>(event.call_op),
      static_cast<jint>(event.call_media_flags),
      payload);
  env->DeleteLocalRef(peer);
  env->DeleteLocalRef(sender);
  env->DeleteLocalRef(group_id);
  env->DeleteLocalRef(message_id);
  env->DeleteLocalRef(text);
  env->DeleteLocalRef(file_id);
  env->DeleteLocalRef(file_name);
  env->DeleteLocalRef(file_key);
  env->DeleteLocalRef(sticker_id);
  env->DeleteLocalRef(actor);
  env->DeleteLocalRef(target);
  env->DeleteLocalRef(call_id);
  env->DeleteLocalRef(payload);
  return obj;
}
}  // namespace

extern "C" JNIEXPORT jobject JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_getVersion(JNIEnv* env, jobject) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_sdk_version version{};
  mi_client_get_version(&version);
  return NewSdkVersion(env, version);
}

extern "C" JNIEXPORT jint JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_getCapabilities(JNIEnv*, jobject) {
  return static_cast<jint>(mi_client_get_capabilities());
}

extern "C" JNIEXPORT jlong JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_createClient(JNIEnv* env,
                                                jobject,
                                                jstring config_path) {
  std::string path = JStringToString(env, config_path);
  const char* raw = path.empty() ? nullptr : path.c_str();
  mi_client_handle* handle = mi_client_create(raw);
  return reinterpret_cast<jlong>(handle);
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_lastCreateError(JNIEnv* env, jobject) {
  return NewJString(env, mi_client_last_create_error());
}

extern "C" JNIEXPORT void JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_destroyClient(JNIEnv*, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  if (ptr) {
    mi_client_destroy(ptr);
  }
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_lastError(JNIEnv* env, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return NewJString(env, ptr ? mi_client_last_error(ptr) : "");
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_token(JNIEnv* env, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return NewJString(env, ptr ? mi_client_token(ptr) : "");
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_deviceId(JNIEnv* env, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return NewJString(env, ptr ? mi_client_device_id(ptr) : "");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_remoteOk(JNIEnv*, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return static_cast<jboolean>(ptr && mi_client_remote_ok(ptr));
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_remoteError(JNIEnv* env, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return NewJString(env, ptr ? mi_client_remote_error(ptr) : "");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_isRemoteMode(JNIEnv*, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return static_cast<jboolean>(ptr && mi_client_is_remote_mode(ptr));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_relogin(JNIEnv*, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return static_cast<jboolean>(ptr && mi_client_relogin(ptr));
}
extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_hasPendingServerTrust(JNIEnv*, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return static_cast<jboolean>(ptr && mi_client_has_pending_server_trust(ptr));
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_pendingServerFingerprint(JNIEnv* env,
                                                            jobject,
                                                            jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return NewJString(env, ptr ? mi_client_pending_server_fingerprint(ptr) : "");
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_pendingServerPin(JNIEnv* env, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return NewJString(env, ptr ? mi_client_pending_server_pin(ptr) : "");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_trustPendingServer(JNIEnv* env,
                                                      jobject,
                                                      jlong handle,
                                                      jstring pin) {
  mi_client_handle* ptr = FromHandle(handle);
  std::string pin_str = JStringToString(env, pin);
  return static_cast<jboolean>(ptr &&
      mi_client_trust_pending_server(ptr, pin_str.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_hasPendingPeerTrust(JNIEnv*, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return static_cast<jboolean>(ptr && mi_client_has_pending_peer_trust(ptr));
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_pendingPeerUsername(JNIEnv* env, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return NewJString(env, ptr ? mi_client_pending_peer_username(ptr) : "");
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_pendingPeerFingerprint(JNIEnv* env,
                                                          jobject,
                                                          jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return NewJString(env, ptr ? mi_client_pending_peer_fingerprint(ptr) : "");
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_pendingPeerPin(JNIEnv* env, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return NewJString(env, ptr ? mi_client_pending_peer_pin(ptr) : "");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_trustPendingPeer(JNIEnv* env,
                                                    jobject,
                                                    jlong handle,
                                                    jstring pin) {
  mi_client_handle* ptr = FromHandle(handle);
  std::string pin_str = JStringToString(env, pin);
  return static_cast<jboolean>(ptr &&
      mi_client_trust_pending_peer(ptr, pin_str.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_register(JNIEnv* env,
                                            jobject,
                                            jlong handle,
                                            jstring username,
                                            jstring password) {
  mi_client_handle* ptr = FromHandle(handle);
  std::string user_str = JStringToString(env, username);
  std::string pass_str = JStringToString(env, password);
  return static_cast<jboolean>(ptr &&
      mi_client_register(ptr, user_str.c_str(), pass_str.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_login(JNIEnv* env,
                                         jobject,
                                         jlong handle,
                                         jstring username,
                                         jstring password) {
  mi_client_handle* ptr = FromHandle(handle);
  std::string user_str = JStringToString(env, username);
  std::string pass_str = JStringToString(env, password);
  return static_cast<jboolean>(ptr &&
      mi_client_login(ptr, user_str.c_str(), pass_str.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_logout(JNIEnv*, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return static_cast<jboolean>(ptr && mi_client_logout(ptr));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_heartbeat(JNIEnv*, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  return static_cast<jboolean>(ptr && mi_client_heartbeat(ptr));
}
extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_sendPrivateText(JNIEnv* env,
                                                   jobject,
                                                   jlong handle,
                                                   jstring peer_username,
                                                   jstring text) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string peer = JStringToString(env, peer_username);
  std::string text_str = JStringToString(env, text);
  char* out_id = nullptr;
  int ok = mi_client_send_private_text(ptr, peer.c_str(), text_str.c_str(), &out_id);
  if (!ok || !out_id) {
    if (out_id) {
      mi_client_free(out_id);
    }
    return nullptr;
  }
  jstring result = NewJString(env, out_id);
  mi_client_free(out_id);
  return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_sendPrivateTextWithReply(JNIEnv* env,
                                                            jobject,
                                                            jlong handle,
                                                            jstring peer_username,
                                                            jstring text,
                                                            jstring reply_to_id,
                                                            jstring reply_preview) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string peer = JStringToString(env, peer_username);
  std::string text_str = JStringToString(env, text);
  std::string reply_id = JStringToString(env, reply_to_id);
  std::string preview = JStringToString(env, reply_preview);
  char* out_id = nullptr;
  int ok = mi_client_send_private_text_with_reply(
      ptr, peer.c_str(), text_str.c_str(), reply_id.c_str(), preview.c_str(), &out_id);
  if (!ok || !out_id) {
    if (out_id) {
      mi_client_free(out_id);
    }
    return nullptr;
  }
  jstring result = NewJString(env, out_id);
  mi_client_free(out_id);
  return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_resendPrivateText(JNIEnv* env,
                                                     jobject,
                                                     jlong handle,
                                                     jstring peer_username,
                                                     jstring message_id,
                                                     jstring text) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string peer = JStringToString(env, peer_username);
  std::string msg_id = JStringToString(env, message_id);
  std::string text_str = JStringToString(env, text);
  return static_cast<jboolean>(
      mi_client_resend_private_text(ptr, peer.c_str(), msg_id.c_str(), text_str.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_resendPrivateTextWithReply(JNIEnv* env,
                                                              jobject,
                                                              jlong handle,
                                                              jstring peer_username,
                                                              jstring message_id,
                                                              jstring text,
                                                              jstring reply_to_id,
                                                              jstring reply_preview) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string peer = JStringToString(env, peer_username);
  std::string msg_id = JStringToString(env, message_id);
  std::string text_str = JStringToString(env, text);
  std::string reply_id = JStringToString(env, reply_to_id);
  std::string preview = JStringToString(env, reply_preview);
  return static_cast<jboolean>(
      mi_client_resend_private_text_with_reply(
          ptr, peer.c_str(), msg_id.c_str(), text_str.c_str(), reply_id.c_str(),
          preview.c_str()));
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_sendGroupText(JNIEnv* env,
                                                 jobject,
                                                 jlong handle,
                                                 jstring group_id,
                                                 jstring text) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string group = JStringToString(env, group_id);
  std::string text_str = JStringToString(env, text);
  char* out_id = nullptr;
  int ok = mi_client_send_group_text(ptr, group.c_str(), text_str.c_str(), &out_id);
  if (!ok || !out_id) {
    if (out_id) {
      mi_client_free(out_id);
    }
    return nullptr;
  }
  jstring result = NewJString(env, out_id);
  mi_client_free(out_id);
  return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_resendGroupText(JNIEnv* env,
                                                   jobject,
                                                   jlong handle,
                                                   jstring group_id,
                                                   jstring message_id,
                                                   jstring text) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string group = JStringToString(env, group_id);
  std::string msg_id = JStringToString(env, message_id);
  std::string text_str = JStringToString(env, text);
  return static_cast<jboolean>(
      mi_client_resend_group_text(ptr, group.c_str(), msg_id.c_str(), text_str.c_str()));
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_sendPrivateFile(JNIEnv* env,
                                                   jobject,
                                                   jlong handle,
                                                   jstring peer_username,
                                                   jstring file_path) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string peer = JStringToString(env, peer_username);
  std::string path = JStringToString(env, file_path);
  char* out_id = nullptr;
  int ok = mi_client_send_private_file(ptr, peer.c_str(), path.c_str(), &out_id);
  if (!ok || !out_id) {
    if (out_id) {
      mi_client_free(out_id);
    }
    return nullptr;
  }
  jstring result = NewJString(env, out_id);
  mi_client_free(out_id);
  return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_resendPrivateFile(JNIEnv* env,
                                                     jobject,
                                                     jlong handle,
                                                     jstring peer_username,
                                                     jstring message_id,
                                                     jstring file_path) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string peer = JStringToString(env, peer_username);
  std::string msg_id = JStringToString(env, message_id);
  std::string path = JStringToString(env, file_path);
  return static_cast<jboolean>(
      mi_client_resend_private_file(ptr, peer.c_str(), msg_id.c_str(), path.c_str()));
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_sendGroupFile(JNIEnv* env,
                                                 jobject,
                                                 jlong handle,
                                                 jstring group_id,
                                                 jstring file_path) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string group = JStringToString(env, group_id);
  std::string path = JStringToString(env, file_path);
  char* out_id = nullptr;
  int ok = mi_client_send_group_file(ptr, group.c_str(), path.c_str(), &out_id);
  if (!ok || !out_id) {
    if (out_id) {
      mi_client_free(out_id);
    }
    return nullptr;
  }
  jstring result = NewJString(env, out_id);
  mi_client_free(out_id);
  return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_resendGroupFile(JNIEnv* env,
                                                   jobject,
                                                   jlong handle,
                                                   jstring group_id,
                                                   jstring message_id,
                                                   jstring file_path) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string group = JStringToString(env, group_id);
  std::string msg_id = JStringToString(env, message_id);
  std::string path = JStringToString(env, file_path);
  return static_cast<jboolean>(
      mi_client_resend_group_file(ptr, group.c_str(), msg_id.c_str(), path.c_str()));
}
extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_sendPrivateSticker(JNIEnv* env,
                                                      jobject,
                                                      jlong handle,
                                                      jstring peer_username,
                                                      jstring sticker_id) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string peer = JStringToString(env, peer_username);
  std::string sticker = JStringToString(env, sticker_id);
  char* out_id = nullptr;
  int ok = mi_client_send_private_sticker(ptr, peer.c_str(), sticker.c_str(), &out_id);
  if (!ok || !out_id) {
    if (out_id) {
      mi_client_free(out_id);
    }
    return nullptr;
  }
  jstring result = NewJString(env, out_id);
  mi_client_free(out_id);
  return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_resendPrivateSticker(JNIEnv* env,
                                                        jobject,
                                                        jlong handle,
                                                        jstring peer_username,
                                                        jstring message_id,
                                                        jstring sticker_id) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string peer = JStringToString(env, peer_username);
  std::string msg_id = JStringToString(env, message_id);
  std::string sticker = JStringToString(env, sticker_id);
  return static_cast<jboolean>(
      mi_client_resend_private_sticker(ptr, peer.c_str(), msg_id.c_str(), sticker.c_str()));
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_sendPrivateLocation(JNIEnv* env,
                                                       jobject,
                                                       jlong handle,
                                                       jstring peer_username,
                                                       jint lat_e7,
                                                       jint lon_e7,
                                                       jstring label) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string peer = JStringToString(env, peer_username);
  std::string label_str = JStringToString(env, label);
  char* out_id = nullptr;
  int ok = mi_client_send_private_location(
      ptr, peer.c_str(), static_cast<std::int32_t>(lat_e7),
      static_cast<std::int32_t>(lon_e7), label_str.c_str(), &out_id);
  if (!ok || !out_id) {
    if (out_id) {
      mi_client_free(out_id);
    }
    return nullptr;
  }
  jstring result = NewJString(env, out_id);
  mi_client_free(out_id);
  return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_resendPrivateLocation(JNIEnv* env,
                                                         jobject,
                                                         jlong handle,
                                                         jstring peer_username,
                                                         jstring message_id,
                                                         jint lat_e7,
                                                         jint lon_e7,
                                                         jstring label) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string peer = JStringToString(env, peer_username);
  std::string msg_id = JStringToString(env, message_id);
  std::string label_str = JStringToString(env, label);
  return static_cast<jboolean>(
      mi_client_resend_private_location(
          ptr, peer.c_str(), msg_id.c_str(), static_cast<std::int32_t>(lat_e7),
          static_cast<std::int32_t>(lon_e7), label_str.c_str()));
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_sendPrivateContact(JNIEnv* env,
                                                      jobject,
                                                      jlong handle,
                                                      jstring peer_username,
                                                      jstring card_username,
                                                      jstring card_display) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string peer = JStringToString(env, peer_username);
  std::string card_user = JStringToString(env, card_username);
  std::string card_disp = JStringToString(env, card_display);
  char* out_id = nullptr;
  int ok = mi_client_send_private_contact(
      ptr, peer.c_str(), card_user.c_str(), card_disp.c_str(), &out_id);
  if (!ok || !out_id) {
    if (out_id) {
      mi_client_free(out_id);
    }
    return nullptr;
  }
  jstring result = NewJString(env, out_id);
  mi_client_free(out_id);
  return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_resendPrivateContact(JNIEnv* env,
                                                        jobject,
                                                        jlong handle,
                                                        jstring peer_username,
                                                        jstring message_id,
                                                        jstring card_username,
                                                        jstring card_display) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string peer = JStringToString(env, peer_username);
  std::string msg_id = JStringToString(env, message_id);
  std::string card_user = JStringToString(env, card_username);
  std::string card_disp = JStringToString(env, card_display);
  return static_cast<jboolean>(
      mi_client_resend_private_contact(
          ptr, peer.c_str(), msg_id.c_str(), card_user.c_str(), card_disp.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_sendReadReceipt(JNIEnv* env,
                                                   jobject,
                                                   jlong handle,
                                                   jstring peer_username,
                                                   jstring message_id) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string peer = JStringToString(env, peer_username);
  std::string msg_id = JStringToString(env, message_id);
  return static_cast<jboolean>(
      mi_client_send_read_receipt(ptr, peer.c_str(), msg_id.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_sendTyping(JNIEnv* env,
                                              jobject,
                                              jlong handle,
                                              jstring peer_username,
                                              jboolean typing) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string peer = JStringToString(env, peer_username);
  return static_cast<jboolean>(
      mi_client_send_typing(ptr, peer.c_str(), typing ? 1 : 0));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_sendPresence(JNIEnv* env,
                                                jobject,
                                                jlong handle,
                                                jstring peer_username,
                                                jboolean online) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string peer = JStringToString(env, peer_username);
  return static_cast<jboolean>(
      mi_client_send_presence(ptr, peer.c_str(), online ? 1 : 0));
}
extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_addFriend(JNIEnv* env,
                                             jobject,
                                             jlong handle,
                                             jstring friend_username,
                                             jstring remark) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string user = JStringToString(env, friend_username);
  std::string remark_str = JStringToString(env, remark);
  return static_cast<jboolean>(
      mi_client_add_friend(ptr, user.c_str(), remark_str.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_setFriendRemark(JNIEnv* env,
                                                   jobject,
                                                   jlong handle,
                                                   jstring friend_username,
                                                   jstring remark) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string user = JStringToString(env, friend_username);
  std::string remark_str = JStringToString(env, remark);
  return static_cast<jboolean>(
      mi_client_set_friend_remark(ptr, user.c_str(), remark_str.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_deleteFriend(JNIEnv* env,
                                                jobject,
                                                jlong handle,
                                                jstring friend_username) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string user = JStringToString(env, friend_username);
  return static_cast<jboolean>(mi_client_delete_friend(ptr, user.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_setUserBlocked(JNIEnv* env,
                                                  jobject,
                                                  jlong handle,
                                                  jstring blocked_username,
                                                  jboolean blocked) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string user = JStringToString(env, blocked_username);
  return static_cast<jboolean>(
      mi_client_set_user_blocked(ptr, user.c_str(), blocked ? 1 : 0));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_sendFriendRequest(JNIEnv* env,
                                                     jobject,
                                                     jlong handle,
                                                     jstring target_username,
                                                     jstring remark) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string target = JStringToString(env, target_username);
  std::string remark_str = JStringToString(env, remark);
  return static_cast<jboolean>(
      mi_client_send_friend_request(ptr, target.c_str(), remark_str.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_respondFriendRequest(JNIEnv* env,
                                                        jobject,
                                                        jlong handle,
                                                        jstring requester_username,
                                                        jboolean accept) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string requester = JStringToString(env, requester_username);
  return static_cast<jboolean>(
      mi_client_respond_friend_request(ptr, requester.c_str(), accept ? 1 : 0));
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_listFriends(JNIEnv* env, jobject, jlong handle) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) {
    return env->NewObjectArray(0, g_cache.clsFriendEntry, nullptr);
  }
  std::vector<mi_friend_entry_t> entries;
  FetchList<mi_friend_entry_t>(
      [&](mi_friend_entry_t* buf, std::uint32_t max) {
        return mi_client_list_friends(ptr, buf, max);
      },
      entries);
  jobjectArray arr = env->NewObjectArray(
      static_cast<jsize>(entries.size()), g_cache.clsFriendEntry, nullptr);
  for (std::size_t i = 0; i < entries.size(); ++i) {
    jobject obj = NewFriendEntry(env, entries[i]);
    env->SetObjectArrayElement(arr, static_cast<jsize>(i), obj);
    env->DeleteLocalRef(obj);
  }
  return arr;
}

extern "C" JNIEXPORT jobject JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_syncFriends(JNIEnv* env, jobject, jlong handle) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) {
    return nullptr;
  }
  int changed = 0;
  std::vector<mi_friend_entry_t> entries;
  FetchList<mi_friend_entry_t>(
      [&](mi_friend_entry_t* buf, std::uint32_t max) {
        return mi_client_sync_friends(ptr, buf, max, &changed);
      },
      entries);
  jobjectArray arr = env->NewObjectArray(
      static_cast<jsize>(entries.size()), g_cache.clsFriendEntry, nullptr);
  for (std::size_t i = 0; i < entries.size(); ++i) {
    jobject obj = NewFriendEntry(env, entries[i]);
    env->SetObjectArrayElement(arr, static_cast<jsize>(i), obj);
    env->DeleteLocalRef(obj);
  }
  return NewSyncFriendsResult(env, static_cast<jboolean>(changed != 0), arr);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_listFriendRequests(JNIEnv* env,
                                                      jobject,
                                                      jlong handle) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) {
    return env->NewObjectArray(0, g_cache.clsFriendRequestEntry, nullptr);
  }
  std::vector<mi_friend_request_entry_t> entries;
  FetchList<mi_friend_request_entry_t>(
      [&](mi_friend_request_entry_t* buf, std::uint32_t max) {
        return mi_client_list_friend_requests(ptr, buf, max);
      },
      entries);
  jobjectArray arr = env->NewObjectArray(
      static_cast<jsize>(entries.size()), g_cache.clsFriendRequestEntry, nullptr);
  for (std::size_t i = 0; i < entries.size(); ++i) {
    jobject obj = NewFriendRequestEntry(env, entries[i]);
    env->SetObjectArrayElement(arr, static_cast<jsize>(i), obj);
    env->DeleteLocalRef(obj);
  }
  return arr;
}
extern "C" JNIEXPORT jobjectArray JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_listDevices(JNIEnv* env, jobject, jlong handle) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) {
    return env->NewObjectArray(0, g_cache.clsDeviceEntry, nullptr);
  }
  std::vector<mi_device_entry_t> entries;
  FetchList<mi_device_entry_t>(
      [&](mi_device_entry_t* buf, std::uint32_t max) {
        return mi_client_list_devices(ptr, buf, max);
      },
      entries);
  jobjectArray arr = env->NewObjectArray(
      static_cast<jsize>(entries.size()), g_cache.clsDeviceEntry, nullptr);
  for (std::size_t i = 0; i < entries.size(); ++i) {
    jobject obj = NewDeviceEntry(env, entries[i]);
    env->SetObjectArrayElement(arr, static_cast<jsize>(i), obj);
    env->DeleteLocalRef(obj);
  }
  return arr;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_kickDevice(JNIEnv* env,
                                              jobject,
                                              jlong handle,
                                              jstring device_id) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string device = JStringToString(env, device_id);
  return static_cast<jboolean>(mi_client_kick_device(ptr, device.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_joinGroup(JNIEnv* env,
                                             jobject,
                                             jlong handle,
                                             jstring group_id) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string group = JStringToString(env, group_id);
  return static_cast<jboolean>(mi_client_join_group(ptr, group.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_leaveGroup(JNIEnv* env,
                                              jobject,
                                              jlong handle,
                                              jstring group_id) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string group = JStringToString(env, group_id);
  return static_cast<jboolean>(mi_client_leave_group(ptr, group.c_str()));
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_createGroup(JNIEnv* env, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  char* out_group_id = nullptr;
  int ok = mi_client_create_group(ptr, &out_group_id);
  if (!ok || !out_group_id) {
    if (out_group_id) {
      mi_client_free(out_group_id);
    }
    return nullptr;
  }
  jstring result = NewJString(env, out_group_id);
  mi_client_free(out_group_id);
  return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_sendGroupInvite(JNIEnv* env,
                                                   jobject,
                                                   jlong handle,
                                                   jstring group_id,
                                                   jstring peer_username) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string group = JStringToString(env, group_id);
  std::string peer = JStringToString(env, peer_username);
  char* out_id = nullptr;
  int ok = mi_client_send_group_invite(ptr, group.c_str(), peer.c_str(), &out_id);
  if (!ok || !out_id) {
    if (out_id) {
      mi_client_free(out_id);
    }
    return nullptr;
  }
  jstring result = NewJString(env, out_id);
  mi_client_free(out_id);
  return result;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_listGroupMembers(JNIEnv* env,
                                                    jobject,
                                                    jlong handle,
                                                    jstring group_id) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) {
    return env->NewObjectArray(0, g_cache.clsGroupMemberEntry, nullptr);
  }
  std::string group = JStringToString(env, group_id);
  std::vector<mi_group_member_entry_t> entries;
  FetchList<mi_group_member_entry_t>(
      [&](mi_group_member_entry_t* buf, std::uint32_t max) {
        return mi_client_list_group_members_info(ptr, group.c_str(), buf, max);
      },
      entries);
  jobjectArray arr = env->NewObjectArray(
      static_cast<jsize>(entries.size()), g_cache.clsGroupMemberEntry, nullptr);
  for (std::size_t i = 0; i < entries.size(); ++i) {
    jobject obj = NewGroupMemberEntry(env, entries[i]);
    env->SetObjectArrayElement(arr, static_cast<jsize>(i), obj);
    env->DeleteLocalRef(obj);
  }
  return arr;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_setGroupMemberRole(JNIEnv* env,
                                                      jobject,
                                                      jlong handle,
                                                      jstring group_id,
                                                      jstring peer_username,
                                                      jint role) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string group = JStringToString(env, group_id);
  std::string peer = JStringToString(env, peer_username);
  return static_cast<jboolean>(
      mi_client_set_group_member_role(ptr, group.c_str(), peer.c_str(),
                                      static_cast<std::uint32_t>(role)));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_kickGroupMember(JNIEnv* env,
                                                   jobject,
                                                   jlong handle,
                                                   jstring group_id,
                                                   jstring peer_username) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string group = JStringToString(env, group_id);
  std::string peer = JStringToString(env, peer_username);
  return static_cast<jboolean>(
      mi_client_kick_group_member(ptr, group.c_str(), peer.c_str()));
}
extern "C" JNIEXPORT jobject JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_startGroupCall(JNIEnv* env,
                                                  jobject,
                                                  jlong handle,
                                                  jstring group_id,
                                                  jboolean video) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string group = JStringToString(env, group_id);
  std::array<std::uint8_t, kCallIdLen> call_id{};
  std::uint32_t key_id = 0;
  int ok = mi_client_start_group_call(
      ptr, group.c_str(), video ? 1 : 0, call_id.data(), kCallIdLen, &key_id);
  if (!ok) {
    return nullptr;
  }
  return NewGroupCallInfo(env, call_id.data(), key_id);
}

extern "C" JNIEXPORT jobject JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_joinGroupCall(JNIEnv* env,
                                                 jobject,
                                                 jlong handle,
                                                 jstring group_id,
                                                 jbyteArray call_id,
                                                 jboolean video) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string group = JStringToString(env, group_id);
  std::vector<std::uint8_t> call = JByteArrayToVector(env, call_id);
  if (call.size() != kCallIdLen) {
    return nullptr;
  }
  std::uint32_t key_id = 0;
  int ok = mi_client_join_group_call(
      ptr, group.c_str(), call.data(), static_cast<std::uint32_t>(call.size()),
      video ? 1 : 0, &key_id);
  if (!ok) {
    return nullptr;
  }
  return NewGroupCallInfo(env, call.data(), key_id);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_leaveGroupCall(JNIEnv* env,
                                                  jobject,
                                                  jlong handle,
                                                  jstring group_id,
                                                  jbyteArray call_id) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string group = JStringToString(env, group_id);
  std::vector<std::uint8_t> call = JByteArrayToVector(env, call_id);
  return static_cast<jboolean>(
      mi_client_leave_group_call(
          ptr, group.c_str(), call.data(), static_cast<std::uint32_t>(call.size())));
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_getGroupCallKey(JNIEnv* env,
                                                   jobject,
                                                   jlong handle,
                                                   jstring group_id,
                                                   jbyteArray call_id,
                                                   jint key_id) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string group = JStringToString(env, group_id);
  std::vector<std::uint8_t> call = JByteArrayToVector(env, call_id);
  std::array<std::uint8_t, kGroupCallKeyLen> key{};
  int ok = mi_client_get_group_call_key(
      ptr, group.c_str(), call.data(), static_cast<std::uint32_t>(call.size()),
      static_cast<std::uint32_t>(key_id), key.data(), kGroupCallKeyLen);
  if (!ok) {
    return nullptr;
  }
  return ToJByteArray(env, key.data(), kGroupCallKeyLen);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_rotateGroupCallKey(JNIEnv* env,
                                                      jobject,
                                                      jlong handle,
                                                      jstring group_id,
                                                      jbyteArray call_id,
                                                      jint key_id,
                                                      jobjectArray members) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string group = JStringToString(env, group_id);
  std::vector<std::uint8_t> call = JByteArrayToVector(env, call_id);
  std::vector<std::string> member_list = JStringArrayToVector(env, members);
  std::vector<const char*> member_ptrs;
  member_ptrs.reserve(member_list.size());
  for (const auto& name : member_list) {
    member_ptrs.push_back(name.c_str());
  }
  const char** member_data = member_ptrs.empty() ? nullptr : member_ptrs.data();
  return static_cast<jboolean>(
      mi_client_rotate_group_call_key(
          ptr, group.c_str(), call.data(), static_cast<std::uint32_t>(call.size()),
          static_cast<std::uint32_t>(key_id), member_data,
          static_cast<std::uint32_t>(member_ptrs.size())));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_requestGroupCallKey(JNIEnv* env,
                                                       jobject,
                                                       jlong handle,
                                                       jstring group_id,
                                                       jbyteArray call_id,
                                                       jint key_id,
                                                       jobjectArray members) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string group = JStringToString(env, group_id);
  std::vector<std::uint8_t> call = JByteArrayToVector(env, call_id);
  std::vector<std::string> member_list = JStringArrayToVector(env, members);
  std::vector<const char*> member_ptrs;
  member_ptrs.reserve(member_list.size());
  for (const auto& name : member_list) {
    member_ptrs.push_back(name.c_str());
  }
  const char** member_data = member_ptrs.empty() ? nullptr : member_ptrs.data();
  return static_cast<jboolean>(
      mi_client_request_group_call_key(
          ptr, group.c_str(), call.data(), static_cast<std::uint32_t>(call.size()),
          static_cast<std::uint32_t>(key_id), member_data,
          static_cast<std::uint32_t>(member_ptrs.size())));
}

extern "C" JNIEXPORT jobject JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_sendGroupCallSignal(JNIEnv* env,
                                                       jobject,
                                                       jlong handle,
                                                       jint op,
                                                       jstring group_id,
                                                       jbyteArray call_id,
                                                       jboolean video,
                                                       jint key_id,
                                                       jint seq,
                                                       jlong ts_ms,
                                                       jbyteArray ext) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string group = JStringToString(env, group_id);
  std::vector<std::uint8_t> call = JByteArrayToVector(env, call_id);
  std::vector<std::uint8_t> ext_data = JByteArrayToVector(env, ext);
  std::array<std::uint8_t, kCallIdLen> out_call_id{};
  std::uint32_t out_key_id = 0;
  std::vector<mi_group_call_member_t> members(kGroupCallMaxMembers);
  std::uint32_t member_count = 0;
  int ok = mi_client_send_group_call_signal(
      ptr,
      static_cast<std::uint8_t>(op),
      group.c_str(),
      call.empty() ? nullptr : call.data(),
      static_cast<std::uint32_t>(call.size()),
      video ? 1 : 0,
      static_cast<std::uint32_t>(key_id),
      static_cast<std::uint32_t>(seq),
      static_cast<std::uint64_t>(ts_ms),
      ext_data.empty() ? nullptr : ext_data.data(),
      static_cast<std::uint32_t>(ext_data.size()),
      out_call_id.data(),
      kCallIdLen,
      &out_key_id,
      members.data(),
      kGroupCallMaxMembers,
      &member_count);
  if (!ok) {
    return nullptr;
  }
  jbyteArray call_out = ToJByteArray(env, out_call_id.data(), kCallIdLen);
  jobjectArray member_arr = BuildGroupCallMemberArray(env, members.data(), member_count);
  jobject result = env->NewObject(
      g_cache.clsGroupCallSignalResult,
      g_cache.ctorGroupCallSignalResult,
      call_out,
      static_cast<jint>(out_key_id),
      member_arr);
  env->DeleteLocalRef(call_out);
  env->DeleteLocalRef(member_arr);
  return result;
}
extern "C" JNIEXPORT jobjectArray JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_loadChatHistory(JNIEnv* env,
                                                   jobject,
                                                   jlong handle,
                                                   jstring conv_id,
                                                   jboolean is_group,
                                                   jint limit) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) {
    return env->NewObjectArray(0, g_cache.clsHistoryEntry, nullptr);
  }
  if (limit <= 0) {
    return env->NewObjectArray(0, g_cache.clsHistoryEntry, nullptr);
  }
  std::string conv = JStringToString(env, conv_id);
  std::uint32_t max_entries = static_cast<std::uint32_t>(limit);
  std::vector<mi_history_entry_t> entries(max_entries);
  std::uint32_t count = mi_client_load_chat_history(
      ptr, conv.c_str(), is_group ? 1 : 0, max_entries,
      entries.data(), max_entries);
  if (count > max_entries) {
    count = max_entries;
  }
  jobjectArray arr = env->NewObjectArray(
      static_cast<jsize>(count), g_cache.clsHistoryEntry, nullptr);
  for (std::uint32_t i = 0; i < count; ++i) {
    jobject obj = NewHistoryEntry(env, entries[i]);
    env->SetObjectArrayElement(arr, static_cast<jsize>(i), obj);
    env->DeleteLocalRef(obj);
  }
  return arr;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_deleteChatHistory(JNIEnv* env,
                                                     jobject,
                                                     jlong handle,
                                                     jstring conv_id,
                                                     jboolean is_group,
                                                     jboolean delete_attachments,
                                                     jboolean secure_wipe) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string conv = JStringToString(env, conv_id);
  return static_cast<jboolean>(
      mi_client_delete_chat_history(
          ptr, conv.c_str(), is_group ? 1 : 0,
          delete_attachments ? 1 : 0, secure_wipe ? 1 : 0));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_setHistoryEnabled(JNIEnv*, jobject, jlong handle, jboolean enabled) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  return static_cast<jboolean>(mi_client_set_history_enabled(ptr, enabled ? 1 : 0));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_clearAllHistory(JNIEnv*,
                                                   jobject,
                                                   jlong handle,
                                                   jboolean delete_attachments,
                                                   jboolean secure_wipe) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  return static_cast<jboolean>(
      mi_client_clear_all_history(ptr, delete_attachments ? 1 : 0, secure_wipe ? 1 : 0));
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_beginDevicePairingPrimary(JNIEnv* env,
                                                             jobject,
                                                             jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  char* out_code = nullptr;
  int ok = mi_client_begin_device_pairing_primary(ptr, &out_code);
  if (!ok || !out_code) {
    if (out_code) {
      mi_client_free(out_code);
    }
    return nullptr;
  }
  jstring result = NewJString(env, out_code);
  mi_client_free(out_code);
  return result;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_pollDevicePairingRequests(JNIEnv* env,
                                                             jobject,
                                                             jlong handle) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) {
    return env->NewObjectArray(0, g_cache.clsDevicePairingRequest, nullptr);
  }
  std::vector<mi_device_pairing_request_t> entries;
  FetchList<mi_device_pairing_request_t>(
      [&](mi_device_pairing_request_t* buf, std::uint32_t max) {
        return mi_client_poll_device_pairing_requests(ptr, buf, max);
      },
      entries);
  jobjectArray arr = env->NewObjectArray(
      static_cast<jsize>(entries.size()), g_cache.clsDevicePairingRequest, nullptr);
  for (std::size_t i = 0; i < entries.size(); ++i) {
    jobject obj = NewDevicePairingRequest(env, entries[i]);
    env->SetObjectArrayElement(arr, static_cast<jsize>(i), obj);
    env->DeleteLocalRef(obj);
  }
  return arr;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_approveDevicePairingRequest(JNIEnv* env,
                                                               jobject,
                                                               jlong handle,
                                                               jstring device_id,
                                                               jstring request_id) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string device = JStringToString(env, device_id);
  std::string request = JStringToString(env, request_id);
  return static_cast<jboolean>(
      mi_client_approve_device_pairing_request(ptr, device.c_str(), request.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_beginDevicePairingLinked(JNIEnv* env,
                                                            jobject,
                                                            jlong handle,
                                                            jstring pairing_code) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string code = JStringToString(env, pairing_code);
  return static_cast<jboolean>(
      mi_client_begin_device_pairing_linked(ptr, code.c_str()));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_pollDevicePairingLinked(JNIEnv*,
                                                           jobject,
                                                           jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  int completed = 0;
  int ok = mi_client_poll_device_pairing_linked(ptr, &completed);
  return static_cast<jboolean>(ok && completed != 0);
}

extern "C" JNIEXPORT void JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_cancelDevicePairing(JNIEnv*, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  if (ptr) {
    mi_client_cancel_device_pairing(ptr);
  }
}
extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_storeAttachmentPreviewBytes(JNIEnv* env,
                                                               jobject,
                                                               jlong handle,
                                                               jstring file_id,
                                                               jstring file_name,
                                                               jlong file_size,
                                                               jbyteArray bytes) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string id = JStringToString(env, file_id);
  std::string name = JStringToString(env, file_name);
  std::vector<std::uint8_t> data = JByteArrayToVector(env, bytes);
  return static_cast<jboolean>(
      mi_client_store_attachment_preview_bytes(
          ptr, id.c_str(), name.c_str(), static_cast<std::uint64_t>(file_size),
          data.data(), static_cast<std::uint32_t>(data.size())));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_downloadChatFileToPath(JNIEnv* env,
                                                          jobject,
                                                          jlong handle,
                                                          jstring file_id,
                                                          jbyteArray file_key,
                                                          jstring file_name,
                                                          jlong file_size,
                                                          jstring out_path,
                                                          jboolean wipe_after_read) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string id = JStringToString(env, file_id);
  std::string name = JStringToString(env, file_name);
  std::string path = JStringToString(env, out_path);
  std::vector<std::uint8_t> key = JByteArrayToVector(env, file_key);
  return static_cast<jboolean>(
      mi_client_download_chat_file_to_path(
          ptr, id.c_str(), key.data(), static_cast<std::uint32_t>(key.size()),
          name.c_str(), static_cast<std::uint64_t>(file_size), path.c_str(),
          wipe_after_read ? 1 : 0, nullptr, nullptr));
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_downloadChatFileToBytes(JNIEnv* env,
                                                           jobject,
                                                           jlong handle,
                                                           jstring file_id,
                                                           jbyteArray file_key,
                                                           jstring file_name,
                                                           jlong file_size,
                                                           jboolean wipe_after_read) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string id = JStringToString(env, file_id);
  std::string name = JStringToString(env, file_name);
  std::vector<std::uint8_t> key = JByteArrayToVector(env, file_key);
  std::uint8_t* out_bytes = nullptr;
  std::uint64_t out_len = 0;
  int ok = mi_client_download_chat_file_to_bytes(
      ptr, id.c_str(), key.data(), static_cast<std::uint32_t>(key.size()),
      name.c_str(), static_cast<std::uint64_t>(file_size),
      wipe_after_read ? 1 : 0, &out_bytes, &out_len);
  if (!ok || !out_bytes) {
    if (out_bytes) {
      mi_client_free(out_bytes);
    }
    return nullptr;
  }
  if (out_len > static_cast<std::uint64_t>(std::numeric_limits<jsize>::max())) {
    mi_client_free(out_bytes);
    return nullptr;
  }
  jbyteArray arr = env->NewByteArray(static_cast<jsize>(out_len));
  if (arr && out_len > 0) {
    env->SetByteArrayRegion(
        arr, 0, static_cast<jsize>(out_len),
        reinterpret_cast<const jbyte*>(out_bytes));
  }
  mi_client_free(out_bytes);
  return arr;
}
extern "C" JNIEXPORT jobject JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_getMediaConfig(JNIEnv* env, jobject, jlong handle) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  mi_media_config_t config{};
  int ok = mi_client_get_media_config(ptr, &config);
  if (!ok) {
    return nullptr;
  }
  return NewMediaConfig(env, config);
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_deriveMediaRoot(JNIEnv* env,
                                                   jobject,
                                                   jlong handle,
                                                   jstring peer_username,
                                                   jbyteArray call_id) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return nullptr;
  std::string peer = JStringToString(env, peer_username);
  std::vector<std::uint8_t> call = JByteArrayToVector(env, call_id);
  std::array<std::uint8_t, kMediaRootLen> out{};
  int ok = mi_client_derive_media_root(
      ptr, peer.c_str(), call.data(), static_cast<std::uint32_t>(call.size()),
      out.data(), kMediaRootLen);
  if (!ok) {
    return nullptr;
  }
  return ToJByteArray(env, out.data(), kMediaRootLen);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_pushMedia(JNIEnv* env,
                                             jobject,
                                             jlong handle,
                                             jstring peer_username,
                                             jbyteArray call_id,
                                             jbyteArray packet) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string peer = JStringToString(env, peer_username);
  std::vector<std::uint8_t> call = JByteArrayToVector(env, call_id);
  std::vector<std::uint8_t> data = JByteArrayToVector(env, packet);
  return static_cast<jboolean>(
      mi_client_push_media(
          ptr, peer.c_str(), call.data(), static_cast<std::uint32_t>(call.size()),
          data.data(), static_cast<std::uint32_t>(data.size())));
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_pullMedia(JNIEnv* env,
                                             jobject,
                                             jlong handle,
                                             jbyteArray call_id,
                                             jint max_packets,
                                             jint wait_ms) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr || max_packets <= 0) {
    return env->NewObjectArray(0, g_cache.clsMediaPacket, nullptr);
  }
  std::vector<std::uint8_t> call = JByteArrayToVector(env, call_id);
  std::vector<mi_media_packet_t> packets(static_cast<std::size_t>(max_packets));
  std::uint32_t count = mi_client_pull_media(
      ptr,
      call.data(),
      static_cast<std::uint32_t>(call.size()),
      static_cast<std::uint32_t>(max_packets),
      static_cast<std::uint32_t>(std::max(0, wait_ms)),
      packets.data());
  jobjectArray arr = env->NewObjectArray(
      static_cast<jsize>(count), g_cache.clsMediaPacket, nullptr);
  for (std::uint32_t i = 0; i < count; ++i) {
    jobject obj = NewMediaPacket(env, packets[i]);
    env->SetObjectArrayElement(arr, static_cast<jsize>(i), obj);
    env->DeleteLocalRef(obj);
  }
  return arr;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_pushGroupMedia(JNIEnv* env,
                                                  jobject,
                                                  jlong handle,
                                                  jstring group_id,
                                                  jbyteArray call_id,
                                                  jbyteArray packet) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::string group = JStringToString(env, group_id);
  std::vector<std::uint8_t> call = JByteArrayToVector(env, call_id);
  std::vector<std::uint8_t> data = JByteArrayToVector(env, packet);
  return static_cast<jboolean>(
      mi_client_push_group_media(
          ptr, group.c_str(), call.data(), static_cast<std::uint32_t>(call.size()),
          data.data(), static_cast<std::uint32_t>(data.size())));
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_pullGroupMedia(JNIEnv* env,
                                                  jobject,
                                                  jlong handle,
                                                  jbyteArray call_id,
                                                  jint max_packets,
                                                  jint wait_ms) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr || max_packets <= 0) {
    return env->NewObjectArray(0, g_cache.clsMediaPacket, nullptr);
  }
  std::vector<std::uint8_t> call = JByteArrayToVector(env, call_id);
  std::vector<mi_media_packet_t> packets(static_cast<std::size_t>(max_packets));
  std::uint32_t count = mi_client_pull_group_media(
      ptr,
      call.data(),
      static_cast<std::uint32_t>(call.size()),
      static_cast<std::uint32_t>(max_packets),
      static_cast<std::uint32_t>(std::max(0, wait_ms)),
      packets.data());
  jobjectArray arr = env->NewObjectArray(
      static_cast<jsize>(count), g_cache.clsMediaPacket, nullptr);
  for (std::uint32_t i = 0; i < count; ++i) {
    jobject obj = NewMediaPacket(env, packets[i]);
    env->SetObjectArrayElement(arr, static_cast<jsize>(i), obj);
    env->DeleteLocalRef(obj);
  }
  return arr;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_addMediaSubscription(JNIEnv* env,
                                                        jobject,
                                                        jlong handle,
                                                        jbyteArray call_id,
                                                        jboolean is_group,
                                                        jstring group_id) {
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr) return JNI_FALSE;
  std::vector<std::uint8_t> call = JByteArrayToVector(env, call_id);
  const char* group = nullptr;
  std::string group_str;
  if (is_group && group_id) {
    group_str = JStringToString(env, group_id);
    group = group_str.c_str();
  }
  return static_cast<jboolean>(
      mi_client_add_media_subscription(
          ptr, call.data(), static_cast<std::uint32_t>(call.size()),
          is_group ? 1 : 0, group));
}

extern "C" JNIEXPORT void JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_clearMediaSubscriptions(JNIEnv*, jobject, jlong handle) {
  mi_client_handle* ptr = FromHandle(handle);
  if (ptr) {
    mi_client_clear_media_subscriptions(ptr);
  }
}
extern "C" JNIEXPORT jobjectArray JNICALL
Java_mi_e2ee_android_sdk_NativeSdk_pollEvents(JNIEnv* env,
                                              jobject,
                                              jlong handle,
                                              jint max_events,
                                              jint wait_ms) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  mi_client_handle* ptr = FromHandle(handle);
  if (!ptr || max_events <= 0) {
    return env->NewObjectArray(0, g_cache.clsSdkEvent, nullptr);
  }
  std::vector<mi_event_t> events(static_cast<std::size_t>(max_events));
  std::uint32_t count = mi_client_poll_event(
      ptr,
      events.data(),
      static_cast<std::uint32_t>(max_events),
      static_cast<std::uint32_t>(std::max(0, wait_ms)));
  jobjectArray arr = env->NewObjectArray(
      static_cast<jsize>(count), g_cache.clsSdkEvent, nullptr);
  for (std::uint32_t i = 0; i < count; ++i) {
    jobject obj = NewSdkEvent(env, events[i]);
    env->SetObjectArrayElement(arr, static_cast<jsize>(i), obj);
    env->DeleteLocalRef(obj);
  }
  return arr;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  JNIEnv* env = nullptr;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;
  }
  EnsureCache(env);
  return JNI_VERSION_1_6;
}
