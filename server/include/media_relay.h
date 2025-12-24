#ifndef MI_E2EE_SERVER_MEDIA_RELAY_H
#define MI_E2EE_SERVER_MEDIA_RELAY_H

#include <array>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mi::server {

struct MediaRelayPacket {
  std::string sender;
  std::vector<std::uint8_t> payload;
  std::chrono::steady_clock::time_point created_at{};
};

class MediaRelay {
 public:
  explicit MediaRelay(std::size_t max_queue = 2048,
                      std::chrono::milliseconds ttl = std::chrono::seconds(5));

  void Enqueue(const std::string& recipient,
               const std::array<std::uint8_t, 16>& call_id,
               MediaRelayPacket packet);

  void Pull(const std::string& recipient,
            const std::array<std::uint8_t, 16>& call_id,
            std::size_t max_packets,
            std::chrono::milliseconds wait,
            std::vector<MediaRelayPacket>& out);

  void Cleanup();

 private:
  struct Queue {
    std::deque<MediaRelayPacket> packets;
    std::chrono::steady_clock::time_point last_seen{};
  };

  std::mutex mutex_;
  std::condition_variable cv_;
  std::unordered_map<std::string, Queue> queues_;
  std::size_t max_queue_{0};
  std::chrono::milliseconds ttl_{0};
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_MEDIA_RELAY_H
