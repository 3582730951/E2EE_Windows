#include "mi_client_ios_bridge.h"

#include <cstdlib>
#include <cstring>
#include <string>

struct mi_client_handle {
  std::string configPath;
  std::string lastError;
  std::string remoteError;
  std::string rootAuthKey;
};

namespace {

std::string gLastCreateError;

const char* CStringOrNull(const std::string& value) {
  return value.empty() ? nullptr : value.c_str();
}

bool EnsureHandle(mi_client_handle* handle) {
  return handle != nullptr;
}

void SetUnavailable(mi_client_handle* handle) {
  if (!handle) {
    return;
  }
  handle->lastError = "Native iOS client bridge is not linked.";
  handle->remoteError = handle->lastError;
}

bool EnsureAuthenticated(mi_client_handle* handle) {
  if (!handle) {
    return false;
  }
  SetUnavailable(handle);
  return false;
}

bool Authenticate(mi_client_handle* handle,
                  const char* username,
                  const char* password,
                  const char* rootCode) {
  (void)rootCode;
  if (!EnsureHandle(handle)) {
    return false;
  }
  const std::string user = username ? username : "";
  const std::string pass = password ? password : "";
  if (user.empty() || pass.empty()) {
    handle->lastError = "Username and password are required.";
    handle->remoteError = handle->lastError;
    return false;
  }
  SetUnavailable(handle);
  return false;
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
  (void)handle;
  return "";
}

MI_E2EE_SDK_API const char* mi_client_device_id(mi_client_handle* handle) {
  (void)handle;
  return "";
}

MI_E2EE_SDK_API const char* mi_client_device_display_id(mi_client_handle* handle) {
  (void)handle;
  return "";
}

MI_E2EE_SDK_API int mi_client_remote_ok(mi_client_handle* handle) {
  (void)handle;
  return 0;
}

MI_E2EE_SDK_API const char* mi_client_remote_error(mi_client_handle* handle) {
  return handle ? CStringOrNull(handle->remoteError) : "";
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
  return EnsureAuthenticated(handle) ? 1 : 0;
}

MI_E2EE_SDK_API int mi_client_heartbeat(mi_client_handle* handle) {
  return EnsureAuthenticated(handle) ? 1 : 0;
}

MI_E2EE_SDK_API int mi_client_send_private_text(mi_client_handle* handle,
                                                const char* peer_username,
                                                const char* text_utf8,
                                                char** out_message_id_hex) {
  (void)peer_username;
  (void)text_utf8;
  if (out_message_id_hex) {
    *out_message_id_hex = nullptr;
  }
  return EnsureAuthenticated(handle) ? 1 : 0;
}

MI_E2EE_SDK_API int mi_client_send_group_text(mi_client_handle* handle,
                                              const char* group_id,
                                              const char* text_utf8,
                                              char** out_message_id_hex) {
  (void)group_id;
  (void)text_utf8;
  if (out_message_id_hex) {
    *out_message_id_hex = nullptr;
  }
  return EnsureAuthenticated(handle) ? 1 : 0;
}

MI_E2EE_SDK_API std::uint32_t mi_client_list_friends(mi_client_handle* handle,
                                                     mi_friend_entry_t* out_entries,
                                                     std::uint32_t max_entries) {
  (void)out_entries;
  (void)max_entries;
  EnsureAuthenticated(handle);
  return 0;
}

MI_E2EE_SDK_API std::uint32_t mi_client_list_devices(mi_client_handle* handle,
                                                     mi_device_entry_t* out_entries,
                                                     std::uint32_t max_entries) {
  (void)out_entries;
  (void)max_entries;
  EnsureAuthenticated(handle);
  return 0;
}

MI_E2EE_SDK_API int mi_client_join_group(mi_client_handle* handle, const char* group_id) {
  (void)group_id;
  return EnsureAuthenticated(handle) ? 1 : 0;
}

MI_E2EE_SDK_API int mi_client_leave_group(mi_client_handle* handle, const char* group_id) {
  (void)group_id;
  return EnsureAuthenticated(handle) ? 1 : 0;
}

MI_E2EE_SDK_API int mi_client_create_group(mi_client_handle* handle, char** out_group_id) {
  if (out_group_id) {
    *out_group_id = nullptr;
  }
  return EnsureAuthenticated(handle) ? 1 : 0;
}

MI_E2EE_SDK_API int mi_client_send_group_invite(mi_client_handle* handle,
                                                const char* group_id,
                                                const char* peer_username,
                                                char** out_message_id_hex) {
  (void)group_id;
  (void)peer_username;
  if (out_message_id_hex) {
    *out_message_id_hex = nullptr;
  }
  return EnsureAuthenticated(handle) ? 1 : 0;
}

MI_E2EE_SDK_API std::uint32_t mi_client_list_group_members_info(
    mi_client_handle* handle,
    const char* group_id,
    mi_group_member_entry_t* out_entries,
    std::uint32_t max_entries) {
  (void)group_id;
  (void)out_entries;
  (void)max_entries;
  EnsureAuthenticated(handle);
  return 0;
}

MI_E2EE_SDK_API std::uint32_t mi_client_load_chat_history(mi_client_handle* handle,
                                                          const char* conv_id,
                                                          int is_group,
                                                          std::uint32_t limit,
                                                          mi_history_entry_t* out_entries,
                                                          std::uint32_t max_entries) {
  (void)conv_id;
  (void)is_group;
  (void)limit;
  (void)out_entries;
  (void)max_entries;
  EnsureAuthenticated(handle);
  return 0;
}

MI_E2EE_SDK_API std::uint32_t mi_client_poll_event(mi_client_handle* handle,
                                                   mi_event_t* out_events,
                                                   std::uint32_t max_events,
                                                   std::uint32_t wait_ms) {
  (void)out_events;
  (void)max_events;
  (void)wait_ms;
  EnsureAuthenticated(handle);
  return 0;
}

MI_E2EE_SDK_API void mi_client_free(void* buf) {
  std::free(buf);
}

}  // extern "C"
