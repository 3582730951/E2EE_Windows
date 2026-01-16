#include "c_api_client.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "path_security.h"

namespace {

std::string WriteTestConfig() {
  const auto path =
      std::filesystem::current_path() / "test_client_config.ini";
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
      std::filesystem::current_path() / "test_client_data_event";
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
  std::string data_err;
  if (!PrepareDataDir(data_err)) {
    return 1;
  }
  const std::string config_path = WriteTestConfig();
  mi_client_handle* handle = mi_client_create(config_path.c_str());
  if (!handle) {
    std::filesystem::remove(config_path);
    return 1;
  }

  std::array<std::uint8_t, 16> call_id{};
  call_id[0] = 0x01;
  const int sub_ok =
      mi_client_add_media_subscription(handle, call_id.data(),
                                       static_cast<std::uint32_t>(call_id.size()),
                                       0, nullptr);
  assert(sub_ok == 1);

  mi_event_t events[4] = {};
  const std::uint32_t count = mi_client_poll_event(handle, events, 4, 0);
  assert(count == 0);

  mi_client_clear_media_subscriptions(handle);
  mi_client_destroy(handle);
  std::filesystem::remove(config_path);
  return 0;
}
