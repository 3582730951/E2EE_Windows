#include "group_directory.h"

namespace mi::server {

std::string GroupDirectory::PickNewOwner(const GroupInfo& group) {
  std::string best;
  for (const auto& kv : group.members) {
    if (best.empty() || kv.first < best) {
      best = kv.first;
    }
  }
  return best;
}

bool GroupDirectory::AddGroup(const std::string& group_id,
                              const std::string& owner) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (group_id.empty() || owner.empty()) {
    return false;
  }
  if (groups_.count(group_id) != 0) {
    return false;
  }
  GroupInfo g;
  g.owner = owner;
  g.members.emplace(owner, GroupRole::kOwner);
  groups_[group_id] = std::move(g);
  return true;
}

bool GroupDirectory::AddMember(const std::string& group_id,
                               const std::string& user) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (group_id.empty() || user.empty()) {
    return false;
  }
  auto& g = groups_[group_id];
  if (g.members.empty()) {
    g.owner = user;
    g.members.emplace(user, GroupRole::kOwner);
    return true;
  }
  const auto [_, inserted] = g.members.emplace(user, GroupRole::kMember);
  return inserted;
}

bool GroupDirectory::RemoveMember(const std::string& group_id,
                                  const std::string& user) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return false;
  }
  GroupInfo& g = it->second;
  const auto m_it = g.members.find(user);
  if (m_it == g.members.end()) {
    return false;
  }
  const bool was_owner = (g.owner == user);
  g.members.erase(m_it);

  if (g.members.empty()) {
    groups_.erase(it);
    return true;
  }

  if (was_owner) {
    g.owner.clear();
  }
  if (g.owner.empty()) {
    const std::string new_owner = PickNewOwner(g);
    if (!new_owner.empty()) {
      g.owner = new_owner;
      g.members[new_owner] = GroupRole::kOwner;
    }
  }

  return true;
}

bool GroupDirectory::HasMember(const std::string& group_id,
                               const std::string& user) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return false;
  }
  return it->second.members.count(user) > 0;
}

std::vector<std::string> GroupDirectory::Members(
    const std::string& group_id) const {
  std::vector<std::string> out;
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return out;
  }
  out.reserve(it->second.members.size());
  for (const auto& kv : it->second.members) {
    out.push_back(kv.first);
  }
  return out;
}

std::vector<GroupMemberInfo> GroupDirectory::MembersWithRoles(
    const std::string& group_id) const {
  std::vector<GroupMemberInfo> out;
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return out;
  }
  out.reserve(it->second.members.size());
  for (const auto& kv : it->second.members) {
    GroupMemberInfo e;
    e.username = kv.first;
    e.role = kv.second;
    out.push_back(std::move(e));
  }
  return out;
}

std::optional<GroupRole> GroupDirectory::RoleOf(const std::string& group_id,
                                                const std::string& user) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return std::nullopt;
  }
  const auto m_it = it->second.members.find(user);
  if (m_it == it->second.members.end()) {
    return std::nullopt;
  }
  return m_it->second;
}

bool GroupDirectory::SetRole(const std::string& group_id,
                             const std::string& user, GroupRole role) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return false;
  }
  GroupInfo& g = it->second;
  if (user.empty() || g.owner == user) {
    return false;
  }
  if (role == GroupRole::kOwner) {
    return false;
  }
  const auto m_it = g.members.find(user);
  if (m_it == g.members.end()) {
    return false;
  }
  m_it->second = role;
  return true;
}

}  // namespace mi::server
