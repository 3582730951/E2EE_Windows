#ifndef MI_E2EE_SERVER_SESSION_MANAGER_H
#define MI_E2EE_SERVER_SESSION_MANAGER_H

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "auth_provider.h"
#include "pake.h"

namespace mi::server {

struct Session {
  std::string token;
  std::string username;
  DerivedKeys keys;
  std::chrono::steady_clock::time_point created_at;
};

class SessionManager {
 public:
  explicit SessionManager(std::unique_ptr<AuthProvider> auth,
                          std::chrono::seconds ttl = std::chrono::minutes(30));

  bool Login(const std::string& username, const std::string& password,
             Session& out_session, std::string& error);

  bool UserExists(const std::string& username, std::string& error) const;

  std::optional<Session> GetSession(const std::string& token);

  std::optional<DerivedKeys> GetKeys(const std::string& token);

  void Logout(const std::string& token);

  void Cleanup();

 private:
  std::string GenerateToken();

  std::unique_ptr<AuthProvider> auth_;
  std::chrono::seconds ttl_;
  std::mutex mutex_;
  std::unordered_map<std::string, Session> sessions_;
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_SESSION_MANAGER_H
