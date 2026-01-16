#include "platform_identity.h"

#include <cctype>
#include <fstream>

namespace mi::platform {

std::string MachineId() {
  const char* paths[] = {"/etc/machine-id", "/var/lib/dbus/machine-id"};
  for (const char* path : paths) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
      continue;
    }
    std::string line;
    std::getline(f, line);
    while (!line.empty() &&
           std::isspace(static_cast<unsigned char>(line.back())) != 0) {
      line.pop_back();
    }
    std::size_t start = 0;
    while (start < line.size() &&
           std::isspace(static_cast<unsigned char>(line[start])) != 0) {
      ++start;
    }
    if (start > 0) {
      line.erase(0, start);
    }
    if (!line.empty()) {
      return line;
    }
  }
  return {};
}

bool TpmSupported() {
  return false;
}

bool TpmWrapKey(const std::array<std::uint8_t, 32>& /*key_bytes*/,
                std::vector<std::uint8_t>& out_wrapped,
                std::string& error) {
  out_wrapped.clear();
  error = "tpm unsupported";
  return false;
}

bool TpmUnwrapKey(const std::vector<std::uint8_t>& /*wrapped*/,
                  std::array<std::uint8_t, 32>& out_key,
                  std::string& error) {
  out_key.fill(0);
  error = "tpm unsupported";
  return false;
}

}  // namespace mi::platform
