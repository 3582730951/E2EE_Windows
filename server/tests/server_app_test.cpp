#include <fstream>
#include <string>
#include <vector>

#include "frame.h"
#include "key_transparency.h"
#include "server_app.h"

using mi::server::Frame;
using mi::server::FrameType;
using mi::server::ServerApp;

static void WriteFile(const std::string& path, const std::string& content) {
  std::ofstream f(path, std::ios::binary);
  f << content;
}

int main() {
  WriteFile("config.ini",
            "[mode]\nmode=1\n[server]\nlist_port=7777\n"
            "offline_dir=.\n"
            "kt_signing_key=kt_signing_key.bin\n");
  WriteFile("test_user.txt", "alice:secret\n");
  {
    std::vector<std::uint8_t> key(mi::server::kKtSthSigSecretKeyBytes, 0x42);
    std::ofstream kf("kt_signing_key.bin", std::ios::binary | std::ios::trunc);
    if (!kf) {
      return 1;
    }
    kf.write(reinterpret_cast<const char*>(key.data()),
             static_cast<std::streamsize>(key.size()));
  }

  ServerApp app;
  std::string err;
  bool ok = app.Init("config.ini", err);
  if (!ok) {
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
    return 1;
  }

  return 0;
}
