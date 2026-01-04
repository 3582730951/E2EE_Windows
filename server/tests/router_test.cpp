#include <string>
#include <vector>

#include "api_service.h"
#include "auth_provider.h"
#include "frame.h"
#include "frame_router.h"
#include "group_call_manager.h"
#include "protocol.h"
#include "group_directory.h"
#include "session_manager.h"

using mi::server::ApiService;
using mi::server::DemoAuthProvider;
using mi::server::DemoUser;
using mi::server::DemoUserTable;
using mi::server::Frame;
using mi::server::FrameRouter;
using mi::server::FrameType;
using mi::server::GroupCallManager;
using mi::server::GroupDirectory;
using mi::server::GroupManager;
using mi::server::LoginRequest;
using mi::server::SessionManager;
using mi::server::proto::ReadString;
using mi::server::proto::ReadUint32;
using mi::server::proto::WriteString;
using mi::server::proto::WriteUint32;

static Frame MakeLoginFrame(const std::string& u, const std::string& p) {
  Frame f;
  f.type = FrameType::kLogin;
  WriteString(u, f.payload);
  WriteString(p, f.payload);
  return f;
}

static Frame MakeGroupMessageFrame(const std::string& gid, std::uint32_t th) {
  Frame f;
  f.type = FrameType::kMessage;
  WriteString(gid, f.payload);
  WriteUint32(th, f.payload);
  return f;
}

static Frame MakeGroupEventFrame(std::uint8_t action, const std::string& gid) {
  Frame f;
  f.type = FrameType::kGroupEvent;
  f.payload.push_back(action);
  WriteString(gid, f.payload);
  return f;
}

int main() {
  DemoUserTable table;
  DemoUser user;
  user.username.set("bob");
  user.password.set("pwd");
  user.username_plain = "bob";
  user.password_plain = "pwd";
  table.emplace("bob", user);
  auto auth = std::make_unique<DemoAuthProvider>(std::move(table));
  SessionManager sessions(std::move(auth));
  GroupManager groups;
  GroupCallManager calls;
  GroupDirectory dir;
  ApiService api(&sessions, &groups, &calls, &dir);
  FrameRouter router(&api);

  Frame login = MakeLoginFrame("bob", "pwd");
  Frame resp;
  bool ok = router.Handle(login, resp, "",
                          mi::server::TransportKind::kLocal);
  if (!ok || resp.type != FrameType::kLogin || resp.payload.empty() ||
      resp.payload[0] != 1) {
    return 1;
  }
  std::size_t off = 0;
  std::vector<std::uint8_t> payload = resp.payload;
  std::string token;
  off = 1;
  ok = ReadString(payload, off, token);
  if (!ok || token.empty()) {
    return 1;
  }

  Frame join = MakeGroupEventFrame(0, "g1");
  Frame join_resp;
  ok = router.Handle(join, join_resp, token,
                     mi::server::TransportKind::kLocal);
  if (!ok || join_resp.payload.empty() || join_resp.payload[0] != 1) {
    return 1;
  }

  Frame msg = MakeGroupMessageFrame("g1", 1);
  Frame msg_resp;
  ok = router.Handle(msg, msg_resp, token,
                     mi::server::TransportKind::kLocal);
  if (!ok || msg_resp.payload.empty() || msg_resp.payload[0] != 1) {
    return 1;
  }
  off = 1;
  const bool rotated = msg_resp.payload[off++] != 0;
  if (!rotated) {
    return 1;
  }
  std::uint32_t ver = 0;
  ok = ReadUint32(msg_resp.payload, off, ver);
  if (!ok || ver < 2) {
    return 1;
  }

  Frame logout;
  logout.type = FrameType::kLogout;
  Frame logout_resp;
  ok = router.Handle(logout, logout_resp, token,
                     mi::server::TransportKind::kLocal);
  if (!ok || logout_resp.payload.empty() || logout_resp.payload[0] != 1) {
    return 1;
  }

  return 0;
}
