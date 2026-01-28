#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "config.h"

namespace mi::server {

struct MetadataKeyConfig {
  KeyProtectionMode protection{KeyProtectionMode::kNone};
  std::filesystem::path key_path;
  std::string key_hex;
};

bool LoadOrCreateMetadataKey(const MetadataKeyConfig& cfg,
                             std::array<std::uint8_t, 32>& out_key,
                             std::string& error);

class MetadataProtector {
 public:
  explicit MetadataProtector(const std::array<std::uint8_t, 32>& key);

  std::string HashId(const std::string& id) const;

  bool EncryptBlob(const std::vector<std::uint8_t>& plain,
                   std::vector<std::uint8_t>& out,
                   std::string& error) const;
  bool DecryptBlob(const std::vector<std::uint8_t>& in,
                   std::vector<std::uint8_t>& out,
                   std::string& error) const;

 private:
  std::array<std::uint8_t, 32> key_{};
};

}  // namespace mi::server
