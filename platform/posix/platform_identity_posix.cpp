#include "platform_identity.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>

#include "platform_secure_store.h"

namespace mi::platform {

namespace {

bool ParseEnvFlag(const char* name, bool default_value) {
  const char* env = std::getenv(name);
  if (!env || *env == '\0') {
    return default_value;
  }
  std::string v(env);
  for (auto& ch : v) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  if (v == "1" || v == "true" || v == "on" || v == "yes") {
    return true;
  }
  if (v == "0" || v == "false" || v == "off" || v == "no") {
    return false;
  }
  return default_value;
}

std::string BuildSoftTpmEntropy() {
  std::string entropy = "MI_E2EE_SOFT_TPM_V1";
  const std::string machine_id = MachineId();
  if (!machine_id.empty()) {
    entropy.push_back(':');
    entropy.append(machine_id);
  }
  return entropy;
}

}  // namespace

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
  if (!SecureStoreSupported()) {
    return false;
  }
  return ParseEnvFlag("MI_E2EE_SOFT_TPM", true);
}

bool TpmWrapKey(const std::array<std::uint8_t, 32>& key_bytes,
                std::vector<std::uint8_t>& out_wrapped,
                std::string& error) {
  out_wrapped.clear();
  error.clear();
  if (!TpmSupported()) {
    error = "tpm unsupported";
    return false;
  }
  const std::string entropy = BuildSoftTpmEntropy();
  std::vector<std::uint8_t> plain;
  plain.assign(key_bytes.begin(), key_bytes.end());
  return ProtectSecureBlobScoped(
      plain,
      reinterpret_cast<const std::uint8_t*>(entropy.data()),
      entropy.size(),
      SecureStoreScope::kMachine,
      out_wrapped,
      error);
}

bool TpmUnwrapKey(const std::vector<std::uint8_t>& wrapped,
                  std::array<std::uint8_t, 32>& out_key,
                  std::string& error) {
  out_key.fill(0);
  error.clear();
  if (!TpmSupported()) {
    error = "tpm unsupported";
    return false;
  }
  const std::string entropy = BuildSoftTpmEntropy();
  std::vector<std::uint8_t> plain;
  if (!UnprotectSecureBlobScoped(
          wrapped,
          reinterpret_cast<const std::uint8_t*>(entropy.data()),
          entropy.size(),
          SecureStoreScope::kMachine,
          plain,
          error)) {
    return false;
  }
  if (plain.size() != out_key.size()) {
    error = "tpm unwrap size invalid";
    return false;
  }
  std::copy_n(plain.begin(), out_key.size(), out_key.begin());
  return true;
}

}  // namespace mi::platform
