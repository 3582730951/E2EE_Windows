#include <string>
#include <vector>

#include "group_directory.h"

using mi::server::GroupDirectory;
using mi::server::GroupRole;

int main() {
  GroupDirectory dir;
  bool ok = dir.AddGroup("g1", "alice");
  if (!ok) return 1;
  auto alice_role = dir.RoleOf("g1", "alice");
  if (!alice_role.has_value() || alice_role.value() != GroupRole::kOwner) return 1;
  ok = dir.AddMember("g1", "bob");
  if (!ok) return 1;
  auto bob_role = dir.RoleOf("g1", "bob");
  if (!bob_role.has_value() || bob_role.value() != GroupRole::kMember) return 1;
  ok = dir.AddMember("g1", "alice");  // duplicate
  if (ok) return 1;

  auto members = dir.Members("g1");
  if (members.size() != 2 || !dir.HasMember("g1", "bob")) return 1;

  ok = dir.SetRole("g1", "bob", GroupRole::kAdmin);
  if (!ok) return 1;
  bob_role = dir.RoleOf("g1", "bob");
  if (!bob_role.has_value() || bob_role.value() != GroupRole::kAdmin) return 1;

  ok = dir.RemoveMember("g1", "alice");  // owner leaves, bob becomes owner
  if (!ok || dir.HasMember("g1", "alice")) return 1;
  bob_role = dir.RoleOf("g1", "bob");
  if (!bob_role.has_value() || bob_role.value() != GroupRole::kOwner) return 1;

  ok = dir.RemoveMember("g1", "bob");
  if (!ok || dir.HasMember("g1", "bob")) return 1;

  return 0;
}
