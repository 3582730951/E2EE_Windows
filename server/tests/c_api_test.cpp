#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "c_api.h"
#include "frame.h"
#include "key_transparency.h"
#include "protocol.h"
#include "test_permissions.h"

static void WriteFile(const std::string& path, const std::string& content) {
  std::ofstream f(path, std::ios::binary);
  f << content;
  f.close();
  mi::server::test::SetOwnerOnlyFile(path);
}

int main() {
  std::error_code ec;
  const auto test_root =
      std::filesystem::temp_directory_path(ec) / "mi_e2ee_c_api_test";
  if (ec) {
    return 1;
  }
  std::filesystem::remove_all(test_root, ec);
  if (!mi::server::test::EnsureOwnerOnlyDirectory(test_root)) {
    return 1;
  }
  std::filesystem::current_path(test_root, ec);
  if (ec) {
    return 1;
  }
  std::filesystem::remove_all("offline_c_api_test", ec);

  WriteFile("config.ini",
            "[mode]\nmode=1\n[server]\nlist_port=8888\n"
            "offline_dir=offline_c_api_test\n"
            "tls_enable=1\n"
            "require_tls=1\n"
            "allow_legacy_login=1\n"
            "key_protection=none\n"
            "kt_signing_key=kt_signing_key.bin\n");
  WriteFile("test_user.txt", "u:p\n");
  {
    std::vector<std::uint8_t> key(mi::server::kKtSthSigSecretKeyBytes, 0x44);
    std::ofstream kf("kt_signing_key.bin", std::ios::binary | std::ios::trunc);
    if (!kf) {
      return 1;
    }
    kf.write(reinterpret_cast<const char*>(key.data()),
             static_cast<std::streamsize>(key.size()));
    kf.close();
    mi::server::test::SetOwnerOnlyFile("kt_signing_key.bin");
  }

  mi_server_handle* h = mi_server_create("config.ini");
  if (!h) {
    std::cerr << "mi_server_create failed\n";
    return 1;
  }

  mi::server::Frame login;
  login.type = mi::server::FrameType::kLogin;
  mi::server::proto::WriteString("u", login.payload);
  mi::server::proto::WriteString("p", login.payload);
  auto bytes = mi::server::EncodeFrame(login);

  std::uint8_t* out_buf = nullptr;
  std::size_t out_len = 0;
  int ok = mi_server_process(h, bytes.data(), bytes.size(), &out_buf, &out_len);
  if (ok == 0 || out_len == 0) {
    mi_server_destroy(h);
    return 1;
  }

  mi::server::Frame resp;
  bool parsed = mi::server::DecodeFrame(out_buf, out_len, resp);
  if (!parsed || resp.type != mi::server::FrameType::kLogin ||
      resp.payload.empty() || resp.payload[0] != 1) {
    mi_server_free(out_buf);
    mi_server_destroy(h);
    return 1;
  }

  mi_server_free(out_buf);
  mi_server_destroy(h);
  return 0;
}
