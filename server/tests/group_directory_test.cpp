#include <string>
#include <vector>

#include "group_directory.h"

using mi::server::GroupDirectory;

int main() {
  GroupDirectory dir;
  bool ok = dir.AddGroup("g1", "alice");
  if (!ok) return 1;
  ok = dir.AddMember("g1", "bob");
  if (!ok) return 1;
  ok = dir.AddMember("g1", "alice");  // duplicate
  if (ok) return 1;

  auto members = dir.Members("g1");
  if (members.size() != 2 || !dir.HasMember("g1", "bob")) return 1;

  ok = dir.RemoveMember("g1", "bob");
  if (!ok || dir.HasMember("g1", "bob")) return 1;

  return 0;
}
