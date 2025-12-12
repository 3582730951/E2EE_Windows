#include "session_manager.h"

#include <random>
#include <utility>

namespace mi::server {

SessionManager::SessionManager(std::unique_ptr<AuthProvider> auth,
                               std::chrono::seconds ttl)
    : auth_(std::move(auth)), ttl_(ttl) {}

std::string SessionManager::GenerateToken() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(0, 255);
  std::string token;
  token.resize(64);
  const char* hex = "0123456789abcdef";
  for (std::size_t i = 0; i < 32; ++i) {
    const std::uint8_t v = static_cast<std::uint8_t>(dist(gen));
    token[i * 2] = hex[v >> 4];
    token[i * 2 + 1] = hex[v & 0x0F];
  }
  return token;
}

bool SessionManager::Login(const std::string& username,
                           const std::string& password,
                           Session& out_session, std::string& error) {
  if (!auth_) {
    error = "auth provider missing";
    return false;
  }
  if (!auth_->Validate(username, password, error)) {
    return false;
  }

  DerivedKeys keys{};
  std::string derive_err;
  if (!DeriveKeysFromCredentials(username, password, keys, derive_err)) {
    error = derive_err;
    return false;
  }

  Session session;
  session.username = username;
  session.token = GenerateToken();
  session.keys = keys;
  session.created_at = std::chrono::steady_clock::now();

  {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[session.token] = session;
  }
  out_session = session;
  error.clear();
  return true;
}

bool SessionManager::UserExists(const std::string& username,
                                std::string& error) const {
  if (!auth_) {
    error = "auth provider missing";
    return false;
  }
  return auth_->UserExists(username, error);
}

std::optional<Session> SessionManager::GetSession(const std::string& token) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = sessions_.find(token);
  if (it == sessions_.end()) {
    return std::nullopt;
  }
  const auto now = std::chrono::steady_clock::now();
  if (now - it->second.created_at > ttl_) {
    sessions_.erase(it);
    return std::nullopt;
  }
  return it->second;
}

std::optional<DerivedKeys> SessionManager::GetKeys(const std::string& token) {
  auto s = GetSession(token);
  if (!s.has_value()) {
    return std::nullopt;
  }
  return s->keys;
}

void SessionManager::Logout(const std::string& token) {
  std::lock_guard<std::mutex> lock(mutex_);
  sessions_.erase(token);
}

void SessionManager::Cleanup() {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto now = std::chrono::steady_clock::now();
  for (auto it = sessions_.begin(); it != sessions_.end();) {
    if (now - it->second.created_at > ttl_) {
      it = sessions_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace mi::server
