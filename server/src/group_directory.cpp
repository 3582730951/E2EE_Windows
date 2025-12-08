#include "group_directory.h"

namespace mi::server {

bool GroupDirectory::AddGroup(const std::string& group_id,
                              const std::string& owner) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& set = groups_[group_id];
  const auto [_, inserted] = set.emplace(owner);
  return inserted;
}

bool GroupDirectory::AddMember(const std::string& group_id,
                               const std::string& user) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& set = groups_[group_id];
  const auto [_, inserted] = set.emplace(user);
  return inserted;
}

bool GroupDirectory::RemoveMember(const std::string& group_id,
                                  const std::string& user) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return false;
  }
  return it->second.erase(user) > 0;
}

bool GroupDirectory::HasMember(const std::string& group_id,
                               const std::string& user) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return false;
  }
  return it->second.count(user) > 0;
}

std::vector<std::string> GroupDirectory::Members(
    const std::string& group_id) const {
  std::vector<std::string> out;
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return out;
  }
  out.reserve(it->second.size());
  for (const auto& m : it->second) {
    out.push_back(m);
  }
  return out;
}

}  // namespace mi::server
