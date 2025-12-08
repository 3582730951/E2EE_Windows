#include "offline_storage.h"

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

namespace {

bool Check(bool cond) {
  return cond;
}

std::filesystem::path TempDir(const std::string& name) {
  auto dir = std::filesystem::temp_directory_path() / name;
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  return dir;
}

}  // namespace

int main() {
  {
    const auto dir = TempDir("mi_e2ee_offline_put_fetch");
    mi::server::OfflineStorage storage(dir, std::chrono::seconds(60));

    std::vector<std::uint8_t> payload = {1, 2, 3, 4, 5, 6, 7};
    auto put = storage.Put("alice", payload);
    if (!Check(put.success && !put.file_id.empty() && put.meta.owner == "alice")) {
      return 1;
    }

    std::string err;
    auto fetched = storage.Fetch(put.file_id, put.file_key, true, err);
    if (!fetched.has_value() || payload != fetched.value()) {
      return 1;
    }
    if (std::filesystem::exists(dir / (put.file_id + ".bin"))) {
      return 1;
    }
    if (storage.Meta(put.file_id).has_value()) {
      return 1;
    }
  }

  {
    const auto dir = TempDir("mi_e2ee_offline_cleanup");
    mi::server::OfflineStorage storage(dir, std::chrono::seconds(1));
    std::vector<std::uint8_t> payload(64, 0xAB);
    auto put = storage.Put("bob", payload);
    if (!put.success || !std::filesystem::exists(dir / (put.file_id + ".bin"))) {
      return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    storage.CleanupExpired();
    if (std::filesystem::exists(dir / (put.file_id + ".bin"))) {
      return 1;
    }
    if (storage.Meta(put.file_id).has_value()) {
      return 1;
    }
  }

  {
    mi::server::OfflineQueue queue(std::chrono::seconds(1));
    queue.Enqueue("alice", {1, 2, 3});
    queue.Enqueue("alice", {4, 5, 6});
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    queue.Enqueue("alice", {7, 8, 9}, std::chrono::seconds(2));

    queue.CleanupExpired();
    auto msgs = queue.Drain("alice");
    if (msgs.size() != 1u || msgs[0] != std::vector<std::uint8_t>({7, 8, 9})) {
      return 1;
    }
    if (!queue.Drain("alice").empty()) {
      return 1;
    }
  }

  return 0;
}
