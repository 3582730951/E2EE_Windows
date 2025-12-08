#include "group_manager.h"

#include <random>
#include <utility>

namespace mi::server {

GroupManager::GroupManager() = default;

GroupKey GroupManager::MakeKey(std::uint32_t next_version,
                               RotationReason reason) {
  GroupKey key;
  key.version = next_version;
  key.reason = reason;
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_int_distribution<int> dist(0, 255);
  for (auto& b : key.sender_key) {
    b = static_cast<std::uint8_t>(dist(gen));
  }
  return key;
}

GroupKey GroupManager::Rotate(const std::string& group_id,
                              RotationReason reason) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& state = groups_[group_id];
  state.group_id = group_id;
  const std::uint32_t next_version = state.key.version + 1;
  state.key = MakeKey(next_version, reason);
  state.message_count = 0;
  return state.key;
}

std::optional<GroupKey> GroupManager::GetKey(const std::string& group_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return std::nullopt;
  }
  return it->second.key;
}

std::optional<GroupKey> GroupManager::OnMessage(const std::string& group_id,
                                                std::uint64_t threshold) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    GroupState state;
    state.group_id = group_id;
    state.key = MakeKey(1, RotationReason::kJoin);
    state.message_count = 1;
    groups_.emplace(group_id, std::move(state));
    return std::nullopt;
  }
  it->second.message_count += 1;
  if (threshold > 0 && it->second.message_count >= threshold) {
    const std::uint32_t next_version = it->second.key.version + 1;
    it->second.key = MakeKey(next_version, RotationReason::kMessageThreshold);
    it->second.message_count = 0;
    return it->second.key;
  }
  return std::nullopt;
}

}  // namespace mi::server
