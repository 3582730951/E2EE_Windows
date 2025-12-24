#include "server_app.h"

#include "api_service.h"
#include "key_transparency.h"
#include "opaque_pake.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

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

extern "C" {
int PQCLEAN_MLDSA65_CLEAN_crypto_sign_keypair(std::uint8_t* pk,
                                             std::uint8_t* sk);
}

bool LoadOrCreateOpaqueServerSetup(const std::filesystem::path& dir,
                                   std::vector<std::uint8_t>& out_setup,
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
    const auto file_size = std::filesystem::file_size(path, ec);
    if (ec) {
      error = "opaque setup size stat failed";
      return false;
    }
    if (file_size < 12 || file_size > 12 + kMaxOpaqueSetupBytes) {
      error = "opaque setup size invalid";
      return false;
    }
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
      error = "opaque setup read failed";
      return false;
    }
    std::vector<std::uint8_t> file_bytes(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>());
    if (file_bytes.size() < 12) {
      error = "opaque setup corrupted";
      return false;
    }
    if (!std::equal(std::begin(kOpaqueSetupMagic), std::end(kOpaqueSetupMagic),
                    file_bytes.begin())) {
      error = "opaque setup bad magic";
      return false;
    }
    const std::uint32_t len = static_cast<std::uint32_t>(file_bytes[8]) |
                              (static_cast<std::uint32_t>(file_bytes[9]) << 8) |
                              (static_cast<std::uint32_t>(file_bytes[10]) << 16) |
                              (static_cast<std::uint32_t>(file_bytes[11]) << 24);
    if (len == 0 || len > kMaxOpaqueSetupBytes ||
        file_bytes.size() != 12 + static_cast<std::size_t>(len)) {
      error = "opaque setup bad length";
      return false;
    }
    out_setup.assign(file_bytes.begin() + 12, file_bytes.end());
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
  error.clear();
  return true;
}

bool WriteFileAtomic(const std::filesystem::path& path,
                     const std::uint8_t* data,
                     std::size_t len,
                     bool overwrite,
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
  return true;
}

bool GenerateKtKeyPair(const std::filesystem::path& signing_key,
                       const std::filesystem::path& root_pub,
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
  if (!WriteFileAtomic(signing_key, sk.data(), sk.size(), false, error)) {
    std::fill(sk.begin(), sk.end(), 0);
    return false;
  }
  if (!WriteFileAtomic(root_pub, pk.data(), pk.size(), true, error)) {
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
    if (!GenerateKtKeyPair(signing_key, root_pub, error)) {
      return false;
    }
    generated = true;
  }
  const auto size = std::filesystem::file_size(signing_key, ec);
  if (ec || size != kKtSthSigSecretKeyBytes) {
    error = "kt_signing_key size invalid";
    return false;
  }
  return true;
}

}  // namespace

ServerApp::ServerApp() = default;
ServerApp::~ServerApp() = default;

bool ServerApp::Init(const std::string& config_path, std::string& error) {
  if (!LoadConfig(config_path, config_, error)) {
    return false;
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

  std::filesystem::path kt_signing_key = config_.server.kt_signing_key;
  if (!kt_signing_key.is_absolute() && !kt_signing_key.empty()) {
    kt_signing_key = storage_dir / kt_signing_key;
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
  if (!EnsureKtSigningKey(kt_signing_key, kt_root_pub, kt_generated, error)) {
    return false;
  }
  if (kt_generated) {
    std::cout << "[mi_e2ee_server] generated kt_signing_key at "
              << kt_signing_key.string()
              << " and kt_root_pub at " << kt_root_pub.string() << "\n";
  }

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
  }

  std::vector<std::uint8_t> opaque_setup;
  if (!LoadOrCreateOpaqueServerSetup(storage_dir, opaque_setup, error)) {
    return false;
  }

  auth_ = MakeAuthProvider(config_, opaque_setup, error);
  if (!auth_) {
    return false;
  }

  sessions_ = std::make_unique<SessionManager>(std::move(auth_),
                                               std::chrono::minutes(30),
                                               std::move(opaque_setup));
  groups_ = std::make_unique<GroupManager>();
  directory_ = std::make_unique<GroupDirectory>();
  offline_storage_ = std::make_unique<OfflineStorage>(
      storage_dir, std::chrono::hours(12), secure_delete);
  if (secure_delete.enabled && !offline_storage_->SecureDeleteReady()) {
    error = offline_storage_->SecureDeleteError().empty()
                ? "secure delete plugin load failed"
                : offline_storage_->SecureDeleteError();
    return false;
  }
  offline_queue_ = std::make_unique<OfflineQueue>();
  media_relay_ = std::make_unique<MediaRelay>();
  api_ = std::make_unique<ApiService>(sessions_.get(), groups_.get(),
                                      directory_.get(), offline_storage_.get(),
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
    last_cleanup_ = now;
  }
  //  KCP/TCP 
  return true;
}

bool ServerApp::HandleFrame(const Frame& in, Frame& out,
                            TransportKind transport, std::string& error) {
  if (!router_) {
    error = "router not initialized";
    return false;
  }
  if (!router_->Handle(in, out, "", transport)) {
    error = "handle frame failed";
    return false;
  }
  return true;
}

bool ServerApp::HandleFrameWithToken(const Frame& in, Frame& out,
                                     const std::string& token,
                                     TransportKind transport,
                                     std::string& error) {
  if (!router_) {
    error = "router not initialized";
    return false;
  }
  if (!router_->Handle(in, out, token, transport)) {
    error = "handle frame failed";
    return false;
  }
  return true;
}

}  // namespace mi::server
