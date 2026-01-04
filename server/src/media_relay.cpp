#include "media_relay.h"

#include <functional>

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

MediaRelay::Bucket& MediaRelay::BucketForKey(const std::string& key) {
  const std::size_t idx = std::hash<std::string>{}(key) % kBucketCount;
  return buckets_[idx];
}

void MediaRelay::Enqueue(const std::string& recipient,
                         const std::array<std::uint8_t, 16>& call_id,
                         MediaRelayPacket packet) {
  if (recipient.empty()) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  packet.created_at = now;
  const std::string key = MakeKey(recipient, call_id);
  auto& bucket = BucketForKey(key);
  {
    std::lock_guard<std::mutex> lock(bucket.mutex);
    auto& q = bucket.queues[key];
    q.last_seen = now;
    q.packets.push_back(std::move(packet));
    while (q.packets.size() > max_queue_) {
      q.packets.pop_front();
    }
  }
  bucket.cv.notify_all();
}

void MediaRelay::EnqueueMany(const std::vector<std::string>& recipients,
                             const std::array<std::uint8_t, 16>& call_id,
                             const MediaRelayPacket& packet) {
  if (recipients.empty()) {
    return;
  }
  for (const auto& recipient : recipients) {
    if (recipient.empty()) {
      continue;
    }
    MediaRelayPacket copy;
    copy.sender = packet.sender;
    copy.payload = packet.payload;
    Enqueue(recipient, call_id, std::move(copy));
  }
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
  const std::string key = MakeKey(recipient, call_id);
  auto& bucket = BucketForKey(key);
  std::unique_lock<std::mutex> lock(bucket.mutex);
  const auto has_data = [&]() {
    const auto it = bucket.queues.find(key);
    return it != bucket.queues.end() && !it->second.packets.empty();
  };
  if (!has_data() && wait.count() > 0) {
    bucket.cv.wait_until(lock, deadline, has_data);
  }
  auto it = bucket.queues.find(key);
  if (it == bucket.queues.end() || it->second.packets.empty()) {
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
  for (auto& bucket : buckets_) {
    std::lock_guard<std::mutex> lock(bucket.mutex);
    for (auto it = bucket.queues.begin(); it != bucket.queues.end();) {
      auto& q = it->second;
      while (!q.packets.empty()) {
        const auto age = now - q.packets.front().created_at;
        if (age <= ttl_) {
          break;
        }
        q.packets.pop_front();
      }
      if (q.packets.empty() && now - q.last_seen > ttl_) {
        it = bucket.queues.erase(it);
        continue;
      }
      ++it;
    }
  }
}

MediaRelayStats MediaRelay::GetStats() {
  MediaRelayStats stats;
  for (auto& bucket : buckets_) {
    std::lock_guard<std::mutex> lock(bucket.mutex);
    stats.queues += bucket.queues.size();
    for (const auto& kv : bucket.queues) {
      stats.packets += kv.second.packets.size();
    }
  }
  return stats;
}

}  // namespace mi::server
