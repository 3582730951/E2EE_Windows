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
      std::filesystem::current_path() / "test_client_config_friend.ini";
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
      std::filesystem::current_path() / "test_client_data_friend";
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

const char* SafeError(mi_client_handle* handle) {
  const char* err = mi_client_last_error(handle);
  return err ? err : "";
}

}  // namespace

int main() {
  assert(mi_client_add_friend(nullptr, "alice", "") == 0);
  assert(mi_client_delete_friend(nullptr, "alice") == 0);
  assert(mi_client_send_friend_request(nullptr, "alice", "") == 0);
  assert(mi_client_respond_friend_request(nullptr, "alice", 1) == 0);
  assert(mi_client_list_friends(nullptr, nullptr, 0) == 0);
  assert(mi_client_sync_friends(nullptr, nullptr, 0, nullptr) == 0);
  assert(mi_client_list_friend_requests(nullptr, nullptr, 0) == 0);

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

  mi_friend_entry_t friends[4] = {};
  std::uint32_t count = mi_client_list_friends(handle, friends, 4);
  assert(count == 0);

  int changed = 1;
  count = mi_client_sync_friends(handle, friends, 4, &changed);
  assert(count == 0);
  assert(changed == 0);
  assert(std::strcmp(SafeError(handle), "not logged in") == 0);

  assert(mi_client_send_friend_request(handle, "alice", "hi") == 0);
  assert(std::strcmp(SafeError(handle), "not logged in") == 0);

  mi_friend_request_entry_t reqs[4] = {};
  count = mi_client_list_friend_requests(handle, reqs, 4);
  assert(count == 0);
  assert(std::strcmp(SafeError(handle), "not logged in") == 0);

  assert(mi_client_respond_friend_request(handle, "alice", 1) == 0);
  assert(std::strcmp(SafeError(handle), "not logged in") == 0);

  assert(mi_client_delete_friend(handle, "alice") == 0);
  assert(std::strcmp(SafeError(handle), "not logged in") == 0);

  assert(mi_client_add_friend(handle, "alice", "remark") == 0);

  mi_client_destroy(handle);
  RemoveConfig(config_path);
  return 0;
}
