#ifndef MI_E2EE_SERVER_SESSION_MANAGER_H
#define MI_E2EE_SERVER_SESSION_MANAGER_H

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "auth_provider.h"
#include "pake.h"

namespace mi::server {

struct Session {
  std::string token;
  std::string username;
  DerivedKeys keys;
  std::chrono::steady_clock::time_point created_at;
  std::chrono::steady_clock::time_point last_seen;
};

struct LoginHybridServerHello {
  std::array<std::uint8_t, 32> server_dh_pk{};
  std::vector<std::uint8_t> kem_ct;
};

struct OpaqueRegisterStartRequest {
  std::string username;
  std::vector<std::uint8_t> registration_request;
};

struct OpaqueRegisterStartServerHello {
  std::vector<std::uint8_t> registration_response;
};

struct OpaqueRegisterFinishRequest {
  std::string username;
  std::vector<std::uint8_t> registration_upload;
};

struct OpaqueLoginStartRequest {
  std::string username;
  std::vector<std::uint8_t> credential_request;
};

struct OpaqueLoginStartServerHello {
  std::string login_id;
  std::vector<std::uint8_t> credential_response;
};

struct OpaqueLoginFinishRequest {
  std::string login_id;
  std::vector<std::uint8_t> credential_finalization;
};

struct SessionManagerStats {
  std::uint64_t sessions{0};
  std::uint64_t pending_opaque{0};
  std::uint64_t login_failure_entries{0};
};

class SessionManager {
 public:
  explicit SessionManager(std::unique_ptr<AuthProvider> auth,
                          std::chrono::seconds ttl = std::chrono::minutes(30),
                          std::vector<std::uint8_t> opaque_server_setup = {});

  bool Login(const std::string& username, const std::string& password,
             TransportKind transport, Session& out_session,
             std::string& error);

  bool LoginHybrid(const std::string& username, const std::string& password,
                   const std::array<std::uint8_t, 32>& client_dh_pk,
                   const std::vector<std::uint8_t>& client_kem_pk,
                   TransportKind transport,
                   LoginHybridServerHello& out_hello,
                   Session& out_session,
                   std::string& error);

  bool OpaqueRegisterStart(const OpaqueRegisterStartRequest& req,
                           OpaqueRegisterStartServerHello& out_hello,
                           std::string& error);
  bool OpaqueRegisterFinish(const OpaqueRegisterFinishRequest& req,
                            std::string& error);
  bool OpaqueLoginStart(const OpaqueLoginStartRequest& req,
                        OpaqueLoginStartServerHello& out_hello,
                        std::string& error);
  bool OpaqueLoginFinish(const OpaqueLoginFinishRequest& req,
                         TransportKind transport, Session& out_session,
                         std::string& error);

  bool UserExists(const std::string& username, std::string& error) const;

  std::optional<Session> GetSession(const std::string& token);

  bool TouchSession(const std::string& token);

  std::optional<DerivedKeys> GetKeys(const std::string& token);

  void Logout(const std::string& token);

  SessionManagerStats GetStats();

  void Cleanup();

 private:
  std::string GenerateToken();

  struct PendingOpaqueLogin {
    std::string username;
    std::vector<std::uint8_t> server_state;
    std::chrono::steady_clock::time_point created_at{};
  };

  struct LoginFailureState {
    std::uint32_t failures{0};
    std::chrono::steady_clock::time_point first_failure{};
    std::chrono::steady_clock::time_point last_seen{};
    std::chrono::steady_clock::time_point ban_until{};
  };

  bool IsLoginBannedLocked(const std::string& username,
                           std::chrono::steady_clock::time_point now);
  void RecordLoginFailureLocked(const std::string& username,
                                std::chrono::steady_clock::time_point now);
  void ClearLoginFailuresLocked(const std::string& username);
  void CleanupLoginFailuresLocked(std::chrono::steady_clock::time_point now);

  std::unique_ptr<AuthProvider> auth_;
  std::chrono::seconds ttl_;
  std::vector<std::uint8_t> opaque_server_setup_;
  std::mutex mutex_;
  std::unordered_map<std::string, Session> sessions_;
  std::unordered_map<std::string, PendingOpaqueLogin> pending_opaque_;
  std::chrono::seconds pending_opaque_ttl_{std::chrono::seconds(90)};
  std::unordered_map<std::string, LoginFailureState> login_failures_;
  std::uint64_t login_failure_ops_{0};
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_SESSION_MANAGER_H
