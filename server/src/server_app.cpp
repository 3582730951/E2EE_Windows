#include "server_app.h"

#include "api_service.h"
#include "opaque_pake.h"

#include <filesystem>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <fstream>
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
    if (len == 0 || file_bytes.size() != 12 + static_cast<std::size_t>(len)) {
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
  offline_storage_ = std::make_unique<OfflineStorage>(storage_dir);
  offline_queue_ = std::make_unique<OfflineQueue>();
  api_ = std::make_unique<ApiService>(sessions_.get(), groups_.get(),
                                      directory_.get(), offline_storage_.get(),
                                      offline_queue_.get(),
                                      config_.server.group_rotation_threshold,
                                      config_.mode == AuthMode::kMySQL
                                          ? std::optional<MySqlConfig>(
                                                config_.mysql)
                                          : std::nullopt,
                                      storage_dir);
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
    last_cleanup_ = now;
  }
  //  KCP/TCP 
  return true;
}

bool ServerApp::HandleFrame(const Frame& in, Frame& out, std::string& error) {
  if (!router_) {
    error = "router not initialized";
    return false;
  }
  if (!router_->Handle(in, out, "")) {
    error = "handle frame failed";
    return false;
  }
  return true;
}

bool ServerApp::HandleFrameWithToken(const Frame& in, Frame& out,
                                     const std::string& token,
                                     std::string& error) {
  if (!router_) {
    error = "router not initialized";
    return false;
  }
  if (!router_->Handle(in, out, token)) {
    error = "handle frame failed";
    return false;
  }
  return true;
}

}  // namespace mi::server
