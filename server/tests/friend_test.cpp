#include <algorithm>
#include <string>

#include "api_service.h"
#include "auth_provider.h"
#include "session_manager.h"

using mi::server::ApiService;
using mi::server::DemoAuthProvider;
using mi::server::DemoUser;
using mi::server::DemoUserTable;
using mi::server::GroupManager;
using mi::server::Session;
using mi::server::SessionManager;

static DemoUser MakeDemoUser(const std::string& username,
                             const std::string& password) {
  DemoUser user;
  user.username.set(username);
  user.password.set(password);
  user.username_plain = username;
  user.password_plain = password;
  return user;
}

int main() {
  DemoUserTable users;
  users.emplace("bob", MakeDemoUser("bob", "pwd123"));
  users.emplace("alice", MakeDemoUser("alice", "alice123"));

  SessionManager sessions(std::make_unique<DemoAuthProvider>(std::move(users)));
  GroupManager groups;
  ApiService api(&sessions, &groups);

  Session bob;
  Session alice;
  std::string err;
  if (!sessions.Login("bob", "pwd123", bob, err)) {
    return 1;
  }
  if (!sessions.Login("alice", "alice123", alice, err)) {
    return 1;
  }

  auto add = api.AddFriend(bob.token, "alice", "Ali");
  if (!add.success) {
    return 1;
  }

  auto bob_list = api.ListFriends(bob.token);
  if (!bob_list.success) {
    return 1;
  }
  auto bob_entry = std::find_if(
      bob_list.friends.begin(), bob_list.friends.end(),
      [](const auto& e) { return e.username == "alice"; });
  if (bob_entry == bob_list.friends.end()) {
    return 1;
  }
  if (bob_entry->remark != "Ali") {
    return 1;
  }

  auto alice_list = api.ListFriends(alice.token);
  if (!alice_list.success) {
    return 1;
  }
  auto alice_entry = std::find_if(
      alice_list.friends.begin(), alice_list.friends.end(),
      [](const auto& e) { return e.username == "bob"; });
  if (alice_entry == alice_list.friends.end()) {
    return 1;
  }
  if (!alice_entry->remark.empty()) {
    return 1;
  }

  auto remark = api.SetFriendRemark(bob.token, "alice", "Alice2");
  if (!remark.success) {
    return 1;
  }
  bob_list = api.ListFriends(bob.token);
  if (!bob_list.success) {
    return 1;
  }
  bob_entry = std::find_if(bob_list.friends.begin(), bob_list.friends.end(),
                           [](const auto& e) { return e.username == "alice"; });
  if (bob_entry == bob_list.friends.end() || bob_entry->remark != "Alice2") {
    return 1;
  }

  remark = api.SetFriendRemark(bob.token, "alice", "");
  if (!remark.success) {
    return 1;
  }
  bob_list = api.ListFriends(bob.token);
  if (!bob_list.success) {
    return 1;
  }
  bob_entry = std::find_if(bob_list.friends.begin(), bob_list.friends.end(),
                           [](const auto& e) { return e.username == "alice"; });
  if (bob_entry == bob_list.friends.end() || !bob_entry->remark.empty()) {
    return 1;
  }

  auto bad = api.AddFriend(bob.token, "nobody");
  if (bad.success) {
    return 1;
  }

  return 0;
}
