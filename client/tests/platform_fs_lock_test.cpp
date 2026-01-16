#include <cassert>
#include <filesystem>
#include <string>

#include "platform_fs.h"

namespace {

std::filesystem::path MakeTempDir(const std::string& name_prefix) {
  std::error_code ec;
  auto base = std::filesystem::temp_directory_path(ec);
  if (base.empty()) {
    base = std::filesystem::current_path(ec);
  }
  if (base.empty()) {
    base = std::filesystem::path{"."};
  }
  std::filesystem::path dir = base / name_prefix;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  return dir;
}

}  // namespace

int main() {
  const auto dir = MakeTempDir("mi_e2ee_fs_lock_test");
  const auto lock_path = dir / "history.lock";

  mi::platform::fs::FileLock lock1;
  mi::platform::fs::FileLock lock2;

  const auto s1 =
      mi::platform::fs::AcquireExclusiveFileLock(lock_path, lock1);
  assert(s1 == mi::platform::fs::FileLockStatus::kOk);

  const auto s2 =
      mi::platform::fs::AcquireExclusiveFileLock(lock_path, lock2);
  assert(s2 == mi::platform::fs::FileLockStatus::kBusy);

  mi::platform::fs::ReleaseFileLock(lock1);

  const auto s3 =
      mi::platform::fs::AcquireExclusiveFileLock(lock_path, lock2);
  assert(s3 == mi::platform::fs::FileLockStatus::kOk);

  mi::platform::fs::ReleaseFileLock(lock2);
  return 0;
}
