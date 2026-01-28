#ifndef MI_E2EE_SERVER_GROUP_MANAGER_H
#define MI_E2EE_SERVER_GROUP_MANAGER_H

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "state_store.h"

namespace mi::server {

enum class RotationReason : std::uint8_t {
  kJoin = 0,
  kLeave = 1,
  kKick = 2,
  kPeriodic = 3,
  kMessageThreshold = 4
};

struct GroupKey {
  std::uint32_t version{0};
  RotationReason reason{RotationReason::kPeriodic};
};

struct GroupState {
  std::string group_id;
  GroupKey key;
  std::uint64_t message_count{0};
};

class GroupManager {
 public:
  explicit GroupManager(
      std::filesystem::path persist_dir = {},
      KeyProtectionMode state_protection = KeyProtectionMode::kNone,
      StateStore* state_store = nullptr);

  GroupKey Rotate(const std::string& group_id, RotationReason reason);

  std::optional<GroupKey> GetKey(const std::string& group_id);

  std::optional<GroupKey> OnMessage(const std::string& group_id,
                                    std::uint64_t threshold = 10000);
  bool persistence_enabled() const { return persistence_enabled_; }

 private:
  GroupKey MakeKey(std::uint32_t next_version, RotationReason reason);
  bool LoadFromDisk();
  bool LoadFromStore();
  bool LoadFromStoreLocked();
  bool LoadFromFile();
  bool LoadFromBytes(const std::vector<std::uint8_t>& bytes);
  bool SaveLocked();
  bool SaveToStoreLocked();
  bool SaveToStoreLockedUnlocked();

  std::mutex mutex_;
  std::unordered_map<std::string, GroupState> groups_;
  std::filesystem::path persist_path_;
  bool persistence_enabled_{false};
  KeyProtectionMode state_protection_{KeyProtectionMode::kNone};
  StateStore* state_store_{nullptr};
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_GROUP_MANAGER_H
