#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "c_api.h"
#include "key_transparency.h"

static void WriteFile(const std::string& path, const std::string& content) {
  std::ofstream f(path, std::ios::binary);
  f << content;
}

int main() {
  WriteFile("config.ini",
            "[mode]\nmode=1\n[server]\nlist_port=9999\n"
            "offline_dir=.\n"
            "kt_signing_key=kt_signing_key.bin\n");
  WriteFile("test_user.txt", "u:p\n");
  {
    std::vector<std::uint8_t> key(mi::server::kKtSthSigSecretKeyBytes, 0x33);
    std::ofstream kf("kt_signing_key.bin", std::ios::binary | std::ios::trunc);
    if (!kf) {
      return 1;
    }
    kf.write(reinterpret_cast<const char*>(key.data()),
             static_cast<std::streamsize>(key.size()));
  }

  mi_server_handle* h = mi_server_create("config.ini");
  if (!h) {
    return 1;
  }

  char* token = nullptr;
  int ok = mi_server_login(h, "u", "p", &token);
  if (ok == 0 || token == nullptr) {
    mi_server_destroy(h);
    return 1;
  }

  ok = mi_server_logout(h, token);
  if (ok == 0) {
    mi_server_free(reinterpret_cast<std::uint8_t*>(token));
    mi_server_destroy(h);
    return 1;
  }

  mi_server_free(reinterpret_cast<std::uint8_t*>(token));
  mi_server_destroy(h);
  return 0;
}
