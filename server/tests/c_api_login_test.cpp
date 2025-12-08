#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

#include "c_api.h"

static void WriteFile(const std::string& path, const std::string& content) {
  std::ofstream f(path, std::ios::binary);
  f << content;
}

int main() {
  WriteFile("config.ini",
            "[mode]\nmode=1\n[server]\nlist_port=9999\n");
  WriteFile("test_user.txt", "u:p\n");

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
