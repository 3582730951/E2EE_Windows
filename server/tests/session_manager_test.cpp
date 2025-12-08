#include <chrono>
#include <string>

#include "auth_provider.h"
#include "session_manager.h"

using mi::server::DemoAuthProvider;
using mi::server::DemoUser;
using mi::server::DemoUserTable;
using mi::server::Session;
using mi::server::SessionManager;

int main() {
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
  bool ok = mgr.Login("bob", "pwd123", session, err);
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

  ok = mgr.Login("bob", "wrong", session, err);
  if (ok) {
    return 1;
  }

  // TTL expire path: very short TTL, then force cleanup
  DemoUserTable t2;
  DemoUser u2;
  u2.username.set("c");
  u2.password.set("d");
  u2.username_plain = "c";
  u2.password_plain = "d";
  t2.emplace("c", u2);
  SessionManager short_mgr(std::make_unique<DemoAuthProvider>(std::move(t2)),
                           std::chrono::seconds(0));
  Session s2;
  std::string err2;
  bool ok2 = short_mgr.Login("c", "d", s2, err2);
  if (!ok2) {
    return 1;
  }
  auto fetched2 = short_mgr.GetSession(s2.token);
  // May already be expired due to zero TTL
  if (fetched2.has_value()) {
    short_mgr.Cleanup();
    fetched2 = short_mgr.GetSession(s2.token);
    if (fetched2.has_value()) {
      return 1;
    }
  }

  return 0;
}
