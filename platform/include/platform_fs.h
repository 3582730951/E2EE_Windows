#ifndef MI_E2EE_PLATFORM_FS_H
#define MI_E2EE_PLATFORM_FS_H

#include <cstdint>
#include <filesystem>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifdef CopyFile
#undef CopyFile
#endif
#endif

namespace mi::platform::fs {

std::filesystem::path CurrentPath(std::error_code& ec);
bool Exists(const std::filesystem::path& path, std::error_code& ec);
bool IsDirectory(const std::filesystem::path& path, std::error_code& ec);
std::uint64_t FileSize(const std::filesystem::path& path,
                       std::error_code& ec);
bool CreateDirectories(const std::filesystem::path& path,
                       std::error_code& ec);
bool Remove(const std::filesystem::path& path, std::error_code& ec);
bool RemoveAll(const std::filesystem::path& path, std::error_code& ec);
bool Rename(const std::filesystem::path& from, const std::filesystem::path& to,
            std::error_code& ec);
bool CopyFile(const std::filesystem::path& from,
              const std::filesystem::path& to,
              bool overwrite,
              std::error_code& ec);
bool ListDir(const std::filesystem::path& path,
             std::vector<std::filesystem::path>& out,
             std::error_code& ec);
bool FSyncFile(const std::filesystem::path& path, std::error_code& ec);
bool AtomicWrite(const std::filesystem::path& path,
                 const std::uint8_t* data,
                 std::size_t len,
                 std::error_code& ec);

enum class FileLockStatus {
  kOk = 0,
  kBusy = 1,
  kFailed = 2,
};

struct FileLock {
  void* impl{nullptr};
};

FileLockStatus AcquireExclusiveFileLock(const std::filesystem::path& path,
                                        FileLock& out);
void ReleaseFileLock(FileLock& lock);

}  // namespace mi::platform::fs

#endif  // MI_E2EE_PLATFORM_FS_H
