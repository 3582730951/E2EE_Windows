#include "media_relay.h"

namespace mi::server {

namespace {
std::string MakeKey(const std::string& recipient,
                    const std::array<std::uint8_t, 16>& call_id) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string key;
  key.reserve(recipient.size() + 1 + call_id.size() * 2);
  key.append(recipient);
  key.push_back('|');
  for (const auto b : call_id) {
    key.push_back(kHex[b >> 4]);
    key.push_back(kHex[b & 0x0F]);
  }
  return key;
}
}  // namespace

MediaRelay::MediaRelay(std::size_t max_queue, std::chrono::milliseconds ttl)
    : max_queue_(max_queue), ttl_(ttl) {}

void MediaRelay::Enqueue(const std::string& recipient,
                         const std::array<std::uint8_t, 16>& call_id,
                         MediaRelayPacket packet) {
  if (recipient.empty()) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  packet.created_at = now;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& q = queues_[MakeKey(recipient, call_id)];
    q.last_seen = now;
    q.packets.push_back(std::move(packet));
    while (q.packets.size() > max_queue_) {
      q.packets.pop_front();
    }
  }
  cv_.notify_all();
}

void MediaRelay::Pull(const std::string& recipient,
                      const std::array<std::uint8_t, 16>& call_id,
                      std::size_t max_packets,
                      std::chrono::milliseconds wait,
                      std::vector<MediaRelayPacket>& out) {
  out.clear();
  if (recipient.empty() || max_packets == 0) {
    return;
  }
  const auto deadline = std::chrono::steady_clock::now() + wait;
  std::unique_lock<std::mutex> lock(mutex_);
  const std::string key = MakeKey(recipient, call_id);
  const auto has_data = [&]() {
    const auto it = queues_.find(key);
    return it != queues_.end() && !it->second.packets.empty();
  };
  if (!has_data() && wait.count() > 0) {
    cv_.wait_until(lock, deadline, has_data);
  }
  auto it = queues_.find(key);
  if (it == queues_.end() || it->second.packets.empty()) {
    return;
  }
  auto& q = it->second;
  const std::size_t count =
      std::min<std::size_t>(max_packets, q.packets.size());
  out.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    out.push_back(std::move(q.packets.front()));
    q.packets.pop_front();
  }
  q.last_seen = std::chrono::steady_clock::now();
}

void MediaRelay::Cleanup() {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = queues_.begin(); it != queues_.end();) {
    auto& q = it->second;
    while (!q.packets.empty()) {
      const auto age = now - q.packets.front().created_at;
      if (age <= ttl_) {
        break;
      }
      q.packets.pop_front();
    }
    if (q.packets.empty() && now - q.last_seen > ttl_) {
      it = queues_.erase(it);
      continue;
    }
    ++it;
  }
}

}  // namespace mi::server
