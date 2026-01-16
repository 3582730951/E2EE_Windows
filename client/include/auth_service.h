#ifndef MI_E2EE_CLIENT_AUTH_SERVICE_H
#define MI_E2EE_CLIENT_AUTH_SERVICE_H

#include <string>

namespace mi::client {

class ClientCore;

class AuthService {
 public:
  bool Register(ClientCore& core, const std::string& username,
                const std::string& password) const;
  bool Login(ClientCore& core, const std::string& username,
             const std::string& password) const;
  bool Relogin(ClientCore& core) const;
  bool Logout(ClientCore& core) const;
  bool LoadKtState(ClientCore& core) const;
  bool SaveKtState(ClientCore& core) const;
  bool LoadOrCreateDeviceId(ClientCore& core) const;
};

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_AUTH_SERVICE_H
