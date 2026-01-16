#include "platform_fs.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <limits>
#include <string>

#ifdef CopyFile
#undef CopyFile
#endif

namespace {
struct FileLockImpl {
  HANDLE handle{INVALID_HANDLE_VALUE};
};

void SetLastErrorCode(std::error_code& ec) {
  ec = std::error_code(static_cast<int>(GetLastError()),
                       std::system_category());
}

std::filesystem::path BuildTempPath(const std::filesystem::path& target,
                                    int attempt) {
  std::filesystem::path dir =
      target.has_parent_path() ? target.parent_path() : std::filesystem::path{};
  std::wstring base = target.filename().wstring();
  if (base.empty()) {
    base = L"tmp";
  }
  const DWORD pid = GetCurrentProcessId();
  std::wstring name = base + L".tmp." + std::to_wstring(pid) + L"." +
                      std::to_wstring(attempt);
  return dir.empty() ? std::filesystem::path{name} : (dir / name);
}
}  // namespace

namespace mi::platform::fs {

std::filesystem::path CurrentPath(std::error_code& ec) {
  return std::filesystem::current_path(ec);
}

bool Exists(const std::filesystem::path& path, std::error_code& ec) {
  return std::filesystem::exists(path, ec);
}

bool IsDirectory(const std::filesystem::path& path, std::error_code& ec) {
  return std::filesystem::is_directory(path, ec);
}

std::uint64_t FileSize(const std::filesystem::path& path,
                       std::error_code& ec) {
  const auto size = std::filesystem::file_size(path, ec);
  return ec ? 0 : static_cast<std::uint64_t>(size);
}

bool CreateDirectories(const std::filesystem::path& path,
                       std::error_code& ec) {
  return std::filesystem::create_directories(path, ec);
}

bool Remove(const std::filesystem::path& path, std::error_code& ec) {
  return std::filesystem::remove(path, ec);
}

bool RemoveAll(const std::filesystem::path& path, std::error_code& ec) {
  const auto count = std::filesystem::remove_all(path, ec);
  return !ec && count >= 0;
}

bool Rename(const std::filesystem::path& from, const std::filesystem::path& to,
            std::error_code& ec) {
  std::filesystem::rename(from, to, ec);
  return !ec;
}

bool CopyFile(const std::filesystem::path& from,
              const std::filesystem::path& to,
              bool overwrite,
              std::error_code& ec) {
  const auto opts = overwrite ? std::filesystem::copy_options::overwrite_existing
                              : std::filesystem::copy_options::skip_existing;
  return std::filesystem::copy_file(from, to, opts, ec);
}

bool ListDir(const std::filesystem::path& path,
             std::vector<std::filesystem::path>& out,
             std::error_code& ec) {
  out.clear();
  ec.clear();
  std::filesystem::directory_iterator it(path, ec);
  if (ec) {
    return false;
  }
  const std::filesystem::directory_iterator end;
  for (; it != end; it.increment(ec)) {
    if (ec) {
      out.clear();
      return false;
    }
    out.push_back(it->path());
  }
  return true;
}

bool FSyncFile(const std::filesystem::path& path, std::error_code& ec) {
  ec.clear();
  if (path.empty()) {
    ec = std::make_error_code(std::errc::invalid_argument);
    return false;
  }
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    SetLastErrorCode(ec);
    return false;
  }
  if (!FlushFileBuffers(h)) {
    SetLastErrorCode(ec);
    CloseHandle(h);
    return false;
  }
  CloseHandle(h);
  return true;
}

bool AtomicWrite(const std::filesystem::path& path,
                 const std::uint8_t* data,
                 std::size_t len,
                 std::error_code& ec) {
  ec.clear();
  if (path.empty()) {
    ec = std::make_error_code(std::errc::invalid_argument);
    return false;
  }
  if (len > 0 && !data) {
    ec = std::make_error_code(std::errc::invalid_argument);
    return false;
  }

  for (int attempt = 0; attempt < 16; ++attempt) {
    const std::filesystem::path tmp = BuildTempPath(path, attempt);
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
      const DWORD err = GetLastError();
      if (err == ERROR_FILE_EXISTS || err == ERROR_ALREADY_EXISTS) {
        continue;
      }
      SetLastErrorCode(ec);
      return false;
    }

    std::size_t offset = 0;
    while (offset < len) {
      const std::size_t chunk = len - offset;
      const DWORD to_write =
          chunk > static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())
              ? (std::numeric_limits<DWORD>::max)()
              : static_cast<DWORD>(chunk);
      DWORD written = 0;
      if (!WriteFile(h, data + offset, to_write, &written, nullptr)) {
        SetLastErrorCode(ec);
        CloseHandle(h);
        DeleteFileW(tmp.c_str());
        return false;
      }
      offset += static_cast<std::size_t>(written);
    }

    if (!FlushFileBuffers(h)) {
      SetLastErrorCode(ec);
      CloseHandle(h);
      DeleteFileW(tmp.c_str());
      return false;
    }
    CloseHandle(h);

    if (!MoveFileExW(tmp.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      SetLastErrorCode(ec);
      DeleteFileW(tmp.c_str());
      return false;
    }
    return true;
  }

  ec = std::make_error_code(std::errc::file_exists);
  return false;
}

FileLockStatus AcquireExclusiveFileLock(const std::filesystem::path& path,
                                        FileLock& out) {
  out.impl = nullptr;
  if (path.empty()) {
    return FileLockStatus::kFailed;
  }
  const std::wstring wpath = path.wstring();
  HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                         nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    const DWORD err = GetLastError();
    if (err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION) {
      return FileLockStatus::kBusy;
    }
    return FileLockStatus::kFailed;
  }
  auto* impl = new FileLockImpl();
  impl->handle = h;
  out.impl = impl;
  return FileLockStatus::kOk;
}

void ReleaseFileLock(FileLock& lock) {
  if (!lock.impl) {
    return;
  }
  auto* impl = static_cast<FileLockImpl*>(lock.impl);
  if (impl->handle != INVALID_HANDLE_VALUE) {
    CloseHandle(impl->handle);
    impl->handle = INVALID_HANDLE_VALUE;
  }
  delete impl;
  lock.impl = nullptr;
}

}  // namespace mi::platform::fs
