#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#define private public
#include "client_core.h"
#undef private

#include "chat_history_store.h"
#include "storage_service.h"
#include "test_permissions.h"

namespace {

bool Require(bool value, const char* message) {
  if (!value) {
    std::fprintf(stderr, "history_api_canonical_test failed: %s\n", message);
  }
  return value;
}

void WriteLe16(std::uint16_t value, std::vector<std::uint8_t>& out) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

std::array<std::uint8_t, 16> MakeId(std::uint8_t seed) {
  std::array<std::uint8_t, 16> out{};
  for (std::uint8_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<std::uint8_t>(seed + i);
  }
  return out;
}

std::vector<std::uint8_t> StartEnvelope(std::uint8_t type,
                                        std::uint8_t seed) {
  std::vector<std::uint8_t> out;
  out.insert(out.end(), {'M', 'I', 'C', 'H'});
  out.push_back(1);
  out.push_back(type);
  const auto id = MakeId(seed);
  out.insert(out.end(), id.begin(), id.end());
  return out;
}

std::vector<std::uint8_t> BuildControlEnvelope(std::uint8_t seed) {
  return StartEnvelope(2, seed);
}

std::vector<std::uint8_t> BuildUnknownEnvelope(std::uint8_t seed) {
  auto out = StartEnvelope(0xF2, seed);
  WriteLe16(5, out);
  out.insert(out.end(), {'u', 'n', 'k', 'n', 0});
  return out;
}

}  // namespace

int main() {
  std::error_code ec;
  const auto root =
      std::filesystem::temp_directory_path(ec) / "mi_e2ee_history_api_test";
  if (!Require(!ec, "temp directory unavailable")) {
    return 1;
  }
  std::filesystem::remove_all(root, ec);
  if (!Require(mi::client::test::EnsureOwnerOnlyDirectory(root),
               "test directory setup failed")) {
    return 1;
  }

  mi::client::ClientCore core;
  core.username_ = "alice";
  core.e2ee_state_dir_ = root / "e2ee_state";
  core.history_enabled_ = true;
  core.history_store_ = std::make_unique<mi::client::ChatHistoryStore>();
  std::string error;
  if (!Require(core.history_store_->Init(core.e2ee_state_dir_,
                                         core.username_, error),
               "history store init failed")) {
    return 1;
  }

  const auto control = BuildControlEnvelope(0x40);
  const auto unknown = BuildUnknownEnvelope(0x50);
  mi::client::StorageService storage;
  storage.BestEffortPersistHistoryEnvelope(
      core, false, false, "bob", "bob", control,
      mi::client::ClientCore::HistoryStatus::kDelivered, 1);
  storage.BestEffortPersistHistoryEnvelope(
      core, false, false, "bob", "bob", unknown,
      mi::client::ClientCore::HistoryStatus::kSent, 2);
  if (!Require(core.history_store_->Flush(error), "history flush failed")) {
    return 1;
  }

  const auto entries = storage.LoadChatHistory(core, "bob", false, 0);
  if (!Require(entries.size() == 2,
               "control and unknown history entries must not be dropped")) {
    return 1;
  }
  if (!Require(entries[0].kind ==
                   mi::client::ClientCore::HistoryKind::kSystem,
               "control history entry should surface as system control") ||
      !Require(entries[0].message_type == 2,
               "control history entry message type missing") ||
      !Require(entries[0].canonical_envelope == control,
               "control canonical envelope missing")) {
    return 1;
  }
  if (!Require(entries[1].kind ==
                   mi::client::ClientCore::HistoryKind::kUnknown,
               "unknown history entry should surface as unknown") ||
      !Require(entries[1].message_type == 0xF2,
               "unknown history entry message type missing") ||
      !Require(entries[1].canonical_envelope == unknown,
               "unknown canonical envelope missing")) {
    return 1;
  }
  return 0;
}
