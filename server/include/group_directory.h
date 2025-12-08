#ifndef MI_E2EE_SERVER_GROUP_DIRECTORY_H
#define MI_E2EE_SERVER_GROUP_DIRECTORY_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>

namespace mi::server {

class GroupDirectory {
 public:
  bool AddGroup(const std::string& group_id, const std::string& owner);
  bool AddMember(const std::string& group_id, const std::string& user);
  bool RemoveMember(const std::string& group_id, const std::string& user);
  bool HasMember(const std::string& group_id, const std::string& user) const;
  std::vector<std::string> Members(const std::string& group_id) const;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::unordered_set<std::string>> groups_;
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_GROUP_DIRECTORY_H
