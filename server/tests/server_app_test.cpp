#include <fstream>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "frame.h"
#include "key_transparency.h"
#include "server_app.h"
#include "test_auth_helpers.h"

using mi::server::Frame;
using mi::server::FrameType;
using mi::server::ServerApp;

static void SetOwnerOnlyPermissions(const std::string& path) {
  std::error_code ec;
  std::filesystem::permissions(
      path, std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace, ec);
}

static void WriteFile(const std::string& path, const std::string& content) {
  std::ofstream f(path, std::ios::binary);
  f << content;
  f.close();
  SetOwnerOnlyPermissions(path);
}

static void WriteConfig(const std::string& path,
                        const std::string& offline_dir) {
  std::string content =
      "[mode]\nmode=1\n[server]\nlist_port=7777\n"
      "offline_dir=" + offline_dir + "\n"
      "tls_enable=1\n"
      "require_tls=1\n"
      "allow_legacy_login=1\n"
      "key_protection=none\n"
      "kt_signing_key=kt_signing_key.bin\n";
  WriteFile(path, content);
}

int main() {
  std::error_code ec;
  const auto test_root =
      std::filesystem::temp_directory_path(ec) / "mi_e2ee_server_app_test";
  if (ec) {
    std::cerr << "temp_directory_path failed\n";
    return 1;
  }
  std::filesystem::remove_all(test_root, ec);
  std::filesystem::create_directories(test_root, ec);
  if (ec) {
    std::cerr << "create_directories failed: " << test_root << '\n';
    return 1;
  }
  std::filesystem::current_path(test_root, ec);
  if (ec) {
    std::cerr << "current_path failed: " << test_root << '\n';
    return 1;
  }
  std::filesystem::remove_all("offline_server_app_test", ec);
  std::filesystem::remove_all("offline_server_app_test_corrupt", ec);

  WriteConfig("config.ini", "offline_server_app_test");
  WriteFile("test_user.txt",
            mi::server::test::DemoUserFileLine("alice", "secret"));
  {
    std::vector<std::uint8_t> key(mi::server::kKtSthSigSecretKeyBytes, 0x42);
    std::ofstream kf("kt_signing_key.bin", std::ios::binary | std::ios::trunc);
    if (!kf) {
      return 1;
    }
    kf.write(reinterpret_cast<const char*>(key.data()),
             static_cast<std::streamsize>(key.size()));
    kf.close();
    SetOwnerOnlyPermissions("kt_signing_key.bin");
  }

  ServerApp app;
  std::string err;
  bool ok = app.Init("config.ini", err);
  if (!ok) {
    std::cerr << "Init(config.ini) failed: " << err << '\n';
    return 1;
  }

  Frame login;
  login.type = FrameType::kLogin;
  const std::string user = "alice";
  const std::string pass = "secret";
  login.payload.push_back(static_cast<unsigned char>(user.size()));
  login.payload.push_back(0);
  login.payload.insert(login.payload.end(), user.begin(), user.end());
  login.payload.push_back(static_cast<unsigned char>(pass.size()));
  login.payload.push_back(0);
  login.payload.insert(login.payload.end(), pass.begin(), pass.end());

  Frame resp;
  ok = app.HandleFrame(login, resp, mi::server::TransportKind::kLocal, err);
  if (!ok || resp.payload.empty() || resp.payload[0] != 1) {
    std::cerr << "HandleFrame login failed, ok=" << ok
              << " payload_size=" << resp.payload.size()
              << " err=" << err << '\n';
    return 1;
  }

  std::filesystem::create_directories("offline_server_app_test_corrupt", ec);
  WriteFile("offline_server_app_test_corrupt/kt_directory.bin", "BADMAGIC");
  WriteConfig("config_bad.ini", "offline_server_app_test_corrupt");

  ServerApp bad_app;
  err.clear();
  ok = bad_app.Init("config_bad.ini", err);
  if (ok) {
    std::cerr << "Init(config_bad.ini) unexpectedly succeeded\n";
    return 1;
  }
  if (err.find("kt directory load failed") == std::string::npos) {
    std::cerr << "Init(config_bad.ini) failed with unexpected error: " << err
              << '\n';
    return 1;
  }

  return 0;
}
