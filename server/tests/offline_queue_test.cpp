#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "offline_storage.h"

namespace {

#define FAIL()                                                         \
  do {                                                                 \
    std::cerr << "offline_queue_test failed at " << __FILE__ << ":"    \
              << __LINE__ << "\n";                                     \
    return 1;                                                          \
  } while (false)

std::filesystem::path TempDir(const std::string& name) {
  std::error_code ec;
  auto dir = std::filesystem::temp_directory_path(ec) / name;
  if (ec) {
    dir = std::filesystem::path{"."} / name;
  }
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  return dir;
}

bool SamePayload(const std::vector<std::uint8_t>& a,
                 const std::vector<std::uint8_t>& b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

}  // namespace

int main() {
  const auto dir = TempDir("mi_e2ee_offline_queue_persist");
  const std::string recipient = "alice";
  const std::string sender = "bob";
  const std::string group_id = "group-1";

  const std::vector<std::uint8_t> generic_payload = {1, 2, 3};
  const std::vector<std::uint8_t> private_payload = {4, 5};
  const std::vector<std::uint8_t> group_cipher_payload = {6, 7, 8};
  const std::vector<std::uint8_t> group_notice_payload = {9};
  const std::vector<std::uint8_t> device_sync_payload = {10, 11, 12};

  {
    mi::server::OfflineQueue queue(std::chrono::seconds(60), dir);
    if (!queue.persistence_enabled()) {
      FAIL();
    }
    queue.Enqueue(recipient, generic_payload);
    queue.EnqueuePrivate(recipient, sender, private_payload);
    queue.EnqueueGroupCipher(recipient, group_id, sender, group_cipher_payload);
    queue.EnqueueGroupNotice(recipient, group_id, sender, group_notice_payload);
    queue.EnqueueDeviceSync(recipient, device_sync_payload);
  }

  {
    mi::server::OfflineQueue queue(std::chrono::seconds(60), dir);

    auto generic = queue.Drain(recipient);
    if (generic.size() != 1 || !SamePayload(generic[0], generic_payload)) {
      FAIL();
    }

    auto privates = queue.DrainPrivate(recipient);
    if (privates.size() != 1) {
      FAIL();
    }
    const auto& priv = privates[0];
    if (priv.recipient != recipient || priv.sender != sender ||
        !SamePayload(priv.payload, private_payload)) {
      FAIL();
    }

    auto group_ciphers = queue.DrainGroupCipher(recipient);
    if (group_ciphers.size() != 1) {
      FAIL();
    }
    const auto& cipher = group_ciphers[0];
    if (cipher.recipient != recipient || cipher.sender != sender ||
        cipher.group_id != group_id ||
        !SamePayload(cipher.payload, group_cipher_payload)) {
      FAIL();
    }

    auto group_notices = queue.DrainGroupNotice(recipient);
    if (group_notices.size() != 1) {
      FAIL();
    }
    const auto& notice = group_notices[0];
    if (notice.recipient != recipient || notice.sender != sender ||
        notice.group_id != group_id ||
        !SamePayload(notice.payload, group_notice_payload)) {
      FAIL();
    }

    auto device_sync = queue.DrainDeviceSync(recipient);
    if (device_sync.size() != 1 ||
        !SamePayload(device_sync[0], device_sync_payload)) {
      FAIL();
    }
  }

  return 0;
}
