#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifndef MI_E2EE_KCP_SERVER_SOURCE
#define MI_E2EE_KCP_SERVER_SOURCE ""
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
  const std::string source = ReadFile(MI_E2EE_KCP_SERVER_SOURCE);
  if (source.empty()) {
    std::cerr << "failed to read kcp server source\n";
    return 1;
  }

  const auto require = [&](bool ok, const char* message) {
    if (!ok) {
      std::cerr << message << "\n";
      return false;
    }
    return true;
  };

  if (!require(Contains(source, "next_update_ms"), "missing next_update_ms") ||
      !require(Contains(source, "ikcp_check"), "missing ikcp_check") ||
      !require(Contains(source, "WaitForReadable"), "missing WaitForReadable") ||
      !require(Contains(source, "TimeReached"), "missing TimeReached")) {
    return 1;
  }

  const auto update_pos = source.find("ikcp_update(sess->kcp, now)");
  if (!require(update_pos != std::string::npos, "missing ikcp_update")) {
    return 1;
  }
  const auto guard_pos = source.rfind("TimeReached(now, sess->next_update_ms)",
                                     update_pos);
  if (!require(guard_pos != std::string::npos,
               "ikcp_update is not guarded by next_update_ms")) {
    return 1;
  }

  return 0;
}
