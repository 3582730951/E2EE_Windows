#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#include "auth_provider.h"
#include "session_manager.h"

using mi::server::DemoAuthProvider;
using mi::server::DemoUser;
using mi::server::DemoUserTable;
using mi::server::Session;
using mi::server::SessionManager;

int main() {
  const auto persist_dir =
      std::filesystem::current_path() / "test_state_sessions";
  std::error_code ec;
  std::filesystem::remove_all(persist_dir, ec);
  std::filesystem::create_directories(persist_dir, ec);
  if (ec) {
    return 1;
  }

  std::string persisted_token;
  {
    DemoUserTable table;
    DemoUser user;
    user.username.set("bob");
    user.password.set("pwd123");
    user.username_plain = "bob";
    user.password_plain = "pwd123";
    table.emplace("bob", user);

    auto auth = std::make_unique<DemoAuthProvider>(std::move(table));
    SessionManager persist_mgr(std::move(auth), std::chrono::seconds(600), {},
                               persist_dir);

    Session session;
    std::string err;
    bool ok = persist_mgr.Login("bob", "pwd123",
                                mi::server::TransportKind::kLocal,
                                session, err);
    if (!ok || session.token.empty()) {
      return 1;
    }
    persisted_token = session.token;
  }
  if (persisted_token.empty()) {
    return 1;
  }

  {
    DemoUserTable table;
    DemoUser user;
    user.username.set("bob");
    user.password.set("pwd123");
    user.username_plain = "bob";
    user.password_plain = "pwd123";
    table.emplace("bob", user);

    auto auth = std::make_unique<DemoAuthProvider>(std::move(table));
    SessionManager persist_mgr(std::move(auth), std::chrono::seconds(600), {},
                               persist_dir);
    auto fetched = persist_mgr.GetSession(persisted_token);
    if (!fetched.has_value() || fetched->username != "bob") {
      return 1;
    }
    auto keys = persist_mgr.GetKeys(persisted_token);
    if (!keys.has_value() || keys->root_key.size() != 32) {
      return 1;
    }
    persist_mgr.Logout(persisted_token);
  }

  {
    DemoUserTable table;
    DemoUser user;
    user.username.set("bob");
    user.password.set("pwd123");
    user.username_plain = "bob";
    user.password_plain = "pwd123";
    table.emplace("bob", user);

    auto auth = std::make_unique<DemoAuthProvider>(std::move(table));
    SessionManager persist_mgr(std::move(auth), std::chrono::seconds(600), {},
                               persist_dir);
    auto fetched = persist_mgr.GetSession(persisted_token);
    if (fetched.has_value()) {
      return 1;
    }
  }

  DemoUserTable table;
  DemoUser user;
  user.username.set("bob");
  user.password.set("pwd123");
  user.username_plain = "bob";
  user.password_plain = "pwd123";
  table.emplace("bob", user);

  auto auth = std::make_unique<DemoAuthProvider>(std::move(table));
  SessionManager mgr(std::move(auth));

  Session session;
  std::string err;
  bool ok = mgr.Login("bob", "pwd123", mi::server::TransportKind::kLocal,
                      session, err);
  if (!ok || session.token.empty()) {
    return 1;
  }

  auto fetched = mgr.GetSession(session.token);
  if (!fetched.has_value() || fetched->username != "bob") {
    return 1;
  }
  auto keys = mgr.GetKeys(session.token);
  if (!keys.has_value() || keys->root_key.size() != 32) {
    return 1;
  }

  mgr.Logout(session.token);
  auto missing = mgr.GetSession(session.token);
  if (missing.has_value()) {
    return 1;
  }

  ok = mgr.Login("bob", "wrong", mi::server::TransportKind::kLocal, session, err);
  if (ok) {
    return 1;
  }

  // Account lockout after repeated failures.
  // Threshold is internal (currently 12 failures -> 5 min ban), so only assert
  // the externally visible behavior.
  Session tmp;
  std::string lock_err;
  for (int i = 0; i < 11; ++i) {
    const bool ok_fail =
        mgr.Login("bob", "wrong", mi::server::TransportKind::kLocal, tmp,
                  lock_err);
    if (ok_fail) {
      return 1;
    }
    if (lock_err == "rate limited") {
      return 1;
    }
  }
  const bool ok_banned =
      mgr.Login("bob", "wrong", mi::server::TransportKind::kLocal, tmp,
                lock_err);
  if (ok_banned || lock_err != "rate limited") {
    return 1;
  }
  const bool ok_banned_good =
      mgr.Login("bob", "pwd123", mi::server::TransportKind::kLocal, tmp,
                lock_err);
  if (ok_banned_good || lock_err != "rate limited") {
    return 1;
  }

  // TTL expire path: short TTL, then force cleanup
  DemoUserTable t2;
  DemoUser u2;
  u2.username.set("c");
  u2.password.set("d");
  u2.username_plain = "c";
  u2.password_plain = "d";
  t2.emplace("c", u2);
  SessionManager short_mgr(std::make_unique<DemoAuthProvider>(std::move(t2)),
                           std::chrono::seconds(1));
  Session s2;
  std::string err2;
  bool ok2 = short_mgr.Login("c", "d", mi::server::TransportKind::kLocal, s2,
                             err2);
  if (!ok2) {
    return 1;
  }
  auto fetched2 = short_mgr.GetSession(s2.token);
  // May already be expired due to zero TTL
  if (fetched2.has_value()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    short_mgr.Cleanup();
    fetched2 = short_mgr.GetSession(s2.token);
    if (fetched2.has_value()) {
      return 1;
    }
  }

  return 0;
}
