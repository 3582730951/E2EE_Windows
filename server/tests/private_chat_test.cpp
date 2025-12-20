#include <cstdint>
#include <string>
#include <vector>

#include "api_service.h"
#include "auth_provider.h"
#include "offline_storage.h"
#include "session_manager.h"

using mi::server::ApiService;
using mi::server::DemoAuthProvider;
using mi::server::DemoUser;
using mi::server::DemoUserTable;
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
  OfflineQueue queue;
  ApiService api(&sessions, &groups, nullptr, nullptr, &queue);

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

  // PreKey publish/fetch requires being friends.
  if (!api.AddFriend(bob.token, "alice").success) {
    return 1;
  }
  std::vector<std::uint8_t> bundle = {1, 2, 3, 4};
  if (!api.PublishPreKeyBundle(alice.token, bundle).success) {
    return 1;
  }
  const auto fetched = api.FetchPreKeyBundle(bob.token, "alice");
  if (!fetched.success || fetched.bundle != bundle) {
    return 1;
  }
  const auto denied = api.FetchPreKeyBundle(charlie.token, "alice");
  if (denied.success) {
    return 1;
  }

  // Ensure generic offline vs private queues are separated.
  std::vector<std::uint8_t> offline_payload = {9, 9, 9};
  if (!api.EnqueueOffline(bob.token, "alice", offline_payload).success) {
    return 1;
  }
  std::vector<std::uint8_t> private_payload = {7, 7, 7};
  if (!api.SendPrivate(bob.token, "alice", private_payload).success) {
    return 1;
  }

  const auto pull_offline = api.PullOffline(alice.token);
  if (!pull_offline.success || pull_offline.messages.size() != 1 ||
      pull_offline.messages[0] != offline_payload) {
    return 1;
  }

  const auto pull_private = api.PullPrivate(alice.token);
  if (!pull_private.success || pull_private.messages.size() != 1 ||
      pull_private.messages[0].sender != "bob" ||
      pull_private.messages[0].payload != private_payload) {
    return 1;
  }

  // Private send must require friend relationship.
  const auto private_denied = api.SendPrivate(charlie.token, "alice", {1});
  if (private_denied.success) {
    return 1;
  }

  return 0;
}
