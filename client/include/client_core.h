#ifndef MI_E2EE_CLIENT_CORE_H
#define MI_E2EE_CLIENT_CORE_H

#include <cstdint>
#include <string>
#include <vector>

#include "../server/include/frame.h"
#include "../server/include/pake.h"
#include "../server/include/secure_channel.h"

struct mi_server_handle;

namespace mi::client {

class ClientCore {
 public:
  struct FriendEntry {
    std::string username;
    std::string remark;
  };

  ClientCore();
  ~ClientCore();

  bool Init(const std::string& config_path = "config.ini");
  bool Login(const std::string& username, const std::string& password);
  bool Logout();

  bool JoinGroup(const std::string& group_id);
  bool SendGroupMessage(const std::string& group_id,
                        std::uint32_t threshold = 10000);
  bool SendOffline(const std::string& recipient,
                   const std::vector<std::uint8_t>& payload);
  std::vector<std::vector<std::uint8_t>> PullOffline();

  std::vector<FriendEntry> ListFriends();
  bool AddFriend(const std::string& friend_username,
                 const std::string& remark = "");
  bool SetFriendRemark(const std::string& friend_username,
                       const std::string& remark);

  const std::string& token() const { return token_; }

 private:
  bool EnsureChannel();
  bool ProcessEncrypted(mi::server::FrameType type,
                        const std::vector<std::uint8_t>& plain,
                        std::vector<std::uint8_t>& out_plain);
  bool ProcessRaw(const std::vector<std::uint8_t>& in_bytes,
                  std::vector<std::uint8_t>& out_bytes);

  mi_server_handle* local_handle_{nullptr};
  bool remote_mode_{false};
  std::string server_ip_;
  std::uint16_t server_port_{0};
  std::string config_path_{"config.ini"};
  std::string username_;
  std::string password_;
  std::string token_;
  mi::server::DerivedKeys keys_{};
  mi::server::SecureChannel channel_;
  std::uint64_t send_seq_{0};
  std::uint64_t recv_seq_{0};
};

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_CORE_H
