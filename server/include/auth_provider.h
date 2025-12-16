#ifndef MI_E2EE_SERVER_AUTH_PROVIDER_H
#define MI_E2EE_SERVER_AUTH_PROVIDER_H

#include <memory>
#include <string>
#include <vector>

#include "config.h"

namespace mi::server {

class AuthProvider {
 public:
  virtual ~AuthProvider() = default;
  virtual bool Validate(const std::string& username,
                        const std::string& password,
                        std::string& error) = 0;
  virtual bool GetStoredPassword(const std::string& username,
                                 std::string& out_password,
                                 std::string& error) = 0;
  virtual bool GetOpaqueUserRecord(const std::string& username,
                                   std::vector<std::uint8_t>& out_record,
                                   std::string& error) = 0;
  virtual bool UpsertOpaqueUserRecord(const std::string& username,
                                      const std::vector<std::uint8_t>& record,
                                      std::string& error) = 0;
  virtual bool UserExists(const std::string& username, std::string& error) = 0;
};

class DemoAuthProvider final : public AuthProvider {
 public:
  explicit DemoAuthProvider(DemoUserTable users);
  bool Validate(const std::string& username, const std::string& password,
                std::string& error) override;
  bool GetStoredPassword(const std::string& username, std::string& out_password,
                         std::string& error) override;
  bool GetOpaqueUserRecord(const std::string& username,
                           std::vector<std::uint8_t>& out_record,
                           std::string& error) override;
  bool UpsertOpaqueUserRecord(const std::string& username,
                              const std::vector<std::uint8_t>& record,
                              std::string& error) override;
  bool UserExists(const std::string& username, std::string& error) override;

 private:
  DemoUserTable users_;
};

class MySqlAuthProvider final : public AuthProvider {
 public:
  explicit MySqlAuthProvider(MySqlConfig cfg);
  bool Validate(const std::string& username, const std::string& password,
                std::string& error) override;
  bool GetStoredPassword(const std::string& username, std::string& out_password,
                         std::string& error) override;
  bool GetOpaqueUserRecord(const std::string& username,
                           std::vector<std::uint8_t>& out_record,
                           std::string& error) override;
  bool UpsertOpaqueUserRecord(const std::string& username,
                              const std::vector<std::uint8_t>& record,
                              std::string& error) override;
  bool UserExists(const std::string& username, std::string& error) override;

 private:
  MySqlConfig cfg_;
};

std::unique_ptr<AuthProvider> MakeAuthProvider(
    const ServerConfig& cfg,
    const std::vector<std::uint8_t>& opaque_server_setup,
    std::string& error);

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_AUTH_PROVIDER_H
