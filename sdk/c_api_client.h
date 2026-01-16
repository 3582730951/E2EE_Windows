#ifndef MI_E2EE_CLIENT_C_API_H
#define MI_E2EE_CLIENT_C_API_H

#include <cstddef>
#include <cstdint>

#define MI_E2EE_SDK_ABI_VERSION 1
#define MI_E2EE_SDK_VERSION_MAJOR 1
#define MI_E2EE_SDK_VERSION_MINOR 0
#define MI_E2EE_SDK_VERSION_PATCH 0

#if defined(_WIN32)
#if defined(MI_E2EE_SDK_EXPORTS)
#define MI_E2EE_SDK_API __declspec(dllexport)
#elif defined(MI_E2EE_SDK_IMPORTS)
#define MI_E2EE_SDK_API __declspec(dllimport)
#else
#define MI_E2EE_SDK_API
#endif
#else
#if defined(__GNUC__) || defined(__clang__)
#define MI_E2EE_SDK_API __attribute__((visibility("default")))
#else
#define MI_E2EE_SDK_API
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mi_client_handle mi_client_handle;

#ifndef MI_E2EE_SDK_VERSION_DEFINED
#define MI_E2EE_SDK_VERSION_DEFINED
typedef struct mi_sdk_version {
  std::uint32_t major;
  std::uint32_t minor;
  std::uint32_t patch;
  std::uint32_t abi;
} mi_sdk_version;
#endif

typedef enum mi_event_type {
  MI_EVENT_NONE = 0,
  MI_EVENT_CHAT_TEXT = 1,
  MI_EVENT_CHAT_FILE = 2,
  MI_EVENT_CHAT_STICKER = 3,
  MI_EVENT_GROUP_TEXT = 4,
  MI_EVENT_GROUP_FILE = 5,
  MI_EVENT_GROUP_INVITE = 6,
  MI_EVENT_GROUP_NOTICE = 7,
  MI_EVENT_OUTGOING_TEXT = 8,
  MI_EVENT_OUTGOING_FILE = 9,
  MI_EVENT_OUTGOING_STICKER = 10,
  MI_EVENT_OUTGOING_GROUP_TEXT = 11,
  MI_EVENT_OUTGOING_GROUP_FILE = 12,
  MI_EVENT_DELIVERY = 13,
  MI_EVENT_READ_RECEIPT = 14,
  MI_EVENT_TYPING = 15,
  MI_EVENT_PRESENCE = 16,
  MI_EVENT_GROUP_CALL = 17,
  MI_EVENT_MEDIA_RELAY = 18,
  MI_EVENT_GROUP_MEDIA_RELAY = 19,
  MI_EVENT_OFFLINE_PAYLOAD = 20
} mi_event_type;

typedef enum mi_client_capability {
  MI_CLIENT_CAP_CHAT = 1u << 0,
  MI_CLIENT_CAP_GROUP = 1u << 1,
  MI_CLIENT_CAP_MEDIA = 1u << 2,
  MI_CLIENT_CAP_GROUP_CALL = 1u << 3,
  MI_CLIENT_CAP_OFFLINE = 1u << 4,
  MI_CLIENT_CAP_DEVICE_SYNC = 1u << 5,
  MI_CLIENT_CAP_KCP = 1u << 6,
  MI_CLIENT_CAP_OPAQUE = 1u << 7
} mi_client_capability;

typedef struct mi_event_t {
  std::uint32_t type;
  std::uint64_t ts_ms;
  const char* peer;
  const char* sender;
  const char* group_id;
  const char* message_id;
  const char* text;
  const char* file_id;
  const char* file_name;
  std::uint64_t file_size;
  const std::uint8_t* file_key;
  std::uint32_t file_key_len;
  const char* sticker_id;
  std::uint32_t notice_kind;
  const char* actor;
  const char* target;
  std::uint32_t role;
  std::uint8_t typing;
  std::uint8_t online;
  std::uint8_t reserved0;
  std::uint8_t reserved1;
  std::uint8_t call_id[16];
  std::uint32_t call_key_id;
  std::uint32_t call_op;
  std::uint8_t call_media_flags;
  std::uint8_t call_reserved0;
  std::uint8_t call_reserved1;
  std::uint8_t call_reserved2;
  const std::uint8_t* payload;
  std::uint32_t payload_len;
} mi_event_t;

typedef struct mi_friend_entry_t {
  const char* username;
  const char* remark;
} mi_friend_entry_t;

typedef struct mi_friend_request_entry_t {
  const char* requester_username;
  const char* requester_remark;
} mi_friend_request_entry_t;

typedef struct mi_device_entry_t {
  const char* device_id;
  std::uint32_t last_seen_sec;
} mi_device_entry_t;

typedef struct mi_device_pairing_request_t {
  const char* device_id;
  const char* request_id_hex;
} mi_device_pairing_request_t;

typedef struct mi_group_member_entry_t {
  const char* username;
  std::uint32_t role;
} mi_group_member_entry_t;

typedef struct mi_group_call_member_t {
  const char* username;
} mi_group_call_member_t;

typedef struct mi_media_packet_t {
  const char* sender;
  const std::uint8_t* payload;
  std::uint32_t payload_len;
} mi_media_packet_t;

typedef struct mi_media_config_t {
  std::uint32_t audio_delay_ms;
  std::uint32_t video_delay_ms;
  std::uint32_t audio_max_frames;
  std::uint32_t video_max_frames;
  std::uint32_t pull_max_packets;
  std::uint32_t pull_wait_ms;
  std::uint32_t group_pull_max_packets;
  std::uint32_t group_pull_wait_ms;
} mi_media_config_t;

typedef struct mi_history_entry_t {
  std::uint32_t kind;
  std::uint32_t status;
  std::uint8_t is_group;
  std::uint8_t outgoing;
  std::uint8_t reserved0;
  std::uint8_t reserved1;
  std::uint64_t timestamp_sec;
  const char* conv_id;
  const char* sender;
  const char* message_id;
  const char* text;
  const char* file_id;
  const std::uint8_t* file_key;
  std::uint32_t file_key_len;
  const char* file_name;
  std::uint64_t file_size;
  const char* sticker_id;
} mi_history_entry_t;

typedef void (*mi_progress_callback_t)(std::uint64_t done,
                                       std::uint64_t total,
                                       void* user_data);

MI_E2EE_SDK_API void mi_client_get_version(mi_sdk_version* out_version);
MI_E2EE_SDK_API std::uint32_t mi_client_get_capabilities(void);

// config_path defaults to "config/client_config.ini" when null or empty.
MI_E2EE_SDK_API mi_client_handle* mi_client_create(const char* config_path);
MI_E2EE_SDK_API const char* mi_client_last_create_error(void);
MI_E2EE_SDK_API void mi_client_destroy(mi_client_handle* handle);

MI_E2EE_SDK_API const char* mi_client_last_error(mi_client_handle* handle);
MI_E2EE_SDK_API const char* mi_client_token(mi_client_handle* handle);
MI_E2EE_SDK_API const char* mi_client_device_id(mi_client_handle* handle);
MI_E2EE_SDK_API int mi_client_remote_ok(mi_client_handle* handle);
MI_E2EE_SDK_API const char* mi_client_remote_error(mi_client_handle* handle);
MI_E2EE_SDK_API int mi_client_is_remote_mode(mi_client_handle* handle);
MI_E2EE_SDK_API int mi_client_relogin(mi_client_handle* handle);

MI_E2EE_SDK_API int mi_client_has_pending_server_trust(mi_client_handle* handle);
MI_E2EE_SDK_API const char* mi_client_pending_server_fingerprint(mi_client_handle* handle);
MI_E2EE_SDK_API const char* mi_client_pending_server_pin(mi_client_handle* handle);
MI_E2EE_SDK_API int mi_client_trust_pending_server(mi_client_handle* handle, const char* pin);

MI_E2EE_SDK_API int mi_client_has_pending_peer_trust(mi_client_handle* handle);
MI_E2EE_SDK_API const char* mi_client_pending_peer_username(mi_client_handle* handle);
MI_E2EE_SDK_API const char* mi_client_pending_peer_fingerprint(mi_client_handle* handle);
MI_E2EE_SDK_API const char* mi_client_pending_peer_pin(mi_client_handle* handle);
MI_E2EE_SDK_API int mi_client_trust_pending_peer(mi_client_handle* handle, const char* pin);

MI_E2EE_SDK_API int mi_client_register(mi_client_handle* handle,
                       const char* username,
                       const char* password);
MI_E2EE_SDK_API int mi_client_login(mi_client_handle* handle,
                    const char* username,
                    const char* password);
MI_E2EE_SDK_API int mi_client_logout(mi_client_handle* handle);
MI_E2EE_SDK_API int mi_client_heartbeat(mi_client_handle* handle);

MI_E2EE_SDK_API int mi_client_send_private_text(mi_client_handle* handle,
                                const char* peer_username,
                                const char* text_utf8,
                                char** out_message_id_hex);
MI_E2EE_SDK_API int mi_client_send_private_text_with_reply(mi_client_handle* handle,
                                           const char* peer_username,
                                           const char* text_utf8,
                                           const char* reply_to_message_id_hex,
                                           const char* reply_preview_utf8,
                                           char** out_message_id_hex);
MI_E2EE_SDK_API int mi_client_resend_private_text(mi_client_handle* handle,
                                  const char* peer_username,
                                  const char* message_id_hex,
                                  const char* text_utf8);
MI_E2EE_SDK_API int mi_client_resend_private_text_with_reply(mi_client_handle* handle,
                                             const char* peer_username,
                                             const char* message_id_hex,
                                             const char* text_utf8,
                                             const char* reply_to_message_id_hex,
                                             const char* reply_preview_utf8);
MI_E2EE_SDK_API int mi_client_send_group_text(mi_client_handle* handle,
                              const char* group_id,
                              const char* text_utf8,
                              char** out_message_id_hex);
MI_E2EE_SDK_API int mi_client_resend_group_text(mi_client_handle* handle,
                                const char* group_id,
                                const char* message_id_hex,
                                const char* text_utf8);
MI_E2EE_SDK_API int mi_client_send_private_file(mi_client_handle* handle,
                                const char* peer_username,
                                const char* file_path_utf8,
                                char** out_message_id_hex);
MI_E2EE_SDK_API int mi_client_resend_private_file(mi_client_handle* handle,
                                  const char* peer_username,
                                  const char* message_id_hex,
                                  const char* file_path_utf8);
MI_E2EE_SDK_API int mi_client_send_group_file(mi_client_handle* handle,
                              const char* group_id,
                              const char* file_path_utf8,
                              char** out_message_id_hex);
MI_E2EE_SDK_API int mi_client_resend_group_file(mi_client_handle* handle,
                                const char* group_id,
                                const char* message_id_hex,
                                const char* file_path_utf8);
MI_E2EE_SDK_API int mi_client_send_private_sticker(mi_client_handle* handle,
                                   const char* peer_username,
                                   const char* sticker_id,
                                   char** out_message_id_hex);
MI_E2EE_SDK_API int mi_client_resend_private_sticker(mi_client_handle* handle,
                                     const char* peer_username,
                                     const char* message_id_hex,
                                     const char* sticker_id);
MI_E2EE_SDK_API int mi_client_send_private_location(mi_client_handle* handle,
                                    const char* peer_username,
                                    std::int32_t lat_e7,
                                    std::int32_t lon_e7,
                                    const char* label_utf8,
                                    char** out_message_id_hex);
MI_E2EE_SDK_API int mi_client_resend_private_location(mi_client_handle* handle,
                                      const char* peer_username,
                                      const char* message_id_hex,
                                      std::int32_t lat_e7,
                                      std::int32_t lon_e7,
                                      const char* label_utf8);
MI_E2EE_SDK_API int mi_client_send_private_contact(mi_client_handle* handle,
                                   const char* peer_username,
                                   const char* card_username,
                                   const char* card_display,
                                   char** out_message_id_hex);
MI_E2EE_SDK_API int mi_client_resend_private_contact(mi_client_handle* handle,
                                     const char* peer_username,
                                     const char* message_id_hex,
                                     const char* card_username,
                                     const char* card_display);
MI_E2EE_SDK_API int mi_client_send_read_receipt(mi_client_handle* handle,
                                const char* peer_username,
                                const char* message_id_hex);
MI_E2EE_SDK_API int mi_client_send_typing(mi_client_handle* handle,
                          const char* peer_username,
                          int typing);
MI_E2EE_SDK_API int mi_client_send_presence(mi_client_handle* handle,
                            const char* peer_username,
                            int online);

MI_E2EE_SDK_API int mi_client_add_friend(mi_client_handle* handle,
                         const char* friend_username,
                         const char* remark);
MI_E2EE_SDK_API int mi_client_set_friend_remark(mi_client_handle* handle,
                                const char* friend_username,
                                const char* remark);
MI_E2EE_SDK_API int mi_client_delete_friend(mi_client_handle* handle,
                            const char* friend_username);
MI_E2EE_SDK_API int mi_client_set_user_blocked(mi_client_handle* handle,
                               const char* blocked_username,
                               int blocked);
MI_E2EE_SDK_API int mi_client_send_friend_request(mi_client_handle* handle,
                                  const char* target_username,
                                  const char* remark);
MI_E2EE_SDK_API int mi_client_respond_friend_request(mi_client_handle* handle,
                                     const char* requester_username,
                                     int accept);
MI_E2EE_SDK_API std::uint32_t mi_client_list_friends(mi_client_handle* handle,
                                     mi_friend_entry_t* out_entries,
                                     std::uint32_t max_entries);
MI_E2EE_SDK_API std::uint32_t mi_client_sync_friends(mi_client_handle* handle,
                                     mi_friend_entry_t* out_entries,
                                     std::uint32_t max_entries,
                                     int* out_changed);
MI_E2EE_SDK_API std::uint32_t mi_client_list_friend_requests(
    mi_client_handle* handle,
    mi_friend_request_entry_t* out_entries,
    std::uint32_t max_entries);
MI_E2EE_SDK_API std::uint32_t mi_client_list_devices(mi_client_handle* handle,
                                     mi_device_entry_t* out_entries,
                                     std::uint32_t max_entries);
MI_E2EE_SDK_API int mi_client_kick_device(mi_client_handle* handle,
                          const char* device_id);
MI_E2EE_SDK_API int mi_client_join_group(mi_client_handle* handle, const char* group_id);
MI_E2EE_SDK_API int mi_client_leave_group(mi_client_handle* handle, const char* group_id);
MI_E2EE_SDK_API int mi_client_create_group(mi_client_handle* handle, char** out_group_id);
MI_E2EE_SDK_API int mi_client_send_group_invite(mi_client_handle* handle,
                                const char* group_id,
                                const char* peer_username,
                                char** out_message_id_hex);
MI_E2EE_SDK_API std::uint32_t mi_client_list_group_members_info(
    mi_client_handle* handle,
    const char* group_id,
    mi_group_member_entry_t* out_entries,
    std::uint32_t max_entries);
MI_E2EE_SDK_API int mi_client_set_group_member_role(mi_client_handle* handle,
                                    const char* group_id,
                                    const char* peer_username,
                                    std::uint32_t role);
MI_E2EE_SDK_API int mi_client_kick_group_member(mi_client_handle* handle,
                                const char* group_id,
                                const char* peer_username);
MI_E2EE_SDK_API int mi_client_start_group_call(mi_client_handle* handle,
                               const char* group_id,
                               int video,
                               std::uint8_t* out_call_id,
                               std::uint32_t out_call_id_len,
                               std::uint32_t* out_key_id);
MI_E2EE_SDK_API int mi_client_join_group_call(mi_client_handle* handle,
                              const char* group_id,
                              const std::uint8_t* call_id,
                              std::uint32_t call_id_len,
                              int video,
                              std::uint32_t* out_key_id);
MI_E2EE_SDK_API int mi_client_leave_group_call(mi_client_handle* handle,
                               const char* group_id,
                               const std::uint8_t* call_id,
                               std::uint32_t call_id_len);
MI_E2EE_SDK_API int mi_client_get_group_call_key(mi_client_handle* handle,
                                 const char* group_id,
                                 const std::uint8_t* call_id,
                                 std::uint32_t call_id_len,
                                 std::uint32_t key_id,
                                 std::uint8_t* out_key,
                                 std::uint32_t out_key_len);
MI_E2EE_SDK_API int mi_client_rotate_group_call_key(mi_client_handle* handle,
                                    const char* group_id,
                                    const std::uint8_t* call_id,
                                    std::uint32_t call_id_len,
                                    std::uint32_t key_id,
                                    const char** members,
                                    std::uint32_t member_count);
MI_E2EE_SDK_API int mi_client_request_group_call_key(mi_client_handle* handle,
                                     const char* group_id,
                                     const std::uint8_t* call_id,
                                     std::uint32_t call_id_len,
                                     std::uint32_t key_id,
                                     const char** members,
                                     std::uint32_t member_count);
MI_E2EE_SDK_API int mi_client_send_group_call_signal(mi_client_handle* handle,
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
                                     std::uint32_t* out_member_count);
MI_E2EE_SDK_API std::uint32_t mi_client_load_chat_history(mi_client_handle* handle,
                                          const char* conv_id,
                                          int is_group,
                                          std::uint32_t limit,
                                          mi_history_entry_t* out_entries,
                                          std::uint32_t max_entries);
MI_E2EE_SDK_API int mi_client_delete_chat_history(mi_client_handle* handle,
                                  const char* conv_id,
                                  int is_group,
                                  int delete_attachments,
                                  int secure_wipe);
MI_E2EE_SDK_API int mi_client_set_history_enabled(mi_client_handle* handle,
                                  int enabled);
MI_E2EE_SDK_API int mi_client_clear_all_history(mi_client_handle* handle,
                                int delete_attachments,
                                int secure_wipe);
MI_E2EE_SDK_API int mi_client_begin_device_pairing_primary(mi_client_handle* handle,
                                           char** out_pairing_code);
MI_E2EE_SDK_API std::uint32_t mi_client_poll_device_pairing_requests(
    mi_client_handle* handle,
    mi_device_pairing_request_t* out_entries,
    std::uint32_t max_entries);
MI_E2EE_SDK_API int mi_client_approve_device_pairing_request(mi_client_handle* handle,
                                             const char* device_id,
                                             const char* request_id_hex);
MI_E2EE_SDK_API int mi_client_begin_device_pairing_linked(mi_client_handle* handle,
                                          const char* pairing_code);
MI_E2EE_SDK_API int mi_client_poll_device_pairing_linked(mi_client_handle* handle,
                                         int* out_completed);
MI_E2EE_SDK_API void mi_client_cancel_device_pairing(mi_client_handle* handle);
MI_E2EE_SDK_API int mi_client_store_attachment_preview_bytes(mi_client_handle* handle,
                                             const char* file_id,
                                             const char* file_name,
                                             std::uint64_t file_size,
                                             const std::uint8_t* bytes,
                                             std::uint32_t bytes_len);
MI_E2EE_SDK_API int mi_client_download_chat_file_to_path(mi_client_handle* handle,
                                         const char* file_id,
                                         const std::uint8_t* file_key,
                                         std::uint32_t file_key_len,
                                         const char* file_name,
                                         std::uint64_t file_size,
                                         const char* out_path_utf8,
                                         int wipe_after_read,
                                         mi_progress_callback_t on_progress,
                                         void* user_data);
MI_E2EE_SDK_API int mi_client_download_chat_file_to_bytes(mi_client_handle* handle,
                                          const char* file_id,
                                          const std::uint8_t* file_key,
                                          std::uint32_t file_key_len,
                                          const char* file_name,
                                          std::uint64_t file_size,
                                          int wipe_after_read,
                                          std::uint8_t** out_bytes,
                                          std::uint64_t* out_len);

MI_E2EE_SDK_API int mi_client_get_media_config(mi_client_handle* handle,
                               mi_media_config_t* out_config);
MI_E2EE_SDK_API int mi_client_derive_media_root(mi_client_handle* handle,
                                const char* peer_username,
                                const std::uint8_t* call_id,
                                std::uint32_t call_id_len,
                                std::uint8_t* out_media_root,
                                std::uint32_t out_media_root_len);
MI_E2EE_SDK_API int mi_client_push_media(mi_client_handle* handle,
                         const char* peer_username,
                         const std::uint8_t* call_id,
                         std::uint32_t call_id_len,
                         const std::uint8_t* packet,
                         std::uint32_t packet_len);
MI_E2EE_SDK_API std::uint32_t mi_client_pull_media(mi_client_handle* handle,
                                   const std::uint8_t* call_id,
                                   std::uint32_t call_id_len,
                                   std::uint32_t max_packets,
                                   std::uint32_t wait_ms,
                                   mi_media_packet_t* out_packets);
MI_E2EE_SDK_API int mi_client_push_group_media(mi_client_handle* handle,
                               const char* group_id,
                               const std::uint8_t* call_id,
                               std::uint32_t call_id_len,
                               const std::uint8_t* packet,
                               std::uint32_t packet_len);
MI_E2EE_SDK_API std::uint32_t mi_client_pull_group_media(mi_client_handle* handle,
                                         const std::uint8_t* call_id,
                                         std::uint32_t call_id_len,
                                         std::uint32_t max_packets,
                                         std::uint32_t wait_ms,
                                         mi_media_packet_t* out_packets);

// call_id_len must be 16. group_id is only used when is_group != 0.
MI_E2EE_SDK_API int mi_client_add_media_subscription(mi_client_handle* handle,
                                     const std::uint8_t* call_id,
                                     std::uint32_t call_id_len,
                                     int is_group,
                                     const char* group_id);
MI_E2EE_SDK_API void mi_client_clear_media_subscriptions(mi_client_handle* handle);

// out_events is valid until the next mi_client_poll_event call or destroy.
// wait_ms is a strict upper bound on blocking time; 0 means non-blocking.
MI_E2EE_SDK_API std::uint32_t mi_client_poll_event(mi_client_handle* handle,
                                   mi_event_t* out_events,
                                   std::uint32_t max_events,
                                   std::uint32_t wait_ms);

MI_E2EE_SDK_API void mi_client_free(void* buf);

#ifdef __cplusplus
}

namespace mi::client {
class ClientCore;
}

mi_client_handle* mi_client_wrap_cpp(mi::client::ClientCore* core);
void mi_client_unwrap_cpp(mi_client_handle* handle);
#endif

#endif  // MI_E2EE_CLIENT_C_API_H
