#include "session_manager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <random>
#include <string_view>
#include <vector>
#include <utility>

#include "crypto.h"
#include "monocypher.h"
#include "opaque_pake.h"

extern "C" {
int PQCLEAN_MLKEM768_CLEAN_crypto_kem_enc(std::uint8_t* ct,
                                         std::uint8_t* ss,
                                         const std::uint8_t* pk);
}

namespace mi::server {

SessionManager::SessionManager(std::unique_ptr<AuthProvider> auth,
                               std::chrono::seconds ttl,
                               std::vector<std::uint8_t> opaque_server_setup)
    : auth_(std::move(auth)),
      ttl_(ttl),
      opaque_server_setup_(std::move(opaque_server_setup)) {}

std::string SessionManager::GenerateToken() {
  std::array<std::uint8_t, 32> rnd{};
  if (!crypto::RandomBytes(rnd.data(), rnd.size())) {
    return {};
  }
  static constexpr char kHex[] = "0123456789abcdef";
  std::string token;
  token.resize(rnd.size() * 2);
  for (std::size_t i = 0; i < rnd.size(); ++i) {
    token[i * 2] = kHex[rnd[i] >> 4];
    token[i * 2 + 1] = kHex[rnd[i] & 0x0F];
  }
  return token;
}

bool SessionManager::IsLoginBannedLocked(
    const std::string& username, std::chrono::steady_clock::time_point now) {
  if (username.empty()) {
    return false;
  }
  auto it = login_failures_.find(username);
  if (it == login_failures_.end()) {
    return false;
  }
  it->second.last_seen = now;
  if (it->second.ban_until.time_since_epoch() ==
      std::chrono::steady_clock::duration{}) {
    return false;
  }
  return now < it->second.ban_until;
}

void SessionManager::RecordLoginFailureLocked(
    const std::string& username, std::chrono::steady_clock::time_point now) {
  if (username.empty()) {
    return;
  }
  if ((++login_failure_ops_ & 0xFFu) == 0u) {
    CleanupLoginFailuresLocked(now);
  }

  auto& st = login_failures_[username];
  st.last_seen = now;

  static constexpr auto kWindow = std::chrono::minutes(10);
  static constexpr std::uint32_t kThreshold = 12;
  static constexpr auto kBan = std::chrono::minutes(5);

  if (st.first_failure.time_since_epoch() == std::chrono::steady_clock::duration{} ||
      now - st.first_failure > kWindow) {
    st.first_failure = now;
    st.failures = 1;
    return;
  }

  st.failures++;
  if (st.failures >= kThreshold) {
    st.ban_until = now + kBan;
    st.failures = 0;
    st.first_failure = now;
  }
}

void SessionManager::ClearLoginFailuresLocked(const std::string& username) {
  if (username.empty()) {
    return;
  }
  login_failures_.erase(username);
}

void SessionManager::CleanupLoginFailuresLocked(
    std::chrono::steady_clock::time_point now) {
  if (login_failures_.size() < 1024) {
    return;
  }
  static constexpr auto kTtl = std::chrono::minutes(30);
  for (auto it = login_failures_.begin(); it != login_failures_.end();) {
    if (now - it->second.last_seen > kTtl) {
      it = login_failures_.erase(it);
      continue;
    }
    ++it;
  }
}

bool SessionManager::Login(const std::string& username,
                           const std::string& password,
                           TransportKind transport,
                           Session& out_session, std::string& error) {
  if (!auth_) {
    error = "auth provider missing";
    return false;
  }
  {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    if (IsLoginBannedLocked(username, now)) {
      error = "rate limited";
      return false;
    }
  }
  if (!auth_->Validate(username, password, error)) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    RecordLoginFailureLocked(username, now);
    return false;
  }

  DerivedKeys keys{};
  std::string derive_err;
  if (!DeriveKeysFromCredentials(username, password, transport, keys, derive_err)) {
    error = derive_err;
    return false;
  }

  Session session;
  session.username = username;
  session.token = GenerateToken();
  if (session.token.empty()) {
    error = "token rng failed";
    return false;
  }
  session.keys = keys;
  session.created_at = std::chrono::steady_clock::now();
  session.last_seen = session.created_at;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    ClearLoginFailuresLocked(username);
    sessions_[session.token] = session;
  }
  out_session = session;
  error.clear();
  return true;
}

namespace {
bool IsAllZero(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return true;
  }
  std::uint8_t acc = 0;
  for (std::size_t i = 0; i < len; ++i) {
    acc |= data[i];
  }
  return acc == 0;
}

bool ConstantTimeEqual(const std::uint8_t* a, const std::uint8_t* b,
                       std::size_t len) {
  if (!a || !b || len == 0) {
    return false;
  }
  std::uint8_t acc = 0;
  for (std::size_t i = 0; i < len; ++i) {
    acc |= (a[i] ^ b[i]);
  }
  return acc == 0;
}

int HexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

bool HexToBytes(std::string_view hex, std::vector<std::uint8_t>& out) {
  out.clear();
  if (hex.empty() || (hex.size() % 2) != 0) {
    return false;
  }
  out.reserve(hex.size() / 2);
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    const int hi = HexNibble(hex[i]);
    const int lo = HexNibble(hex[i + 1]);
    if (hi < 0 || lo < 0) {
      out.clear();
      return false;
    }
    out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
  }
  return true;
}

bool LooksLikeSha256Hex(std::string_view s) {
  if (s.size() != 64) {
    return false;
  }
  for (char c : s) {
    if (HexNibble(c) < 0) {
      return false;
    }
  }
  return true;
}

struct PwKeyRecord {
  PakePwScheme scheme{PakePwScheme::kSha256};
  std::uint32_t argon_blocks{0};
  std::uint32_t argon_passes{0};
  std::vector<std::uint8_t> salt;
  std::array<std::uint8_t, 32> key{};
};

bool DerivePwKeyRecord(const std::string& stored, PwKeyRecord& out,
                       std::string& error) {
  out = PwKeyRecord{};
  error.clear();
  if (stored.empty()) {
    error = "stored password empty";
    return false;
  }

  if (stored.rfind("argon2id$", 0) == 0) {
    // argon2id$nb_blocks$nb_passes$salt_hex$hash_hex
    std::vector<std::string_view> parts;
    parts.reserve(5);
    std::string_view sv(stored);
    std::size_t start = 0;
    while (true) {
      const auto pos = sv.find('$', start);
      if (pos == std::string_view::npos) {
        parts.push_back(sv.substr(start));
        break;
      }
      parts.push_back(sv.substr(start, pos - start));
      start = pos + 1;
    }
    if (parts.size() != 5 || parts[0] != "argon2id") {
      error = "argon2id format invalid";
      return false;
    }
    std::uint32_t nb_blocks = 0;
    std::uint32_t nb_passes = 0;
    try {
      nb_blocks = static_cast<std::uint32_t>(std::stoul(std::string(parts[1])));
      nb_passes = static_cast<std::uint32_t>(std::stoul(std::string(parts[2])));
    } catch (...) {
      error = "argon2id params invalid";
      return false;
    }
    if (nb_blocks == 0 || nb_passes == 0 || nb_blocks > 8192 ||
        nb_passes > 16) {
      error = "argon2id params out of range";
      return false;
    }
    std::vector<std::uint8_t> salt;
    std::vector<std::uint8_t> hash;
    if (!HexToBytes(parts[3], salt) || !HexToBytes(parts[4], hash) ||
        salt.empty() || hash.size() != out.key.size()) {
      error = "argon2id salt/hash invalid";
      return false;
    }
    out.scheme = PakePwScheme::kArgon2id;
    out.argon_blocks = nb_blocks;
    out.argon_passes = nb_passes;
    out.salt = std::move(salt);
    std::copy_n(hash.begin(), out.key.size(), out.key.begin());
    return true;
  }

  const auto pos = stored.find(':');
  if (pos != std::string::npos) {
    // salt:hash_hex where hash_hex = SHA256(salt + password) hex
    const std::string salt_str = stored.substr(0, pos);
    const std::string hash_hex = stored.substr(pos + 1);
    if (salt_str.empty() || !LooksLikeSha256Hex(hash_hex)) {
      error = "salted sha256 format invalid";
      return false;
    }
    std::vector<std::uint8_t> hash;
    if (!HexToBytes(hash_hex, hash) || hash.size() != out.key.size()) {
      error = "salted sha256 hash invalid";
      return false;
    }
    out.scheme = PakePwScheme::kSaltedSha256;
    out.salt.assign(salt_str.begin(), salt_str.end());
    std::copy_n(hash.begin(), out.key.size(), out.key.begin());
    return true;
  }

  if (LooksLikeSha256Hex(stored)) {
    std::vector<std::uint8_t> hash;
    if (!HexToBytes(stored, hash) || hash.size() != out.key.size()) {
      error = "sha256 hex invalid";
      return false;
    }
    out.scheme = PakePwScheme::kSha256;
    std::copy_n(hash.begin(), out.key.size(), out.key.begin());
    return true;
  }

  // Plaintext fallback: use SHA256(password) as pw key.
  crypto::Sha256Digest d;
  crypto::Sha256(reinterpret_cast<const std::uint8_t*>(stored.data()),
                 stored.size(), d);
  out.scheme = PakePwScheme::kSha256;
  out.key = d.bytes;
  return true;
}

std::vector<std::uint8_t> BuildPakeTranscript(
    const std::string& username, const std::string& pake_id,
    const PwKeyRecord& pw, const std::array<std::uint8_t, 32>& client_nonce,
    const std::array<std::uint8_t, 32>& server_nonce,
    const std::array<std::uint8_t, 32>& client_dh_pk,
    const std::array<std::uint8_t, 32>& server_dh_pk,
    const std::vector<std::uint8_t>& client_kem_pk,
    const std::vector<std::uint8_t>& kem_ct) {
  std::vector<std::uint8_t> t;
  constexpr char kPrefix[] = "mi_e2ee_pake_login_v1";
  t.insert(t.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  t.push_back(0);
  t.insert(t.end(), username.begin(), username.end());
  t.push_back(0);
  t.insert(t.end(), pake_id.begin(), pake_id.end());
  t.push_back(0);
  t.push_back(static_cast<std::uint8_t>(pw.scheme));
  for (int i = 0; i < 4; ++i) {
    t.push_back(static_cast<std::uint8_t>((pw.argon_blocks >> (i * 8)) & 0xFF));
  }
  for (int i = 0; i < 4; ++i) {
    t.push_back(static_cast<std::uint8_t>((pw.argon_passes >> (i * 8)) & 0xFF));
  }
  t.push_back(static_cast<std::uint8_t>(pw.salt.size() & 0xFF));
  t.push_back(static_cast<std::uint8_t>((pw.salt.size() >> 8) & 0xFF));
  t.insert(t.end(), pw.salt.begin(), pw.salt.end());
  t.insert(t.end(), client_nonce.begin(), client_nonce.end());
  t.insert(t.end(), server_nonce.begin(), server_nonce.end());
  t.insert(t.end(), client_dh_pk.begin(), client_dh_pk.end());
  t.insert(t.end(), server_dh_pk.begin(), server_dh_pk.end());
  t.insert(t.end(), client_kem_pk.begin(), client_kem_pk.end());
  t.insert(t.end(), kem_ct.begin(), kem_ct.end());
  return t;
}
}  // namespace

bool SessionManager::LoginHybrid(
    const std::string& username,
    const std::string& password,
    const std::array<std::uint8_t, 32>& client_dh_pk,
    const std::vector<std::uint8_t>& client_kem_pk,
    TransportKind transport,
    LoginHybridServerHello& out_hello,
    Session& out_session,
    std::string& error) {
  error.clear();
  out_hello = LoginHybridServerHello{};

  if (!auth_) {
    error = "auth provider missing";
    return false;
  }
  {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    if (IsLoginBannedLocked(username, now)) {
      error = "rate limited";
      return false;
    }
  }
  if (!auth_->Validate(username, password, error)) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    RecordLoginFailureLocked(username, now);
    return false;
  }
  if (client_kem_pk.size() != kMlKem768PublicKeyBytes) {
    error = "invalid client kem pk";
    return false;
  }

  std::array<std::uint8_t, 32> server_dh_sk{};
  std::array<std::uint8_t, 32> server_dh_pk{};
  if (!crypto::RandomBytes(server_dh_sk.data(), server_dh_sk.size())) {
    error = "rng failed";
    return false;
  }
  crypto_x25519_public_key(server_dh_pk.data(), server_dh_sk.data());

  std::array<std::uint8_t, 32> dh_shared{};
  crypto_x25519(dh_shared.data(), server_dh_sk.data(), client_dh_pk.data());
  if (IsAllZero(dh_shared.data(), dh_shared.size())) {
    error = "x25519 shared invalid";
    return false;
  }

  out_hello.server_dh_pk = server_dh_pk;
  out_hello.kem_ct.resize(kMlKem768CiphertextBytes);
  std::array<std::uint8_t, 32> kem_shared{};
  if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_enc(out_hello.kem_ct.data(),
                                           kem_shared.data(),
                                           client_kem_pk.data()) != 0) {
    error = "mlkem encaps failed";
    return false;
  }

  const std::string token = GenerateToken();
  if (token.empty()) {
    error = "token rng failed";
    return false;
  }

  DerivedKeys keys{};
  std::string derive_err;
  if (!DeriveKeysFromHybridKeyExchange(dh_shared, kem_shared, username, token,
                                       transport, keys, derive_err)) {
    error = derive_err.empty() ? "key derivation failed" : derive_err;
    return false;
  }

  Session session;
  session.username = username;
  session.token = token;
  session.keys = keys;
  session.created_at = std::chrono::steady_clock::now();
  session.last_seen = session.created_at;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    ClearLoginFailuresLocked(username);
    sessions_[session.token] = session;
  }
  out_session = session;
  return true;
}

namespace {

struct RustBuf {
  std::uint8_t* ptr{nullptr};
  std::size_t len{0};
  ~RustBuf() {
    if (ptr && len) {
      mi_opaque_free(ptr, len);
    }
  }
};

bool SetRustError(const RustBuf& buf, std::string& error,
                  const std::string& fallback) {
  if (buf.ptr && buf.len) {
    error.assign(reinterpret_cast<const char*>(buf.ptr), buf.len);
    return true;
  }
  error = fallback;
  return false;
}

constexpr std::size_t kMaxOpaqueMessageBytes = 16384;

}  // namespace

bool SessionManager::OpaqueRegisterStart(const OpaqueRegisterStartRequest& req,
                                         OpaqueRegisterStartServerHello& out_hello,
                                         std::string& error) {
  error.clear();
  out_hello = OpaqueRegisterStartServerHello{};
  if (opaque_server_setup_.empty()) {
    error = "opaque setup missing";
    return false;
  }
  if (req.username.empty()) {
    error = "username empty";
    return false;
  }
  if (req.registration_request.empty() ||
      req.registration_request.size() > kMaxOpaqueMessageBytes) {
    error = "registration request invalid";
    return false;
  }

  RustBuf resp;
  RustBuf err;
  const int rc = mi_opaque_server_register_response(
      opaque_server_setup_.data(), opaque_server_setup_.size(),
      reinterpret_cast<const std::uint8_t*>(req.username.data()),
      req.username.size(), req.registration_request.data(),
      req.registration_request.size(), &resp.ptr, &resp.len, &err.ptr,
      &err.len);
  if (rc != 0 || !resp.ptr || resp.len == 0) {
    SetRustError(err, error, "registration start failed");
    return false;
  }

  out_hello.registration_response.assign(resp.ptr, resp.ptr + resp.len);
  return true;
}

bool SessionManager::OpaqueRegisterFinish(const OpaqueRegisterFinishRequest& req,
                                          std::string& error) {
  error.clear();
  if (!auth_) {
    error = "auth provider missing";
    return false;
  }
  if (req.username.empty()) {
    error = "username empty";
    return false;
  }
  if (req.registration_upload.empty() ||
      req.registration_upload.size() > kMaxOpaqueMessageBytes) {
    error = "registration upload invalid";
    return false;
  }

  std::string exists_error;
  if (auth_->UserExists(req.username, exists_error)) {
    error = "user already exists";
    return false;
  }
  if (!exists_error.empty() && exists_error != "user not found") {
    error = exists_error;
    return false;
  }

  RustBuf file;
  RustBuf err;
  const int rc = mi_opaque_server_register_finish(
      req.registration_upload.data(), req.registration_upload.size(), &file.ptr,
      &file.len, &err.ptr, &err.len);
  if (rc != 0 || !file.ptr || file.len == 0) {
    SetRustError(err, error, "registration finish failed");
    return false;
  }
  std::vector<std::uint8_t> password_file(file.ptr, file.ptr + file.len);
  if (!auth_->UpsertOpaqueUserRecord(req.username, password_file, error)) {
    return false;
  }
  error.clear();
  return true;
}

bool SessionManager::OpaqueLoginStart(const OpaqueLoginStartRequest& req,
                                      OpaqueLoginStartServerHello& out_hello,
                                      std::string& error) {
  error.clear();
  out_hello = OpaqueLoginStartServerHello{};
  if (!auth_) {
    error = "auth provider missing";
    return false;
  }
  if (opaque_server_setup_.empty()) {
    error = "opaque setup missing";
    return false;
  }
  if (req.username.empty()) {
    error = "username empty";
    return false;
  }
  {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    if (IsLoginBannedLocked(req.username, now)) {
      error = "rate limited";
      return false;
    }
  }
  if (req.credential_request.empty() ||
      req.credential_request.size() > kMaxOpaqueMessageBytes) {
    error = "credential request invalid";
    return false;
  }

  std::vector<std::uint8_t> password_file;
  std::string ignore_err;
  const bool has_password_file =
      auth_->GetOpaqueUserRecord(req.username, password_file, ignore_err) &&
      !password_file.empty();

  RustBuf resp;
  RustBuf state;
  RustBuf err;
  const int rc = mi_opaque_server_login_start(
      opaque_server_setup_.data(), opaque_server_setup_.size(),
      reinterpret_cast<const std::uint8_t*>(req.username.data()),
      req.username.size(), has_password_file ? 1 : 0,
      has_password_file ? password_file.data() : nullptr,
      has_password_file ? password_file.size() : 0, req.credential_request.data(),
      req.credential_request.size(), &resp.ptr, &resp.len, &state.ptr,
      &state.len, &err.ptr, &err.len);
  if (rc != 0 || !resp.ptr || resp.len == 0 || !state.ptr || state.len == 0) {
    SetRustError(err, error, "login start failed");
    return false;
  }

  const std::string login_id = GenerateToken();
  if (login_id.empty()) {
    error = "token rng failed";
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    for (auto it = pending_opaque_.begin(); it != pending_opaque_.end();) {
      if (now - it->second.created_at > pending_opaque_ttl_) {
        it = pending_opaque_.erase(it);
      } else {
        ++it;
      }
    }
    if (pending_opaque_.size() > 4096) {
      error = "too many pending handshakes";
      return false;
    }
    PendingOpaqueLogin p;
    p.username = req.username;
    p.server_state.assign(state.ptr, state.ptr + state.len);
    p.created_at = now;
    pending_opaque_[login_id] = std::move(p);
  }

  out_hello.login_id = login_id;
  out_hello.credential_response.assign(resp.ptr, resp.ptr + resp.len);
  return true;
}

bool SessionManager::OpaqueLoginFinish(const OpaqueLoginFinishRequest& req,
                                       TransportKind transport,
                                       Session& out_session,
                                       std::string& error) {
  error.clear();
  if (req.login_id.empty()) {
    error = "login id empty";
    return false;
  }
  if (req.credential_finalization.empty() ||
      req.credential_finalization.size() > kMaxOpaqueMessageBytes) {
    error = "credential finalization invalid";
    return false;
  }

  PendingOpaqueLogin p;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = pending_opaque_.find(req.login_id);
    if (it == pending_opaque_.end()) {
      error = "login state not found";
      return false;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - it->second.created_at > pending_opaque_ttl_) {
      pending_opaque_.erase(it);
      error = "login expired";
      return false;
    }
    p = it->second;
    pending_opaque_.erase(it);
  }
  {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    if (IsLoginBannedLocked(p.username, now)) {
      error = "rate limited";
      return false;
    }
  }

  RustBuf session_key;
  RustBuf err;
  const int rc = mi_opaque_server_login_finish(
      reinterpret_cast<const std::uint8_t*>(p.username.data()), p.username.size(),
      p.server_state.data(), p.server_state.size(),
      req.credential_finalization.data(), req.credential_finalization.size(),
      &session_key.ptr, &session_key.len, &err.ptr, &err.len);
  if (rc != 0 || !session_key.ptr || session_key.len == 0) {
    // Do not leak server-side failure details for login.
    error = "invalid credentials";
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    RecordLoginFailureLocked(p.username, now);
    return false;
  }

  const std::string token = GenerateToken();
  if (token.empty()) {
    error = "token rng failed";
    return false;
  }

  DerivedKeys keys{};
  std::string derive_err;
  std::vector<std::uint8_t> sk(session_key.ptr, session_key.ptr + session_key.len);
  if (!DeriveKeysFromOpaqueSessionKey(sk, p.username, token, transport, keys,
                                      derive_err)) {
    error = derive_err.empty() ? "key derivation failed" : derive_err;
    return false;
  }

  Session session;
  session.username = p.username;
  session.token = token;
  session.keys = keys;
  session.created_at = std::chrono::steady_clock::now();
  session.last_seen = session.created_at;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    ClearLoginFailuresLocked(p.username);
    sessions_[session.token] = session;
  }
  out_session = session;
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
  if (ttl_.count() > 0 &&
      now - it->second.last_seen > ttl_) {
    sessions_.erase(it);
    return std::nullopt;
  }
  it->second.last_seen = now;
  return it->second;
}

bool SessionManager::TouchSession(const std::string& token) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = sessions_.find(token);
  if (it == sessions_.end()) {
    return false;
  }
  const auto now = std::chrono::steady_clock::now();
  if (ttl_.count() > 0 &&
      now - it->second.last_seen > ttl_) {
    sessions_.erase(it);
    return false;
  }
  it->second.last_seen = now;
  return true;
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

SessionManagerStats SessionManager::GetStats() {
  std::lock_guard<std::mutex> lock(mutex_);
  SessionManagerStats stats;
  stats.sessions = static_cast<std::uint64_t>(sessions_.size());
  stats.pending_opaque = static_cast<std::uint64_t>(pending_opaque_.size());
  stats.login_failure_entries =
      static_cast<std::uint64_t>(login_failures_.size());
  return stats;
}

void SessionManager::Cleanup() {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto now = std::chrono::steady_clock::now();
  for (auto it = sessions_.begin(); it != sessions_.end();) {
    if (ttl_.count() > 0 &&
        now - it->second.last_seen > ttl_) {
      it = sessions_.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = pending_opaque_.begin(); it != pending_opaque_.end();) {
    if (now - it->second.created_at > pending_opaque_ttl_) {
      it = pending_opaque_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace mi::server
