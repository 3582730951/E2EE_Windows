#ifndef MI_E2EE_SERVER_APP_H
#define MI_E2EE_SERVER_APP_H

#include <memory>
#include <string>

#include "auth_provider.h"
#include "config.h"
#include "group_directory.h"
#include "group_call_manager.h"
#include "group_manager.h"
#include "frame.h"
#include "frame_router.h"
#include "media_relay.h"
#include "offline_storage.h"
#include "secure_channel.h"
#include "session_manager.h"

namespace mi::server {

class ApiService;

class ServerApp {
 public:
  ServerApp();
  ~ServerApp();

  bool Init(const std::string& config_path, std::string& error);

  bool RunOnce(std::string& error);  // 占位：未来替换为事件循环/监听

  bool HandleFrame(const Frame& in, Frame& out, TransportKind transport,
                   std::string& error);
  bool HandleFrameView(const FrameView& in, Frame& out, TransportKind transport,
                       std::string& error);
  bool HandleFrameWithToken(const Frame& in, Frame& out,
                            const std::string& token, TransportKind transport,
                            std::string& error);
  bool HandleFrameWithTokenView(const FrameView& in, Frame& out,
                                const std::string& token,
                                TransportKind transport,
                                std::string& error);

  const ServerConfig& config() const { return config_; }
  SessionManager* sessions() { return sessions_.get(); }
  const SessionManager* sessions() const { return sessions_.get(); }
  OfflineStorage* offline_storage() { return offline_storage_.get(); }
  OfflineQueue* offline_queue() { return offline_queue_.get(); }
  MediaRelay* media_relay() { return media_relay_.get(); }
  GroupCallManager* group_calls() { return group_calls_.get(); }

 private:
  ServerConfig config_;
  std::unique_ptr<AuthProvider> auth_;
  std::unique_ptr<SessionManager> sessions_;
  std::unique_ptr<GroupManager> groups_;
  std::unique_ptr<GroupCallManager> group_calls_;
  std::unique_ptr<GroupDirectory> directory_;
  std::unique_ptr<OfflineStorage> offline_storage_;
  std::unique_ptr<OfflineQueue> offline_queue_;
  std::unique_ptr<MediaRelay> media_relay_;
  std::unique_ptr<ApiService> api_;
  std::unique_ptr<FrameRouter> router_;
  std::chrono::steady_clock::time_point last_cleanup_{};
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_APP_H
