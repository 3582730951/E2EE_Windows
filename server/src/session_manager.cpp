#include "session_manager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <string_view>
#include <vector>
#include <utility>

#include "path_security.h"
#include "crypto.h"
#include "hex_utils.h"
#include "monocypher.h"
#include "opaque_pake.h"
#include "protected_store.h"

extern "C" {
int PQCLEAN_MLKEM768_CLEAN_crypto_kem_enc(std::uint8_t* ct,
                                         std::uint8_t* ss,
                                         const std::uint8_t* pk);
}

namespace mi::server {

namespace {

constexpr std::array<std::uint8_t, 8> kSessionMagic = {
    'M', 'I', 'S', 'E', 'S', 'S', '0', '1'};
constexpr std::uint8_t kSessionVersion = 1;
constexpr std::size_t kSessionHeaderBytes =
    kSessionMagic.size() + 1 + 3 + 4;

void WriteUint32Le(std::uint32_t v, std::vector<std::uint8_t>& out) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
}

void WriteUint64Le(std::uint64_t v, std::vector<std::uint8_t>& out) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 32) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 40) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 48) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 56) & 0xFFu));
}

std::uint32_t ReadUint32Le(const std::uint8_t* in) {
  return static_cast<std::uint32_t>(in[0]) |
         (static_cast<std::uint32_t>(in[1]) << 8) |
         (static_cast<std::uint32_t>(in[2]) << 16) |
         (static_cast<std::uint32_t>(in[3]) << 24);
}

std::uint64_t ReadUint64Le(const std::uint8_t* in) {
  return static_cast<std::uint64_t>(in[0]) |
         (static_cast<std::uint64_t>(in[1]) << 8) |
         (static_cast<std::uint64_t>(in[2]) << 16) |
         (static_cast<std::uint64_t>(in[3]) << 24) |
         (static_cast<std::uint64_t>(in[4]) << 32) |
         (static_cast<std::uint64_t>(in[5]) << 40) |
         (static_cast<std::uint64_t>(in[6]) << 48) |
         (static_cast<std::uint64_t>(in[7]) << 56);
}

std::uint64_t UnixMsFrom(const std::chrono::system_clock::time_point& tp) {
  const auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          tp.time_since_epoch())
          .count();
  return ms < 0 ? 0u : static_cast<std::uint64_t>(ms);
}

std::chrono::system_clock::time_point UnixMsToTimepoint(std::uint64_t ms) {
  return std::chrono::system_clock::time_point(
      std::chrono::milliseconds(ms));
}

void SetOwnerOnlyPermissions(const std::filesystem::path& path) {
#ifdef _WIN32
  std::string acl_err;
  (void)mi::shard::security::HardenPathAcl(path, acl_err);
#else
  std::error_code ec;
  std::filesystem::permissions(
      path, std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace, ec);
#endif
}

}  // namespace

SessionManager::SessionManager(std::unique_ptr<AuthProvider> auth,
                               std::chrono::seconds ttl,
                               std::vector<std::uint8_t> opaque_server_setup,
                               std::filesystem::path persist_dir,
                               KeyProtectionMode state_protection,
                               StateStore* state_store)
    : auth_(std::move(auth)),
      ttl_(ttl),
      opaque_server_setup_(std::move(opaque_server_setup)),
      state_protection_(state_protection),
      state_store_(state_store) {
  if (persist_dir.empty()) {
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(persist_dir, ec);
  if (ec) {
    return;
  }
  persist_path_ = persist_dir / "sessions.bin";
  persistence_enabled_ = true;
  std::lock_guard<std::mutex> lock(mutex_);
  if (!LoadSessionsLocked()) {
    if (!state_store_) {
      const auto bad_path = persist_path_.string() + ".bad";
      std::filesystem::rename(persist_path_, bad_path, ec);
    }
  }
  dirty_ = false;
}

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

bool SessionManager::LoadSessionsLocked() {
  if (state_store_) {
    return LoadSessionsFromStoreLocked();
  }
  return LoadSessionsFromFileLocked();
}

bool SessionManager::LoadSessionsFromFileLocked() {
  if (!persistence_enabled_ || persist_path_.empty()) {
    return true;
  }
  std::error_code ec;
  if (!std::filesystem::exists(persist_path_, ec) || ec) {
    return true;
  }
  const auto size = std::filesystem::file_size(persist_path_, ec);
  if (ec || size < kSessionHeaderBytes ||
      size > static_cast<std::uint64_t>(
                 (std::numeric_limits<std::size_t>::max)())) {
    return false;
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  std::ifstream ifs(persist_path_, std::ios::binary);
  if (!ifs) {
    return false;
  }
  ifs.read(reinterpret_cast<char*>(bytes.data()),
           static_cast<std::streamsize>(bytes.size()));
  if (!ifs ||
      ifs.gcount() != static_cast<std::streamsize>(bytes.size())) {
    return false;
  }

  bool need_rewrap = false;
  {
    std::vector<std::uint8_t> plain;
    bool was_protected = false;
    std::string protect_err;
    if (!DecodeProtectedFileBytes(bytes, state_protection_, plain, was_protected,
                                  protect_err)) {
      return false;
    }
    need_rewrap =
        !was_protected && state_protection_ != KeyProtectionMode::kNone;
    bytes.swap(plain);
  }
  if (!LoadSessionsFromBytesLocked(bytes)) {
    return false;
  }
  if (need_rewrap && !state_store_) {
    SaveSessionsLocked();
  }
  return true;
}

bool SessionManager::LoadSessionsFromStoreLocked() {
  if (!state_store_) {
    return true;
  }
  BlobLoadResult blob;
  std::string load_err;
  if (!state_store_->LoadBlob("sessions", blob, load_err)) {
    return false;
  }
  if (!blob.found || blob.data.empty()) {
    if (!persist_path_.empty()) {
      std::error_code ec;
      if (std::filesystem::exists(persist_path_, ec) && !ec) {
        if (!LoadSessionsFromFileLocked()) {
          return false;
        }
        return SaveSessionsToStoreLocked();
      }
    }
    return true;
  }
  return LoadSessionsFromBytesLocked(blob.data);
}

bool SessionManager::LoadSessionsFromBytesLocked(
    const std::vector<std::uint8_t>& bytes) {
  std::size_t off = 0;
  if (bytes.size() < kSessionHeaderBytes ||
      !std::equal(kSessionMagic.begin(), kSessionMagic.end(),
                  bytes.begin())) {
    return false;
  }
  off += kSessionMagic.size();
  const std::uint8_t version = bytes[off++];
  if (version != kSessionVersion) {
    return false;
  }
  off += 3;
  if (off + 4 > bytes.size()) {
    return false;
  }
  const std::uint32_t session_count = ReadUint32Le(bytes.data() + off);
  off += 4;

  std::unordered_map<std::string, Session> loaded;
  loaded.reserve(session_count);
  const auto now_sys = std::chrono::system_clock::now();
  const auto now_steady = std::chrono::steady_clock::now();

  for (std::uint32_t i = 0; i < session_count; ++i) {
    if (off + 8 + 16 + 128 > bytes.size()) {
      return false;
    }
    const std::uint32_t token_len = ReadUint32Le(bytes.data() + off);
    off += 4;
    const std::uint32_t user_len = ReadUint32Le(bytes.data() + off);
    off += 4;
    const std::uint64_t created_ms = ReadUint64Le(bytes.data() + off);
    off += 8;
    const std::uint64_t last_seen_ms = ReadUint64Le(bytes.data() + off);
    off += 8;

    DerivedKeys keys{};
    std::memcpy(keys.root_key.data(), bytes.data() + off,
                keys.root_key.size());
    off += keys.root_key.size();
    std::memcpy(keys.header_key.data(), bytes.data() + off,
                keys.header_key.size());
    off += keys.header_key.size();
    std::memcpy(keys.kcp_key.data(), bytes.data() + off,
                keys.kcp_key.size());
    off += keys.kcp_key.size();
    std::memcpy(keys.ratchet_root.data(), bytes.data() + off,
                keys.ratchet_root.size());
    off += keys.ratchet_root.size();

    if (token_len == 0 || user_len == 0 ||
        off + token_len + user_len > bytes.size()) {
      return false;
    }
    std::string token(
        reinterpret_cast<const char*>(bytes.data() + off),
        reinterpret_cast<const char*>(bytes.data() + off + token_len));
    off += token_len;
    std::string username(
        reinterpret_cast<const char*>(bytes.data() + off),
        reinterpret_cast<const char*>(bytes.data() + off + user_len));
    off += user_len;

    auto created_sys = UnixMsToTimepoint(created_ms);
    auto last_seen_sys = UnixMsToTimepoint(last_seen_ms);
    if (created_sys > now_sys) {
      created_sys = now_sys;
    }
    if (last_seen_sys > now_sys) {
      last_seen_sys = now_sys;
    }
    if (last_seen_sys < created_sys) {
      last_seen_sys = created_sys;
    }

    if (ttl_.count() > 0 &&
        now_sys - last_seen_sys > ttl_) {
      continue;
    }

    const auto created_age = now_sys - created_sys;
    const auto last_seen_age = now_sys - last_seen_sys;
    const auto created_steady =
        now_steady - std::chrono::duration_cast<
                         std::chrono::steady_clock::duration>(created_age);
    const auto last_seen_steady =
        now_steady - std::chrono::duration_cast<
                         std::chrono::steady_clock::duration>(last_seen_age);

    Session s;
    s.token = std::move(token);
    s.username = std::move(username);
    s.keys = keys;
    s.created_at = created_steady;
    s.last_seen = last_seen_steady;
    loaded.emplace(s.token, std::move(s));
  }

  if (off != bytes.size()) {
    return false;
  }
  sessions_.swap(loaded);
  return true;
}

bool SessionManager::SaveSessionsLocked() {
  if (state_store_) {
    return SaveSessionsToStoreLocked();
  }
  if (!persistence_enabled_ || persist_path_.empty()) {
    return true;
  }
  if (sessions_.size() >
      static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())) {
    return false;
  }

  std::vector<std::string> keys;
  keys.reserve(sessions_.size());
  for (const auto& kv : sessions_) {
    keys.push_back(kv.first);
  }
  std::sort(keys.begin(), keys.end());

  const auto now_sys = std::chrono::system_clock::now();
  const auto now_steady = std::chrono::steady_clock::now();

  std::vector<std::uint8_t> out;
  out.reserve(kSessionHeaderBytes + keys.size() * 196);
  out.insert(out.end(), kSessionMagic.begin(), kSessionMagic.end());
  out.push_back(kSessionVersion);
  out.push_back(0);
  out.push_back(0);
  out.push_back(0);
  WriteUint32Le(static_cast<std::uint32_t>(keys.size()), out);

  for (const auto& token : keys) {
    const auto it = sessions_.find(token);
    if (it == sessions_.end()) {
      continue;
    }
    const auto& session = it->second;
    if (token.empty() || session.username.empty()) {
      return false;
    }
    if (token.size() >
            static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)()) ||
        session.username.size() >
            static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)())) {
      return false;
    }

    const auto created_age =
        now_steady > session.created_at
            ? (now_steady - session.created_at)
            : std::chrono::steady_clock::duration::zero();
    const auto last_seen_age =
        now_steady > session.last_seen
            ? (now_steady - session.last_seen)
            : std::chrono::steady_clock::duration::zero();
    const auto created_sys =
        now_sys -
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            created_age);
    const auto last_seen_sys =
        now_sys -
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            last_seen_age);

    WriteUint32Le(static_cast<std::uint32_t>(token.size()), out);
    WriteUint32Le(static_cast<std::uint32_t>(session.username.size()), out);
    WriteUint64Le(UnixMsFrom(created_sys), out);
    WriteUint64Le(UnixMsFrom(last_seen_sys), out);
    out.insert(out.end(), session.keys.root_key.begin(),
               session.keys.root_key.end());
    out.insert(out.end(), session.keys.header_key.begin(),
               session.keys.header_key.end());
    out.insert(out.end(), session.keys.kcp_key.begin(),
               session.keys.kcp_key.end());
    out.insert(out.end(), session.keys.ratchet_root.begin(),
               session.keys.ratchet_root.end());
    out.insert(out.end(), token.begin(), token.end());
    out.insert(out.end(), session.username.begin(), session.username.end());
  }

  std::vector<std::uint8_t> protected_bytes;
  std::string protect_err;
  if (!EncodeProtectedFileBytes(out, state_protection_, protected_bytes,
                                protect_err)) {
    return false;
  }

  const std::filesystem::path tmp = persist_path_.string() + ".tmp";
  std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    return false;
  }
  ofs.write(reinterpret_cast<const char*>(protected_bytes.data()),
            static_cast<std::streamsize>(protected_bytes.size()));
  ofs.close();
  if (!ofs.good()) {
    std::error_code rm_ec;
    std::filesystem::remove(tmp, rm_ec);
    return false;
  }
  std::error_code ec;
  std::filesystem::remove(persist_path_, ec);
  std::filesystem::rename(tmp, persist_path_, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    return false;
  }
  SetOwnerOnlyPermissions(persist_path_);
  dirty_ = false;
  return true;
}

bool SessionManager::SaveSessionsToStoreLocked() {
  if (!state_store_) {
    return true;
  }
  std::string lock_err;
  StateStoreLock lock(state_store_, "sessions",
                      std::chrono::milliseconds(5000), lock_err);
  if (!lock.locked()) {
    return false;
  }
  return SaveSessionsToStoreLockedUnlocked();
}

bool SessionManager::SaveSessionsToStoreLockedUnlocked() {
  if (sessions_.size() >
      static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())) {
    return false;
  }

  std::vector<std::string> keys;
  keys.reserve(sessions_.size());
  for (const auto& kv : sessions_) {
    keys.push_back(kv.first);
  }
  std::sort(keys.begin(), keys.end());

  const auto now_sys = std::chrono::system_clock::now();
  const auto now_steady = std::chrono::steady_clock::now();

  std::vector<std::uint8_t> out;
  out.reserve(kSessionHeaderBytes + keys.size() * 196);
  out.insert(out.end(), kSessionMagic.begin(), kSessionMagic.end());
  out.push_back(kSessionVersion);
  out.push_back(0);
  out.push_back(0);
  out.push_back(0);
  WriteUint32Le(static_cast<std::uint32_t>(keys.size()), out);

  for (const auto& token : keys) {
    const auto it = sessions_.find(token);
    if (it == sessions_.end()) {
      continue;
    }
    const auto& session = it->second;
    if (token.empty() || session.username.empty()) {
      return false;
    }
    if (token.size() >
            static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)()) ||
        session.username.size() >
            static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)())) {
      return false;
    }

    const auto created_age =
        now_steady > session.created_at
            ? (now_steady - session.created_at)
            : std::chrono::steady_clock::duration::zero();
    const auto last_seen_age =
        now_steady > session.last_seen
            ? (now_steady - session.last_seen)
            : std::chrono::steady_clock::duration::zero();
    const auto created_sys =
        now_sys -
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            created_age);
    const auto last_seen_sys =
        now_sys -
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            last_seen_age);

    WriteUint32Le(static_cast<std::uint32_t>(token.size()), out);
    WriteUint32Le(static_cast<std::uint32_t>(session.username.size()), out);
    WriteUint64Le(UnixMsFrom(created_sys), out);
    WriteUint64Le(UnixMsFrom(last_seen_sys), out);
    out.insert(out.end(), session.keys.root_key.begin(),
               session.keys.root_key.end());
    out.insert(out.end(), session.keys.header_key.begin(),
               session.keys.header_key.end());
    out.insert(out.end(), session.keys.kcp_key.begin(),
               session.keys.kcp_key.end());
    out.insert(out.end(), session.keys.ratchet_root.begin(),
               session.keys.ratchet_root.end());
    out.insert(out.end(), token.begin(), token.end());
    out.insert(out.end(), session.username.begin(), session.username.end());
  }

  std::string store_err;
  if (!state_store_->SaveBlob("sessions", out, store_err)) {
    return false;
  }
  dirty_ = false;
  return true;
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
    if (state_store_) {
      if (!LoadSessionsFromStoreLocked()) {
        error = "session state load failed";
        return false;
      }
    }
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
    dirty_ = true;
    if (state_store_) {
      std::string lock_err;
      StateStoreLock store_lock(state_store_, "sessions",
                                std::chrono::milliseconds(5000), lock_err);
      if (!store_lock.locked()) {
        error = "session state lock failed";
        return false;
      }
      if (!SaveSessionsToStoreLockedUnlocked()) {
        error = "session state save failed";
        return false;
      }
    } else {
      SaveSessionsLocked();
    }
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

bool LooksLikeSha256Hex(std::string_view s) {
  if (s.size() != 64) {
    return false;
  }
  for (char c : s) {
    if (std::isxdigit(static_cast<unsigned char>(c)) == 0) {
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
    if (!mi::common::HexToBytes(parts[3], salt) ||
        !mi::common::HexToBytes(parts[4], hash) ||
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
    if (!mi::common::HexToBytes(hash_hex, hash) ||
        hash.size() != out.key.size()) {
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
    if (!mi::common::HexToBytes(stored, hash) ||
        hash.size() != out.key.size()) {
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
    if (state_store_) {
      if (!LoadSessionsFromStoreLocked()) {
        error = "session state load failed";
        return false;
      }
    }
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
    dirty_ = true;
    if (state_store_) {
      std::string lock_err;
      StateStoreLock store_lock(state_store_, "sessions",
                                std::chrono::milliseconds(5000), lock_err);
      if (!store_lock.locked()) {
        error = "session state lock failed";
        return false;
      }
      if (!SaveSessionsToStoreLockedUnlocked()) {
        error = "session state save failed";
        return false;
      }
    } else {
      SaveSessionsLocked();
    }
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
    if (state_store_) {
      if (!LoadSessionsFromStoreLocked()) {
        error = "session state load failed";
        return false;
      }
    }
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
    dirty_ = true;
    if (state_store_) {
      std::string lock_err;
      StateStoreLock store_lock(state_store_, "sessions",
                                std::chrono::milliseconds(5000), lock_err);
      if (!store_lock.locked()) {
        error = "session state lock failed";
        return false;
      }
      if (!SaveSessionsToStoreLockedUnlocked()) {
        error = "session state save failed";
        return false;
      }
    } else {
      SaveSessionsLocked();
    }
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
  if (state_store_) {
    if (!LoadSessionsFromStoreLocked()) {
      return std::nullopt;
    }
  }
  const auto it = sessions_.find(token);
  if (it == sessions_.end()) {
    return std::nullopt;
  }
  const auto now = std::chrono::steady_clock::now();
  if (ttl_.count() > 0 &&
      now - it->second.last_seen > ttl_) {
    sessions_.erase(it);
    dirty_ = true;
    return std::nullopt;
  }
  it->second.last_seen = now;
  dirty_ = true;
  return it->second;
}

bool SessionManager::TouchSession(const std::string& token) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_store_) {
    if (!LoadSessionsFromStoreLocked()) {
      return false;
    }
  }
  const auto it = sessions_.find(token);
  if (it == sessions_.end()) {
    return false;
  }
  const auto now = std::chrono::steady_clock::now();
  if (ttl_.count() > 0 &&
      now - it->second.last_seen > ttl_) {
    sessions_.erase(it);
    dirty_ = true;
    return false;
  }
  it->second.last_seen = now;
  dirty_ = true;
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
  if (state_store_) {
    if (!LoadSessionsFromStoreLocked()) {
      return;
    }
  }
  sessions_.erase(token);
  dirty_ = true;
  SaveSessionsLocked();
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
  if (state_store_) {
    if (!LoadSessionsFromStoreLocked()) {
      return;
    }
  }
  const auto now = std::chrono::steady_clock::now();
  bool removed = false;
  for (auto it = sessions_.begin(); it != sessions_.end();) {
    if (ttl_.count() > 0 &&
        now - it->second.last_seen > ttl_) {
      it = sessions_.erase(it);
      removed = true;
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
  if ((removed || dirty_) && persistence_enabled_) {
    SaveSessionsLocked();
  }
}

}  // namespace mi::server
