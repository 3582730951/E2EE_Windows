#ifndef MI_E2EE_SERVER_CONNECTION_HANDLER_H
#define MI_E2EE_SERVER_CONNECTION_HANDLER_H

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>

#include "frame.h"
#include "server_app.h"
#include "secure_channel.h"

namespace mi::server {

class ConnectionHandler {
 public:
  explicit ConnectionHandler(ServerApp* app);

  // 
  bool OnData(const std::uint8_t* data, std::size_t len,
              std::vector<std::uint8_t>& out_bytes,
              const std::string& remote_ip);

 private:
  struct ChannelState {
    SecureChannel channel;
    std::uint64_t send_seq{0};
    std::mutex mutex;
  };

  struct IpRateBucket {
    double tokens{0.0};
    std::chrono::steady_clock::time_point last{};
    std::chrono::steady_clock::time_point last_seen{};
  };

  struct UnauthIpState {
    IpRateBucket bucket;
    std::uint32_t failures{0};
    std::chrono::steady_clock::time_point first_failure{};
    std::chrono::steady_clock::time_point ban_until{};
  };

  struct AuthTokenState {
    std::uint32_t failures{0};
    std::chrono::steady_clock::time_point first_failure{};
    std::chrono::steady_clock::time_point ban_until{};
    std::chrono::steady_clock::time_point last_seen{};
  };

  struct OpsMetrics {
    std::chrono::steady_clock::time_point started_at{};
    std::atomic<std::uint64_t> decode_fail{0};
    std::atomic<std::uint64_t> requests_total{0};
    std::atomic<std::uint64_t> requests_ok{0};
    std::atomic<std::uint64_t> requests_fail{0};
    std::atomic<std::uint64_t> rate_limited{0};
    std::atomic<std::uint64_t> total_latency_us{0};
    std::atomic<std::uint64_t> max_latency_us{0};
  };

  bool AllowUnauthByIp(const std::string& remote_ip);
  void ReportUnauthOutcome(const std::string& remote_ip, bool success);
  void CleanupUnauthStateLocked(std::chrono::steady_clock::time_point now);

  bool IsAuthTokenBanned(const std::string& token);
  void ReportAuthDecryptFailure(const std::string& token);
  void ClearAuthDecryptFailures(const std::string& token);
  void CleanupAuthTokenStateLocked(std::chrono::steady_clock::time_point now);

  ServerApp* app_;
  std::mutex mutex_;
  std::mutex channel_mutex_;
  std::unordered_map<std::string, UnauthIpState> unauth_by_ip_;
  std::uint64_t unauth_ops_{0};
  std::unordered_map<std::string, AuthTokenState> auth_by_token_;
  std::uint64_t auth_ops_{0};
  OpsMetrics metrics_;
  std::unordered_map<std::string, std::shared_ptr<ChannelState>> channel_states_;
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_CONNECTION_HANDLER_H
