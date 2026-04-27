#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifndef MI_E2EE_NETWORK_SERVER_SOURCE
#define MI_E2EE_NETWORK_SERVER_SOURCE ""
#endif

#ifndef MI_E2EE_CLIENT_TRANSPORT_SOURCE
#define MI_E2EE_CLIENT_TRANSPORT_SOURCE ""
#endif

namespace {

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    return {};
  }
  std::ostringstream oss;
  oss << ifs.rdbuf();
  return oss.str();
}

bool Contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

}  // namespace

int main() {
  const std::string server = ReadFile(MI_E2EE_NETWORK_SERVER_SOURCE);
  const std::string client = ReadFile(MI_E2EE_CLIENT_TRANSPORT_SOURCE);
  if (server.empty() || client.empty()) {
    std::cerr << "failed to read network or client transport source\n";
    return 1;
  }

  const auto require = [&](bool ok, const char* message) {
    if (!ok) {
      std::cerr << message << "\n";
      return false;
    }
    return true;
  };

  if (!require(Contains(server, "read_tmp"), "missing server read_tmp") ||
      !require(Contains(server, "plain_tmp"), "missing server plain_tmp") ||
      !require(Contains(server, "frame_tmp"), "missing server frame_tmp") ||
      !require(Contains(server, "tls_tmp"), "missing server tls_tmp") ||
      !require(!Contains(server, "std::vector<std::uint8_t> plain;"),
               "server still creates TLS plain vector in hot path") ||
      !require(!Contains(server, "std::uint8_t tmp[4096]"),
               "server still creates stack read buffers in hot path") ||
      !require(Contains(client, "plain_tmp"), "missing client plain_tmp") ||
      !require(!Contains(client, "std::vector<std::uint8_t> plain_chunk;"),
               "client still creates TLS plain_chunk in hot path") ||
      !require(Contains(client, "out_frame.resize(total)"),
               "client frame output does not reuse vector capacity")) {
    return 1;
  }

  return 0;
}
