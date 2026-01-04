#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "group_call_manager.h"

int main() {
  using mi::server::GroupCallConfig;
  using mi::server::GroupCallManager;
  using mi::server::GroupCallSubscription;
  using mi::server::GroupCallSnapshot;

  GroupCallConfig cfg;
  cfg.enable_group_call = true;
  cfg.max_room_size = 3;
  cfg.idle_timeout_sec = 60;
  cfg.call_timeout_sec = 300;

  GroupCallManager mgr(cfg);
  std::array<std::uint8_t, 16> call_id{};
  GroupCallSnapshot snap;
  std::string err;

  const bool created = mgr.CreateCall("g1", "alice", 1, call_id, snap, err);
  assert(created);
  assert(err.empty());
  const bool call_id_set = call_id != std::array<std::uint8_t, 16>{};
  assert(call_id_set);
  assert(snap.members.size() == 1);
  assert(snap.key_id == 1);

  const bool join_bob = mgr.JoinCall("g1", call_id, "bob", 1, snap, err);
  assert(join_bob);
  assert(err.empty());
  assert(snap.key_id == 2);

  const bool join_carol = mgr.JoinCall("g1", call_id, "carol", 1, snap, err);
  assert(join_carol);
  assert(err.empty());
  assert(snap.key_id == 3);

  std::vector<GroupCallSubscription> subs;
  GroupCallSubscription sub;
  sub.sender = "bob";
  sub.media_flags = mi::server::kGroupCallMediaAudio;
  subs.push_back(sub);
  const bool subs_ok = mgr.UpdateSubscriptions(call_id, "alice", subs, err);
  assert(subs_ok);
  assert(err.empty());
  const bool alice_audio = mgr.IsSubscribed(call_id, "alice", "bob",
                                            mi::server::kGroupCallMediaAudio);
  const bool alice_video = mgr.IsSubscribed(call_id, "alice", "bob",
                                            mi::server::kGroupCallMediaVideo);
  const bool alice_carol = mgr.IsSubscribed(call_id, "alice", "carol",
                                            mi::server::kGroupCallMediaAudio);
  const bool bob_audio = mgr.IsSubscribed(call_id, "bob", "alice",
                                          mi::server::kGroupCallMediaAudio);
  assert(alice_audio);
  assert(!alice_video);
  assert(!alice_carol);
  assert(bob_audio);

  // Room full after 3 members.
  const bool join_dave = mgr.JoinCall("g1", call_id, "dave", 1, snap, err);
  assert(!join_dave);
  assert(!err.empty());

  bool ended = false;
  const bool leave_bob = mgr.LeaveCall("g1", call_id, "bob", snap, ended, err);
  assert(leave_bob);
  assert(!ended);
  assert(snap.key_id == 4);

  ended = false;
  const bool leave_alice = mgr.LeaveCall("g1", call_id, "alice", snap, ended, err);
  assert(leave_alice);
  assert(ended);

  GroupCallSnapshot missing;
  const bool has_call = mgr.GetCall(call_id, missing);
  assert(!has_call);

  return 0;
}
