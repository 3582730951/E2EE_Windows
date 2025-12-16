#include <cassert>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "e2ee_engine.h"

using mi::client::e2ee::Engine;
using mi::client::e2ee::PrivateMessage;

namespace {

std::filesystem::path MakeTempDir(const std::string& name_prefix) {
  std::error_code ec;
  auto base = std::filesystem::temp_directory_path(ec);
  if (base.empty()) {
    base = std::filesystem::current_path(ec);
  }
  if (base.empty()) {
    base = std::filesystem::path{"."};
  }

  std::filesystem::path dir = base / name_prefix;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  return dir;
}

std::vector<std::uint8_t> Bytes(const std::string& s) {
  return std::vector<std::uint8_t>(s.begin(), s.end());
}

std::string String(const std::vector<std::uint8_t>& b) {
  return std::string(b.begin(), b.end());
}

}  // namespace

int main() {
  const auto dir = MakeTempDir("mi_e2ee_engine_test");

  Engine alice;
  Engine bob;
  std::string err;

  assert(alice.Init(dir / "alice", err));
  assert(err.empty());
  assert(bob.Init(dir / "bob", err));
  assert(err.empty());

  alice.SetLocalUsername("alice");
  bob.SetLocalUsername("bob");

  std::vector<std::uint8_t> alice_bundle;
  std::vector<std::uint8_t> bob_bundle;
  assert(alice.BuildPublishBundle(alice_bundle, err));
  assert(err.empty());
  assert(bob.BuildPublishBundle(bob_bundle, err));
  assert(err.empty());

  // Alice -> Bob (first contact) requires TOFU trust.
  std::vector<std::uint8_t> payload1;
  assert(!alice.EncryptToPeer("bob", bob_bundle, Bytes("hello"), payload1, err));
  assert(!err.empty());
  assert(alice.HasPendingPeerTrust());
  const auto pending_bob = alice.pending_peer_trust();
  assert(pending_bob.peer_username == "bob");
  assert(!pending_bob.fingerprint_hex.empty());
  assert(pending_bob.pin6.size() == 24);

  assert(alice.TrustPendingPeer(pending_bob.pin6, err));
  assert(err.empty());
  assert(!alice.HasPendingPeerTrust());

  assert(alice.EncryptToPeer("bob", bob_bundle, Bytes("hello"), payload1, err));
  assert(err.empty());
  assert(!payload1.empty());

  PrivateMessage msg1;
  assert(!bob.DecryptFromPayload("alice", payload1, msg1, err));
  assert(!err.empty());
  assert(bob.HasPendingPeerTrust());
  const auto pending_alice = bob.pending_peer_trust();
  assert(pending_alice.peer_username == "alice");
  assert(pending_alice.pin6.size() == 24);

  assert(bob.TrustPendingPeer(pending_alice.pin6, err));
  assert(err.empty());
  assert(!bob.HasPendingPeerTrust());

  const auto ready = bob.DrainReadyMessages();
  assert(ready.size() == 1);
  assert(ready[0].from_username == "alice");
  assert(String(ready[0].plaintext) == "hello");

  // Bob -> Alice (first reply) should succeed with existing session.
  std::vector<std::uint8_t> payload2;
  assert(bob.EncryptToPeer("alice", {}, Bytes("yo"), payload2, err));
  assert(err.empty());

  PrivateMessage msg2;
  assert(alice.DecryptFromPayload("bob", payload2, msg2, err));
  assert(err.empty());
  assert(msg2.from_username == "bob");
  assert(String(msg2.plaintext) == "yo");

  // Another round trip after ratchet.
  std::vector<std::uint8_t> payload3;
  assert(alice.EncryptToPeer("bob", {}, Bytes("second"), payload3, err));
  assert(err.empty());

  PrivateMessage msg3;
  assert(bob.DecryptFromPayload("alice", payload3, msg3, err));
  assert(err.empty());
  assert(msg3.from_username == "alice");
  assert(String(msg3.plaintext) == "second");

  // Out-of-order receive within the same chain should succeed via skipped keys.
  std::vector<std::uint8_t> payload4;
  std::vector<std::uint8_t> payload5;
  assert(alice.EncryptToPeer("bob", {}, Bytes("m1"), payload4, err));
  assert(err.empty());
  assert(alice.EncryptToPeer("bob", {}, Bytes("m2"), payload5, err));
  assert(err.empty());

  PrivateMessage msg5;
  assert(bob.DecryptFromPayload("alice", payload5, msg5, err));
  assert(err.empty());
  assert(String(msg5.plaintext) == "m2");

  PrivateMessage msg4;
  assert(bob.DecryptFromPayload("alice", payload4, msg4, err));
  assert(err.empty());
  assert(String(msg4.plaintext) == "m1");

  // Replay should fail.
  PrivateMessage replay;
  assert(!bob.DecryptFromPayload("alice", payload5, replay, err));
  assert(!err.empty());

  return 0;
}
