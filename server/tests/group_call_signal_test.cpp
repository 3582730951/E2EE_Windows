#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "api_service.h"
#include "auth_provider.h"
#include "frame.h"
#include "frame_router.h"
#include "group_call_manager.h"
#include "group_directory.h"
#include "media_relay.h"
#include "protocol.h"
#include "session_manager.h"

using mi::server::ApiService;
using mi::server::DemoAuthProvider;
using mi::server::DemoUser;
using mi::server::DemoUserTable;
using mi::server::Frame;
using mi::server::FrameRouter;
using mi::server::FrameType;
using mi::server::GroupCallConfig;
using mi::server::GroupCallManager;
using mi::server::GroupDirectory;
using mi::server::GroupManager;
using mi::server::MediaRelay;
using mi::server::SessionManager;
using mi::server::proto::ReadBytes;
using mi::server::proto::ReadString;
using mi::server::proto::ReadUint32;
using mi::server::proto::ReadUint64;
using mi::server::proto::WriteBytes;
using mi::server::proto::WriteString;
using mi::server::proto::WriteUint32;
using mi::server::proto::WriteUint64;

namespace {

Frame MakeLoginFrame(const std::string& u, const std::string& p) {
  Frame f;
  f.type = FrameType::kLogin;
  WriteString(u, f.payload);
  WriteString(p, f.payload);
  return f;
}

void WriteFixed16(const std::array<std::uint8_t, 16>& in,
                  std::vector<std::uint8_t>& out) {
  out.insert(out.end(), in.begin(), in.end());
}

bool ReadFixed16(const std::vector<std::uint8_t>& data,
                 std::size_t& offset,
                 std::array<std::uint8_t, 16>& out) {
  if (offset + out.size() > data.size()) {
    return false;
  }
  std::memcpy(out.data(), data.data() + offset, out.size());
  offset += out.size();
  return true;
}

Frame MakeGroupCallSignal(std::uint8_t op,
                          const std::string& group_id,
                          const std::array<std::uint8_t, 16>& call_id,
                          std::uint8_t media_flags,
                          std::uint32_t key_id) {
  Frame f;
  f.type = FrameType::kGroupCallSignal;
  f.payload.push_back(op);
  WriteString(group_id, f.payload);
  WriteFixed16(call_id, f.payload);
  f.payload.push_back(media_flags);
  WriteUint32(key_id, f.payload);
  WriteUint32(0, f.payload);  // seq
  WriteUint64(0, f.payload);  // ts
  WriteBytes(nullptr, 0, f.payload);
  return f;
}

}  // namespace

int main() {
  DemoUserTable table;
  DemoUser alice;
  alice.username.set("alice");
  alice.password.set("pwd");
  alice.username_plain = "alice";
  alice.password_plain = "pwd";
  table.emplace("alice", alice);
  DemoUser bob;
  bob.username.set("bob");
  bob.password.set("pwd");
  bob.username_plain = "bob";
  bob.password_plain = "pwd";
  table.emplace("bob", bob);

  auto auth = std::make_unique<DemoAuthProvider>(std::move(table));
  SessionManager sessions(std::move(auth));
  GroupManager groups;
  GroupDirectory directory;
  directory.AddGroup("g1", "alice");
  directory.AddMember("g1", "bob");

  GroupCallConfig call_cfg;
  call_cfg.enable_group_call = true;
  GroupCallManager calls(call_cfg);
  MediaRelay relay(256, std::chrono::milliseconds(500));

  ApiService api(&sessions, &groups, &calls, &directory, nullptr, nullptr,
                 &relay);
  FrameRouter router(&api);

  Frame login_alice = MakeLoginFrame("alice", "pwd");
  Frame login_resp;
  bool ok = router.Handle(login_alice, login_resp, "",
                          mi::server::TransportKind::kLocal);
  assert(ok);
  std::size_t off = 1;
  std::string token_alice;
  assert(ReadString(login_resp.payload, off, token_alice));
  assert(!token_alice.empty());

  Frame login_bob = MakeLoginFrame("bob", "pwd");
  Frame login_resp_bob;
  ok = router.Handle(login_bob, login_resp_bob, "",
                     mi::server::TransportKind::kLocal);
  assert(ok);
  off = 1;
  std::string token_bob;
  assert(ReadString(login_resp_bob.payload, off, token_bob));
  assert(!token_bob.empty());

  std::array<std::uint8_t, 16> empty_call{};
  Frame create = MakeGroupCallSignal(1, "g1", empty_call, 1, 0);
  Frame create_resp;
  ok = router.Handle(create, create_resp, token_alice,
                     mi::server::TransportKind::kLocal);
  assert(ok);
  assert(!create_resp.payload.empty());
  assert(create_resp.payload[0] == 1);

  off = 1;
  std::array<std::uint8_t, 16> call_id{};
  std::uint32_t key_id = 0;
  std::uint32_t member_count = 0;
  assert(ReadFixed16(create_resp.payload, off, call_id));
  assert(ReadUint32(create_resp.payload, off, key_id));
  assert(key_id == 1);
  assert(ReadUint32(create_resp.payload, off, member_count));
  assert(member_count >= 1);

  Frame join = MakeGroupCallSignal(2, "g1", call_id, 1, 0);
  Frame join_resp;
  ok = router.Handle(join, join_resp, token_bob,
                     mi::server::TransportKind::kLocal);
  assert(ok);
  assert(join_resp.payload[0] == 1);

  Frame pull_events;
  pull_events.type = FrameType::kGroupCallSignalPull;
  WriteUint32(8, pull_events.payload);
  WriteUint32(0, pull_events.payload);
  Frame pull_resp;
  ok = router.Handle(pull_events, pull_resp, token_bob,
                     mi::server::TransportKind::kLocal);
  assert(ok);
  assert(pull_resp.payload[0] == 1);
  off = 1;
  std::uint32_t ev_count = 0;
  assert(ReadUint32(pull_resp.payload, off, ev_count));
  assert(ev_count >= 1);

  Frame media_push;
  media_push.type = FrameType::kGroupMediaPush;
  WriteString("g1", media_push.payload);
  WriteFixed16(call_id, media_push.payload);
  std::vector<std::uint8_t> payload(22, 0);
  payload[0] = 2;  // MediaPacket v2
  payload[1] = 1;  // audio kind
  WriteBytes(payload, media_push.payload);
  Frame media_push_resp;
  ok = router.Handle(media_push, media_push_resp, token_alice,
                     mi::server::TransportKind::kLocal);
  assert(ok);
  assert(media_push_resp.payload[0] == 1);

  Frame media_pull;
  media_pull.type = FrameType::kGroupMediaPull;
  WriteFixed16(call_id, media_pull.payload);
  WriteUint32(8, media_pull.payload);
  WriteUint32(0, media_pull.payload);
  Frame media_pull_resp;
  ok = router.Handle(media_pull, media_pull_resp, token_bob,
                     mi::server::TransportKind::kLocal);
  assert(ok);
  assert(media_pull_resp.payload[0] == 1);
  off = 1;
  std::uint32_t pkt_count = 0;
  assert(ReadUint32(media_pull_resp.payload, off, pkt_count));
  assert(pkt_count >= 1);
  std::string sender;
  std::vector<std::uint8_t> pulled;
  assert(ReadString(media_pull_resp.payload, off, sender));
  assert(ReadBytes(media_pull_resp.payload, off, pulled));
  assert(sender == "alice");
  assert(pulled == payload);

  return 0;
}
