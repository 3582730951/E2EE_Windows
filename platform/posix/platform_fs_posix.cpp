#include "platform_fs.h"

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <cerrno>
#include <string>

namespace {
struct FileLockImpl {
  int fd{-1};
};

void SetErrno(std::error_code& ec) {
  ec = std::error_code(errno, std::generic_category());
}

bool WriteAllFd(int fd, const std::uint8_t* data, std::size_t len,
                std::error_code& ec) {
  std::size_t offset = 0;
  while (offset < len) {
    const std::size_t chunk = len - offset;
    const ssize_t rc =
        ::write(fd, data + offset, static_cast<size_t>(chunk));
    if (rc < 0) {
      if (errno == EINTR) {
        continue;
      }
      SetErrno(ec);
      return false;
    }
    offset += static_cast<std::size_t>(rc);
  }
  return true;
}

std::filesystem::path BuildTempPath(const std::filesystem::path& target,
                                    int attempt) {
  std::filesystem::path dir =
      target.has_parent_path() ? target.parent_path() : std::filesystem::path{};
  std::string base = target.filename().string();
  if (base.empty()) {
    base = "tmp";
  }
  const int pid = static_cast<int>(::getpid());
  std::string name = base + ".tmp." + std::to_string(pid) + "." +
                     std::to_string(attempt);
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
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    SetErrno(ec);
    return false;
  }
  if (::fsync(fd) != 0) {
    SetErrno(ec);
    ::close(fd);
    return false;
  }
  if (::close(fd) != 0) {
    SetErrno(ec);
    return false;
  }
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
    const int fd =
        ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, 0600);
    if (fd < 0) {
      if (errno == EEXIST) {
        continue;
      }
      SetErrno(ec);
      return false;
    }

    if (len > 0 && !WriteAllFd(fd, data, len, ec)) {
      ::close(fd);
      std::error_code ignore_ec;
      std::filesystem::remove(tmp, ignore_ec);
      return false;
    }
    if (::fsync(fd) != 0) {
      SetErrno(ec);
      ::close(fd);
      std::error_code ignore_ec;
      std::filesystem::remove(tmp, ignore_ec);
      return false;
    }
    if (::close(fd) != 0) {
      SetErrno(ec);
      std::error_code ignore_ec;
      std::filesystem::remove(tmp, ignore_ec);
      return false;
    }

    if (::rename(tmp.c_str(), path.c_str()) != 0) {
      SetErrno(ec);
      std::error_code ignore_ec;
      std::filesystem::remove(tmp, ignore_ec);
      return false;
    }

    std::filesystem::path dir =
        path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
    const int dfd = ::open(dir.c_str(), O_RDONLY);
    if (dfd >= 0) {
      (void)::fsync(dfd);
      (void)::close(dfd);
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
  int fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0600);
  if (fd < 0) {
    return FileLockStatus::kFailed;
  }
  if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
    ::close(fd);
    return FileLockStatus::kBusy;
  }
  auto* impl = new FileLockImpl();
  impl->fd = fd;
  out.impl = impl;
  return FileLockStatus::kOk;
}

void ReleaseFileLock(FileLock& lock) {
  if (!lock.impl) {
    return;
  }
  auto* impl = static_cast<FileLockImpl*>(lock.impl);
  if (impl->fd >= 0) {
    flock(impl->fd, LOCK_UN);
    ::close(impl->fd);
    impl->fd = -1;
  }
  delete impl;
  lock.impl = nullptr;
}

}  // namespace mi::platform::fs
