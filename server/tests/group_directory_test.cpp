#include <filesystem>
#include <string>
#include <vector>

#include "group_directory.h"

using mi::server::GroupDirectory;
using mi::server::GroupRole;

int main() {
  const auto base_dir =
      std::filesystem::current_path() / "test_state_group_directory";
  std::error_code ec;
  std::filesystem::remove_all(base_dir, ec);
  std::filesystem::create_directories(base_dir, ec);
  if (ec) {
    return 1;
  }

  {
    GroupDirectory dir(base_dir);
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
  }

  {
    GroupDirectory dir_reload(base_dir);
    auto alice_role = dir_reload.RoleOf("g1", "alice");
    auto bob_role = dir_reload.RoleOf("g1", "bob");
    if (!alice_role.has_value() || alice_role.value() != GroupRole::kOwner) return 1;
    if (!bob_role.has_value() || bob_role.value() != GroupRole::kAdmin) return 1;

    bool ok = dir_reload.RemoveMember("g1", "alice");  // owner leaves, bob becomes owner
    if (!ok || dir_reload.HasMember("g1", "alice")) return 1;
    bob_role = dir_reload.RoleOf("g1", "bob");
    if (!bob_role.has_value() || bob_role.value() != GroupRole::kOwner) return 1;
  }

  {
    GroupDirectory dir_reload(base_dir);
    auto bob_role = dir_reload.RoleOf("g1", "bob");
    if (!bob_role.has_value() || bob_role.value() != GroupRole::kOwner) return 1;
    bool ok = dir_reload.RemoveMember("g1", "bob");
    if (!ok || dir_reload.HasMember("g1", "bob")) return 1;
  }

  GroupDirectory dir_reload(base_dir);
  if (dir_reload.HasMember("g1", "bob") || dir_reload.HasMember("g1", "alice")) return 1;

  return 0;
}
