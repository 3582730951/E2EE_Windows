#pragma once

#include <filesystem>

#ifdef _WIN32
#include <string>

#include "path_security.h"
#endif

namespace mi::server::test {

inline bool SetOwnerOnlyFile(const std::filesystem::path& path) {
#ifdef _WIN32
  std::string error;
  return mi::shard::security::HardenPathAcl(path, error) &&
         mi::shard::security::CheckPathNotWorldWritable(path, error);
#else
  std::error_code ec;
  std::filesystem::permissions(
      path, std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace, ec);
  return !ec;
#endif
}

inline bool SetOwnerOnlyDirectory(const std::filesystem::path& path) {
#ifdef _WIN32
  std::string error;
  return mi::shard::security::HardenPathAcl(path, error) &&
         mi::shard::security::CheckPathNotWorldWritable(path, error);
#else
  std::error_code ec;
  std::filesystem::permissions(path, std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace, ec);
  return !ec;
#endif
}

inline bool EnsureOwnerOnlyDirectory(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  return !ec && SetOwnerOnlyDirectory(path);
}

}  // namespace mi::server::test
