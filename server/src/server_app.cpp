#include "server_app.h"

#include "api_service.h"
#include "group_call_manager.h"
#include "crypto.h"
#include "hex_utils.h"
#include "key_transparency.h"
#include "opaque_pake.h"
#include "path_security.h"
#include "platform_log.h"
#include "platform_secure_store.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

namespace mi::server {

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

constexpr std::uint8_t kOpaqueSetupMagic[8] = {'M', 'I', 'O', 'P',
                                               'A', 'Q', 'S', '1'};
constexpr std::size_t kMaxOpaqueSetupBytes = 64u * 1024u;
constexpr std::uint8_t kDpapiMagic[8] = {'M', 'I', 'D', 'P',
                                         'A', 'P', 'I', '1'};
constexpr std::size_t kDpapiHeaderBytes = 12;

extern "C" {
int PQCLEAN_MLDSA65_CLEAN_crypto_sign_keypair(std::uint8_t* pk,
                                             std::uint8_t* sk);
}

bool CheckPathNotWorldWritable(const std::filesystem::path& path,
                               std::string& error) {
#ifdef _WIN32
  if (mi::shard::security::CheckPathNotWorldWritable(path, error)) {
    return true;
  }
  const std::string kInsecureAclPrefix = "insecure acl (world-writable)";
  if (error.compare(0, kInsecureAclPrefix.size(), kInsecureAclPrefix) != 0) {
    return false;
  }
  std::string fix_error;
  if (!mi::shard::security::HardenPathAcl(path, fix_error)) {
    if (!fix_error.empty()) {
      error = fix_error;
    }
    return false;
  }
  error.clear();
  return mi::shard::security::CheckPathNotWorldWritable(path, error);
#else
  std::error_code ec;
  const auto perms = std::filesystem::status(path, ec).permissions();
  if (ec || perms == std::filesystem::perms::unknown) {
    return true;  // best-effort on filesystems without perms
  }
  const auto writable =
      std::filesystem::perms::group_write | std::filesystem::perms::others_write;
  if ((perms & writable) != std::filesystem::perms::none) {
    error = "insecure file permissions: " + path.string() +
            "; fix: chmod 600 and remove group/world write";
    return false;
  }
  return true;
#endif
}

bool SetOwnerOnlyPermissions(const std::filesystem::path& path,
                             std::string& error) {
#ifdef _WIN32
  std::string acl_err;
  if (!mi::shard::security::HardenPathAcl(path, acl_err)) {
    error = acl_err.empty() ? "acl set failed" : acl_err;
    return false;
  }
  error.clear();
  return true;
#else
  std::error_code ec;
  std::filesystem::permissions(
      path, std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace, ec);
  if (ec) {
    error = "secure permissions set failed";
    return false;
  }
  return true;
#endif
}

bool IsDpapiBlob(const std::vector<std::uint8_t>& data) {
  return data.size() >= kDpapiHeaderBytes &&
         std::equal(std::begin(kDpapiMagic), std::end(kDpapiMagic),
                    data.begin());
}

mi::platform::SecureStoreScope ScopeForKeyProtection(
    KeyProtectionMode mode) {
  return mode == KeyProtectionMode::kDpapiMachine
             ? mi::platform::SecureStoreScope::kMachine
             : mi::platform::SecureStoreScope::kUser;
}

bool EncodeProtectedFileBytes(const std::vector<std::uint8_t>& plain,
                              KeyProtectionMode mode,
                              std::vector<std::uint8_t>& out,
                              std::string& error) {
  error.clear();
  if (mode == KeyProtectionMode::kNone) {
    out = plain;
    return true;
  }
  std::vector<std::uint8_t> blob;
  if (!mi::platform::ProtectSecureBlobScoped(
          plain, nullptr, 0, ScopeForKeyProtection(mode), blob, error)) {
    return false;
  }
  if (blob.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "secure store blob too large";
    return false;
  }
  const std::uint32_t len = static_cast<std::uint32_t>(blob.size());
  out.clear();
  out.reserve(kDpapiHeaderBytes + blob.size());
  out.insert(out.end(), std::begin(kDpapiMagic), std::end(kDpapiMagic));
  out.push_back(static_cast<std::uint8_t>(len & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len >> 24) & 0xFF));
  out.insert(out.end(), blob.begin(), blob.end());
  return true;
}

bool DecodeProtectedFileBytes(const std::vector<std::uint8_t>& file_bytes,
                              std::vector<std::uint8_t>& out_plain,
                              bool& was_protected,
                              std::string& error) {
  error.clear();
  was_protected = false;
  if (!IsDpapiBlob(file_bytes)) {
    out_plain = file_bytes;
    return true;
  }
  was_protected = true;
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
      blob, nullptr, 0, mi::platform::SecureStoreScope::kUser, out_plain,
      error);
}

bool ParseSha256Hex(const std::string& hex,
                    std::array<std::uint8_t, 32>& out) {
  std::vector<std::uint8_t> tmp;
  if (!mi::common::HexToBytes(hex, tmp) || tmp.size() != out.size()) {
    return false;
  }
  std::copy_n(tmp.begin(), out.size(), out.begin());
  return true;
}

bool ReadFileToBytes(const std::filesystem::path& path,
                     std::vector<std::uint8_t>& out,
                     std::string& error);

bool VerifyFileSha256(const std::filesystem::path& path,
                      const std::string& expected_hex,
                      std::string& error) {
  error.clear();
  if (expected_hex.empty()) {
    return true;
  }
  std::array<std::uint8_t, 32> expected{};
  if (!ParseSha256Hex(expected_hex, expected)) {
    error = "secure_delete_plugin_sha256 invalid";
    return false;
  }
  std::vector<std::uint8_t> bytes;
  std::string read_err;
  if (!ReadFileToBytes(path, bytes, read_err)) {
    error = "secure_delete_plugin read failed";
    return false;
  }
  crypto::Sha256Digest digest;
  crypto::Sha256(bytes.data(), bytes.size(), digest);
  if (digest.bytes != expected) {
    error = "secure_delete_plugin_sha256 mismatch";
    return false;
  }
  return true;
}

bool WriteFileAtomic(const std::filesystem::path& path,
                     const std::uint8_t* data,
                     std::size_t len,
                     bool overwrite,
                     bool owner_only,
                     std::string& error);

bool LoadOrCreateOpaqueServerSetup(const std::filesystem::path& dir,
                                   std::vector<std::uint8_t>& out_setup,
                                   KeyProtectionMode key_protection,
                                   std::string& error) {
  out_setup.clear();
  const auto path = dir / "opaque_server_setup.bin";
  std::error_code ec;
  const bool exists = std::filesystem::exists(path, ec);
  if (ec) {
    error = "opaque setup path error";
    return false;
  }
  if (exists) {
    if (!CheckPathNotWorldWritable(path, error)) {
      return false;
    }
    std::vector<std::uint8_t> file_bytes;
    std::string read_err;
    if (!ReadFileToBytes(path, file_bytes, read_err)) {
      error = "opaque setup read failed";
      return false;
    }
    std::vector<std::uint8_t> decoded;
    bool was_protected = false;
    if (!DecodeProtectedFileBytes(file_bytes, decoded, was_protected, error)) {
      return false;
    }
    if (was_protected) {
      out_setup = std::move(decoded);
    } else {
      if (file_bytes.size() < 12) {
        error = "opaque setup corrupted";
        return false;
      }
      if (!std::equal(std::begin(kOpaqueSetupMagic), std::end(kOpaqueSetupMagic),
                      file_bytes.begin())) {
        error = "opaque setup bad magic";
        return false;
      }
      const std::uint32_t len =
          static_cast<std::uint32_t>(file_bytes[8]) |
          (static_cast<std::uint32_t>(file_bytes[9]) << 8) |
          (static_cast<std::uint32_t>(file_bytes[10]) << 16) |
          (static_cast<std::uint32_t>(file_bytes[11]) << 24);
      if (len == 0 || len > kMaxOpaqueSetupBytes ||
          file_bytes.size() != 12 + static_cast<std::size_t>(len)) {
        error = "opaque setup bad length";
        return false;
      }
      out_setup.assign(file_bytes.begin() + 12, file_bytes.end());
    }
    if (out_setup.empty() || out_setup.size() > kMaxOpaqueSetupBytes) {
      error = "opaque setup size invalid";
      return false;
    }
    RustBuf err_buf;
    const int rc = mi_opaque_server_setup_validate(
        out_setup.data(), out_setup.size(), &err_buf.ptr, &err_buf.len);
    if (rc != 0) {
      if (err_buf.ptr && err_buf.len) {
        error.assign(reinterpret_cast<const char*>(err_buf.ptr), err_buf.len);
      } else {
        error = "opaque setup invalid";
      }
      out_setup.clear();
      return false;
    }
    if (!was_protected && key_protection != KeyProtectionMode::kNone) {
      std::vector<std::uint8_t> protected_bytes;
      if (!EncodeProtectedFileBytes(out_setup, key_protection, protected_bytes,
                                    error)) {
        out_setup.clear();
        return false;
      }
      if (!WriteFileAtomic(path, protected_bytes.data(), protected_bytes.size(),
                           true, true, error)) {
        out_setup.clear();
        return false;
      }
    }
    error.clear();
    return true;
  }

  RustBuf setup_buf;
  RustBuf err_buf;
  const int rc = mi_opaque_server_setup_generate(&setup_buf.ptr, &setup_buf.len,
                                                 &err_buf.ptr, &err_buf.len);
  if (rc != 0 || !setup_buf.ptr || setup_buf.len == 0) {
    if (err_buf.ptr && err_buf.len) {
      error.assign(reinterpret_cast<const char*>(err_buf.ptr), err_buf.len);
    } else {
      error = "opaque setup generate failed";
    }
    return false;
  }
  out_setup.assign(setup_buf.ptr, setup_buf.ptr + setup_buf.len);
  if (out_setup.size() > kMaxOpaqueSetupBytes) {
    error = "opaque setup too large";
    out_setup.clear();
    return false;
  }

  if (key_protection != KeyProtectionMode::kNone) {
    std::vector<std::uint8_t> protected_bytes;
    if (!EncodeProtectedFileBytes(out_setup, key_protection, protected_bytes,
                                  error)) {
      out_setup.clear();
      return false;
    }
    if (!WriteFileAtomic(path, protected_bytes.data(), protected_bytes.size(),
                         false, true, error)) {
      out_setup.clear();
      return false;
    }
    error.clear();
    return true;
  }

  // Write atomically.
  const auto tmp = path.string() + ".tmp";
  {
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      error = "opaque setup write failed";
      out_setup.clear();
      return false;
    }
    ofs.write(reinterpret_cast<const char*>(kOpaqueSetupMagic),
              sizeof(kOpaqueSetupMagic));
    const std::uint32_t out_len = static_cast<std::uint32_t>(out_setup.size());
    const std::uint8_t len_le[4] = {
        static_cast<std::uint8_t>(out_len & 0xFF),
        static_cast<std::uint8_t>((out_len >> 8) & 0xFF),
        static_cast<std::uint8_t>((out_len >> 16) & 0xFF),
        static_cast<std::uint8_t>((out_len >> 24) & 0xFF),
    };
    ofs.write(reinterpret_cast<const char*>(len_le), sizeof(len_le));
    ofs.write(reinterpret_cast<const char*>(out_setup.data()),
              static_cast<std::streamsize>(out_setup.size()));
    if (!ofs) {
      error = "opaque setup write failed";
      out_setup.clear();
      std::filesystem::remove(tmp, ec);
      return false;
    }
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    error = "opaque setup rename failed";
    out_setup.clear();
    std::filesystem::remove(tmp, ec);
    return false;
  }
  if (!SetOwnerOnlyPermissions(path, error)) {
    out_setup.clear();
    return false;
  }
  error.clear();
  return true;
}

bool WriteFileAtomic(const std::filesystem::path& path,
                     const std::uint8_t* data,
                     std::size_t len,
                     bool overwrite,
                     bool owner_only,
                     std::string& error) {
  error.clear();
  if (path.empty() || !data || len == 0) {
    error = "kt key path empty";
    return false;
  }
  std::error_code ec;
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      error = "kt key dir create failed";
      return false;
    }
  }
  if (!overwrite && std::filesystem::exists(path, ec)) {
    if (ec) {
      error = "kt key path error";
      return false;
    }
    error = "kt key exists";
    return false;
  }
  const auto tmp = path.string() + ".tmp";
  {
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      error = "kt key write failed";
      return false;
    }
    ofs.write(reinterpret_cast<const char*>(data),
              static_cast<std::streamsize>(len));
    if (!ofs) {
      error = "kt key write failed";
      std::filesystem::remove(tmp, ec);
      return false;
    }
  }
  if (overwrite) {
    std::filesystem::remove(path, ec);
    if (ec) {
      std::filesystem::remove(tmp, ec);
      error = "kt key remove failed";
      return false;
    }
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    error = "kt key rename failed";
    return false;
  }
  if (owner_only && !SetOwnerOnlyPermissions(path, error)) {
    return false;
  }
  return true;
}

bool ReadFileToBytes(const std::filesystem::path& path,
                     std::vector<std::uint8_t>& out,
                     std::string& error) {
  error.clear();
  out.clear();
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    error = "file read failed";
    return false;
  }
  std::error_code ec;
  const std::uint64_t size = std::filesystem::file_size(path, ec);
  if (ec ||
      size > static_cast<std::uint64_t>(
                 (std::numeric_limits<std::size_t>::max)())) {
    error = "file read failed";
    return false;
  }
  out.resize(static_cast<std::size_t>(size));
  if (!out.empty()) {
    ifs.read(reinterpret_cast<char*>(out.data()),
             static_cast<std::streamsize>(out.size()));
    if (!ifs || ifs.gcount() != static_cast<std::streamsize>(out.size())) {
      error = "file read failed";
      out.clear();
      return false;
    }
  }
  return true;
}

bool GenerateKtKeyPair(const std::filesystem::path& signing_key,
                       const std::filesystem::path& root_pub,
                       KeyProtectionMode key_protection,
                       std::string& error) {
  if (signing_key.empty() || root_pub.empty()) {
    error = "kt key path empty";
    return false;
  }
  if (signing_key == root_pub) {
    error = "kt key path invalid";
    return false;
  }
  std::array<std::uint8_t, kKtSthSigPublicKeyBytes> pk{};
  std::array<std::uint8_t, kKtSthSigSecretKeyBytes> sk{};
  if (PQCLEAN_MLDSA65_CLEAN_crypto_sign_keypair(pk.data(), sk.data()) != 0) {
    error = "kt signing key generate failed";
    return false;
  }
  std::vector<std::uint8_t> signing_bytes;
  if (key_protection != KeyProtectionMode::kNone) {
    std::vector<std::uint8_t> plain(sk.begin(), sk.end());
    if (!EncodeProtectedFileBytes(plain, key_protection, signing_bytes,
                                  error)) {
      std::fill(sk.begin(), sk.end(), 0);
      return false;
    }
  } else {
    signing_bytes.assign(sk.begin(), sk.end());
  }
  if (!WriteFileAtomic(signing_key, signing_bytes.data(),
                       signing_bytes.size(), false, true, error)) {
    std::fill(sk.begin(), sk.end(), 0);
    return false;
  }
  if (!WriteFileAtomic(root_pub, pk.data(), pk.size(), true, true, error)) {
    std::error_code ec;
    std::filesystem::remove(signing_key, ec);
    std::fill(sk.begin(), sk.end(), 0);
    return false;
  }
  std::fill(sk.begin(), sk.end(), 0);
  return true;
}

bool EnsureKtSigningKey(const std::filesystem::path& signing_key,
                        const std::filesystem::path& root_pub,
                        KeyProtectionMode key_protection,
                        bool& generated,
                        std::string& error) {
  generated = false;
  std::error_code ec;
  const bool exists = std::filesystem::exists(signing_key, ec);
  if (ec) {
    error = "kt_signing_key path error";
    return false;
  }
  if (!exists) {
    if (!GenerateKtKeyPair(signing_key, root_pub, key_protection, error)) {
      return false;
    }
    generated = true;
  }
  if (!CheckPathNotWorldWritable(signing_key, error)) {
    return false;
  }
  std::vector<std::uint8_t> file_bytes;
  if (!ReadFileToBytes(signing_key, file_bytes, error)) {
    error = error.empty() ? "kt_signing_key read failed" : error;
    return false;
  }
  std::vector<std::uint8_t> plain;
  bool was_protected = false;
  if (!DecodeProtectedFileBytes(file_bytes, plain, was_protected, error)) {
    return false;
  }
  if (plain.size() != kKtSthSigSecretKeyBytes) {
    error = "kt_signing_key size invalid";
    return false;
  }
  if (!was_protected && key_protection != KeyProtectionMode::kNone) {
    std::vector<std::uint8_t> protected_bytes;
    if (!EncodeProtectedFileBytes(plain, key_protection, protected_bytes,
                                  error)) {
      return false;
    }
    if (!WriteFileAtomic(signing_key, protected_bytes.data(),
                         protected_bytes.size(), true, true, error)) {
      return false;
    }
  }
  return true;
}

}  // namespace

ServerApp::ServerApp() = default;
ServerApp::~ServerApp() {
  if (state_lock_held_) {
    mi::platform::fs::ReleaseFileLock(state_lock_);
    state_lock_held_ = false;
  }
}

bool ServerApp::Init(const std::string& config_path, std::string& error) {
  if (!LoadConfig(config_path, config_, error)) {
    return false;
  }

  std::filesystem::path config_dir;
  if (!config_path.empty()) {
    config_dir = std::filesystem::path(config_path).parent_path();
    if (config_dir.empty()) {
      config_dir = std::filesystem::current_path();
    } else if (config_dir.is_relative()) {
      std::error_code ec;
      const auto cwd = std::filesystem::current_path(ec);
      if (!cwd.empty()) {
        config_dir = cwd / config_dir;
      }
    }
  }

  auto storage_dir = config_.server.offline_dir.empty()
                         ? std::filesystem::current_path() / "offline_store"
                         : std::filesystem::path(config_.server.offline_dir);
  std::error_code ec;
  std::filesystem::create_directories(storage_dir, ec);
  if (ec) {
    error = "offline_dir not accessible";
    return false;
  }
  // simple writability probe
  const auto probe = storage_dir / ".probe";
  {
    std::ofstream ofs(probe, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      error = "offline_dir not writable";
      return false;
    }
  }
  std::filesystem::remove(probe, ec);

  const auto state_dir = storage_dir / "state";
  std::filesystem::create_directories(state_dir, ec);
  if (ec) {
    error = "state dir not accessible";
    return false;
  }
  {
    const auto probe = state_dir / ".probe";
    std::ofstream ofs(probe, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      error = "state dir not writable";
      return false;
    }
  }
  std::filesystem::remove(state_dir / ".probe", ec);
  if (!state_lock_held_) {
    const auto lock_path = state_dir / "server.lock";
    const auto lock_status =
        mi::platform::fs::AcquireExclusiveFileLock(lock_path, state_lock_);
    if (lock_status != mi::platform::fs::FileLockStatus::kOk) {
      error = (lock_status == mi::platform::fs::FileLockStatus::kBusy)
                  ? "server state locked (another instance running)"
                  : "server state lock failed";
      return false;
    }
    state_lock_held_ = true;
  }

  std::filesystem::path kt_signing_key = config_.server.kt_signing_key;
  if (!kt_signing_key.is_absolute() && !kt_signing_key.empty()) {
    if (!config_dir.empty()) {
      kt_signing_key = config_dir / kt_signing_key;
    } else {
      kt_signing_key = storage_dir / kt_signing_key;
    }
  }
  if (kt_signing_key.empty()) {
    error = "kt_signing_key not found";
    return false;
  }
  const auto kt_root_pub_dir =
      kt_signing_key.has_parent_path() ? kt_signing_key.parent_path()
                                       : storage_dir;
  const auto kt_root_pub = kt_root_pub_dir / "kt_root_pub.bin";
  bool kt_generated = false;
  if (!EnsureKtSigningKey(kt_signing_key, kt_root_pub,
                          config_.server.key_protection,
                          kt_generated, error)) {
    return false;
  }
  if (kt_generated) {
    const std::string message =
        "generated kt_signing_key at " + kt_signing_key.string() +
        " and kt_root_pub at " + kt_root_pub.string();
    mi::platform::log::Log(mi::platform::log::Level::kInfo, "server", message);
  }

  const bool require_secure_delete = config_.server.secure_delete_required;
  SecureDeleteConfig secure_delete;
  secure_delete.enabled = config_.server.secure_delete_enabled;
  if (secure_delete.enabled) {
    secure_delete.plugin_path = config_.server.secure_delete_plugin;
    if (!secure_delete.plugin_path.is_absolute()) {
      secure_delete.plugin_path = storage_dir / secure_delete.plugin_path;
    }
    if (secure_delete.plugin_path.empty() ||
        !std::filesystem::exists(secure_delete.plugin_path, ec) || ec) {
      error = "secure_delete_plugin not found";
      return false;
    }
    if (!VerifyFileSha256(secure_delete.plugin_path,
                          config_.server.secure_delete_plugin_sha256,
                          error)) {
      return false;
    }
  }

  std::vector<std::uint8_t> opaque_setup;
  if (!LoadOrCreateOpaqueServerSetup(storage_dir, opaque_setup,
                                     config_.server.key_protection, error)) {
    return false;
  }

  auth_ = MakeAuthProvider(config_, opaque_setup, error);
  if (!auth_) {
    return false;
  }

  sessions_ = std::make_unique<SessionManager>(
      std::move(auth_),
      std::chrono::seconds(config_.server.session_ttl_sec),
      std::move(opaque_setup),
      state_dir);
  groups_ = std::make_unique<GroupManager>(state_dir);
  GroupCallConfig call_cfg;
  call_cfg.enable_group_call = config_.call.enable_group_call;
  call_cfg.max_room_size = config_.call.max_room_size;
  call_cfg.idle_timeout_sec = config_.call.idle_timeout_sec;
  call_cfg.call_timeout_sec = config_.call.call_timeout_sec;
  call_cfg.max_subscriptions = config_.call.max_subscriptions;
  group_calls_ = std::make_unique<GroupCallManager>(call_cfg);
  directory_ = std::make_unique<GroupDirectory>(state_dir);
  offline_storage_ = std::make_unique<OfflineStorage>(
      storage_dir, std::chrono::hours(12), secure_delete);
  if ((secure_delete.enabled || require_secure_delete) &&
      !offline_storage_->SecureDeleteReady()) {
    error = offline_storage_->SecureDeleteError().empty()
                ? "secure delete plugin load failed"
                : offline_storage_->SecureDeleteError();
    return false;
  }
  offline_queue_ = std::make_unique<OfflineQueue>(
      std::chrono::seconds::zero(), storage_dir / "offline_queue");
  media_relay_ = std::make_unique<MediaRelay>(
      2048, std::chrono::milliseconds(config_.call.media_ttl_ms));
  api_ = std::make_unique<ApiService>(sessions_.get(), groups_.get(),
                                      group_calls_.get(), directory_.get(),
                                      offline_storage_.get(),
                                      offline_queue_.get(), media_relay_.get(),
                                      config_.server.group_rotation_threshold,
                                      config_.mode == AuthMode::kMySQL
                                          ? std::optional<MySqlConfig>(
                                                config_.mysql)
                                          : std::nullopt,
                                      storage_dir,
                                      kt_signing_key);
  router_ = std::make_unique<FrameRouter>(api_.get());
  last_cleanup_ = std::chrono::steady_clock::now();
  return true;
}

bool ServerApp::RunOnce(std::string& error) {
  if (!sessions_ || !groups_) {
    error = "server not initialized";
    return false;
  }
  const auto now = std::chrono::steady_clock::now();
  if (now - last_cleanup_ > std::chrono::minutes(5)) {
    sessions_->Cleanup();
    if (offline_storage_) {
      offline_storage_->CleanupExpired();
    }
    if (offline_queue_) {
      offline_queue_->CleanupExpired();
    }
    if (media_relay_) {
      media_relay_->Cleanup();
    }
    if (group_calls_) {
      group_calls_->Cleanup();
    }
    last_cleanup_ = now;
  }
  //  KCP/TCP 
  return true;
}

bool ServerApp::HandleFrame(const Frame& in, Frame& out,
                            TransportKind transport, std::string& error) {
  FrameView view{in.type, in.payload.data(), in.payload.size()};
  return HandleFrameView(view, out, transport, error);
}

bool ServerApp::HandleFrameView(const FrameView& in, Frame& out,
                                TransportKind transport,
                                std::string& error) {
  if (!router_) {
    error = "router not initialized";
    return false;
  }
  if (in.type == FrameType::kLogin && !config_.server.allow_legacy_login) {
    error = "legacy login disabled";
    return false;
  }
  if (!router_->HandleView(in, out, "", transport)) {
    error = "handle frame failed";
    return false;
  }
  return true;
}

bool ServerApp::HandleFrameWithToken(const Frame& in, Frame& out,
                                     const std::string& token,
                                     TransportKind transport,
                                     std::string& error) {
  FrameView view{in.type, in.payload.data(), in.payload.size()};
  return HandleFrameWithTokenView(view, out, token, transport, error);
}

bool ServerApp::HandleFrameWithTokenView(const FrameView& in, Frame& out,
                                         const std::string& token,
                                         TransportKind transport,
                                         std::string& error) {
  if (!router_) {
    error = "router not initialized";
    return false;
  }
  if (!router_->HandleView(in, out, token, transport)) {
    error = "handle frame failed";
    return false;
  }
  return true;
}

}  // namespace mi::server
