#include <cstdint>
#include <string>
#include <vector>

#include "api_service.h"
#include "auth_provider.h"
#include "group_directory.h"
#include "offline_storage.h"
#include "session_manager.h"

using mi::server::ApiService;
using mi::server::DemoAuthProvider;
using mi::server::DemoUser;
using mi::server::DemoUserTable;
using mi::server::GroupDirectory;
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
  users.emplace("charlie", MakeDemoUser("charlie", "c123"));

  SessionManager sessions(std::make_unique<DemoAuthProvider>(std::move(users)));
  GroupManager groups;
  GroupDirectory directory;
  OfflineQueue queue;
  ApiService api(&sessions, &groups, &directory, nullptr, &queue);

  Session bob;
  Session alice;
  Session charlie;
  std::string err;
  if (!sessions.Login("bob", "pwd123", TransportKind::kLocal, bob, err)) {
    return 1;
  }
  if (!sessions.Login("alice", "alice123", TransportKind::kLocal, alice, err)) {
    return 1;
  }
  if (!sessions.Login("charlie", "c123", TransportKind::kLocal, charlie, err)) {
    return 1;
  }

  // Join group.
  if (!api.JoinGroup(bob.token, "g1").success) {
    return 1;
  }
  if (!api.JoinGroup(alice.token, "g1").success) {
    return 1;
  }

  // Non-member cannot send.
  if (api.SendGroupCipher(charlie.token, "g1", {1}).success) {
    return 1;
  }

  std::vector<std::uint8_t> payload = {9, 8, 7};
  if (!api.SendGroupCipher(bob.token, "g1", payload).success) {
    return 1;
  }

  const auto pulled = api.PullGroupCipher(alice.token);
  if (!pulled.success || pulled.messages.size() != 1) {
    return 1;
  }
  if (pulled.messages[0].group_id != "g1" || pulled.messages[0].sender != "bob" ||
      pulled.messages[0].payload != payload) {
    return 1;
  }

  // Leave group should stop delivery.
  if (!api.LeaveGroup(alice.token, "g1").success) {
    return 1;
  }
  if (!api.SendGroupCipher(bob.token, "g1", {1, 2}).success) {
    return 1;
  }
  const auto pulled2 = api.PullGroupCipher(alice.token);
  if (!pulled2.success || !pulled2.messages.empty()) {
    return 1;
  }

  return 0;
}
