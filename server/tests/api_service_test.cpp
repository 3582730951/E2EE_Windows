#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "api_service.h"
#include "auth_provider.h"
#include "group_call_manager.h"
#include "group_directory.h"
#include "offline_storage.h"
#include "session_manager.h"

using mi::server::ApiService;
using mi::server::DemoAuthProvider;
using mi::server::DemoUser;
using mi::server::DemoUserTable;
using mi::server::GroupCallManager;
using mi::server::GroupManager;
using mi::server::GroupDirectory;
using mi::server::LoginRequest;
using mi::server::LogoutRequest;
using mi::server::OfflineQueue;
using mi::server::OfflineStorage;
using mi::server::SessionManager;

int main() {
  DemoUserTable table;
  DemoUser user;
  user.username.set("alice");
  user.password.set("secret");
  user.username_plain = "alice";
  user.password_plain = "secret";
  table.emplace("alice", user);

  auto auth = std::make_unique<DemoAuthProvider>(std::move(table));
  SessionManager sessions(std::move(auth));
  GroupManager groups;
  GroupCallManager calls;
  GroupDirectory dir;
  auto offline_dir =
      std::filesystem::temp_directory_path() / "mi_e2ee_api_offline";
  std::error_code ec;
  std::filesystem::remove_all(offline_dir, ec);
  OfflineStorage storage(offline_dir, std::chrono::seconds(60));
  OfflineQueue queue(std::chrono::seconds(60));
  ApiService api(&sessions, &groups, &calls, &dir, &storage, &queue);

  auto login_ok =
      api.Login(LoginRequest{"alice", "secret"}, mi::server::TransportKind::kLocal);
  if (!login_ok.success || login_ok.token.empty()) {
    return 1;
  }

  auto login_fail =
      api.Login(LoginRequest{"alice", "bad"}, mi::server::TransportKind::kLocal);
  if (login_fail.success) {
    return 1;
  }

  auto join = api.JoinGroup(login_ok.token, "g1");
  if (!join.success || join.version != 1) {
    return 1;
  }
  auto members = api.GroupMembers(login_ok.token, "g1");
  if (!members.success || members.members.empty()) {
    return 1;
  }

  auto msg = api.OnGroupMessage(login_ok.token, "g1", 2);
  if (!msg.success) {
    return 1;
  }
  auto msg2 = api.OnGroupMessage(login_ok.token, "g1", 2);
  if (!msg2.success || !msg2.rotated.has_value() || msg2.rotated->version != 2) {
    return 1;
  }

  const std::vector<std::uint8_t> file_payload = {1, 2, 3, 4};
  auto upload = api.StoreEphemeralFile(login_ok.token, file_payload);
  if (!upload.success) {
    return 1;
  }
  auto download =
      api.LoadEphemeralFile(login_ok.token, upload.file_id, upload.file_key);
  if (!download.success || download.plaintext != file_payload) {
    return 1;
  }

  const std::vector<std::uint8_t> blob_payload = {5, 6, 7, 8, 9};
  auto blob_upload = api.StoreE2eeFileBlob(login_ok.token, blob_payload);
  if (!blob_upload.success || blob_upload.file_id.empty()) {
    return 1;
  }
  auto blob_download =
      api.LoadE2eeFileBlob(login_ok.token, blob_upload.file_id, true);
  if (!blob_download.success || blob_download.blob != blob_payload) {
    return 1;
  }
  auto blob_download2 =
      api.LoadE2eeFileBlob(login_ok.token, blob_upload.file_id, true);
  if (blob_download2.success) {
    return 1;
  }

  queue.Enqueue("alice", {9, 9, 9});
  auto offline = api.PullOffline(login_ok.token);
  if (!offline.success || offline.messages.size() != 1) {
    return 1;
  }

  auto logout = api.Logout(LogoutRequest{login_ok.token});
  if (!logout.success) {
    return 1;
  }

  return 0;
}
