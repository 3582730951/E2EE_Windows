#include "api_service.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <thread>
#include <type_traits>
#include <utility>

#include "protocol.h"
#include "media_frame.h"
#include "platform_secure_store.h"
#include "platform_time.h"

extern "C" {
int PQCLEAN_MLDSA65_CLEAN_crypto_sign_signature(std::uint8_t* sig,
                                               std::size_t* siglen,
                                               const std::uint8_t* m,
                                               std::size_t mlen,
                                               const std::uint8_t* sk);
}

#ifdef MI_E2EE_ENABLE_MYSQL
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#endif
#include <mysql.h>
#ifdef _WIN32
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif
#endif

namespace {

#ifdef MI_E2EE_ENABLE_MYSQL
MYSQL* ConnectMysql(const mi::server::MySqlConfig& cfg, std::string& error) {
  error.clear();
  constexpr int kMaxAttempts = 2;
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
      error = "mysql_init failed";
      return nullptr;
    }
    unsigned int timeout = 5;
    mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(conn, MYSQL_OPT_READ_TIMEOUT, &timeout);
    mysql_options(conn, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
#ifdef MYSQL_OPT_RECONNECT
    bool reconnect = true;
    mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);
#endif
    MYSQL* res = mysql_real_connect(conn, cfg.host.c_str(),
                                    cfg.username.c_str(),
                                    cfg.password.get().c_str(),
                                    cfg.database.c_str(), cfg.port, nullptr, 0);
    if (res) {
      return conn;
    }
    error = "mysql_connect failed";
    mysql_close(conn);
    if (attempt + 1 < kMaxAttempts) {
      mi::platform::SleepMs(200);
    }
  }
  return nullptr;
}
#endif

constexpr std::uint8_t kDpapiMagic[8] = {'M', 'I', 'D', 'P',
                                         'A', 'P', 'I', '1'};
constexpr std::size_t kDpapiHeaderBytes = 12;

bool IsDpapiBlob(const std::vector<std::uint8_t>& data) {
  return data.size() >= kDpapiHeaderBytes &&
         std::equal(std::begin(kDpapiMagic), std::end(kDpapiMagic),
                    data.begin());
}

bool DecodeProtectedFileBytes(const std::vector<std::uint8_t>& file_bytes,
                              std::vector<std::uint8_t>& out,
                              std::string& error) {
  error.clear();
  if (!IsDpapiBlob(file_bytes)) {
    out = file_bytes;
    return true;
  }
  if (file_bytes.size() < kDpapiHeaderBytes) {
    error = "secure store blob invalid";
    return false;
  }
  const std::uint32_t len =
      static_cast<std::uint32_t>(file_bytes[8]) |
      (static_cast<std::uint32_t>(file_bytes[9]) << 8) |
      (static_cast<std::uint32_t>(file_bytes[10]) << 16) |
      (static_cast<std::uint32_t>(file_bytes[11]) << 24);
  if (len == 0 || file_bytes.size() != kDpapiHeaderBytes + len) {
    error = "secure store blob size invalid";
    return false;
  }
  const std::vector<std::uint8_t> blob(file_bytes.begin() + kDpapiHeaderBytes,
                                       file_bytes.end());
  return mi::platform::UnprotectSecureBlobScoped(
      blob, nullptr, 0, mi::platform::SecureStoreScope::kUser, out, error);
}

constexpr std::uint8_t kGroupNoticeJoin = 1;
constexpr std::uint8_t kGroupNoticeLeave = 2;
constexpr std::uint8_t kGroupNoticeKick = 3;
constexpr std::uint8_t kGroupNoticeRoleSet = 4;

std::vector<std::uint8_t> BuildGroupNoticePayload(
    std::uint8_t kind, const std::string& target_username,
    std::optional<mi::server::GroupRole> role = std::nullopt) {
  std::vector<std::uint8_t> out;
  out.reserve(1 + 2 + target_username.size() + (role.has_value() ? 1u : 0u));
  out.push_back(kind);
  mi::server::proto::WriteString(target_username, out);
  if (kind == kGroupNoticeRoleSet && role.has_value()) {
    out.push_back(static_cast<std::uint8_t>(role.value()));
  }
  return out;
}

bool DecodeGroupCallSubscriptions(const std::vector<std::uint8_t>& ext,
                                  std::vector<mi::server::GroupCallSubscription>& out,
                                  std::string& error) {
  out.clear();
  error.clear();
  if (ext.empty()) {
    return true;
  }
  std::size_t off = 0;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(ext, off, count)) {
    error = "subscription payload invalid";
    return false;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::string sender;
    if (!mi::server::proto::ReadString(ext, off, sender)) {
      error = "subscription payload invalid";
      return false;
    }
    if (off >= ext.size()) {
      error = "subscription payload invalid";
      return false;
    }
    const std::uint8_t flags = ext[off++];
    mi::server::GroupCallSubscription sub;
    sub.sender = std::move(sender);
    sub.media_flags = flags;
    out.push_back(std::move(sub));
  }
  if (off != ext.size()) {
    error = "subscription payload invalid";
    return false;
  }
  return true;
}

bool PeekMediaPacketKindFlag(const std::vector<std::uint8_t>& payload,
                             std::uint8_t& out_flag) {
  out_flag = 0;
  if (payload.size() < 2) {
    return false;
  }
  const std::uint8_t version = payload[0];
  const std::uint8_t kind = payload[1];
  const std::size_t min_size_v2 = 1 + 1 + 4 + 16;
  const std::size_t min_size_v3 = 1 + 1 + 4 + 4 + 16;
  if (version == 2) {
    if (payload.size() < min_size_v2) {
      return false;
    }
  } else if (version == 3) {
    if (payload.size() < min_size_v3) {
      return false;
    }
  } else {
    return false;
  }

  if (kind == static_cast<std::uint8_t>(mi::media::StreamKind::kAudio)) {
    out_flag = mi::server::kGroupCallMediaAudio;
    return true;
  }
  if (kind == static_cast<std::uint8_t>(mi::media::StreamKind::kVideo)) {
    out_flag = mi::server::kGroupCallMediaVideo;
    return true;
  }
  return false;
}

}  // namespace

namespace mi::server {

namespace {
#ifdef MI_E2EE_ENABLE_MYSQL
bool AreFriendsMysql(const MySqlConfig& cfg, const std::string& username,
                     const std::string& friend_username, std::string& error);

bool IsBlockedMysql(const MySqlConfig& cfg, const std::string& username,
                    const std::string& blocked_username, std::string& error);
#endif

bool ReadFileBytes(const std::filesystem::path& path,
                   std::vector<std::uint8_t>& out,
                   std::string& error) {
  error.clear();
  out.clear();
  if (path.empty()) {
    error = "kt signing key path empty";
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    error = ec ? "kt signing key path error" : "kt signing key not found";
    return false;
  }
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    error = "kt signing key not found";
    return false;
  }
  std::error_code size_ec;
  const std::uint64_t size = std::filesystem::file_size(path, size_ec);
  if (size_ec ||
      size > static_cast<std::uint64_t>(
                 (std::numeric_limits<std::size_t>::max)())) {
    error = "kt signing key read failed";
    return false;
  }
  std::vector<std::uint8_t> file_bytes;
  file_bytes.resize(static_cast<std::size_t>(size));
  if (!file_bytes.empty()) {
    ifs.read(reinterpret_cast<char*>(file_bytes.data()),
             static_cast<std::streamsize>(file_bytes.size()));
    if (!ifs || ifs.gcount() != static_cast<std::streamsize>(file_bytes.size())) {
      error = "kt signing key read failed";
      return false;
    }
  }
  if (!DecodeProtectedFileBytes(file_bytes, out, error)) {
    return false;
  }
  if (out.size() != kKtSthSigSecretKeyBytes) {
    error = "kt signing key size invalid";
    out.clear();
    return false;
  }
  return true;
}
}  // namespace

ApiService::ApiService(SessionManager* sessions, GroupManager* groups,
                       GroupCallManager* calls,
                       GroupDirectory* directory, OfflineStorage* storage,
                       OfflineQueue* queue, MediaRelay* media_relay,
                       std::uint32_t group_threshold,
                       std::optional<MySqlConfig> friend_mysql,
                       std::filesystem::path kt_dir,
                       std::filesystem::path kt_signing_key)
    : sessions_(sessions),
      groups_(groups),
      calls_(calls),
      directory_(directory),
      storage_(storage),
      queue_(queue),
      media_relay_(media_relay),
      group_threshold_(group_threshold == 0 ? 10000 : group_threshold),
      friend_mysql_(std::move(friend_mysql)),
      rl_global_unauth_(30.0, 10.0),
      rl_user_unauth_(8.0, 0.25),
      rl_user_api_(200.0, 50.0),
      rl_user_file_(3.0, 0.05) {
  if (!kt_dir.empty()) {
    const std::filesystem::path path = kt_dir / "kt_log.bin";
    kt_log_ = std::make_unique<KeyTransparencyLog>(path);
    std::string err;
    if (!kt_log_->Load(err)) {
      // Best effort recovery: start a new log if the on-disk log is missing/corrupt.
      std::error_code ec;
      std::filesystem::remove(path, ec);
      kt_log_ = std::make_unique<KeyTransparencyLog>(path);
      kt_log_->Load(err);
    }
  }
  if (kt_log_) {
    if (!kt_signing_key.empty()) {
      std::vector<std::uint8_t> bytes;
      std::string err;
      if (!ReadFileBytes(kt_signing_key, bytes, err)) {
        kt_signing_error_ = err.empty() ? "kt signing key load failed" : err;
      } else if (bytes.size() != kKtSthSigSecretKeyBytes) {
        kt_signing_error_ = "kt signing key size invalid";
      } else {
        std::copy_n(bytes.begin(), kt_signing_sk_.size(),
                    kt_signing_sk_.begin());
        kt_signing_ready_ = true;
      }
    } else {
      kt_signing_error_ = "kt signing key missing";
    }
  }
}

ApiService::RateLimiter::RateLimiter(double capacity, double refill_per_sec,
                                     std::chrono::seconds ttl)
    : capacity_(capacity),
      refill_per_sec_(refill_per_sec),
      ttl_(ttl <= std::chrono::seconds(0) ? std::chrono::minutes(10) : ttl) {
  if (capacity_ < 0.0) {
    capacity_ = 0.0;
  }
  if (refill_per_sec_ < 0.0) {
    refill_per_sec_ = 0.0;
  }
}

bool ApiService::RateLimiter::Allow(const std::string& key) {
  return AllowAt(key, std::chrono::steady_clock::now());
}

bool ApiService::RateLimiter::AllowAt(const std::string& key,
                                      std::chrono::steady_clock::time_point now) {
  if (capacity_ <= 0.0) {
    return true;
  }

  const std::size_t shard_idx =
      std::hash<std::string>{}(key) % shards_.size();
  auto& shard = shards_[shard_idx];
  std::lock_guard<std::mutex> lock(shard.mutex);

  if ((++shard.ops & 0xFFu) == 0u) {
    CleanupShardLocked(shard, now);
  }

  auto [it, inserted] = shard.buckets.try_emplace(key);
  auto& bucket = it->second;
  if (inserted ||
      bucket.last.time_since_epoch() == std::chrono::steady_clock::duration{}) {
    bucket.tokens = capacity_;
    bucket.last = now;
    bucket.last_seen = now;
    shard.expiries.push(ExpiryItem{now + ttl_, it->first});
  }

  const double dt =
      std::chrono::duration_cast<std::chrono::duration<double>>(now - bucket.last)
          .count();
  if (dt > 0.0) {
    bucket.tokens = std::min(capacity_, bucket.tokens + dt * refill_per_sec_);
    bucket.last = now;
  }
  bucket.last_seen = now;

  if (bucket.tokens < 1.0) {
    return false;
  }
  bucket.tokens -= 1.0;
  return true;
}

void ApiService::RateLimiter::CleanupShardLocked(
    Shard& shard, std::chrono::steady_clock::time_point now) {
  while (!shard.expiries.empty()) {
    const auto& top = shard.expiries.top();
    if (top.expires_at > now) {
      break;
    }
    const std::string key = top.key;
    shard.expiries.pop();

    const auto it = shard.buckets.find(key);
    if (it == shard.buckets.end()) {
      continue;
    }
    if (now - it->second.last_seen > ttl_) {
      shard.buckets.erase(it);
      continue;
    }
    shard.expiries.push(ExpiryItem{it->second.last_seen + ttl_, key});
  }
}

bool ApiService::RateLimitUnauth(const std::string& action,
                                const std::string& username,
                                std::string& out_error) {
  out_error.clear();
  if (!rl_global_unauth_.Allow(action)) {
    out_error = "rate limited";
    return false;
  }
  if (!username.empty()) {
    const std::string key = action + "|" + username;
    if (!rl_user_unauth_.Allow(key)) {
      out_error = "rate limited";
      return false;
    }
  }
  return true;
}

bool ApiService::RateLimitAuth(const std::string& action, const std::string& token,
                              std::optional<Session>& out_session,
                              std::string& out_error) {
  out_session.reset();
  out_error.clear();
  if (!sessions_) {
    out_error = "session manager unavailable";
    return false;
  }
  out_session = sessions_->GetSession(token);
  if (!out_session.has_value()) {
    out_error = "unauthorized";
    return false;
  }
  const std::string key = action + "|" + out_session->username;
  if (!rl_user_api_.Allow(key)) {
    out_error = "rate limited";
    return false;
  }
  return true;
}

bool ApiService::RateLimitFile(const std::string& action, const std::string& token,
                              std::optional<Session>& out_session,
                              std::string& out_error) {
  out_session.reset();
  out_error.clear();
  if (!sessions_) {
    out_error = "session manager unavailable";
    return false;
  }
  out_session = sessions_->GetSession(token);
  if (!out_session.has_value()) {
    out_error = "unauthorized";
    return false;
  }
  const std::string key = action + "|" + out_session->username;
  if (!rl_user_file_.Allow(key)) {
    out_error = "rate limited";
    return false;
  }
  return true;
}

bool ApiService::SignKtSth(KeyTransparencySth& sth, std::string& out_error) {
  out_error.clear();
  sth.signature.clear();
  if (!kt_signing_ready_) {
    out_error = kt_signing_error_.empty() ? "kt signing unavailable"
                                          : kt_signing_error_;
    return false;
  }
  const auto msg = BuildKtSthSignatureMessage(sth);
  sth.signature.resize(kKtSthSigBytes);
  std::size_t sig_len = 0;
  if (PQCLEAN_MLDSA65_CLEAN_crypto_sign_signature(
          sth.signature.data(), &sig_len, msg.data(), msg.size(),
          kt_signing_sk_.data()) != 0) {
    out_error = "kt sign failed";
    sth.signature.clear();
    return false;
  }
  if (sig_len != sth.signature.size()) {
    out_error = "kt signature size invalid";
    sth.signature.clear();
    return false;
  }
  return true;
}

LoginResponse ApiService::Login(const LoginRequest& req, TransportKind transport) {
  LoginResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::string rl_error;
  if (!RateLimitUnauth("login", req.username, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  Session session;
  std::string err;
  if (req.kex_version != 0 && req.kex_version != kLoginKeyExchangeV1) {
    resp.error = "unsupported key exchange version";
    return resp;
  }
  if (req.kex_version == kLoginKeyExchangeV1) {
    LoginHybridServerHello hello;
    if (!sessions_->LoginHybrid(req.username, req.password, req.client_dh_pk,
                                req.client_kem_pk, transport, hello, session,
                                err)) {
      resp.error = err;
      return resp;
    }
    resp.success = true;
    resp.token = session.token;
    resp.kex_version = req.kex_version;
    resp.server_dh_pk = hello.server_dh_pk;
    resp.kem_ct = std::move(hello.kem_ct);
    return resp;
  }

  if (!sessions_->Login(req.username, req.password, transport, session, err)) {
    resp.error = err;
    return resp;
  }
  resp.success = true;
  resp.token = session.token;
  return resp;
}

OpaqueRegisterStartResponse ApiService::OpaqueRegisterStart(
    const OpaqueRegisterStartRequest& req) {
  OpaqueRegisterStartResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::string rl_error;
  if (!RateLimitUnauth("opaque_register_start", req.username, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  OpaqueRegisterStartServerHello hello;
  std::string err;
  if (!sessions_->OpaqueRegisterStart(req, hello, err)) {
    resp.error = err.empty() ? "opaque register start failed" : err;
    return resp;
  }
  resp.success = true;
  resp.hello = std::move(hello);
  return resp;
}

OpaqueRegisterFinishResponse ApiService::OpaqueRegisterFinish(
    const OpaqueRegisterFinishRequest& req) {
  OpaqueRegisterFinishResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::string rl_error;
  if (!RateLimitUnauth("opaque_register_finish", req.username, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  std::string err;
  if (!sessions_->OpaqueRegisterFinish(req, err)) {
    resp.error = err.empty() ? "opaque register finish failed" : err;
    return resp;
  }
  resp.success = true;
  return resp;
}

OpaqueLoginStartResponse ApiService::OpaqueLoginStart(
    const OpaqueLoginStartRequest& req) {
  OpaqueLoginStartResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::string rl_error;
  if (!RateLimitUnauth("opaque_login_start", req.username, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  OpaqueLoginStartServerHello hello;
  std::string err;
  if (!sessions_->OpaqueLoginStart(req, hello, err)) {
    resp.error = err.empty() ? "opaque login start failed" : err;
    return resp;
  }
  resp.success = true;
  resp.hello = std::move(hello);
  return resp;
}

OpaqueLoginFinishResponse ApiService::OpaqueLoginFinish(
    const OpaqueLoginFinishRequest& req, TransportKind transport) {
  OpaqueLoginFinishResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::string rl_error;
  if (!RateLimitUnauth("opaque_login_finish", {}, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  Session session;
  std::string err;
  if (!sessions_->OpaqueLoginFinish(req, transport, session, err)) {
    resp.error = err.empty() ? "opaque login finish failed" : err;
    return resp;
  }
  resp.success = true;
  resp.token = session.token;
  return resp;
}

LogoutResponse ApiService::Logout(const LogoutRequest& req) {
  LogoutResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  sessions_->Logout(req.token);
  resp.success = true;
  return resp;
}

GroupEventResponse ApiService::JoinGroup(const std::string& token,
                                         const std::string& group_id) {
  GroupEventResponse resp;
  if (!groups_ || !sessions_) {
    resp.error = "group manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("join_group", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  auto key = groups_->Rotate(group_id, RotationReason::kJoin);
  if (directory_) {
    directory_->AddGroup(group_id, sess->username);
    directory_->AddMember(group_id, sess->username);
  }
  if (queue_ && directory_) {
    const auto notice =
        BuildGroupNoticePayload(kGroupNoticeJoin, sess->username);
    const auto members = directory_->Members(group_id);
    for (const auto& m : members) {
      if (m.empty()) {
        continue;
      }
      queue_->EnqueueGroupNotice(m, group_id, sess->username, notice);
    }
  }
  resp.success = true;
  resp.version = key.version;
  resp.reason = key.reason;
  return resp;
}

GroupEventResponse ApiService::LeaveGroup(const std::string& token,
                                          const std::string& group_id) {
  GroupEventResponse resp;
  if (!groups_ || !sessions_) {
    resp.error = "group manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("leave_group", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  auto key = groups_->Rotate(group_id, RotationReason::kLeave);
  if (directory_) {
    directory_->RemoveMember(group_id, sess->username);
  }
  if (queue_ && directory_) {
    const auto notice =
        BuildGroupNoticePayload(kGroupNoticeLeave, sess->username);
    auto recipients = directory_->Members(group_id);
    recipients.push_back(sess->username);
    std::sort(recipients.begin(), recipients.end());
    recipients.erase(std::unique(recipients.begin(), recipients.end()),
                     recipients.end());
    for (const auto& m : recipients) {
      if (m.empty()) {
        continue;
      }
      queue_->EnqueueGroupNotice(m, group_id, sess->username, notice);
    }
  }
  resp.success = true;
  resp.version = key.version;
  resp.reason = key.reason;
  return resp;
}

GroupEventResponse ApiService::KickGroup(const std::string& token,
                                         const std::string& group_id) {
  GroupEventResponse resp;
  if (!groups_ || !sessions_) {
    resp.error = "group manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("kick_group", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  auto key = groups_->Rotate(group_id, RotationReason::kKick);
  if (directory_) {
    directory_->RemoveMember(group_id, sess->username);
  }
  if (queue_ && directory_) {
    const auto notice =
        BuildGroupNoticePayload(kGroupNoticeKick, sess->username);
    auto recipients = directory_->Members(group_id);
    recipients.push_back(sess->username);
    std::sort(recipients.begin(), recipients.end());
    recipients.erase(std::unique(recipients.begin(), recipients.end()),
                     recipients.end());
    for (const auto& m : recipients) {
      if (m.empty()) {
        continue;
      }
      queue_->EnqueueGroupNotice(m, group_id, sess->username, notice);
    }
  }
  resp.success = true;
  resp.version = key.version;
  resp.reason = key.reason;
  return resp;
}

GroupMessageResponse ApiService::OnGroupMessage(const std::string& token,
                                                const std::string& group_id,
                                                std::uint64_t threshold) {
  GroupMessageResponse resp;
  if (!groups_ || !sessions_) {
    resp.error = "group manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("group_message", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (directory_) {
    if (!directory_->HasMember(group_id, sess->username)) {
      resp.error = "not in group";
      return resp;
    }
  }
  const std::uint64_t use_threshold =
      (threshold == 0) ? group_threshold_ : threshold;
  auto rotated = groups_->OnMessage(group_id, use_threshold);
  resp.success = true;
  resp.rotated = rotated;
  return resp;
}

std::optional<GroupKey> ApiService::CurrentGroupKey(
    const std::string& group_id) {
  if (!groups_) {
    return std::nullopt;
  }
  return groups_->GetKey(group_id);
}

GroupMembersResponse ApiService::GroupMembers(const std::string& token,
                                              const std::string& group_id) {
  GroupMembersResponse resp;
  if (!directory_ || !sessions_) {
    resp.error = "group directory unavailable";
    return resp;
  }

  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("group_members", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }

  if (group_id.empty()) {
    resp.error = "group id empty";
    return resp;
  }

  if (!directory_->HasMember(group_id, sess->username)) {
    resp.error = "not in group";
    return resp;
  }

  resp.members = directory_->Members(group_id);
  std::sort(resp.members.begin(), resp.members.end());
  resp.success = true;
  return resp;
}

GroupMembersInfoResponse ApiService::GroupMembersInfo(const std::string& token,
                                                      const std::string& group_id) {
  GroupMembersInfoResponse resp;
  if (!directory_ || !sessions_) {
    resp.error = "group directory unavailable";
    return resp;
  }

  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("group_member_info", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }

  if (group_id.empty()) {
    resp.error = "group id empty";
    return resp;
  }

  if (!directory_->HasMember(group_id, sess->username)) {
    resp.error = "not in group";
    return resp;
  }

  resp.members = directory_->MembersWithRoles(group_id);
  std::sort(resp.members.begin(), resp.members.end(),
            [](const GroupMemberInfo& a, const GroupMemberInfo& b) {
              return a.username < b.username;
            });
  resp.success = true;
  return resp;
}

GroupRoleSetResponse ApiService::SetGroupRole(const std::string& token,
                                              const std::string& group_id,
                                              const std::string& target_username,
                                              GroupRole role) {
  GroupRoleSetResponse resp;
  if (!directory_ || !sessions_) {
    resp.error = "group directory unavailable";
    return resp;
  }

  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("group_role_set", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }

  if (group_id.empty() || target_username.empty()) {
    resp.error = "invalid params";
    return resp;
  }

  if (role != GroupRole::kAdmin && role != GroupRole::kMember) {
    resp.error = "invalid role";
    return resp;
  }

  const auto self_role = directory_->RoleOf(group_id, sess->username);
  if (!self_role.has_value()) {
    resp.error = "not in group";
    return resp;
  }
  if (self_role.value() != GroupRole::kOwner) {
    resp.error = "permission denied";
    return resp;
  }

  if (target_username == sess->username) {
    resp.error = "cannot change self";
    return resp;
  }

  const auto target_role = directory_->RoleOf(group_id, target_username);
  if (!target_role.has_value()) {
    resp.error = "target not in group";
    return resp;
  }
  if (target_role.value() == GroupRole::kOwner) {
    resp.error = "cannot change owner";
    return resp;
  }

  if (!directory_->SetRole(group_id, target_username, role)) {
    resp.error = "set role failed";
    return resp;
  }

  if (queue_) {
    const auto notice = BuildGroupNoticePayload(kGroupNoticeRoleSet, target_username, role);
    const auto members = directory_->Members(group_id);
    for (const auto& m : members) {
      if (m.empty()) {
        continue;
      }
      queue_->EnqueueGroupNotice(m, group_id, sess->username, notice);
    }
  }

  resp.success = true;
  return resp;
}

GroupEventResponse ApiService::KickGroupMember(const std::string& token,
                                               const std::string& group_id,
                                               const std::string& target_username) {
  GroupEventResponse resp;
  if (!directory_ || !sessions_ || !groups_) {
    resp.error = "group manager unavailable";
    return resp;
  }

  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("group_kick_member", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }

  if (group_id.empty() || target_username.empty()) {
    resp.error = "invalid params";
    return resp;
  }

  const auto self_role = directory_->RoleOf(group_id, sess->username);
  if (!self_role.has_value()) {
    resp.error = "not in group";
    return resp;
  }

  if (target_username == sess->username) {
    resp.error = "cannot kick self";
    return resp;
  }

  const auto target_role = directory_->RoleOf(group_id, target_username);
  if (!target_role.has_value()) {
    resp.error = "target not in group";
    return resp;
  }
  if (target_role.value() == GroupRole::kOwner) {
    resp.error = "cannot kick owner";
    return resp;
  }

  if (self_role.value() == GroupRole::kMember) {
    resp.error = "permission denied";
    return resp;
  }
  if (self_role.value() == GroupRole::kAdmin &&
      target_role.value() != GroupRole::kMember) {
    resp.error = "permission denied";
    return resp;
  }

  auto key = groups_->Rotate(group_id, RotationReason::kKick);
  directory_->RemoveMember(group_id, target_username);

  if (queue_) {
    const auto notice = BuildGroupNoticePayload(kGroupNoticeKick, target_username);
    auto recipients = directory_->Members(group_id);
    recipients.push_back(target_username);
    std::sort(recipients.begin(), recipients.end());
    recipients.erase(std::unique(recipients.begin(), recipients.end()),
                     recipients.end());
    for (const auto& m : recipients) {
      if (m.empty()) {
        continue;
      }
      queue_->EnqueueGroupNotice(m, group_id, sess->username, notice);
    }
  }

  resp.success = true;
  resp.version = key.version;
  resp.reason = key.reason;
  return resp;
}

FileUploadResponse ApiService::StoreEphemeralFile(
    const std::string& token, const std::vector<std::uint8_t>& data) {
  FileUploadResponse resp;
  if (!sessions_ || !storage_) {
    resp.error = "storage unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitFile("file_ephemeral_upload", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  auto put = storage_->Put(sess->username, data);
  if (!put.success) {
    resp.error = put.error;
    return resp;
  }
  resp.success = true;
  resp.file_id = put.file_id;
  resp.file_key = put.file_key;
  resp.meta = put.meta;
  return resp;
}

FileDownloadResponse ApiService::LoadEphemeralFile(
    const std::string& token, const std::string& file_id,
    const std::array<std::uint8_t, 32>& key, bool wipe_after_read) {
  FileDownloadResponse resp;
  if (!sessions_ || !storage_) {
    resp.error = "storage unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitFile("file_ephemeral_download", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  std::string err;
  auto data = storage_->Fetch(file_id, key, wipe_after_read, err);
  if (!data.has_value()) {
    resp.error = err;
    return resp;
  }
  resp.success = true;
  resp.plaintext = std::move(*data);
  auto meta = storage_->Meta(file_id);
  if (meta.has_value()) {
    resp.meta = *meta;
  }
  return resp;
}

FileBlobUploadResponse ApiService::StoreE2eeFileBlob(
    const std::string& token, const std::vector<std::uint8_t>& blob) {
  FileBlobUploadResponse resp;
  if (!sessions_ || !storage_) {
    resp.error = "storage unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitFile("file_blob_upload", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (blob.empty()) {
    resp.error = "empty payload";
    return resp;
  }
  if (blob.size() > (320u * 1024u * 1024u)) {
    resp.error = "payload too large";
    return resp;
  }

  auto put = storage_->PutBlob(sess->username, blob);
  if (!put.success) {
    resp.error = put.error;
    return resp;
  }
  resp.success = true;
  resp.file_id = put.file_id;
  resp.meta = put.meta;
  return resp;
}

FileBlobDownloadResponse ApiService::LoadE2eeFileBlob(
    const std::string& token, const std::string& file_id,
    bool wipe_after_read) {
  FileBlobDownloadResponse resp;
  if (!sessions_ || !storage_) {
    resp.error = "storage unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitFile("file_blob_download", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (file_id.empty()) {
    resp.error = "file id empty";
    return resp;
  }

  const auto meta = storage_->Meta(file_id);

  std::string err;
  auto data = storage_->FetchBlob(file_id, wipe_after_read, err);
  if (!data.has_value()) {
    resp.error = err;
    return resp;
  }
  resp.success = true;
  resp.blob = std::move(*data);
  if (meta.has_value()) {
    resp.meta = *meta;
  }
  return resp;
}

FileBlobUploadStartResponse ApiService::StartE2eeFileBlobUpload(
    const std::string& token, std::uint64_t expected_size) {
  FileBlobUploadStartResponse resp;
  if (!sessions_ || !storage_) {
    resp.error = "storage unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitFile("file_blob_upload_start", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }

  const auto started = storage_->BeginBlobUpload(sess->username, expected_size);
  if (!started.success) {
    resp.error = started.error;
    return resp;
  }
  resp.success = true;
  resp.file_id = started.file_id;
  resp.upload_id = started.upload_id;
  return resp;
}

FileBlobUploadChunkResponse ApiService::UploadE2eeFileBlobChunk(
    const std::string& token, const std::string& file_id,
    const std::string& upload_id, std::uint64_t offset,
    const std::vector<std::uint8_t>& chunk) {
  FileBlobUploadChunkResponse resp;
  if (!sessions_ || !storage_) {
    resp.error = "storage unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("file_blob_upload_chunk", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  const auto appended = storage_->AppendBlobUploadChunk(
      sess->username, file_id, upload_id, offset, chunk);
  if (!appended.success) {
    resp.error = appended.error;
    return resp;
  }
  resp.success = true;
  resp.bytes_received = appended.bytes_received;
  return resp;
}

FileBlobUploadFinishResponse ApiService::FinishE2eeFileBlobUpload(
    const std::string& token, const std::string& file_id,
    const std::string& upload_id, std::uint64_t total_size) {
  FileBlobUploadFinishResponse resp;
  if (!sessions_ || !storage_) {
    resp.error = "storage unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitFile("file_blob_upload_finish", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }

  const auto finished = storage_->FinishBlobUpload(sess->username, file_id,
                                                   upload_id, total_size);
  if (!finished.success) {
    resp.error = finished.error;
    return resp;
  }
  resp.success = true;
  resp.meta = finished.meta;
  return resp;
}

FileBlobDownloadStartResponse ApiService::StartE2eeFileBlobDownload(
    const std::string& token, const std::string& file_id, bool wipe_after_read) {
  FileBlobDownloadStartResponse resp;
  if (!sessions_ || !storage_) {
    resp.error = "storage unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitFile("file_blob_download_start", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (file_id.empty()) {
    resp.error = "file id empty";
    return resp;
  }

  const auto started =
      storage_->BeginBlobDownload(sess->username, file_id, wipe_after_read);
  if (!started.success) {
    resp.error = started.error;
    return resp;
  }
  resp.success = true;
  resp.download_id = started.download_id;
  resp.meta = started.meta;
  resp.size = started.meta.size;
  return resp;
}

FileBlobDownloadChunkResponse ApiService::DownloadE2eeFileBlobChunk(
    const std::string& token, const std::string& file_id,
    const std::string& download_id, std::uint64_t offset,
    std::uint32_t max_len) {
  FileBlobDownloadChunkResponse resp;
  if (!sessions_ || !storage_) {
    resp.error = "storage unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("file_blob_download_chunk", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }

  const auto got = storage_->ReadBlobDownloadChunk(
      sess->username, file_id, download_id, offset, max_len);
  if (!got.success) {
    resp.error = got.error;
    return resp;
  }
  resp.success = true;
  resp.offset = got.offset;
  resp.eof = got.eof;
  resp.chunk = got.chunk;
  return resp;
}

OfflinePushResponse ApiService::EnqueueOffline(const std::string& token,
                                               const std::string& recipient,
                                               std::vector<std::uint8_t> payload) {
  OfflinePushResponse resp;
  if (!sessions_ || !queue_) {
    resp.error = "queue unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("offline_push", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (recipient.empty()) {
    resp.error = "recipient empty";
    return resp;
  }

  bool blocked = false;
#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    std::string block_err;
    const bool recipient_blocks_sender =
        IsBlockedMysql(*friend_mysql_, recipient, sess->username, block_err);
    if (!block_err.empty()) {
      resp.error = block_err;
      return resp;
    }
    std::string block_err2;
    const bool sender_blocks_recipient =
        IsBlockedMysql(*friend_mysql_, sess->username, recipient, block_err2);
    if (!block_err2.empty()) {
      resp.error = block_err2;
      return resp;
    }
    blocked = recipient_blocks_sender || sender_blocks_recipient;
  } else {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = blocks_.find(sess->username);
    if (it != blocks_.end() && it->second.count(recipient) != 0) {
      blocked = true;
    }
    const auto it2 = blocks_.find(recipient);
    if (it2 != blocks_.end() && it2->second.count(sess->username) != 0) {
      blocked = true;
    }
  }
#else
  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = blocks_.find(sess->username);
    if (it != blocks_.end() && it->second.count(recipient) != 0) {
      blocked = true;
    }
    const auto it2 = blocks_.find(recipient);
    if (it2 != blocks_.end() && it2->second.count(sess->username) != 0) {
      blocked = true;
    }
  }
#endif

  if (blocked) {
    resp.success = true;
    return resp;
  }

  queue_->Enqueue(recipient, std::move(payload));
  resp.success = true;
  return resp;
}

OfflinePullResponse ApiService::PullOffline(const std::string& token) {
  OfflinePullResponse resp;
  if (!sessions_ || !queue_) {
    resp.error = "queue unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("offline_pull", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  resp.messages = queue_->Drain(sess->username);
  resp.success = true;
  return resp;
}

std::uint32_t ApiService::CurrentFriendVersionLocked(
    const std::string& username) const {
  const auto it = friend_versions_.find(username);
  if (it == friend_versions_.end()) {
    return 0;
  }
  return it->second;
}

void ApiService::BumpFriendVersionLocked(const std::string& username) {
  auto& ver = friend_versions_[username];
  if (ver == 0xFFFFFFFFu) {
    ver = 1;
  } else {
    ++ver;
  }
}

FriendListResponse ApiService::ListFriendsInternal(const Session& sess) {
  FriendListResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }

#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
      resp.error = "mysql_init failed";
      return resp;
    }
    MYSQL* res = mysql_real_connect(conn, friend_mysql_->host.c_str(),
                                    friend_mysql_->username.c_str(),
                                    friend_mysql_->password.get().c_str(),
                                    friend_mysql_->database.c_str(),
                                    friend_mysql_->port, nullptr, 0);
    if (!res) {
      resp.error = "mysql_connect failed";
      mysql_close(conn);
      return resp;
    }

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS user_friend ("
        "username VARCHAR(64) NOT NULL,"
        "friend_username VARCHAR(64) NOT NULL,"
        "remark VARCHAR(128) NOT NULL DEFAULT '',"
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY(username, friend_username),"
        "INDEX idx_friend_username(friend_username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
    if (mysql_query(conn, ddl) != 0) {
      resp.error = "mysql_schema_failed";
      mysql_close(conn);
      return resp;
    }
    // Best-effort migration for older schemas missing remark column.
    mysql_query(conn,
                "ALTER TABLE user_friend "
                "ADD COLUMN remark VARCHAR(128) NOT NULL DEFAULT ''");

    const char* query =
        "SELECT friend_username, remark FROM user_friend WHERE username=? "
        "ORDER BY friend_username";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
      resp.error = "mysql_stmt_init failed";
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_prepare(stmt, query,
                           static_cast<unsigned long>(std::strlen(query))) !=
        0) {
      resp.error = "mysql_stmt_prepare failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }

    MYSQL_BIND bind_param[1];
    std::memset(bind_param, 0, sizeof(bind_param));
    bind_param[0].buffer_type = MYSQL_TYPE_STRING;
    bind_param[0].buffer = const_cast<char*>(sess.username.c_str());
    bind_param[0].buffer_length =
        static_cast<unsigned long>(sess.username.size());

    if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
      resp.error = "mysql_stmt_bind_param failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_execute(stmt) != 0) {
      resp.error = "mysql_stmt_execute failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }

    char name_buf[256] = {0};
    unsigned long name_len = 0;
    char remark_buf[256] = {0};
    unsigned long remark_len = 0;
    MYSQL_BIND bind_result[2];
    std::memset(bind_result, 0, sizeof(bind_result));
    using BindBool0 = std::remove_pointer_t<decltype(bind_result[0].is_null)>;
    using BindBool1 = std::remove_pointer_t<decltype(bind_result[1].is_null)>;
    BindBool0 name_is_null = 0;
    BindBool0 name_error_flag = 0;
    BindBool1 remark_is_null = 0;
    BindBool1 remark_error_flag = 0;
    bind_result[0].buffer_type = MYSQL_TYPE_STRING;
    bind_result[0].buffer = name_buf;
    bind_result[0].buffer_length = sizeof(name_buf) - 1;
    bind_result[0].length = &name_len;
    bind_result[0].is_null = &name_is_null;
    bind_result[0].error = &name_error_flag;
    bind_result[1].buffer_type = MYSQL_TYPE_STRING;
    bind_result[1].buffer = remark_buf;
    bind_result[1].buffer_length = sizeof(remark_buf) - 1;
    bind_result[1].length = &remark_len;
    bind_result[1].is_null = &remark_is_null;
    bind_result[1].error = &remark_error_flag;

    if (mysql_stmt_bind_result(stmt, bind_result) != 0) {
      resp.error = "mysql_stmt_bind_result failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_store_result(stmt) != 0) {
      resp.error = "mysql_stmt_store_result failed";
      mysql_stmt_free_result(stmt);
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }

    std::vector<FriendListResponse::Entry> out;
    while (true) {
      const int fetch_status = mysql_stmt_fetch(stmt);
      if (fetch_status == MYSQL_NO_DATA) {
        break;
      }
      if (fetch_status != 0 && fetch_status != MYSQL_DATA_TRUNCATED) {
        resp.error = "mysql_stmt_fetch failed";
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      if (name_is_null) {
        continue;
      }
      if (name_len >= sizeof(name_buf)) {
        resp.error = "friend name too long";
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      name_buf[name_len] = '\0';
      if (!remark_is_null) {
        if (remark_len >= sizeof(remark_buf)) {
          resp.error = "remark too long";
          mysql_stmt_free_result(stmt);
          mysql_stmt_close(stmt);
          mysql_close(conn);
          return resp;
        }
        remark_buf[remark_len] = '\0';
      } else {
        remark_len = 0;
      }
      FriendListResponse::Entry e;
      e.username.assign(name_buf, name_len);
      if (remark_len > 0) {
        e.remark.assign(remark_buf, remark_len);
      }
      out.push_back(std::move(e));
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    mysql_close(conn);

    std::sort(out.begin(), out.end(),
              [](const FriendListResponse::Entry& a,
                 const FriendListResponse::Entry& b) {
                return a.username < b.username;
              });
    resp.success = true;
    resp.friends = std::move(out);
    return resp;
  }
#endif

  std::vector<FriendListResponse::Entry> out;
  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    auto it = friends_.find(sess.username);
    if (it != friends_.end()) {
      out.reserve(it->second.size());
      const auto remarks_it = friend_remarks_.find(sess.username);
      for (const auto& f : it->second) {
        FriendListResponse::Entry e;
        e.username = f;
        if (remarks_it != friend_remarks_.end()) {
          const auto r = remarks_it->second.find(f);
          if (r != remarks_it->second.end()) {
            e.remark = r->second;
          }
        }
        out.push_back(std::move(e));
      }
    }
  }
  std::sort(out.begin(), out.end(),
            [](const FriendListResponse::Entry& a,
               const FriendListResponse::Entry& b) {
              return a.username < b.username;
            });
  resp.success = true;
  resp.friends = std::move(out);
  return resp;
}

FriendListResponse ApiService::ListFriends(const std::string& token) {
  FriendListResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("friend_list", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  return ListFriendsInternal(*sess);
}

FriendSyncResponse ApiService::SyncFriends(const std::string& token,
                                           std::uint32_t last_version) {
  FriendSyncResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("friend_sync", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }

  std::uint32_t current_version = 0;
  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    current_version = CurrentFriendVersionLocked(sess->username);
  }
  resp.version = current_version;
  if (last_version == current_version) {
    resp.success = true;
    resp.changed = false;
    return resp;
  }

  FriendListResponse list = ListFriendsInternal(*sess);
  if (!list.success) {
    resp.error = list.error.empty() ? "friend list failed" : list.error;
    return resp;
  }
  resp.success = true;
  resp.changed = true;
  resp.friends = std::move(list.friends);
  return resp;
}

FriendAddResponse ApiService::AddFriend(const std::string& token,
                                        const std::string& friend_username,
                                        const std::string& remark) {
  FriendAddResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("friend_add", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (friend_username.empty()) {
    resp.error = "friend username empty";
    return resp;
  }
  if (friend_username == sess->username) {
    resp.error = "cannot add self";
    return resp;
  }
  if (remark.size() > 128) {
    resp.error = "remark too long";
    return resp;
  }

  std::string exist_err;
  if (!sessions_->UserExists(friend_username, exist_err)) {
    resp.error = exist_err.empty() ? "friend not found" : exist_err;
    return resp;
  }

#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
      resp.error = "mysql_init failed";
      return resp;
    }
    MYSQL* res = mysql_real_connect(conn, friend_mysql_->host.c_str(),
                                    friend_mysql_->username.c_str(),
                                    friend_mysql_->password.get().c_str(),
                                    friend_mysql_->database.c_str(),
                                    friend_mysql_->port, nullptr, 0);
    if (!res) {
      resp.error = "mysql_connect failed";
      mysql_close(conn);
      return resp;
    }

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS user_friend ("
        "username VARCHAR(64) NOT NULL,"
        "friend_username VARCHAR(64) NOT NULL,"
        "remark VARCHAR(128) NOT NULL DEFAULT '',"
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY(username, friend_username),"
        "INDEX idx_friend_username(friend_username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
    if (mysql_query(conn, ddl) != 0) {
      resp.error = "mysql_schema_failed";
      mysql_close(conn);
      return resp;
    }
    mysql_query(conn,
                "ALTER TABLE user_friend "
                "ADD COLUMN remark VARCHAR(128) NOT NULL DEFAULT ''");

    const char* query =
        "INSERT IGNORE INTO user_friend(username, friend_username, remark) "
        "VALUES(?, ?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
      resp.error = "mysql_stmt_init failed";
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_prepare(stmt, query,
                           static_cast<unsigned long>(std::strlen(query))) !=
        0) {
      resp.error = "mysql_stmt_prepare failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }

    auto bind_and_exec = [&](const std::string& u,
                             const std::string& f,
                             const std::string& r) -> bool {
      MYSQL_BIND bind_param[3];
      std::memset(bind_param, 0, sizeof(bind_param));
      bind_param[0].buffer_type = MYSQL_TYPE_STRING;
      bind_param[0].buffer = const_cast<char*>(u.c_str());
      bind_param[0].buffer_length = static_cast<unsigned long>(u.size());
      bind_param[1].buffer_type = MYSQL_TYPE_STRING;
      bind_param[1].buffer = const_cast<char*>(f.c_str());
      bind_param[1].buffer_length = static_cast<unsigned long>(f.size());
      bind_param[2].buffer_type = MYSQL_TYPE_STRING;
      bind_param[2].buffer = const_cast<char*>(r.c_str());
      bind_param[2].buffer_length = static_cast<unsigned long>(r.size());
      if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
        return false;
      }
      return mysql_stmt_execute(stmt) == 0;
    };

    const bool ok1 = bind_and_exec(sess->username, friend_username, remark);
    const bool ok2 = bind_and_exec(friend_username, sess->username, "");

    mysql_stmt_close(stmt);
    mysql_close(conn);

    if (!ok1 || !ok2) {
      resp.error = "mysql insert failed";
      return resp;
    }
    {
      std::lock_guard<std::mutex> lock(friends_mutex_);
      BumpFriendVersionLocked(sess->username);
      BumpFriendVersionLocked(friend_username);
    }
    resp.success = true;
    return resp;
  }
#endif

  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    friends_[sess->username].insert(friend_username);
    friends_[friend_username].insert(sess->username);
    if (!remark.empty()) {
      friend_remarks_[sess->username][friend_username] = remark;
    }
    BumpFriendVersionLocked(sess->username);
    BumpFriendVersionLocked(friend_username);
  }
  resp.success = true;
  return resp;
}

FriendRemarkResponse ApiService::SetFriendRemark(const std::string& token,
                                                 const std::string& friend_username,
                                                 const std::string& remark) {
  FriendRemarkResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("friend_remark_set", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (friend_username.empty()) {
    resp.error = "friend username empty";
    return resp;
  }
  if (remark.size() > 128) {
    resp.error = "remark too long";
    return resp;
  }

#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
      resp.error = "mysql_init failed";
      return resp;
    }
    MYSQL* res = mysql_real_connect(conn, friend_mysql_->host.c_str(),
                                    friend_mysql_->username.c_str(),
                                    friend_mysql_->password.get().c_str(),
                                    friend_mysql_->database.c_str(),
                                    friend_mysql_->port, nullptr, 0);
    if (!res) {
      resp.error = "mysql_connect failed";
      mysql_close(conn);
      return resp;
    }

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS user_friend ("
        "username VARCHAR(64) NOT NULL,"
        "friend_username VARCHAR(64) NOT NULL,"
        "remark VARCHAR(128) NOT NULL DEFAULT '',"
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY(username, friend_username),"
        "INDEX idx_friend_username(friend_username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
    if (mysql_query(conn, ddl) != 0) {
      resp.error = "mysql_schema_failed";
      mysql_close(conn);
      return resp;
    }
    mysql_query(conn,
                "ALTER TABLE user_friend "
                "ADD COLUMN remark VARCHAR(128) NOT NULL DEFAULT ''");

    // Ensure friend relation exists.
    {
      const char* exist_query =
          "SELECT 1 FROM user_friend WHERE username=? AND friend_username=? "
          "LIMIT 1";
      MYSQL_STMT* stmt = mysql_stmt_init(conn);
      if (!stmt) {
        resp.error = "mysql_stmt_init failed";
        mysql_close(conn);
        return resp;
      }
      if (mysql_stmt_prepare(stmt, exist_query,
                             static_cast<unsigned long>(
                                 std::strlen(exist_query))) != 0) {
        resp.error = "mysql_stmt_prepare failed";
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      MYSQL_BIND bind_param[2];
      std::memset(bind_param, 0, sizeof(bind_param));
      bind_param[0].buffer_type = MYSQL_TYPE_STRING;
      bind_param[0].buffer = const_cast<char*>(sess->username.c_str());
      bind_param[0].buffer_length =
          static_cast<unsigned long>(sess->username.size());
      bind_param[1].buffer_type = MYSQL_TYPE_STRING;
      bind_param[1].buffer = const_cast<char*>(friend_username.c_str());
      bind_param[1].buffer_length =
          static_cast<unsigned long>(friend_username.size());
      if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
        resp.error = "mysql_stmt_bind_param failed";
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      if (mysql_stmt_execute(stmt) != 0) {
        resp.error = "mysql_stmt_execute failed";
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      int value = 0;
      MYSQL_BIND bind_result[1];
      std::memset(bind_result, 0, sizeof(bind_result));
      using BindBool = std::remove_pointer_t<decltype(bind_result[0].is_null)>;
      BindBool is_null = 0;
      BindBool error_flag = 0;
      bind_result[0].buffer_type = MYSQL_TYPE_LONG;
      bind_result[0].buffer = &value;
      bind_result[0].is_null = &is_null;
      bind_result[0].error = &error_flag;
      if (mysql_stmt_bind_result(stmt, bind_result) != 0) {
        resp.error = "mysql_stmt_bind_result failed";
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      if (mysql_stmt_store_result(stmt) != 0) {
        resp.error = "mysql_stmt_store_result failed";
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      const int fetch_status = mysql_stmt_fetch(stmt);
      mysql_stmt_free_result(stmt);
      mysql_stmt_close(stmt);
      if (fetch_status == MYSQL_NO_DATA || is_null) {
        resp.error = "not friends";
        mysql_close(conn);
        return resp;
      }
      if (fetch_status != 0 && fetch_status != MYSQL_DATA_TRUNCATED) {
        resp.error = "mysql_stmt_fetch failed";
        mysql_close(conn);
        return resp;
      }
    }

    const char* query =
        "UPDATE user_friend SET remark=? WHERE username=? AND friend_username=?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
      resp.error = "mysql_stmt_init failed";
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_prepare(stmt, query,
                           static_cast<unsigned long>(std::strlen(query))) !=
        0) {
      resp.error = "mysql_stmt_prepare failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    MYSQL_BIND bind_param[3];
    std::memset(bind_param, 0, sizeof(bind_param));
    bind_param[0].buffer_type = MYSQL_TYPE_STRING;
    bind_param[0].buffer = const_cast<char*>(remark.c_str());
    bind_param[0].buffer_length = static_cast<unsigned long>(remark.size());
    bind_param[1].buffer_type = MYSQL_TYPE_STRING;
    bind_param[1].buffer = const_cast<char*>(sess->username.c_str());
    bind_param[1].buffer_length =
        static_cast<unsigned long>(sess->username.size());
    bind_param[2].buffer_type = MYSQL_TYPE_STRING;
    bind_param[2].buffer = const_cast<char*>(friend_username.c_str());
    bind_param[2].buffer_length =
        static_cast<unsigned long>(friend_username.size());
    if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
      resp.error = "mysql_stmt_bind_param failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_execute(stmt) != 0) {
      resp.error = "mysql_stmt_execute failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    mysql_stmt_close(stmt);
    mysql_close(conn);

    {
      std::lock_guard<std::mutex> lock(friends_mutex_);
      BumpFriendVersionLocked(sess->username);
    }
    resp.success = true;
    return resp;
  }
#endif

  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    auto it = friends_.find(sess->username);
    if (it == friends_.end() ||
        it->second.find(friend_username) == it->second.end()) {
      resp.error = "not friends";
      return resp;
    }
    if (remark.empty()) {
      auto r = friend_remarks_.find(sess->username);
      if (r != friend_remarks_.end()) {
        r->second.erase(friend_username);
      }
    } else {
      friend_remarks_[sess->username][friend_username] = remark;
    }
    BumpFriendVersionLocked(sess->username);
  }
  resp.success = true;
  return resp;
}

FriendRequestSendResponse ApiService::SendFriendRequest(
    const std::string& token, const std::string& target_username,
    const std::string& requester_remark) {
  FriendRequestSendResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("friend_request_send", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (target_username.empty()) {
    resp.error = "target username empty";
    return resp;
  }
  if (target_username == sess->username) {
    resp.error = "cannot add self";
    return resp;
  }
  if (requester_remark.size() > 128) {
    resp.error = "remark too long";
    return resp;
  }

  std::string exist_err;
  if (!sessions_->UserExists(target_username, exist_err)) {
    resp.error = exist_err.empty() ? "target not found" : exist_err;
    return resp;
  }

  bool blocked = false;
#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    std::string block_err;
    const bool target_blocks_sender =
        IsBlockedMysql(*friend_mysql_, target_username, sess->username, block_err);
    if (!block_err.empty()) {
      resp.error = block_err;
      return resp;
    }
    std::string block_err2;
    const bool sender_blocks_target =
        IsBlockedMysql(*friend_mysql_, sess->username, target_username, block_err2);
    if (!block_err2.empty()) {
      resp.error = block_err2;
      return resp;
    }
    blocked = target_blocks_sender || sender_blocks_target;
  } else {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = blocks_.find(sess->username);
    if (it != blocks_.end() && it->second.count(target_username) != 0) {
      blocked = true;
    }
    const auto it2 = blocks_.find(target_username);
    if (it2 != blocks_.end() && it2->second.count(sess->username) != 0) {
      blocked = true;
    }
  }
#else
  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = blocks_.find(sess->username);
    if (it != blocks_.end() && it->second.count(target_username) != 0) {
      blocked = true;
    }
    const auto it2 = blocks_.find(target_username);
    if (it2 != blocks_.end() && it2->second.count(sess->username) != 0) {
      blocked = true;
    }
  }
#endif

  if (blocked) {
    resp.success = true;
    return resp;
  }

  bool is_friend = false;
#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    std::string err;
    is_friend =
        AreFriendsMysql(*friend_mysql_, sess->username, target_username, err);
    if (!err.empty()) {
      resp.error = err;
      return resp;
    }
  } else {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = friends_.find(sess->username);
    is_friend = (it != friends_.end() && it->second.count(target_username) != 0);
  }
#else
  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = friends_.find(sess->username);
    is_friend = (it != friends_.end() && it->second.count(target_username) != 0);
  }
#endif

  if (is_friend) {
    resp.success = true;
    return resp;
  }

#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
      resp.error = "mysql_init failed";
      return resp;
    }
    MYSQL* res = mysql_real_connect(conn, friend_mysql_->host.c_str(),
                                    friend_mysql_->username.c_str(),
                                    friend_mysql_->password.get().c_str(),
                                    friend_mysql_->database.c_str(),
                                    friend_mysql_->port, nullptr, 0);
    if (!res) {
      resp.error = "mysql_connect failed";
      mysql_close(conn);
      return resp;
    }

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS user_friend_request ("
        "target_username VARCHAR(64) NOT NULL,"
        "requester_username VARCHAR(64) NOT NULL,"
        "requester_remark VARCHAR(128) NOT NULL DEFAULT '',"
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY(target_username, requester_username),"
        "INDEX idx_requester_username(requester_username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
    if (mysql_query(conn, ddl) != 0) {
      resp.error = "mysql_schema_failed";
      mysql_close(conn);
      return resp;
    }
    mysql_query(conn,
                "ALTER TABLE user_friend_request "
                "ADD COLUMN requester_remark VARCHAR(128) NOT NULL DEFAULT ''");

    const char* query =
        "INSERT IGNORE INTO user_friend_request("
        "target_username, requester_username, requester_remark) "
        "VALUES(?, ?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
      resp.error = "mysql_stmt_init failed";
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_prepare(stmt, query,
                           static_cast<unsigned long>(std::strlen(query))) != 0) {
      resp.error = "mysql_stmt_prepare failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }

    MYSQL_BIND bind_param[3];
    std::memset(bind_param, 0, sizeof(bind_param));
    bind_param[0].buffer_type = MYSQL_TYPE_STRING;
    bind_param[0].buffer = const_cast<char*>(target_username.c_str());
    bind_param[0].buffer_length =
        static_cast<unsigned long>(target_username.size());
    bind_param[1].buffer_type = MYSQL_TYPE_STRING;
    bind_param[1].buffer = const_cast<char*>(sess->username.c_str());
    bind_param[1].buffer_length =
        static_cast<unsigned long>(sess->username.size());
    bind_param[2].buffer_type = MYSQL_TYPE_STRING;
    bind_param[2].buffer = const_cast<char*>(requester_remark.c_str());
    bind_param[2].buffer_length =
        static_cast<unsigned long>(requester_remark.size());

    if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
      resp.error = "mysql_stmt_bind_param failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_execute(stmt) != 0) {
      resp.error = "mysql_stmt_execute failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }

    mysql_stmt_close(stmt);
    mysql_close(conn);

    resp.success = true;
    return resp;
  }
#endif

  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    friend_requests_by_target_[target_username][sess->username] =
        PendingFriendRequest{requester_remark, std::chrono::steady_clock::now()};
  }

  resp.success = true;
  return resp;
}

FriendRequestListResponse ApiService::ListFriendRequests(const std::string& token) {
  FriendRequestListResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("friend_request_list", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }

#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
      resp.error = "mysql_init failed";
      return resp;
    }
    MYSQL* res = mysql_real_connect(conn, friend_mysql_->host.c_str(),
                                    friend_mysql_->username.c_str(),
                                    friend_mysql_->password.get().c_str(),
                                    friend_mysql_->database.c_str(),
                                    friend_mysql_->port, nullptr, 0);
    if (!res) {
      resp.error = "mysql_connect failed";
      mysql_close(conn);
      return resp;
    }

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS user_friend_request ("
        "target_username VARCHAR(64) NOT NULL,"
        "requester_username VARCHAR(64) NOT NULL,"
        "requester_remark VARCHAR(128) NOT NULL DEFAULT '',"
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY(target_username, requester_username),"
        "INDEX idx_requester_username(requester_username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
    if (mysql_query(conn, ddl) != 0) {
      resp.error = "mysql_schema_failed";
      mysql_close(conn);
      return resp;
    }
    mysql_query(conn,
                "ALTER TABLE user_friend_request "
                "ADD COLUMN requester_remark VARCHAR(128) NOT NULL DEFAULT ''");

    const char* query =
        "SELECT requester_username, requester_remark "
        "FROM user_friend_request WHERE target_username=? "
        "ORDER BY created_at";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
      resp.error = "mysql_stmt_init failed";
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_prepare(stmt, query,
                           static_cast<unsigned long>(std::strlen(query))) != 0) {
      resp.error = "mysql_stmt_prepare failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }

    MYSQL_BIND bind_param[1];
    std::memset(bind_param, 0, sizeof(bind_param));
    bind_param[0].buffer_type = MYSQL_TYPE_STRING;
    bind_param[0].buffer = const_cast<char*>(sess->username.c_str());
    bind_param[0].buffer_length =
        static_cast<unsigned long>(sess->username.size());
    if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
      resp.error = "mysql_stmt_bind_param failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_execute(stmt) != 0) {
      resp.error = "mysql_stmt_execute failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }

    char requester_buf[256] = {0};
    unsigned long requester_len = 0;
    char remark_buf[256] = {0};
    unsigned long remark_len = 0;
    MYSQL_BIND bind_result[2];
    std::memset(bind_result, 0, sizeof(bind_result));
    using BindBool0 = std::remove_pointer_t<decltype(bind_result[0].is_null)>;
    using BindBool1 = std::remove_pointer_t<decltype(bind_result[1].is_null)>;
    BindBool0 requester_is_null = 0;
    BindBool0 requester_error_flag = 0;
    BindBool1 remark_is_null = 0;
    BindBool1 remark_error_flag = 0;
    bind_result[0].buffer_type = MYSQL_TYPE_STRING;
    bind_result[0].buffer = requester_buf;
    bind_result[0].buffer_length = sizeof(requester_buf) - 1;
    bind_result[0].length = &requester_len;
    bind_result[0].is_null = &requester_is_null;
    bind_result[0].error = &requester_error_flag;
    bind_result[1].buffer_type = MYSQL_TYPE_STRING;
    bind_result[1].buffer = remark_buf;
    bind_result[1].buffer_length = sizeof(remark_buf) - 1;
    bind_result[1].length = &remark_len;
    bind_result[1].is_null = &remark_is_null;
    bind_result[1].error = &remark_error_flag;

    if (mysql_stmt_bind_result(stmt, bind_result) != 0) {
      resp.error = "mysql_stmt_bind_result failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_store_result(stmt) != 0) {
      resp.error = "mysql_stmt_store_result failed";
      mysql_stmt_free_result(stmt);
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }

    while (true) {
      const int fetch_status = mysql_stmt_fetch(stmt);
      if (fetch_status == MYSQL_NO_DATA) {
        break;
      }
      if (fetch_status != 0 && fetch_status != MYSQL_DATA_TRUNCATED) {
        resp.error = "mysql_stmt_fetch failed";
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return resp;
      }
      FriendRequestListResponse::Entry e;
      if (!requester_is_null) {
        e.requester_username.assign(requester_buf,
                                    requester_buf + requester_len);
      }
      if (!remark_is_null) {
        e.requester_remark.assign(remark_buf, remark_buf + remark_len);
      }
      resp.requests.push_back(std::move(e));
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    mysql_close(conn);

    resp.success = true;
    return resp;
  }
#endif

  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = friend_requests_by_target_.find(sess->username);
    if (it != friend_requests_by_target_.end()) {
      resp.requests.reserve(it->second.size());
      for (const auto& pair : it->second) {
        FriendRequestListResponse::Entry e;
        e.requester_username = pair.first;
        e.requester_remark = pair.second.requester_remark;
        resp.requests.push_back(std::move(e));
      }
      std::sort(resp.requests.begin(), resp.requests.end(),
                [](const auto& a, const auto& b) {
                  return a.requester_username < b.requester_username;
                });
    }
  }

  resp.success = true;
  return resp;
}

FriendRequestRespondResponse ApiService::RespondFriendRequest(
    const std::string& token, const std::string& requester_username,
    bool accept) {
  FriendRequestRespondResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("friend_request_respond", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (requester_username.empty()) {
    resp.error = "requester username empty";
    return resp;
  }
  if (requester_username == sess->username) {
    resp.error = "invalid requester";
    return resp;
  }

  if (accept) {
    bool blocked = false;
#ifdef MI_E2EE_ENABLE_MYSQL
    if (friend_mysql_.has_value()) {
      std::string block_err;
      const bool self_blocks_requester =
          IsBlockedMysql(*friend_mysql_, sess->username, requester_username, block_err);
      if (!block_err.empty()) {
        resp.error = block_err;
        return resp;
      }
      std::string block_err2;
      const bool requester_blocks_self =
          IsBlockedMysql(*friend_mysql_, requester_username, sess->username, block_err2);
      if (!block_err2.empty()) {
        resp.error = block_err2;
        return resp;
      }
      blocked = self_blocks_requester || requester_blocks_self;
    } else {
      std::lock_guard<std::mutex> lock(friends_mutex_);
      const auto it = blocks_.find(sess->username);
      if (it != blocks_.end() && it->second.count(requester_username) != 0) {
        blocked = true;
      }
      const auto it2 = blocks_.find(requester_username);
      if (it2 != blocks_.end() && it2->second.count(sess->username) != 0) {
        blocked = true;
      }
    }
#else
    {
      std::lock_guard<std::mutex> lock(friends_mutex_);
      const auto it = blocks_.find(sess->username);
      if (it != blocks_.end() && it->second.count(requester_username) != 0) {
        blocked = true;
      }
      const auto it2 = blocks_.find(requester_username);
      if (it2 != blocks_.end() && it2->second.count(sess->username) != 0) {
        blocked = true;
      }
    }
#endif
    if (blocked) {
      resp.error = "blocked";
      return resp;
    }
  }

#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
      resp.error = "mysql_init failed";
      return resp;
    }
    MYSQL* res = mysql_real_connect(conn, friend_mysql_->host.c_str(),
                                    friend_mysql_->username.c_str(),
                                    friend_mysql_->password.get().c_str(),
                                    friend_mysql_->database.c_str(),
                                    friend_mysql_->port, nullptr, 0);
    if (!res) {
      resp.error = "mysql_connect failed";
      mysql_close(conn);
      return resp;
    }

    const char* ddl_req =
        "CREATE TABLE IF NOT EXISTS user_friend_request ("
        "target_username VARCHAR(64) NOT NULL,"
        "requester_username VARCHAR(64) NOT NULL,"
        "requester_remark VARCHAR(128) NOT NULL DEFAULT '',"
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY(target_username, requester_username),"
        "INDEX idx_requester_username(requester_username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
    if (mysql_query(conn, ddl_req) != 0) {
      resp.error = "mysql_schema_failed";
      mysql_close(conn);
      return resp;
    }
    mysql_query(conn,
                "ALTER TABLE user_friend_request "
                "ADD COLUMN requester_remark VARCHAR(128) NOT NULL DEFAULT ''");

    const char* ddl_friend =
        "CREATE TABLE IF NOT EXISTS user_friend ("
        "username VARCHAR(64) NOT NULL,"
        "friend_username VARCHAR(64) NOT NULL,"
        "remark VARCHAR(128) NOT NULL DEFAULT '',"
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY(username, friend_username),"
        "INDEX idx_friend_username(friend_username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
    if (mysql_query(conn, ddl_friend) != 0) {
      resp.error = "mysql_schema_failed";
      mysql_close(conn);
      return resp;
    }
    mysql_query(conn,
                "ALTER TABLE user_friend "
                "ADD COLUMN remark VARCHAR(128) NOT NULL DEFAULT ''");

    // Delete the pending request (idempotent for reject). Accept requires a row.
    const char* del_q =
        "DELETE FROM user_friend_request "
        "WHERE target_username=? AND requester_username=?";
    MYSQL_STMT* del_stmt = mysql_stmt_init(conn);
    if (!del_stmt) {
      resp.error = "mysql_stmt_init failed";
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_prepare(del_stmt, del_q,
                           static_cast<unsigned long>(std::strlen(del_q))) != 0) {
      resp.error = "mysql_stmt_prepare failed";
      mysql_stmt_close(del_stmt);
      mysql_close(conn);
      return resp;
    }
    MYSQL_BIND del_param[2];
    std::memset(del_param, 0, sizeof(del_param));
    del_param[0].buffer_type = MYSQL_TYPE_STRING;
    del_param[0].buffer = const_cast<char*>(sess->username.c_str());
    del_param[0].buffer_length =
        static_cast<unsigned long>(sess->username.size());
    del_param[1].buffer_type = MYSQL_TYPE_STRING;
    del_param[1].buffer = const_cast<char*>(requester_username.c_str());
    del_param[1].buffer_length =
        static_cast<unsigned long>(requester_username.size());
    if (mysql_stmt_bind_param(del_stmt, del_param) != 0) {
      resp.error = "mysql_stmt_bind_param failed";
      mysql_stmt_close(del_stmt);
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_execute(del_stmt) != 0) {
      resp.error = "mysql_stmt_execute failed";
      mysql_stmt_close(del_stmt);
      mysql_close(conn);
      return resp;
    }
    const my_ulonglong deleted = mysql_stmt_affected_rows(del_stmt);
    mysql_stmt_close(del_stmt);

    if (!accept) {
      mysql_close(conn);
      resp.success = true;
      return resp;
    }
    if (deleted == 0) {
      mysql_close(conn);
      resp.error = "no pending request";
      return resp;
    }

    const char* ins_q =
        "INSERT IGNORE INTO user_friend(username, friend_username, remark) "
        "VALUES(?, ?, ?)";
    MYSQL_STMT* ins_stmt = mysql_stmt_init(conn);
    if (!ins_stmt) {
      resp.error = "mysql_stmt_init failed";
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_prepare(ins_stmt, ins_q,
                           static_cast<unsigned long>(std::strlen(ins_q))) != 0) {
      resp.error = "mysql_stmt_prepare failed";
      mysql_stmt_close(ins_stmt);
      mysql_close(conn);
      return resp;
    }
    auto bind_and_exec = [&](const std::string& u,
                             const std::string& f) -> bool {
      const std::string r;
      MYSQL_BIND p[3];
      std::memset(p, 0, sizeof(p));
      p[0].buffer_type = MYSQL_TYPE_STRING;
      p[0].buffer = const_cast<char*>(u.c_str());
      p[0].buffer_length = static_cast<unsigned long>(u.size());
      p[1].buffer_type = MYSQL_TYPE_STRING;
      p[1].buffer = const_cast<char*>(f.c_str());
      p[1].buffer_length = static_cast<unsigned long>(f.size());
      p[2].buffer_type = MYSQL_TYPE_STRING;
      p[2].buffer = const_cast<char*>(r.c_str());
      p[2].buffer_length = static_cast<unsigned long>(r.size());
      if (mysql_stmt_bind_param(ins_stmt, p) != 0) {
        return false;
      }
      return mysql_stmt_execute(ins_stmt) == 0;
    };

    const bool ok1 = bind_and_exec(sess->username, requester_username);
    const bool ok2 = bind_and_exec(requester_username, sess->username);
    mysql_stmt_close(ins_stmt);
    mysql_close(conn);

    if (!ok1 || !ok2) {
      resp.error = "mysql insert failed";
      return resp;
    }
    {
      std::lock_guard<std::mutex> lock(friends_mutex_);
      BumpFriendVersionLocked(sess->username);
      BumpFriendVersionLocked(requester_username);
    }
    resp.success = true;
    return resp;
  }
#endif

  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = friend_requests_by_target_.find(sess->username);
    if (it == friend_requests_by_target_.end()) {
      resp.error = "no pending request";
      return resp;
    }
    const auto it2 = it->second.find(requester_username);
    if (it2 == it->second.end()) {
      resp.error = "no pending request";
      return resp;
    }
    const std::string remark = it2->second.requester_remark;
    it->second.erase(it2);
    if (it->second.empty()) {
      friend_requests_by_target_.erase(sess->username);
    }
    if (accept) {
      friends_[sess->username].insert(requester_username);
      friends_[requester_username].insert(sess->username);
      if (!remark.empty()) {
        friend_remarks_[requester_username][sess->username] = remark;
      }
      BumpFriendVersionLocked(sess->username);
      BumpFriendVersionLocked(requester_username);
    }
  }

  resp.success = true;
  return resp;
}

FriendDeleteResponse ApiService::DeleteFriend(const std::string& token,
                                              const std::string& friend_username) {
  FriendDeleteResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("friend_delete", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (friend_username.empty()) {
    resp.error = "friend username empty";
    return resp;
  }
  if (friend_username == sess->username) {
    resp.error = "invalid friend";
    return resp;
  }

#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
      resp.error = "mysql_init failed";
      return resp;
    }
    MYSQL* res = mysql_real_connect(conn, friend_mysql_->host.c_str(),
                                    friend_mysql_->username.c_str(),
                                    friend_mysql_->password.get().c_str(),
                                    friend_mysql_->database.c_str(),
                                    friend_mysql_->port, nullptr, 0);
    if (!res) {
      resp.error = "mysql_connect failed";
      mysql_close(conn);
      return resp;
    }

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS user_friend ("
        "username VARCHAR(64) NOT NULL,"
        "friend_username VARCHAR(64) NOT NULL,"
        "remark VARCHAR(128) NOT NULL DEFAULT '',"
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY(username, friend_username),"
        "INDEX idx_friend_username(friend_username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
    if (mysql_query(conn, ddl) != 0) {
      resp.error = "mysql_schema_failed";
      mysql_close(conn);
      return resp;
    }
    mysql_query(conn,
                "ALTER TABLE user_friend "
                "ADD COLUMN remark VARCHAR(128) NOT NULL DEFAULT ''");

    const char* del_q =
        "DELETE FROM user_friend WHERE username=? AND friend_username=?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
      resp.error = "mysql_stmt_init failed";
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_prepare(stmt, del_q,
                           static_cast<unsigned long>(std::strlen(del_q))) != 0) {
      resp.error = "mysql_stmt_prepare failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }

    auto bind_and_exec = [&](const std::string& u,
                             const std::string& f) -> bool {
      MYSQL_BIND p[2];
      std::memset(p, 0, sizeof(p));
      p[0].buffer_type = MYSQL_TYPE_STRING;
      p[0].buffer = const_cast<char*>(u.c_str());
      p[0].buffer_length = static_cast<unsigned long>(u.size());
      p[1].buffer_type = MYSQL_TYPE_STRING;
      p[1].buffer = const_cast<char*>(f.c_str());
      p[1].buffer_length = static_cast<unsigned long>(f.size());
      if (mysql_stmt_bind_param(stmt, p) != 0) {
        return false;
      }
      return mysql_stmt_execute(stmt) == 0;
    };

    const bool ok1 = bind_and_exec(sess->username, friend_username);
    const bool ok2 = bind_and_exec(friend_username, sess->username);
    mysql_stmt_close(stmt);
    mysql_close(conn);
    if (!ok1 || !ok2) {
      resp.error = "mysql delete failed";
      return resp;
    }
    {
      std::lock_guard<std::mutex> lock(friends_mutex_);
      BumpFriendVersionLocked(sess->username);
      BumpFriendVersionLocked(friend_username);
    }
    resp.success = true;
    return resp;
  }
#endif

  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    bool removed = false;
    auto it = friends_.find(sess->username);
    if (it != friends_.end()) {
      if (it->second.erase(friend_username) > 0) {
        removed = true;
      }
    }
    auto it2 = friends_.find(friend_username);
    if (it2 != friends_.end()) {
      if (it2->second.erase(sess->username) > 0) {
        removed = true;
      }
    }
    auto r = friend_remarks_.find(sess->username);
    if (r != friend_remarks_.end()) {
      r->second.erase(friend_username);
    }
    auto r2 = friend_remarks_.find(friend_username);
    if (r2 != friend_remarks_.end()) {
      r2->second.erase(sess->username);
    }
    if (removed) {
      BumpFriendVersionLocked(sess->username);
      BumpFriendVersionLocked(friend_username);
    }
  }

  resp.success = true;
  return resp;
}

UserBlockSetResponse ApiService::SetUserBlocked(const std::string& token,
                                                const std::string& blocked_username,
                                                bool blocked) {
  UserBlockSetResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("user_block_set", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (blocked_username.empty()) {
    resp.error = "blocked username empty";
    return resp;
  }
  if (blocked_username == sess->username) {
    resp.error = "invalid blocked username";
    return resp;
  }

#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
      resp.error = "mysql_init failed";
      return resp;
    }
    MYSQL* res = mysql_real_connect(conn, friend_mysql_->host.c_str(),
                                    friend_mysql_->username.c_str(),
                                    friend_mysql_->password.get().c_str(),
                                    friend_mysql_->database.c_str(),
                                    friend_mysql_->port, nullptr, 0);
    if (!res) {
      resp.error = "mysql_connect failed";
      mysql_close(conn);
      return resp;
    }

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS user_block ("
        "username VARCHAR(64) NOT NULL,"
        "blocked_username VARCHAR(64) NOT NULL,"
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY(username, blocked_username),"
        "INDEX idx_blocked_username(blocked_username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
    if (mysql_query(conn, ddl) != 0) {
      resp.error = "mysql_schema_failed";
      mysql_close(conn);
      return resp;
    }

    const char* q =
        blocked ? "INSERT IGNORE INTO user_block(username, blocked_username) "
                  "VALUES(?, ?)"
                : "DELETE FROM user_block WHERE username=? AND blocked_username=?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
      resp.error = "mysql_stmt_init failed";
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_prepare(stmt, q,
                           static_cast<unsigned long>(std::strlen(q))) != 0) {
      resp.error = "mysql_stmt_prepare failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    MYSQL_BIND bind_param[2];
    std::memset(bind_param, 0, sizeof(bind_param));
    bind_param[0].buffer_type = MYSQL_TYPE_STRING;
    bind_param[0].buffer = const_cast<char*>(sess->username.c_str());
    bind_param[0].buffer_length =
        static_cast<unsigned long>(sess->username.size());
    bind_param[1].buffer_type = MYSQL_TYPE_STRING;
    bind_param[1].buffer = const_cast<char*>(blocked_username.c_str());
    bind_param[1].buffer_length =
        static_cast<unsigned long>(blocked_username.size());
    if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
      resp.error = "mysql_stmt_bind_param failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    if (mysql_stmt_execute(stmt) != 0) {
      resp.error = "mysql_stmt_execute failed";
      mysql_stmt_close(stmt);
      mysql_close(conn);
      return resp;
    }
    mysql_stmt_close(stmt);

    if (blocked) {
      // Best-effort cleanup: remove friend relation and pending requests.
      mysql_query(conn,
                  "CREATE TABLE IF NOT EXISTS user_friend ("
                  "username VARCHAR(64) NOT NULL,"
                  "friend_username VARCHAR(64) NOT NULL,"
                  "remark VARCHAR(128) NOT NULL DEFAULT '',"
                  "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                  "PRIMARY KEY(username, friend_username),"
                  "INDEX idx_friend_username(friend_username)"
                  ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin");
      mysql_query(conn,
                  "ALTER TABLE user_friend "
                  "ADD COLUMN remark VARCHAR(128) NOT NULL DEFAULT ''");
      const char* del_friend_q =
          "DELETE FROM user_friend WHERE username=? AND friend_username=?";
      MYSQL_STMT* del_friend = mysql_stmt_init(conn);
      if (del_friend &&
          mysql_stmt_prepare(
              del_friend, del_friend_q,
              static_cast<unsigned long>(std::strlen(del_friend_q))) == 0) {
        auto del_pair = [&](const std::string& u, const std::string& f) {
          MYSQL_BIND p[2];
          std::memset(p, 0, sizeof(p));
          p[0].buffer_type = MYSQL_TYPE_STRING;
          p[0].buffer = const_cast<char*>(u.c_str());
          p[0].buffer_length = static_cast<unsigned long>(u.size());
          p[1].buffer_type = MYSQL_TYPE_STRING;
          p[1].buffer = const_cast<char*>(f.c_str());
          p[1].buffer_length = static_cast<unsigned long>(f.size());
          if (mysql_stmt_bind_param(del_friend, p) == 0) {
            mysql_stmt_execute(del_friend);
          }
        };
        del_pair(sess->username, blocked_username);
        del_pair(blocked_username, sess->username);
        mysql_stmt_close(del_friend);
      } else if (del_friend) {
        mysql_stmt_close(del_friend);
      }

      mysql_query(conn,
                  "CREATE TABLE IF NOT EXISTS user_friend_request ("
                  "target_username VARCHAR(64) NOT NULL,"
                  "requester_username VARCHAR(64) NOT NULL,"
                  "requester_remark VARCHAR(128) NOT NULL DEFAULT '',"
                  "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                  "PRIMARY KEY(target_username, requester_username),"
                  "INDEX idx_requester_username(requester_username)"
                  ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin");
      mysql_query(conn,
                  "ALTER TABLE user_friend_request "
                  "ADD COLUMN requester_remark VARCHAR(128) NOT NULL DEFAULT ''");
      const char* del_req_q =
          "DELETE FROM user_friend_request WHERE target_username=? AND requester_username=?";
      MYSQL_STMT* del_req = mysql_stmt_init(conn);
      if (del_req &&
          mysql_stmt_prepare(
              del_req, del_req_q,
              static_cast<unsigned long>(std::strlen(del_req_q))) == 0) {
        auto del_pair = [&](const std::string& t, const std::string& r) {
          MYSQL_BIND p[2];
          std::memset(p, 0, sizeof(p));
          p[0].buffer_type = MYSQL_TYPE_STRING;
          p[0].buffer = const_cast<char*>(t.c_str());
          p[0].buffer_length = static_cast<unsigned long>(t.size());
          p[1].buffer_type = MYSQL_TYPE_STRING;
          p[1].buffer = const_cast<char*>(r.c_str());
          p[1].buffer_length = static_cast<unsigned long>(r.size());
          if (mysql_stmt_bind_param(del_req, p) == 0) {
            mysql_stmt_execute(del_req);
          }
        };
        del_pair(sess->username, blocked_username);
        del_pair(blocked_username, sess->username);
        mysql_stmt_close(del_req);
      } else if (del_req) {
        mysql_stmt_close(del_req);
      }
    }

    if (blocked) {
      std::lock_guard<std::mutex> lock(friends_mutex_);
      BumpFriendVersionLocked(sess->username);
      BumpFriendVersionLocked(blocked_username);
    }
    mysql_close(conn);
    resp.success = true;
    return resp;
  }
#endif

  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    if (blocked) {
      bool removed = false;
      blocks_[sess->username].insert(blocked_username);
      auto it = friends_.find(sess->username);
      if (it != friends_.end()) {
        if (it->second.erase(blocked_username) > 0) {
          removed = true;
        }
      }
      auto it2 = friends_.find(blocked_username);
      if (it2 != friends_.end()) {
        if (it2->second.erase(sess->username) > 0) {
          removed = true;
        }
      }
      auto r = friend_remarks_.find(sess->username);
      if (r != friend_remarks_.end()) {
        r->second.erase(blocked_username);
      }
      auto r2 = friend_remarks_.find(blocked_username);
      if (r2 != friend_remarks_.end()) {
        r2->second.erase(sess->username);
      }
      friend_requests_by_target_[sess->username].erase(blocked_username);
      friend_requests_by_target_[blocked_username].erase(sess->username);
      if (removed) {
        BumpFriendVersionLocked(sess->username);
        BumpFriendVersionLocked(blocked_username);
      }
    } else {
      const auto it = blocks_.find(sess->username);
      if (it != blocks_.end()) {
        it->second.erase(blocked_username);
      }
    }
  }

  resp.success = true;
  return resp;
}

namespace {

#ifdef MI_E2EE_ENABLE_MYSQL
bool AreFriendsMysql(const MySqlConfig& cfg, const std::string& username,
                     const std::string& friend_username, std::string& error) {
  error.clear();
  MYSQL* conn = ConnectMysql(cfg, error);
  if (!conn) {
    return false;
  }

  const char* ddl =
      "CREATE TABLE IF NOT EXISTS user_friend ("
      "username VARCHAR(64) NOT NULL,"
      "friend_username VARCHAR(64) NOT NULL,"
      "remark VARCHAR(128) NOT NULL DEFAULT '',"
      "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
      "PRIMARY KEY(username, friend_username),"
      "INDEX idx_friend_username(friend_username)"
      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
  if (mysql_query(conn, ddl) != 0) {
    error = "mysql_schema_failed";
    mysql_close(conn);
    return false;
  }
  mysql_query(conn,
              "ALTER TABLE user_friend "
              "ADD COLUMN remark VARCHAR(128) NOT NULL DEFAULT ''");

  const char* query =
      "SELECT 1 FROM user_friend WHERE username=? AND friend_username=? "
      "LIMIT 1";
  MYSQL_STMT* stmt = mysql_stmt_init(conn);
  if (!stmt) {
    error = "mysql_stmt_init failed";
    mysql_close(conn);
    return false;
  }
  if (mysql_stmt_prepare(stmt, query,
                         static_cast<unsigned long>(std::strlen(query))) != 0) {
    error = "mysql_stmt_prepare failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }

  MYSQL_BIND bind_param[2];
  std::memset(bind_param, 0, sizeof(bind_param));
  bind_param[0].buffer_type = MYSQL_TYPE_STRING;
  bind_param[0].buffer = const_cast<char*>(username.c_str());
  bind_param[0].buffer_length = static_cast<unsigned long>(username.size());
  bind_param[1].buffer_type = MYSQL_TYPE_STRING;
  bind_param[1].buffer = const_cast<char*>(friend_username.c_str());
  bind_param[1].buffer_length =
      static_cast<unsigned long>(friend_username.size());
  if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
    error = "mysql_stmt_bind_param failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }
  if (mysql_stmt_execute(stmt) != 0) {
    error = "mysql_stmt_execute failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }

  int value = 0;
  MYSQL_BIND bind_result[1];
  std::memset(bind_result, 0, sizeof(bind_result));
  using BindBool = std::remove_pointer_t<decltype(bind_result[0].is_null)>;
  BindBool is_null = 0;
  BindBool error_flag = 0;
  bind_result[0].buffer_type = MYSQL_TYPE_LONG;
  bind_result[0].buffer = &value;
  bind_result[0].is_null = &is_null;
  bind_result[0].error = &error_flag;
  if (mysql_stmt_bind_result(stmt, bind_result) != 0) {
    error = "mysql_stmt_bind_result failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }
  if (mysql_stmt_store_result(stmt) != 0) {
    error = "mysql_stmt_store_result failed";
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }
  const int fetch_status = mysql_stmt_fetch(stmt);
  mysql_stmt_free_result(stmt);
  mysql_stmt_close(stmt);
  mysql_close(conn);

  if (fetch_status == MYSQL_NO_DATA || is_null) {
    return false;
  }
  if (fetch_status != 0 && fetch_status != MYSQL_DATA_TRUNCATED) {
    error = "mysql_stmt_fetch failed";
    return false;
  }
  return true;
}

bool IsBlockedMysql(const MySqlConfig& cfg, const std::string& username,
                    const std::string& blocked_username, std::string& error) {
  error.clear();
  MYSQL* conn = ConnectMysql(cfg, error);
  if (!conn) {
    return false;
  }

  const char* ddl =
      "CREATE TABLE IF NOT EXISTS user_block ("
      "username VARCHAR(64) NOT NULL,"
      "blocked_username VARCHAR(64) NOT NULL,"
      "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
      "PRIMARY KEY(username, blocked_username),"
      "INDEX idx_blocked_username(blocked_username)"
      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
  if (mysql_query(conn, ddl) != 0) {
    error = "mysql_schema_failed";
    mysql_close(conn);
    return false;
  }

  const char* query =
      "SELECT 1 FROM user_block WHERE username=? AND blocked_username=? "
      "LIMIT 1";
  MYSQL_STMT* stmt = mysql_stmt_init(conn);
  if (!stmt) {
    error = "mysql_stmt_init failed";
    mysql_close(conn);
    return false;
  }
  if (mysql_stmt_prepare(stmt, query,
                         static_cast<unsigned long>(std::strlen(query))) != 0) {
    error = "mysql_stmt_prepare failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }

  MYSQL_BIND bind_param[2];
  std::memset(bind_param, 0, sizeof(bind_param));
  bind_param[0].buffer_type = MYSQL_TYPE_STRING;
  bind_param[0].buffer = const_cast<char*>(username.c_str());
  bind_param[0].buffer_length = static_cast<unsigned long>(username.size());
  bind_param[1].buffer_type = MYSQL_TYPE_STRING;
  bind_param[1].buffer = const_cast<char*>(blocked_username.c_str());
  bind_param[1].buffer_length =
      static_cast<unsigned long>(blocked_username.size());
  if (mysql_stmt_bind_param(stmt, bind_param) != 0) {
    error = "mysql_stmt_bind_param failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }
  if (mysql_stmt_execute(stmt) != 0) {
    error = "mysql_stmt_execute failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }

  int value = 0;
  MYSQL_BIND bind_result[1];
  std::memset(bind_result, 0, sizeof(bind_result));
  using BindBool = std::remove_pointer_t<decltype(bind_result[0].is_null)>;
  BindBool is_null = 0;
  BindBool error_flag = 0;
  bind_result[0].buffer_type = MYSQL_TYPE_LONG;
  bind_result[0].buffer = &value;
  bind_result[0].is_null = &is_null;
  bind_result[0].error = &error_flag;
  if (mysql_stmt_bind_result(stmt, bind_result) != 0) {
    error = "mysql_stmt_bind_result failed";
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }
  if (mysql_stmt_store_result(stmt) != 0) {
    error = "mysql_stmt_store_result failed";
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return false;
  }
  const int fetch_status = mysql_stmt_fetch(stmt);
  mysql_stmt_free_result(stmt);
  mysql_stmt_close(stmt);
  mysql_close(conn);

  if (fetch_status == MYSQL_NO_DATA || is_null) {
    return false;
  }
  if (fetch_status != 0 && fetch_status != MYSQL_DATA_TRUNCATED) {
    error = "mysql_stmt_fetch failed";
    return false;
  }
  return true;
}
#endif

}  // namespace

PreKeyPublishResponse ApiService::PublishPreKeyBundle(
    const std::string& token, std::vector<std::uint8_t> bundle) {
  PreKeyPublishResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("prekey_publish", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (bundle.empty()) {
    resp.error = "bundle empty";
    return resp;
  }
  if (bundle.size() > (16u * 1024u)) {
    resp.error = "bundle too large";
    return resp;
  }

  if (kt_log_) {
    if (bundle.size() < 1 + kKtIdentitySigPublicKeyBytes + kKtIdentityDhPublicKeyBytes) {
      resp.error = "bundle invalid";
      return resp;
    }
    std::array<std::uint8_t, kKtIdentitySigPublicKeyBytes> id_sig_pk{};
    std::array<std::uint8_t, kKtIdentityDhPublicKeyBytes> id_dh_pk{};
    std::memcpy(id_sig_pk.data(), bundle.data() + 1, id_sig_pk.size());
    std::memcpy(id_dh_pk.data(), bundle.data() + 1 + id_sig_pk.size(),
                id_dh_pk.size());
    std::string kt_err;
    if (!kt_log_->UpdateIdentityKeys(sess->username, id_sig_pk, id_dh_pk,
                                    kt_err)) {
      resp.error = kt_err.empty() ? "kt update failed" : kt_err;
      return resp;
    }
  }

  {
    std::lock_guard<std::mutex> lock(prekeys_mutex_);
    prekey_bundles_[sess->username] = std::move(bundle);
  }
  resp.success = true;
  return resp;
}

PreKeyFetchResponse ApiService::FetchPreKeyBundle(
    const std::string& token, const std::string& friend_username,
    std::uint64_t client_kt_tree_size) {
  PreKeyFetchResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("prekey_fetch", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (friend_username.empty()) {
    resp.error = "friend username empty";
    return resp;
  }
  if (friend_username == sess->username) {
    resp.error = "invalid friend";
    return resp;
  }

  bool is_friend = false;
#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    std::string err;
    is_friend = AreFriendsMysql(*friend_mysql_, sess->username, friend_username,
                                err);
    if (!err.empty()) {
      resp.error = err;
      return resp;
    }
  } else {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    auto it = friends_.find(sess->username);
    is_friend = (it != friends_.end() &&
                 it->second.find(friend_username) != it->second.end());
  }
#else
  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    auto it = friends_.find(sess->username);
    is_friend = (it != friends_.end() &&
                 it->second.find(friend_username) != it->second.end());
  }
#endif

  if (!is_friend) {
    resp.error = "not friends";
    return resp;
  }

  {
    std::lock_guard<std::mutex> lock(prekeys_mutex_);
    const auto it = prekey_bundles_.find(friend_username);
    if (it == prekey_bundles_.end()) {
      resp.error = "prekey not found";
      return resp;
    }
    resp.bundle = it->second;
  }

  if (kt_log_) {
    KeyTransparencyProof proof;
    std::string kt_err;
    if (!kt_log_->BuildProofForLatestKey(friend_username, client_kt_tree_size,
                                         proof, kt_err)) {
      resp.error = kt_err.empty() ? "kt proof failed" : kt_err;
      return resp;
    }
    resp.kt_version = 1;
    resp.kt_tree_size = proof.sth.tree_size;
    resp.kt_root = proof.sth.root;
    KeyTransparencySth sth = proof.sth;
    std::string sign_err;
    if (!SignKtSth(sth, sign_err)) {
      resp.error = sign_err.empty() ? "kt sign failed" : sign_err;
      return resp;
    }
    resp.kt_signature = std::move(sth.signature);
    resp.kt_leaf_index = proof.leaf_index;
    resp.kt_audit_path = std::move(proof.audit_path);
    resp.kt_consistency_path = std::move(proof.consistency_path);
  }

  resp.success = true;
  return resp;
}

KeyTransparencyHeadResponse ApiService::GetKeyTransparencyHead(
    const std::string& token) {
  KeyTransparencyHeadResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("kt_head", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (!kt_log_) {
    resp.error = "kt disabled";
    return resp;
  }
  resp.sth = kt_log_->Head();
  std::string sign_err;
  if (!SignKtSth(resp.sth, sign_err)) {
    resp.error = sign_err.empty() ? "kt sign failed" : sign_err;
    return resp;
  }
  resp.success = true;
  return resp;
}

KeyTransparencyConsistencyResponse ApiService::GetKeyTransparencyConsistency(
    const std::string& token, std::uint64_t old_size, std::uint64_t new_size) {
  KeyTransparencyConsistencyResponse resp;
  resp.old_size = old_size;
  resp.new_size = new_size;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("kt_consistency", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (!kt_log_) {
    resp.error = "kt disabled";
    return resp;
  }
  std::string err;
  if (!kt_log_->BuildConsistencyProof(old_size, new_size, resp.proof, err)) {
    resp.error = err.empty() ? "kt consistency failed" : err;
    return resp;
  }
  resp.success = true;
  return resp;
}

PrivateSendResponse ApiService::SendPrivate(const std::string& token,
                                            const std::string& recipient,
                                            std::vector<std::uint8_t> payload) {
  PrivateSendResponse resp;
  if (!sessions_ || !queue_) {
    resp.error = "queue unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("private_send", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (recipient.empty()) {
    resp.error = "recipient empty";
    return resp;
  }
  if (recipient == sess->username) {
    resp.error = "invalid recipient";
    return resp;
  }
  if (payload.empty()) {
    resp.error = "payload empty";
    return resp;
  }
  if (payload.size() > (256u * 1024u)) {
    resp.error = "payload too large";
    return resp;
  }

  {
    std::string exists_err;
    if (!sessions_->UserExists(recipient, exists_err)) {
      resp.error = exists_err.empty() ? "recipient not found" : exists_err;
      return resp;
    }
  }

  bool blocked = false;
#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    std::string block_err;
    const bool recipient_blocks_sender =
        IsBlockedMysql(*friend_mysql_, recipient, sess->username, block_err);
    if (!block_err.empty()) {
      resp.error = block_err;
      return resp;
    }
    std::string block_err2;
    const bool sender_blocks_recipient =
        IsBlockedMysql(*friend_mysql_, sess->username, recipient, block_err2);
    if (!block_err2.empty()) {
      resp.error = block_err2;
      return resp;
    }
    blocked = recipient_blocks_sender || sender_blocks_recipient;
  } else {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = blocks_.find(sess->username);
    if (it != blocks_.end() && it->second.count(recipient) != 0) {
      blocked = true;
    }
    const auto it2 = blocks_.find(recipient);
    if (it2 != blocks_.end() && it2->second.count(sess->username) != 0) {
      blocked = true;
    }
  }
#else
  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = blocks_.find(sess->username);
    if (it != blocks_.end() && it->second.count(recipient) != 0) {
      blocked = true;
    }
    const auto it2 = blocks_.find(recipient);
    if (it2 != blocks_.end() && it2->second.count(sess->username) != 0) {
      blocked = true;
    }
  }
#endif

  if (blocked) {
    resp.success = true;
    return resp;
  }

  bool is_friend = false;
#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    std::string err;
    is_friend = AreFriendsMysql(*friend_mysql_, sess->username, recipient, err);
    if (!err.empty()) {
      resp.error = err;
      return resp;
    }
  } else {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = friends_.find(sess->username);
    is_friend = (it != friends_.end() && it->second.count(recipient) != 0);
  }
#else
  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = friends_.find(sess->username);
    is_friend = (it != friends_.end() && it->second.count(recipient) != 0);
  }
#endif

  if (!is_friend) {
    resp.error = "not friends";
    return resp;
  }

  queue_->EnqueuePrivate(recipient, sess->username, std::move(payload));
  resp.success = true;
  return resp;
}

MediaPushResponse ApiService::PushMedia(
    const std::string& token, const std::string& recipient,
    const std::array<std::uint8_t, 16>& call_id,
    std::vector<std::uint8_t> payload) {
  MediaPushResponse resp;
  if (!sessions_ || !media_relay_) {
    resp.error = "media relay unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("media_push", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (recipient.empty()) {
    resp.error = "recipient empty";
    return resp;
  }
  if (recipient == sess->username) {
    resp.error = "invalid recipient";
    return resp;
  }
  if (payload.empty()) {
    resp.error = "payload empty";
    return resp;
  }
  if (payload.size() > (512u * 1024u)) {
    resp.error = "payload too large";
    return resp;
  }

  {
    std::string exists_err;
    if (!sessions_->UserExists(recipient, exists_err)) {
      resp.error = exists_err.empty() ? "recipient not found" : exists_err;
      return resp;
    }
  }

  bool blocked = false;
#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    std::string block_err;
    const bool recipient_blocks_sender =
        IsBlockedMysql(*friend_mysql_, recipient, sess->username, block_err);
    if (!block_err.empty()) {
      resp.error = block_err;
      return resp;
    }
    std::string block_err2;
    const bool sender_blocks_recipient =
        IsBlockedMysql(*friend_mysql_, sess->username, recipient, block_err2);
    if (!block_err2.empty()) {
      resp.error = block_err2;
      return resp;
    }
    blocked = recipient_blocks_sender || sender_blocks_recipient;
  } else {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = blocks_.find(sess->username);
    if (it != blocks_.end() && it->second.count(recipient) != 0) {
      blocked = true;
    }
    const auto it2 = blocks_.find(recipient);
    if (it2 != blocks_.end() && it2->second.count(sess->username) != 0) {
      blocked = true;
    }
  }
#else
  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = blocks_.find(sess->username);
    if (it != blocks_.end() && it->second.count(recipient) != 0) {
      blocked = true;
    }
    const auto it2 = blocks_.find(recipient);
    if (it2 != blocks_.end() && it2->second.count(sess->username) != 0) {
      blocked = true;
    }
  }
#endif

  if (blocked) {
    resp.success = true;
    return resp;
  }

  bool is_friend = false;
#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    std::string err;
    is_friend = AreFriendsMysql(*friend_mysql_, sess->username, recipient, err);
    if (!err.empty()) {
      resp.error = err;
      return resp;
    }
  } else {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = friends_.find(sess->username);
    is_friend = (it != friends_.end() && it->second.count(recipient) != 0);
  }
#else
  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = friends_.find(sess->username);
    is_friend = (it != friends_.end() && it->second.count(recipient) != 0);
  }
#endif

  if (!is_friend) {
    resp.error = "not friends";
    return resp;
  }

  MediaRelayPacket packet;
  packet.sender = sess->username;
  packet.payload = std::move(payload);
  media_relay_->Enqueue(recipient, call_id, std::move(packet));
  resp.success = true;
  return resp;
}

MediaPullResponse ApiService::PullMedia(const std::string& token,
                                        const std::array<std::uint8_t, 16>& call_id,
                                        std::uint32_t max_packets,
                                        std::uint32_t wait_ms) {
  MediaPullResponse resp;
  if (!sessions_ || !media_relay_) {
    resp.error = "media relay unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("media_pull", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (max_packets == 0) {
    max_packets = 1;
  } else if (max_packets > 256) {
    max_packets = 256;
  }
  if (wait_ms > 1000) {
    wait_ms = 1000;
  }

  std::vector<MediaRelayPacket> pulled;
  media_relay_->Pull(sess->username, call_id, max_packets,
                     std::chrono::milliseconds(wait_ms), pulled);
  resp.success = true;
  resp.packets.reserve(pulled.size());
  for (auto& pkt : pulled) {
    MediaPullResponse::Entry entry;
    entry.sender = std::move(pkt.sender);
    entry.payload = std::move(pkt.payload);
    resp.packets.push_back(std::move(entry));
  }
  return resp;
}

GroupCallSignalResponse ApiService::GroupCallSignal(
    const std::string& token, std::uint8_t op, const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id, std::uint8_t media_flags,
    std::uint32_t key_id, std::uint32_t seq, std::uint64_t ts_ms,
    std::vector<std::uint8_t> ext) {
  (void)key_id;
  (void)seq;
  GroupCallSignalResponse resp;
  if (!sessions_ || !directory_ || !calls_) {
    resp.error = "group call unavailable";
    return resp;
  }
  if (!calls_->enabled()) {
    resp.error = "group call disabled";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("group_call_signal", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (group_id.empty()) {
    resp.error = "group id empty";
    return resp;
  }
  if (!directory_->HasMember(group_id, sess->username)) {
    resp.error = "not in group";
    return resp;
  }

  std::vector<GroupCallSubscription> subscriptions;
  const bool has_subscriptions = !ext.empty();
  if (has_subscriptions) {
    if (!DecodeGroupCallSubscriptions(ext, subscriptions, resp.error)) {
      return resp;
    }
  }

  GroupCallSnapshot snapshot;
  std::array<std::uint8_t, 16> call_id_mut = call_id;
  const auto op_enum = static_cast<GroupCallOp>(op);
  if (op_enum == GroupCallOp::kCreate) {
    if (!calls_->CreateCall(group_id, sess->username, media_flags, call_id_mut,
                            snapshot, resp.error)) {
      return resp;
    }
    if (has_subscriptions) {
      if (!calls_->UpdateSubscriptions(call_id_mut, sess->username,
                                       subscriptions, resp.error)) {
        return resp;
      }
    }
    resp.call_id = call_id_mut;
    resp.key_id = snapshot.key_id;
    resp.members = snapshot.members;
    resp.success = true;

    GroupCallEvent ev;
    ev.op = GroupCallOp::kCreate;
    ev.group_id = group_id;
    ev.call_id = call_id_mut;
    ev.key_id = snapshot.key_id;
    ev.sender = sess->username;
    ev.media_flags = media_flags;
    ev.ts_ms = ts_ms;
    calls_->EnqueueEventForMembers(directory_->Members(group_id), ev);
    return resp;
  }

  if (op_enum == GroupCallOp::kJoin) {
    if (!calls_->JoinCall(group_id, call_id, sess->username, media_flags,
                          snapshot, resp.error)) {
      return resp;
    }
    if (has_subscriptions) {
      if (!calls_->UpdateSubscriptions(snapshot.call_id, sess->username,
                                       subscriptions, resp.error)) {
        return resp;
      }
    }
    resp.call_id = snapshot.call_id;
    resp.key_id = snapshot.key_id;
    resp.members = snapshot.members;
    resp.success = true;

    GroupCallEvent ev;
    ev.op = GroupCallOp::kJoin;
    ev.group_id = group_id;
    ev.call_id = snapshot.call_id;
    ev.key_id = snapshot.key_id;
    ev.sender = sess->username;
    ev.media_flags = media_flags;
    ev.ts_ms = ts_ms;
    calls_->EnqueueEventForMembers(snapshot.members, ev);
    return resp;
  }

  if (op_enum == GroupCallOp::kLeave) {
    bool ended = false;
    if (!calls_->LeaveCall(group_id, call_id, sess->username, snapshot, ended,
                           resp.error)) {
      return resp;
    }
    resp.call_id = snapshot.call_id;
    resp.key_id = snapshot.key_id;
    resp.members = snapshot.members;
    resp.success = true;

    GroupCallEvent ev;
    ev.op = ended ? GroupCallOp::kEnd : GroupCallOp::kLeave;
    ev.group_id = group_id;
    ev.call_id = snapshot.call_id;
    ev.key_id = snapshot.key_id;
    ev.sender = sess->username;
    ev.media_flags = media_flags;
    ev.ts_ms = ts_ms;
    calls_->EnqueueEventForMembers(snapshot.members, ev);
    return resp;
  }

  if (op_enum == GroupCallOp::kEnd) {
    if (!calls_->EndCall(group_id, call_id, sess->username, snapshot,
                         resp.error)) {
      return resp;
    }
    resp.call_id = snapshot.call_id;
    resp.key_id = snapshot.key_id;
    resp.members = snapshot.members;
    resp.success = true;

    GroupCallEvent ev;
    ev.op = GroupCallOp::kEnd;
    ev.group_id = group_id;
    ev.call_id = snapshot.call_id;
    ev.key_id = snapshot.key_id;
    ev.sender = sess->username;
    ev.media_flags = media_flags;
    ev.ts_ms = ts_ms;
    calls_->EnqueueEventForMembers(snapshot.members, ev);
    return resp;
  }

  if (op_enum == GroupCallOp::kUpdate || op_enum == GroupCallOp::kPing) {
    if (!calls_->TouchCall(call_id, sess->username, snapshot, resp.error)) {
      return resp;
    }
    if (snapshot.group_id != group_id) {
      resp.error = "call mismatch";
      return resp;
    }
    if (has_subscriptions) {
      if (!calls_->UpdateSubscriptions(snapshot.call_id, sess->username,
                                       subscriptions, resp.error)) {
        return resp;
      }
    }
    resp.call_id = snapshot.call_id;
    resp.key_id = snapshot.key_id;
    resp.members = snapshot.members;
    resp.success = true;

    if (op_enum == GroupCallOp::kUpdate) {
      GroupCallEvent ev;
      ev.op = GroupCallOp::kUpdate;
      ev.group_id = group_id;
      ev.call_id = snapshot.call_id;
      ev.key_id = snapshot.key_id;
      ev.sender = sess->username;
      ev.media_flags = media_flags;
      ev.ts_ms = ts_ms;
      calls_->EnqueueEventForMembers(snapshot.members, ev);
    }
    return resp;
  }

  resp.error = "unknown op";
  return resp;
}

GroupCallSignalPullResponse ApiService::PullGroupCallSignals(
    const std::string& token, std::uint32_t max_events,
    std::uint32_t wait_ms) {
  GroupCallSignalPullResponse resp;
  if (!sessions_ || !calls_) {
    resp.error = "group call unavailable";
    return resp;
  }
  if (!calls_->enabled()) {
    resp.error = "group call disabled";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("group_call_signal_pull", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (max_events == 0) {
    max_events = 1;
  } else if (max_events > 256) {
    max_events = 256;
  }
  if (wait_ms > 1000) {
    wait_ms = 1000;
  }
  std::vector<GroupCallEvent> events;
  calls_->PullEvents(sess->username, max_events,
                     std::chrono::milliseconds(wait_ms), events);
  resp.success = true;
  resp.events.reserve(events.size());
  for (const auto& ev : events) {
    GroupCallSignalPullResponse::Entry entry;
    entry.op = static_cast<std::uint8_t>(ev.op);
    entry.group_id = ev.group_id;
    entry.call_id = ev.call_id;
    entry.key_id = ev.key_id;
    entry.sender = ev.sender;
    entry.media_flags = ev.media_flags;
    entry.ts_ms = ev.ts_ms;
    resp.events.push_back(std::move(entry));
  }
  return resp;
}

MediaPushResponse ApiService::PushGroupMedia(
    const std::string& token, const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::vector<std::uint8_t> payload) {
  MediaPushResponse resp;
  if (!sessions_ || !media_relay_ || !calls_ || !directory_) {
    resp.error = "media relay unavailable";
    return resp;
  }
  if (!calls_->enabled()) {
    resp.error = "group call disabled";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("group_media_push", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (group_id.empty()) {
    resp.error = "group id empty";
    return resp;
  }
  if (!directory_->HasMember(group_id, sess->username)) {
    resp.error = "not in group";
    return resp;
  }
  if (payload.empty()) {
    resp.error = "payload empty";
    return resp;
  }

  GroupCallSnapshot snapshot;
  if (!calls_->GetCall(call_id, snapshot) || snapshot.group_id != group_id) {
    resp.error = "call not found";
    return resp;
  }
  if (std::find(snapshot.members.begin(), snapshot.members.end(),
                sess->username) == snapshot.members.end()) {
    resp.error = "not in call";
    return resp;
  }

  std::uint8_t kind_flag = 0;
  if (!PeekMediaPacketKindFlag(payload, kind_flag)) {
    resp.error = "media packet invalid";
    return resp;
  }

  std::vector<std::string> recipients;
  recipients.reserve(snapshot.members.size());
  for (const auto& member : snapshot.members) {
    if (member == sess->username) {
      continue;
    }
    if (!calls_->IsSubscribed(call_id, member, sess->username, kind_flag)) {
      continue;
    }
    recipients.push_back(member);
  }
  if (!recipients.empty()) {
    MediaRelayPacket packet;
    packet.sender = sess->username;
    packet.payload = payload;
    media_relay_->EnqueueMany(recipients, call_id, packet);
  }
  resp.success = true;
  return resp;
}

MediaPullResponse ApiService::PullGroupMedia(
    const std::string& token, const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t max_packets, std::uint32_t wait_ms) {
  MediaPullResponse resp;
  if (!sessions_ || !media_relay_ || !calls_) {
    resp.error = "media relay unavailable";
    return resp;
  }
  if (!calls_->enabled()) {
    resp.error = "group call disabled";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("group_media_pull", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (max_packets == 0) {
    max_packets = 1;
  } else if (max_packets > 256) {
    max_packets = 256;
  }
  if (wait_ms > 1000) {
    wait_ms = 1000;
  }

  GroupCallSnapshot snapshot;
  if (!calls_->GetCall(call_id, snapshot)) {
    resp.error = "call not found";
    return resp;
  }
  if (std::find(snapshot.members.begin(), snapshot.members.end(),
                sess->username) == snapshot.members.end()) {
    resp.error = "not in call";
    return resp;
  }

  std::vector<MediaRelayPacket> pulled;
  media_relay_->Pull(sess->username, call_id, max_packets,
                     std::chrono::milliseconds(wait_ms), pulled);
  resp.success = true;
  resp.packets.reserve(pulled.size());
  for (auto& pkt : pulled) {
    MediaPullResponse::Entry entry;
    entry.sender = std::move(pkt.sender);
    entry.payload = std::move(pkt.payload);
    resp.packets.push_back(std::move(entry));
  }
  return resp;
}

GroupSenderKeySendResponse ApiService::SendGroupSenderKey(
    const std::string& token, const std::string& group_id,
    const std::string& recipient, std::vector<std::uint8_t> payload) {
  GroupSenderKeySendResponse resp;
  if (!sessions_ || !queue_ || !directory_) {
    resp.error = "queue unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("group_sender_key_send", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (group_id.empty()) {
    resp.error = "group id empty";
    return resp;
  }
  if (recipient.empty()) {
    resp.error = "recipient empty";
    return resp;
  }
  if (!directory_->HasMember(group_id, sess->username) ||
      !directory_->HasMember(group_id, recipient)) {
    resp.error = "not in group";
    return resp;
  }
  if (payload.empty()) {
    resp.error = "payload empty";
    return resp;
  }
  if (payload.size() > (256u * 1024u)) {
    resp.error = "payload too large";
    return resp;
  }

  bool blocked = false;
#ifdef MI_E2EE_ENABLE_MYSQL
  if (friend_mysql_.has_value()) {
    std::string block_err;
    const bool recipient_blocks_sender =
        IsBlockedMysql(*friend_mysql_, recipient, sess->username, block_err);
    if (!block_err.empty()) {
      resp.error = block_err;
      return resp;
    }
    std::string block_err2;
    const bool sender_blocks_recipient =
        IsBlockedMysql(*friend_mysql_, sess->username, recipient, block_err2);
    if (!block_err2.empty()) {
      resp.error = block_err2;
      return resp;
    }
    blocked = recipient_blocks_sender || sender_blocks_recipient;
  } else {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = blocks_.find(sess->username);
    if (it != blocks_.end() && it->second.count(recipient) != 0) {
      blocked = true;
    }
    const auto it2 = blocks_.find(recipient);
    if (it2 != blocks_.end() && it2->second.count(sess->username) != 0) {
      blocked = true;
    }
  }
#else
  {
    std::lock_guard<std::mutex> lock(friends_mutex_);
    const auto it = blocks_.find(sess->username);
    if (it != blocks_.end() && it->second.count(recipient) != 0) {
      blocked = true;
    }
    const auto it2 = blocks_.find(recipient);
    if (it2 != blocks_.end() && it2->second.count(sess->username) != 0) {
      blocked = true;
    }
  }
#endif

  if (!blocked) {
    queue_->EnqueuePrivate(recipient, sess->username, std::move(payload));
  }
  resp.success = true;
  return resp;
}

PrivatePullResponse ApiService::PullPrivate(const std::string& token) {
  PrivatePullResponse resp;
  if (!sessions_ || !queue_) {
    resp.error = "queue unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("private_pull", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }

  const auto messages = queue_->DrainPrivate(sess->username);
  resp.messages.reserve(messages.size());
  for (const auto& m : messages) {
    PrivatePullResponse::Entry e;
    e.sender = m.sender;
    e.payload = m.payload;
    resp.messages.push_back(std::move(e));
  }
  resp.success = true;
  return resp;
}

GroupCipherSendResponse ApiService::SendGroupCipher(
    const std::string& token, const std::string& group_id,
    std::vector<std::uint8_t> payload) {
  GroupCipherSendResponse resp;
  if (!sessions_ || !queue_ || !directory_) {
    resp.error = "queue unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("group_cipher_send", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (group_id.empty()) {
    resp.error = "group id empty";
    return resp;
  }
  if (!directory_->HasMember(group_id, sess->username)) {
    resp.error = "not in group";
    return resp;
  }
  if (payload.empty()) {
    resp.error = "payload empty";
    return resp;
  }
  if (payload.size() > (256u * 1024u)) {
    resp.error = "payload too large";
    return resp;
  }

  const auto members = directory_->Members(group_id);
  for (const auto& recipient : members) {
    if (recipient.empty() || recipient == sess->username) {
      continue;
    }

    bool blocked = false;
#ifdef MI_E2EE_ENABLE_MYSQL
    if (friend_mysql_.has_value()) {
      std::string block_err;
      const bool recipient_blocks_sender =
          IsBlockedMysql(*friend_mysql_, recipient, sess->username, block_err);
      if (!block_err.empty()) {
        resp.error = block_err;
        return resp;
      }
      std::string block_err2;
      const bool sender_blocks_recipient =
          IsBlockedMysql(*friend_mysql_, sess->username, recipient, block_err2);
      if (!block_err2.empty()) {
        resp.error = block_err2;
        return resp;
      }
      blocked = recipient_blocks_sender || sender_blocks_recipient;
    } else {
      std::lock_guard<std::mutex> lock(friends_mutex_);
      const auto it = blocks_.find(sess->username);
      if (it != blocks_.end() && it->second.count(recipient) != 0) {
        blocked = true;
      }
      const auto it2 = blocks_.find(recipient);
      if (it2 != blocks_.end() && it2->second.count(sess->username) != 0) {
        blocked = true;
      }
    }
#else
    {
      std::lock_guard<std::mutex> lock(friends_mutex_);
      const auto it = blocks_.find(sess->username);
      if (it != blocks_.end() && it->second.count(recipient) != 0) {
        blocked = true;
      }
      const auto it2 = blocks_.find(recipient);
      if (it2 != blocks_.end() && it2->second.count(sess->username) != 0) {
        blocked = true;
      }
    }
#endif

    if (blocked) {
      continue;
    }

    queue_->EnqueueGroupCipher(recipient, group_id, sess->username, payload);
  }

  resp.success = true;
  return resp;
}

GroupCipherPullResponse ApiService::PullGroupCipher(const std::string& token) {
  GroupCipherPullResponse resp;
  if (!sessions_ || !queue_) {
    resp.error = "queue unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("group_cipher_pull", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }

  const auto messages = queue_->DrainGroupCipher(sess->username);
  resp.messages.reserve(messages.size());
  for (const auto& m : messages) {
    if (!m.group_id.empty() && directory_ &&
        !directory_->HasMember(m.group_id, sess->username)) {
      continue;
    }
    GroupCipherPullResponse::Entry e;
    e.group_id = m.group_id;
    e.sender = m.sender;
    e.payload = m.payload;
    resp.messages.push_back(std::move(e));
  }
  resp.success = true;
  return resp;
}

GroupNoticePullResponse ApiService::PullGroupNotices(const std::string& token) {
  GroupNoticePullResponse resp;
  if (!sessions_ || !queue_) {
    resp.error = "queue unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("group_notice_pull", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }

  const auto messages = queue_->DrainGroupNotice(sess->username);
  resp.notices.reserve(messages.size());
  for (const auto& m : messages) {
    if (!m.group_id.empty() && directory_ &&
        !directory_->HasMember(m.group_id, sess->username)) {
      if (m.payload.empty()) {
        continue;
      }
      std::size_t off = 0;
      const std::uint8_t kind = m.payload[off++];
      std::string target;
      if (!mi::server::proto::ReadString(m.payload, off, target) ||
          off != m.payload.size()) {
        continue;
      }
      const bool is_membership_removal =
          (kind == kGroupNoticeLeave || kind == kGroupNoticeKick);
      if (!is_membership_removal || target != sess->username) {
        continue;
      }
    }
    GroupNoticePullResponse::Entry e;
    e.group_id = m.group_id;
    e.sender = m.sender;
    e.payload = m.payload;
    resp.notices.push_back(std::move(e));
  }
  resp.success = true;
  return resp;
}

namespace {
bool LooksLikeHexId(const std::string& s, std::size_t expect_len) {
  if (expect_len != 0 && s.size() != expect_len) {
    return false;
  }
  if (s.empty()) {
    return false;
  }
  for (const char c : s) {
    const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                    (c >= 'A' && c <= 'F');
    if (!ok) {
      return false;
    }
  }
  return true;
}

std::string MakeDeviceQueueKey(const std::string& username,
                              const std::string& device_id) {
  return username + "|" + device_id;
}

std::string MakePairingRequestQueueKey(const std::string& username,
                                      const std::string& pairing_id_hex) {
  return "pair_req|" + username + "|" + pairing_id_hex;
}

std::string MakePairingResponseQueueKey(const std::string& username,
                                       const std::string& pairing_id_hex,
                                       const std::string& device_id) {
  return "pair_resp|" + username + "|" + pairing_id_hex + "|" + device_id;
}
}  // namespace

DeviceSyncPushResponse ApiService::PushDeviceSync(
    const std::string& token, const std::string& device_id,
    std::vector<std::uint8_t> payload) {
  DeviceSyncPushResponse resp;
  if (!sessions_ || !queue_) {
    resp.error = "queue unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("device_sync_push", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (device_id.empty()) {
    resp.error = "device id empty";
    return resp;
  }
  if (!LooksLikeHexId(device_id, 32) && device_id.size() > 64) {
    resp.error = "device id invalid";
    return resp;
  }
  if (payload.empty()) {
    resp.error = "payload empty";
    return resp;
  }
  if (payload.size() > (256u * 1024u)) {
    resp.error = "payload too large";
    return resp;
  }

  std::vector<std::string> targets;
  {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(devices_mutex_);
    auto& map = devices_by_user_[sess->username];
    auto it = map.find(device_id);
    if (it == map.end()) {
      if (map.size() < 64) {
        DeviceRecord rec;
        rec.last_seen = now;
        rec.last_token = sess->token;
        map.emplace(device_id, std::move(rec));
      }
    } else {
      it->second.last_seen = now;
      it->second.last_token = sess->token;
    }

    targets.reserve(map.size());
    for (const auto& kv : map) {
      if (kv.first != device_id) {
        targets.push_back(kv.first);
      }
    }
  }

  for (const auto& d : targets) {
    queue_->EnqueueDeviceSync(MakeDeviceQueueKey(sess->username, d), payload);
  }

  resp.success = true;
  return resp;
}

DeviceSyncPullResponse ApiService::PullDeviceSync(const std::string& token,
                                                  const std::string& device_id) {
  DeviceSyncPullResponse resp;
  if (!sessions_ || !queue_) {
    resp.error = "queue unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("device_sync_pull", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (device_id.empty()) {
    resp.error = "device id empty";
    return resp;
  }
  if (!LooksLikeHexId(device_id, 32) && device_id.size() > 64) {
    resp.error = "device id invalid";
    return resp;
  }

  {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(devices_mutex_);
    auto& map = devices_by_user_[sess->username];
    auto it = map.find(device_id);
    if (it == map.end()) {
      if (map.size() < 64) {
        DeviceRecord rec;
        rec.last_seen = now;
        rec.last_token = sess->token;
        map.emplace(device_id, std::move(rec));
      }
    } else {
      it->second.last_seen = now;
      it->second.last_token = sess->token;
    }
  }

  resp.messages = queue_->DrainDeviceSync(MakeDeviceQueueKey(sess->username, device_id));
  resp.success = true;
  return resp;
}

DeviceListResponse ApiService::ListDevices(const std::string& token,
                                           const std::string& device_id) {
  DeviceListResponse resp;
  if (!sessions_) {
    resp.error = "session manager unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("device_list", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (device_id.empty()) {
    resp.error = "device id empty";
    return resp;
  }
  if (!LooksLikeHexId(device_id, 32) && device_id.size() > 64) {
    resp.error = "device id invalid";
    return resp;
  }

  const auto now = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    auto& map = devices_by_user_[sess->username];
    auto it = map.find(device_id);
    if (it == map.end()) {
      if (map.size() < 64) {
        DeviceRecord rec;
        rec.last_seen = now;
        rec.last_token = sess->token;
        map.emplace(device_id, std::move(rec));
      }
    } else {
      it->second.last_seen = now;
      it->second.last_token = sess->token;
    }

    resp.devices.reserve(map.size());
    for (const auto& kv : map) {
      DeviceListResponse::Entry e;
      e.device_id = kv.first;
      const auto seen = kv.second.last_seen;
      if (seen.time_since_epoch().count() == 0) {
        e.last_seen_sec = 0;
      } else if (now >= seen) {
        const auto age = now - seen;
        const auto sec = std::chrono::duration_cast<std::chrono::seconds>(age).count();
        e.last_seen_sec =
            sec < 0 ? 0 : (sec > std::numeric_limits<std::uint32_t>::max()
                                ? std::numeric_limits<std::uint32_t>::max()
                                : static_cast<std::uint32_t>(sec));
      } else {
        e.last_seen_sec = 0;
      }
      resp.devices.push_back(std::move(e));
    }
  }

  std::sort(resp.devices.begin(), resp.devices.end(),
            [](const DeviceListResponse::Entry& a,
               const DeviceListResponse::Entry& b) { return a.device_id < b.device_id; });
  resp.success = true;
  return resp;
}

DeviceKickResponse ApiService::KickDevice(const std::string& token,
                                          const std::string& requester_device_id,
                                          const std::string& target_device_id) {
  DeviceKickResponse resp;
  if (!sessions_ || !queue_) {
    resp.error = "queue unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("device_kick", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (requester_device_id.empty() || target_device_id.empty()) {
    resp.error = "device id empty";
    return resp;
  }
  if (!LooksLikeHexId(requester_device_id, 32) && requester_device_id.size() > 64) {
    resp.error = "device id invalid";
    return resp;
  }
  if (!LooksLikeHexId(target_device_id, 32) && target_device_id.size() > 64) {
    resp.error = "device id invalid";
    return resp;
  }
  if (requester_device_id == target_device_id) {
    resp.error = "cannot kick self";
    return resp;
  }

  std::string token_to_logout;
  {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(devices_mutex_);
    auto& map = devices_by_user_[sess->username];
    auto it_req = map.find(requester_device_id);
    if (it_req == map.end()) {
      if (map.size() < 64) {
        DeviceRecord rec;
        rec.last_seen = now;
        rec.last_token = sess->token;
        it_req = map.emplace(requester_device_id, std::move(rec)).first;
      }
    }
    if (it_req == map.end()) {
      resp.error = "device not found";
      return resp;
    }
    it_req->second.last_seen = now;
    it_req->second.last_token = sess->token;

    const auto it_dev = map.find(target_device_id);
    if (it_dev == map.end()) {
      resp.error = "device not found";
      return resp;
    }
    token_to_logout = it_dev->second.last_token;
    map.erase(it_dev);
    if (map.empty()) {
      devices_by_user_.erase(sess->username);
    }
  }

  if (!token_to_logout.empty()) {
    sessions_->Logout(token_to_logout);
  }
  queue_->DrainDeviceSync(MakeDeviceQueueKey(sess->username, target_device_id));
  resp.success = true;
  return resp;
}

DevicePairingPushResponse ApiService::PushDevicePairingRequest(
    const std::string& token, const std::string& pairing_id_hex,
    std::vector<std::uint8_t> payload) {
  DevicePairingPushResponse resp;
  if (!sessions_ || !queue_) {
    resp.error = "queue unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("pairing_request", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (!LooksLikeHexId(pairing_id_hex, 32)) {
    resp.error = "pairing id invalid";
    return resp;
  }
  if (payload.empty()) {
    resp.error = "payload empty";
    return resp;
  }
  if (payload.size() > (16u * 1024u)) {
    resp.error = "payload too large";
    return resp;
  }

  queue_->Enqueue(MakePairingRequestQueueKey(sess->username, pairing_id_hex),
                  std::move(payload), std::chrono::minutes(10));
  resp.success = true;
  return resp;
}

DevicePairingPullResponse ApiService::PullDevicePairing(
    const std::string& token, std::uint8_t mode, const std::string& pairing_id_hex,
    const std::string& device_id) {
  DevicePairingPullResponse resp;
  if (!sessions_ || !queue_) {
    resp.error = "queue unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("pairing_pull", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (!LooksLikeHexId(pairing_id_hex, 32)) {
    resp.error = "pairing id invalid";
    return resp;
  }

  std::string key;
  if (mode == 0) {
    key = MakePairingRequestQueueKey(sess->username, pairing_id_hex);
  } else if (mode == 1) {
    if (device_id.empty()) {
      resp.error = "device id empty";
      return resp;
    }
    if (!LooksLikeHexId(device_id, 32) && device_id.size() > 64) {
      resp.error = "device id invalid";
      return resp;
    }
    key = MakePairingResponseQueueKey(sess->username, pairing_id_hex, device_id);
  } else {
    resp.error = "invalid mode";
    return resp;
  }

  resp.messages = queue_->Drain(key);
  resp.success = true;
  return resp;
}

DevicePairingPushResponse ApiService::PushDevicePairingResponse(
    const std::string& token, const std::string& pairing_id_hex,
    const std::string& target_device_id, std::vector<std::uint8_t> payload) {
  DevicePairingPushResponse resp;
  if (!sessions_ || !queue_) {
    resp.error = "queue unavailable";
    return resp;
  }
  std::optional<Session> sess;
  std::string rl_error;
  if (!RateLimitAuth("pairing_response", token, sess, rl_error)) {
    resp.error = rl_error;
    return resp;
  }
  if (!LooksLikeHexId(pairing_id_hex, 32)) {
    resp.error = "pairing id invalid";
    return resp;
  }
  if (target_device_id.empty()) {
    resp.error = "device id empty";
    return resp;
  }
  if (!LooksLikeHexId(target_device_id, 32) && target_device_id.size() > 64) {
    resp.error = "device id invalid";
    return resp;
  }
  if (payload.empty()) {
    resp.error = "payload empty";
    return resp;
  }
  if (payload.size() > (16u * 1024u)) {
    resp.error = "payload too large";
    return resp;
  }

  queue_->Enqueue(
      MakePairingResponseQueueKey(sess->username, pairing_id_hex, target_device_id),
      std::move(payload), std::chrono::minutes(10));
  resp.success = true;
  return resp;
}

}  // namespace mi::server
