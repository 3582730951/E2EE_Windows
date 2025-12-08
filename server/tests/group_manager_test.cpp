#include <cstdint>
#include <string>
#include <iostream>

#include "group_manager.h"

using mi::server::GroupManager;
using mi::server::RotationReason;

int main() {
  std::cout << "start" << std::endl;
  GroupManager gm;

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

  std::cout << "ok\n";
  return 0;
}
