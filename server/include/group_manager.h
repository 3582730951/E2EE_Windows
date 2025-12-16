#ifndef MI_E2EE_SERVER_GROUP_MANAGER_H
#define MI_E2EE_SERVER_GROUP_MANAGER_H

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

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
  GroupManager();

  GroupKey Rotate(const std::string& group_id, RotationReason reason);

  std::optional<GroupKey> GetKey(const std::string& group_id);

  std::optional<GroupKey> OnMessage(const std::string& group_id,
                                    std::uint64_t threshold = 10000);

 private:
  GroupKey MakeKey(std::uint32_t next_version, RotationReason reason);

  std::mutex mutex_;
  std::unordered_map<std::string, GroupState> groups_;
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_GROUP_MANAGER_H
