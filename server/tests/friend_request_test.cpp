#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "api_service.h"
#include "auth_provider.h"
#include "group_call_manager.h"
#include "offline_storage.h"
#include "session_manager.h"

using mi::server::ApiService;
using mi::server::DemoAuthProvider;
using mi::server::DemoUser;
using mi::server::DemoUserTable;
using mi::server::GroupCallManager;
using mi::server::GroupManager;
using mi::server::OfflineQueue;
using mi::server::Session;
using mi::server::SessionManager;
using mi::server::TransportKind;

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
  GroupCallManager calls;
  OfflineQueue queue;
  ApiService api(&sessions, &groups, &calls, nullptr, nullptr, &queue);

  Session bob;
  Session alice;
  std::string err;
  if (!sessions.Login("bob", "pwd123", TransportKind::kLocal, bob, err)) {
    return 1;
  }
  if (!sessions.Login("alice", "alice123", TransportKind::kLocal, alice, err)) {
    return 1;
  }

  // Bob requests Alice.
  auto send_req = api.SendFriendRequest(bob.token, "alice", "Ali");
  if (!send_req.success) {
    return 1;
  }
  auto list_req = api.ListFriendRequests(alice.token);
  if (!list_req.success) {
    return 1;
  }
  if (list_req.requests.size() != 1 ||
      list_req.requests[0].requester_username != "bob") {
    return 1;
  }

  // Alice accepts.
  auto accept = api.RespondFriendRequest(alice.token, "bob", true);
  if (!accept.success) {
    return 1;
  }
  list_req = api.ListFriendRequests(alice.token);
  if (!list_req.success || !list_req.requests.empty()) {
    return 1;
  }

  // Friend relation established.
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

  // Blocking should silently drop new requests and private messages.
  auto blocked = api.SetUserBlocked(alice.token, "bob", true);
  if (!blocked.success) {
    return 1;
  }
  send_req = api.SendFriendRequest(bob.token, "alice", "x");
  if (!send_req.success) {
    return 1;
  }
  list_req = api.ListFriendRequests(alice.token);
  if (!list_req.success || !list_req.requests.empty()) {
    return 1;
  }

  std::vector<std::uint8_t> payload = {1, 2, 3};
  auto private_send = api.SendPrivate(bob.token, "alice", payload);
  if (!private_send.success) {
    return 1;
  }
  auto private_pull = api.PullPrivate(alice.token);
  if (!private_pull.success) {
    return 1;
  }
  if (!private_pull.messages.empty()) {
    return 1;
  }

  return 0;
}
