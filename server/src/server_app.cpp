#include "server_app.h"

#include "api_service.h"

#include <filesystem>
#include <iostream>
#include <chrono>
#include <fstream>

namespace mi::server {

ServerApp::ServerApp() = default;
ServerApp::~ServerApp() = default;

bool ServerApp::Init(const std::string& config_path, std::string& error) {
  if (!LoadConfig(config_path, config_, error)) {
    return false;
  }

  auth_ = MakeAuthProvider(config_, error);
  if (!auth_) {
    return false;
  }

  sessions_ = std::make_unique<SessionManager>(std::move(auth_));
  groups_ = std::make_unique<GroupManager>();
  directory_ = std::make_unique<GroupDirectory>();
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

  offline_storage_ = std::make_unique<OfflineStorage>(storage_dir);
  offline_queue_ = std::make_unique<OfflineQueue>();
  api_ = std::make_unique<ApiService>(sessions_.get(), groups_.get(),
                                      directory_.get(), offline_storage_.get(),
                                      offline_queue_.get(),
                                      config_.server.group_rotation_threshold);
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
