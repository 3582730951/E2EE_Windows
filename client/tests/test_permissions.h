#pragma once

#include <filesystem>
#include <string>

namespace mi::client::test {

inline bool SetOwnerOnlyFile(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::permissions(
      path, std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace, ec);
  return !ec;
}

inline bool SetOwnerOnlyDirectory(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::permissions(path, std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace, ec);
  return !ec;
}

inline bool EnsureOwnerOnlyDirectory(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  return !ec && SetOwnerOnlyDirectory(path);
}

inline bool UseTempWorkingDirectory(const std::string& name) {
  std::error_code ec;
  const auto dir = std::filesystem::temp_directory_path(ec) / name;
  if (ec) {
    return false;
  }
  std::filesystem::remove_all(dir, ec);
  if (!EnsureOwnerOnlyDirectory(dir)) {
    return false;
  }
  std::filesystem::current_path(dir, ec);
  return !ec;
}

}  // namespace mi::client::test
