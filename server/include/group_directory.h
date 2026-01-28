#ifndef MI_E2EE_SERVER_GROUP_DIRECTORY_H
#define MI_E2EE_SERVER_GROUP_DIRECTORY_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <optional>

#include "config.h"
#include "state_store.h"

namespace mi::server {

enum class GroupRole : std::uint8_t { kOwner = 0, kAdmin = 1, kMember = 2 };

struct GroupMemberInfo {
  std::string username;
  GroupRole role{GroupRole::kMember};
};

class GroupDirectory {
 public:
  explicit GroupDirectory(
      std::filesystem::path persist_dir = {},
      KeyProtectionMode state_protection = KeyProtectionMode::kNone,
      StateStore* state_store = nullptr);

  bool AddGroup(const std::string& group_id, const std::string& owner);
  bool AddMember(const std::string& group_id, const std::string& user);
  bool RemoveMember(const std::string& group_id, const std::string& user);
  bool HasMember(const std::string& group_id, const std::string& user);
  std::vector<std::string> Members(const std::string& group_id);
  std::vector<GroupMemberInfo> MembersWithRoles(const std::string& group_id);
  std::optional<GroupRole> RoleOf(const std::string& group_id,
                                  const std::string& user);
  bool SetRole(const std::string& group_id, const std::string& user,
               GroupRole role);
  bool persistence_enabled() const { return persistence_enabled_; }

 private:
  struct GroupInfo {
    std::string owner;
    std::unordered_map<std::string, GroupRole> members;
  };

  static std::string PickNewOwner(const GroupInfo& group);
  bool LoadFromDisk();
  bool LoadFromStore();
  bool LoadFromStoreLocked();
  bool LoadFromFile();
  bool LoadFromBytes(const std::vector<std::uint8_t>& bytes);
  bool SaveLocked();
  bool SaveToStoreLocked();
  bool SaveToStoreLockedUnlocked();

  mutable std::mutex mutex_;
  std::unordered_map<std::string, GroupInfo> groups_;
  std::filesystem::path persist_path_;
  bool persistence_enabled_{false};
  KeyProtectionMode state_protection_{KeyProtectionMode::kNone};
  StateStore* state_store_{nullptr};
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_GROUP_DIRECTORY_H
