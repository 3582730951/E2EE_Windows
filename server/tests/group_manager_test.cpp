#include <cstdint>
#include <filesystem>
#include <string>

#include "group_manager.h"

using mi::server::GroupManager;
using mi::server::RotationReason;

int main() {
  const auto base_dir =
      std::filesystem::current_path() / "test_state_group_manager";
  std::error_code ec;
  std::filesystem::remove_all(base_dir, ec);
  std::filesystem::create_directories(base_dir, ec);
  if (ec) {
    return 1;
  }

  GroupManager gm(base_dir);

  auto key1 = gm.Rotate("g1", RotationReason::kJoin);
  if (key1.version != 1) {
    return 1;
  }

  auto next = gm.OnMessage("g1", 3);
  if (next.has_value()) {
    return 1;
  }
  gm.OnMessage("g1", 3);
  next = gm.OnMessage("g1", 3);
  if (!next.has_value() || next->version != 2 ||
      next->reason != RotationReason::kMessageThreshold) {
    return 1;
  }

  auto cur = gm.GetKey("g1");
  if (!cur.has_value() || cur->version != 2) {
    return 1;
  }

  auto kicked = gm.Rotate("g1", RotationReason::kKick);
  if (kicked.version != 3 || kicked.reason != RotationReason::kKick) {
    return 1;
  }

  GroupManager reload(base_dir);
  auto cur2 = reload.GetKey("g1");
  if (!cur2.has_value() || cur2->version != 3 ||
      cur2->reason != RotationReason::kKick) {
    return 1;
  }

  return 0;
}
