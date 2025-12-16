#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "api_service.h"
#include "auth_provider.h"
#include "group_directory.h"
#include "offline_storage.h"
#include "protocol.h"
#include "session_manager.h"

using mi::server::ApiService;
using mi::server::DemoAuthProvider;
using mi::server::DemoUser;
using mi::server::DemoUserTable;
using mi::server::GroupDirectory;
using mi::server::GroupManager;
using mi::server::GroupRole;
using mi::server::OfflineQueue;
using mi::server::Session;
using mi::server::SessionManager;

namespace {

constexpr std::uint8_t kGroupNoticeJoin = 1;
constexpr std::uint8_t kGroupNoticeLeave = 2;
constexpr std::uint8_t kGroupNoticeKick = 3;
constexpr std::uint8_t kGroupNoticeRoleSet = 4;

static DemoUser MakeDemoUser(const std::string& username,
                             const std::string& password) {
  DemoUser user;
  user.username.set(username);
  user.password.set(password);
  user.username_plain = username;
  user.password_plain = password;
  return user;
}

bool DecodeNoticePayload(const std::vector<std::uint8_t>& payload,
                         std::uint8_t& out_kind, std::string& out_target,
                         std::optional<std::uint8_t>& out_role) {
  out_kind = 0;
  out_target.clear();
  out_role = std::nullopt;
  if (payload.empty()) {
    return false;
  }
  std::size_t off = 0;
  out_kind = payload[off++];
  if (!mi::server::proto::ReadString(payload, off, out_target)) {
    return false;
  }
  if (out_kind == kGroupNoticeRoleSet) {
    if (off >= payload.size()) {
      return false;
    }
    out_role = payload[off++];
  }
  return off == payload.size();
}

}  // namespace

int main() {
  DemoUserTable users;
  users.emplace("bob", MakeDemoUser("bob", "pwd123"));
  users.emplace("alice", MakeDemoUser("alice", "alice123"));

  SessionManager sessions(std::make_unique<DemoAuthProvider>(std::move(users)));
  GroupManager groups;
  GroupDirectory directory;
  OfflineQueue queue;
  ApiService api(&sessions, &groups, &directory, nullptr, &queue);

  Session bob;
  Session alice;
  std::string err;
  if (!sessions.Login("bob", "pwd123", bob, err)) {
    return 1;
  }
  if (!sessions.Login("alice", "alice123", alice, err)) {
    return 1;
  }

  if (!api.JoinGroup(bob.token, "g1").success) {
    return 1;
  }
  {
    const auto pulled = api.PullGroupNotices(bob.token);
    if (!pulled.success || pulled.notices.size() != 1) {
      return 1;
    }
    std::uint8_t kind = 0;
    std::string target;
    std::optional<std::uint8_t> role;
    if (pulled.notices[0].group_id != "g1" || pulled.notices[0].sender != "bob" ||
        !DecodeNoticePayload(pulled.notices[0].payload, kind, target, role) ||
        kind != kGroupNoticeJoin || target != "bob" || role.has_value()) {
      return 1;
    }
  }

  if (!api.JoinGroup(alice.token, "g1").success) {
    return 1;
  }
  {
    const auto pulled = api.PullGroupNotices(bob.token);
    if (!pulled.success || pulled.notices.size() != 1) {
      return 1;
    }
    std::uint8_t kind = 0;
    std::string target;
    std::optional<std::uint8_t> role;
    if (pulled.notices[0].group_id != "g1" || pulled.notices[0].sender != "alice" ||
        !DecodeNoticePayload(pulled.notices[0].payload, kind, target, role) ||
        kind != kGroupNoticeJoin || target != "alice" || role.has_value()) {
      return 1;
    }
  }

  if (!api.SetGroupRole(bob.token, "g1", "alice", GroupRole::kAdmin).success) {
    return 1;
  }
  {
    const auto pulled = api.PullGroupNotices(bob.token);
    if (!pulled.success || pulled.notices.size() != 1) {
      return 1;
    }
    std::uint8_t kind = 0;
    std::string target;
    std::optional<std::uint8_t> role;
    if (pulled.notices[0].group_id != "g1" || pulled.notices[0].sender != "bob" ||
        !DecodeNoticePayload(pulled.notices[0].payload, kind, target, role) ||
        kind != kGroupNoticeRoleSet || target != "alice" || !role.has_value() ||
        role.value() != static_cast<std::uint8_t>(GroupRole::kAdmin)) {
      return 1;
    }
  }
  {
    const auto pulled = api.PullGroupNotices(alice.token);
    if (!pulled.success || pulled.notices.size() != 2) {
      return 1;
    }
    // alice should first see her own join, then role set
    std::uint8_t kind0 = 0;
    std::string target0;
    std::optional<std::uint8_t> role0;
    std::uint8_t kind1 = 0;
    std::string target1;
    std::optional<std::uint8_t> role1;
    if (pulled.notices[0].sender != "alice" ||
        !DecodeNoticePayload(pulled.notices[0].payload, kind0, target0, role0) ||
        kind0 != kGroupNoticeJoin || target0 != "alice") {
      return 1;
    }
    if (pulled.notices[1].sender != "bob" ||
        !DecodeNoticePayload(pulled.notices[1].payload, kind1, target1, role1) ||
        kind1 != kGroupNoticeRoleSet || target1 != "alice" || !role1.has_value() ||
        role1.value() != static_cast<std::uint8_t>(GroupRole::kAdmin)) {
      return 1;
    }
  }

  if (!api.KickGroupMember(bob.token, "g1", "alice").success) {
    return 1;
  }
  {
    const auto pulled = api.PullGroupNotices(bob.token);
    if (!pulled.success || pulled.notices.size() != 1) {
      return 1;
    }
    std::uint8_t kind = 0;
    std::string target;
    std::optional<std::uint8_t> role;
    if (pulled.notices[0].group_id != "g1" || pulled.notices[0].sender != "bob" ||
        !DecodeNoticePayload(pulled.notices[0].payload, kind, target, role) ||
        kind != kGroupNoticeKick || target != "alice" || role.has_value()) {
      return 1;
    }
  }
  {
    const auto pulled = api.PullGroupNotices(alice.token);
    if (!pulled.success || pulled.notices.size() != 1) {
      return 1;
    }
    std::uint8_t kind = 0;
    std::string target;
    std::optional<std::uint8_t> role;
    if (pulled.notices[0].group_id != "g1" || pulled.notices[0].sender != "bob" ||
        !DecodeNoticePayload(pulled.notices[0].payload, kind, target, role) ||
        kind != kGroupNoticeKick || target != "alice" || role.has_value()) {
      return 1;
    }
  }

  return 0;
}
