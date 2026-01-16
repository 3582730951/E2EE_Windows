#include "c_api_client.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "path_security.h"

namespace {

std::string WriteTestConfig() {
  const auto path =
      std::filesystem::current_path() / "test_client_config_status.ini";
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << "[client]\n";
  out << "server_ip=127.0.0.1\n";
  out << "server_port=9000\n";
  out << "use_tls=1\n";
  out << "require_tls=1\n";
  out << "require_pinned_fingerprint=1\n";
  out << "auth_mode=opaque\n";
  out << "\n[kt]\n";
  out << "require_signature=0\n";
  out.flush();
  return path.string();
}

void RemoveConfig(const std::string& path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

bool SetDataDirEnv(const std::filesystem::path& dir) {
  const std::string value = dir.string();
#ifdef _WIN32
  return _putenv_s("MI_E2EE_DATA_DIR", value.c_str()) == 0;
#else
  return ::setenv("MI_E2EE_DATA_DIR", value.c_str(), 1) == 0;
#endif
}

bool PrepareDataDir(std::string& error) {
  error.clear();
  const auto dir =
      std::filesystem::current_path() / "test_client_data_status";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    error = "data dir create failed";
    return false;
  }
#ifdef _WIN32
  std::string perm_err;
  if (!mi::shard::security::HardenPathAcl(dir, perm_err)) {
    error = perm_err.empty() ? "data dir acl failed" : perm_err;
    return false;
  }
#else
  std::filesystem::permissions(
      dir,
      std::filesystem::perms::owner_read |
          std::filesystem::perms::owner_write |
          std::filesystem::perms::owner_exec,
      std::filesystem::perm_options::replace, ec);
  if (ec) {
    error = "data dir perms failed";
    return false;
  }
#endif
  if (!SetDataDirEnv(dir)) {
    error = "data dir env failed";
    return false;
  }
  return true;
}

}  // namespace

int main() {
  assert(std::strcmp(mi_client_last_error(nullptr), "") == 0);
  assert(std::strcmp(mi_client_token(nullptr), "") == 0);
  assert(std::strcmp(mi_client_device_id(nullptr), "") == 0);
  assert(std::strcmp(mi_client_last_create_error(), "") == 0);
  assert(mi_client_remote_ok(nullptr) == 0);
  assert(std::strcmp(mi_client_remote_error(nullptr), "") == 0);
  assert(mi_client_is_remote_mode(nullptr) == 0);
  assert(mi_client_relogin(nullptr) == 0);
  assert(mi_client_has_pending_server_trust(nullptr) == 0);
  assert(mi_client_has_pending_peer_trust(nullptr) == 0);
  assert(mi_client_heartbeat(nullptr) == 0);
  assert(mi_client_trust_pending_server(nullptr, "123456") == 0);
  assert(mi_client_trust_pending_peer(nullptr, "123456") == 0);
  assert(mi_client_send_private_text_with_reply(nullptr, "peer", "text",
                                                "msg", "preview", nullptr) == 0);
  assert(mi_client_resend_private_text(nullptr, "peer", "msg", "text") == 0);
  assert(mi_client_resend_private_text_with_reply(nullptr, "peer", "msg",
                                                   "text", "reply", "preview") == 0);
  assert(mi_client_resend_group_text(nullptr, "group", "msg", "text") == 0);
  assert(mi_client_send_private_file(nullptr, "peer", "file", nullptr) == 0);
  assert(mi_client_resend_private_file(nullptr, "peer", "msg", "file") == 0);
  assert(mi_client_send_group_file(nullptr, "group", "file", nullptr) == 0);
  assert(mi_client_resend_group_file(nullptr, "group", "msg", "file") == 0);
  assert(mi_client_send_private_sticker(nullptr, "peer", "sticker", nullptr) == 0);
  assert(mi_client_resend_private_sticker(nullptr, "peer", "msg", "sticker") == 0);
  assert(mi_client_send_private_location(nullptr, "peer", 1, 2, "loc", nullptr) == 0);
  assert(mi_client_resend_private_location(nullptr, "peer", "msg", 1, 2, "loc") == 0);
  assert(mi_client_send_private_contact(nullptr, "peer", "user", "disp", nullptr) == 0);
  assert(mi_client_resend_private_contact(nullptr, "peer", "msg", "user", "disp") == 0);
  assert(mi_client_set_friend_remark(nullptr, "peer", "remark") == 0);
  assert(mi_client_set_user_blocked(nullptr, "peer", 1) == 0);
  mi_device_entry_t device_entries[1]{};
  assert(mi_client_list_devices(nullptr, device_entries, 1) == 0);
  assert(mi_client_kick_device(nullptr, "dev") == 0);
  assert(mi_client_send_read_receipt(nullptr, "peer", "msg") == 0);
  assert(mi_client_send_typing(nullptr, "peer", 1) == 0);
  assert(mi_client_send_presence(nullptr, "peer", 1) == 0);
  assert(mi_client_join_group(nullptr, "group") == 0);
  assert(mi_client_leave_group(nullptr, "group") == 0);
  char* group_id = nullptr;
  assert(mi_client_create_group(nullptr, &group_id) == 0);
  assert(group_id == nullptr);
  assert(mi_client_send_group_invite(nullptr, "group", "peer", nullptr) == 0);
  mi_group_member_entry_t group_entries[1]{};
  assert(mi_client_list_group_members_info(nullptr, "group", group_entries, 1) == 0);
  assert(mi_client_set_group_member_role(nullptr, "group", "peer", 1) == 0);
  assert(mi_client_kick_group_member(nullptr, "group", "peer") == 0);
  std::uint8_t dummy_call_id[16]{};
  std::uint32_t dummy_key_id = 7;
  assert(mi_client_start_group_call(nullptr, "group", 1, dummy_call_id, 16,
                                    &dummy_key_id) == 0);
  assert(dummy_key_id == 0);
  assert(mi_client_join_group_call(nullptr, "group", dummy_call_id, 16, 1,
                                   &dummy_key_id) == 0);
  assert(mi_client_leave_group_call(nullptr, "group", dummy_call_id, 16) == 0);
  std::uint8_t dummy_call_key[32];
  std::memset(dummy_call_key, 0xAB, sizeof(dummy_call_key));
  assert(mi_client_get_group_call_key(nullptr, "group", dummy_call_id, 16, 1,
                                      dummy_call_key, 32) == 0);
  for (const auto b : dummy_call_key) {
    assert(b == 0);
  }
  mi_media_config_t media_cfg{};
  assert(mi_client_get_media_config(nullptr, &media_cfg) == 0);
  std::uint8_t media_root[32];
  std::memset(media_root, 0xAB, sizeof(media_root));
  assert(mi_client_derive_media_root(nullptr, "peer", dummy_call_id, 16,
                                     media_root, 32) == 0);
  for (const auto b : media_root) {
    assert(b == 0);
  }
  std::uint8_t dummy_packet[1]{0};
  assert(mi_client_push_media(nullptr, "peer", dummy_call_id, 16, dummy_packet,
                              1) == 0);
  mi_media_packet_t media_packets[1]{};
  assert(mi_client_pull_media(nullptr, dummy_call_id, 16, 1, 0,
                              media_packets) == 0);
  assert(mi_client_push_group_media(nullptr, "group", dummy_call_id, 16,
                                    dummy_packet, 1) == 0);
  assert(mi_client_pull_group_media(nullptr, dummy_call_id, 16, 1, 0,
                                    media_packets) == 0);
  const char* dummy_members[1] = {"peer"};
  assert(mi_client_rotate_group_call_key(nullptr, "group", dummy_call_id, 16, 1,
                                         dummy_members, 1) == 0);
  assert(mi_client_request_group_call_key(nullptr, "group", dummy_call_id, 16, 1,
                                          dummy_members, 1) == 0);
  mi_group_call_member_t call_members[1]{};
  std::uint32_t member_count = 5;
  std::uint8_t out_call_id[16];
  std::memset(out_call_id, 0xAB, sizeof(out_call_id));
  std::uint32_t out_key_id = 9;
  assert(mi_client_send_group_call_signal(nullptr, 1, "group", dummy_call_id, 16,
                                          1, 1, 0, 0, nullptr, 0, out_call_id,
                                          16, &out_key_id, call_members, 1,
                                          &member_count) == 0);
  assert(out_key_id == 0);
  assert(member_count == 0);
  mi_history_entry_t history_entries[1]{};
  assert(mi_client_load_chat_history(nullptr, "conv", 0, 1, history_entries, 1) == 0);
  assert(mi_client_delete_chat_history(nullptr, "conv", 0, 1, 0) == 0);
  assert(mi_client_set_history_enabled(nullptr, 1) == 0);
  assert(mi_client_clear_all_history(nullptr, 1, 0) == 0);
  mi_device_pairing_request_t pairing_entries[1]{};
  assert(mi_client_begin_device_pairing_primary(nullptr, nullptr) == 0);
  char* pairing_code = nullptr;
  assert(mi_client_begin_device_pairing_primary(nullptr, &pairing_code) == 0);
  assert(pairing_code == nullptr);
  assert(mi_client_poll_device_pairing_requests(nullptr, pairing_entries, 1) == 0);
  assert(mi_client_approve_device_pairing_request(nullptr, "dev", "req") == 0);
  assert(mi_client_begin_device_pairing_linked(nullptr, "code") == 0);
  int completed = 0;
  assert(mi_client_poll_device_pairing_linked(nullptr, &completed) == 0);
  mi_client_cancel_device_pairing(nullptr);
  std::uint8_t preview_bytes[1]{0};
  assert(mi_client_store_attachment_preview_bytes(
             nullptr, "file", "name", 1, preview_bytes, 1) == 0);
  std::uint8_t dummy_key[32]{};
  assert(mi_client_download_chat_file_to_path(
             nullptr, "file", dummy_key, 32, "name", 1, "path", 0, nullptr,
             nullptr) == 0);
  std::uint8_t* out_bytes =
      reinterpret_cast<std::uint8_t*>(0x1);
  std::uint64_t out_len = 12;
  assert(mi_client_download_chat_file_to_bytes(
             nullptr, "file", dummy_key, 32, "name", 1, 0, &out_bytes,
             &out_len) == 0);
  assert(out_bytes == nullptr);
  assert(out_len == 0);

  std::string data_err;
  if (!PrepareDataDir(data_err)) {
    return 1;
  }
  const std::string config_path = WriteTestConfig();
  mi_client_handle* handle = mi_client_create(config_path.c_str());
  if (!handle) {
    RemoveConfig(config_path);
    return 1;
  }

  assert(std::strlen(mi_client_token(handle)) == 0);
  assert(std::strlen(mi_client_device_id(handle)) > 0);
  assert(mi_client_has_pending_server_trust(handle) == 0);
  assert(mi_client_has_pending_peer_trust(handle) == 0);
  assert(mi_client_remote_ok(handle) == 1);
  assert(std::strlen(mi_client_remote_error(handle)) == 0);
  assert(mi_client_heartbeat(handle) == 0);

  mi_client_destroy(handle);
  RemoveConfig(config_path);
  return 0;
}
