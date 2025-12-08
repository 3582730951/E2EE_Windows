#include <cstdio>
#include <string>

#include "include/client_core.h"

int main() {
  mi::client::ClientCore client;
  if (!client.Init("config.ini")) {
    std::puts("[error] init failed");
    return 1;
  }
  if (!client.Login("u", "p")) {
    std::puts("[error] login failed");
    return 1;
  }
  if (!client.JoinGroup("g1")) {
    std::puts("[error] join failed");
    return 1;
  }
  if (!client.SendGroupMessage("g1", 1)) {
    std::puts("[error] send group message failed");
    return 1;
  }
  std::puts("[client] demo finished");
  return 0;
}
