#include "group_call_manager.h"

#include <algorithm>
#include <cstring>

#include "crypto.h"

namespace mi::server {

namespace {
std::uint64_t NowMs() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}
}  // namespace

GroupCallManager::GroupCallManager(GroupCallConfig config)
    : config_(config) {
  call_timeout_ = std::chrono::seconds(config_.call_timeout_sec);
  idle_timeout_ = std::chrono::seconds(config_.idle_timeout_sec);
  const std::uint32_t ttl =
      std::max<std::uint32_t>(config_.idle_timeout_sec, 60);
  event_ttl_ = std::chrono::seconds(ttl);
}

bool GroupCallManager::IsAllZero(const std::array<std::uint8_t, 16>& call_id) {
  for (const auto b : call_id) {
    if (b != 0) {
      return false;
    }
  }
  return true;
}

std::string GroupCallManager::CallIdKey(
    const std::array<std::uint8_t, 16>& call_id) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string key;
  key.reserve(call_id.size() * 2);
  for (const auto b : call_id) {
    key.push_back(kHex[b >> 4]);
    key.push_back(kHex[b & 0x0F]);
  }
  return key;
}

bool GroupCallManager::GenerateUniqueCallId(
    std::array<std::uint8_t, 16>& out_call_id) {
  for (int i = 0; i < 4; ++i) {
    if (!crypto::RandomBytes(out_call_id.data(), out_call_id.size())) {
      return false;
    }
    const std::string key = CallIdKey(out_call_id);
    if (calls_by_id_.find(key) == calls_by_id_.end()) {
      return true;
    }
  }
  return false;
}

GroupCallSnapshot GroupCallManager::BuildSnapshotLocked(
    const CallState& state) const {
  GroupCallSnapshot snap;
  snap.group_id = state.group_id;
  snap.owner = state.owner;
  snap.call_id = state.call_id;
  snap.key_id = state.key_id;
  snap.media_flags = state.media_flags;
  snap.members.reserve(state.members.size());
  for (const auto& m : state.members) {
    snap.members.push_back(m);
  }
  return snap;
}

GroupCallManager::Bucket& GroupCallManager::BucketForKey(
    const std::string& key) {
  const std::size_t idx = std::hash<std::string>{}(key) % kBucketCount;
  return buckets_[idx];
}

bool GroupCallManager::CreateCall(
    const std::string& group_id,
    const std::string& owner,
    std::uint8_t media_flags,
    std::array<std::uint8_t, 16>& out_call_id,
    GroupCallSnapshot& out_snapshot,
    std::string& error) {
  error.clear();
  out_snapshot = GroupCallSnapshot{};
  if (!config_.enable_group_call) {
    error = "group call disabled";
    return false;
  }
  if (group_id.empty() || owner.empty()) {
    error = "invalid params";
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (call_by_group_.find(group_id) != call_by_group_.end()) {
    error = "call already active";
    return false;
  }
  if (call_by_user_.find(owner) != call_by_user_.end()) {
    error = "already in call";
    return false;
  }

  std::array<std::uint8_t, 16> call_id = out_call_id;
  if (IsAllZero(call_id)) {
    if (!GenerateUniqueCallId(call_id)) {
      error = "call id generate failed";
      return false;
    }
  } else {
    const std::string key = CallIdKey(call_id);
    if (calls_by_id_.find(key) != calls_by_id_.end()) {
      error = "call id conflict";
      return false;
    }
  }

  CallState state;
  state.group_id = group_id;
  state.owner = owner;
  state.call_id = call_id;
  state.key_id = 1;
  state.media_flags = media_flags;
  state.members.insert(owner);
  state.created_at = std::chrono::steady_clock::now();
  state.last_active = state.created_at;

  const std::string id_key = CallIdKey(call_id);
  calls_by_id_[id_key] = state;
  call_by_group_[group_id] = id_key;
  call_by_user_[owner] = id_key;

  out_call_id = call_id;
  out_snapshot = BuildSnapshotLocked(state);
  return true;
}

bool GroupCallManager::JoinCall(
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    const std::string& username,
    std::uint8_t media_flags,
    GroupCallSnapshot& out_snapshot,
    std::string& error) {
  error.clear();
  out_snapshot = GroupCallSnapshot{};
  if (!config_.enable_group_call) {
    error = "group call disabled";
    return false;
  }
  if (group_id.empty() || username.empty()) {
    error = "invalid params";
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (call_by_user_.find(username) != call_by_user_.end()) {
    error = "already in call";
    return false;
  }

  const std::string id_key = CallIdKey(call_id);
  auto it = calls_by_id_.find(id_key);
  if (it == calls_by_id_.end()) {
    error = "call not found";
    return false;
  }
  CallState& state = it->second;
  if (state.group_id != group_id) {
    error = "call mismatch";
    return false;
  }
  if (config_.max_room_size > 0 &&
      state.members.size() >= config_.max_room_size) {
    error = "room full";
    return false;
  }
  if (state.members.insert(username).second) {
    state.key_id++;
  }
  state.media_flags = media_flags;
  state.last_active = std::chrono::steady_clock::now();
  call_by_user_[username] = id_key;

  out_snapshot = BuildSnapshotLocked(state);
  return true;
}

bool GroupCallManager::LeaveCall(
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    const std::string& username,
    GroupCallSnapshot& out_snapshot,
    bool& out_ended,
    std::string& error) {
  error.clear();
  out_snapshot = GroupCallSnapshot{};
  out_ended = false;
  if (!config_.enable_group_call) {
    error = "group call disabled";
    return false;
  }
  if (group_id.empty() || username.empty()) {
    error = "invalid params";
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  const std::string id_key = CallIdKey(call_id);
  auto it = calls_by_id_.find(id_key);
  if (it == calls_by_id_.end()) {
    error = "call not found";
    return false;
  }
  CallState& state = it->second;
  if (state.group_id != group_id) {
    error = "call mismatch";
    return false;
  }
  if (state.members.find(username) == state.members.end()) {
    error = "not in call";
    return false;
  }

  out_snapshot = BuildSnapshotLocked(state);
  state.members.erase(username);
  call_by_user_.erase(username);
  state.subscriptions.erase(username);
  for (auto& kv : state.subscriptions) {
    kv.second.senders.erase(username);
  }

  if (state.members.empty() || state.owner == username) {
    calls_by_id_.erase(it);
    call_by_group_.erase(group_id);
    for (const auto& member : out_snapshot.members) {
      call_by_user_.erase(member);
    }
    out_ended = true;
    return true;
  }

  state.key_id++;
  state.last_active = std::chrono::steady_clock::now();
  out_snapshot = BuildSnapshotLocked(state);
  return true;
}

bool GroupCallManager::EndCall(
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    const std::string& username,
    GroupCallSnapshot& out_snapshot,
    std::string& error) {
  error.clear();
  out_snapshot = GroupCallSnapshot{};
  if (!config_.enable_group_call) {
    error = "group call disabled";
    return false;
  }
  if (group_id.empty() || username.empty()) {
    error = "invalid params";
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  const std::string id_key = CallIdKey(call_id);
  auto it = calls_by_id_.find(id_key);
  if (it == calls_by_id_.end()) {
    error = "call not found";
    return false;
  }
  CallState& state = it->second;
  if (state.group_id != group_id) {
    error = "call mismatch";
    return false;
  }
  if (state.members.find(username) == state.members.end()) {
    error = "not in call";
    return false;
  }
  out_snapshot = BuildSnapshotLocked(state);
  calls_by_id_.erase(it);
  call_by_group_.erase(group_id);
  for (const auto& member : out_snapshot.members) {
    call_by_user_.erase(member);
  }
  return true;
}

bool GroupCallManager::TouchCall(
    const std::array<std::uint8_t, 16>& call_id,
    const std::string& username,
    GroupCallSnapshot& out_snapshot,
    std::string& error) {
  error.clear();
  out_snapshot = GroupCallSnapshot{};
  if (!config_.enable_group_call) {
    error = "group call disabled";
    return false;
  }
  if (username.empty()) {
    error = "invalid params";
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  const std::string id_key = CallIdKey(call_id);
  auto it = calls_by_id_.find(id_key);
  if (it == calls_by_id_.end()) {
    error = "call not found";
    return false;
  }
  CallState& state = it->second;
  if (state.members.find(username) == state.members.end()) {
    error = "not in call";
    return false;
  }
  state.last_active = std::chrono::steady_clock::now();
  out_snapshot = BuildSnapshotLocked(state);
  return true;
}

bool GroupCallManager::GetCall(
    const std::array<std::uint8_t, 16>& call_id,
    GroupCallSnapshot& out_snapshot) const {
  out_snapshot = GroupCallSnapshot{};
  std::lock_guard<std::mutex> lock(mutex_);
  const std::string id_key = CallIdKey(call_id);
  auto it = calls_by_id_.find(id_key);
  if (it == calls_by_id_.end()) {
    return false;
  }
  out_snapshot = BuildSnapshotLocked(it->second);
  return true;
}

bool GroupCallManager::GetUserCallId(
    const std::string& username,
    std::array<std::uint8_t, 16>& out_call_id) const {
  out_call_id.fill(0);
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = call_by_user_.find(username);
  if (it == call_by_user_.end()) {
    return false;
  }
  const std::string& id_key = it->second;
  auto it_call = calls_by_id_.find(id_key);
  if (it_call == calls_by_id_.end()) {
    return false;
  }
  out_call_id = it_call->second.call_id;
  return true;
}

bool GroupCallManager::UpdateSubscriptions(
    const std::array<std::uint8_t, 16>& call_id,
    const std::string& recipient,
    const std::vector<GroupCallSubscription>& subs,
    std::string& error) {
  error.clear();
  if (!config_.enable_group_call) {
    error = "group call disabled";
    return false;
  }
  if (recipient.empty()) {
    error = "recipient empty";
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  const std::string id_key = CallIdKey(call_id);
  auto it = calls_by_id_.find(id_key);
  if (it == calls_by_id_.end()) {
    error = "call not found";
    return false;
  }
  CallState& state = it->second;
  if (state.members.find(recipient) == state.members.end()) {
    error = "not in call";
    return false;
  }
  auto& entry = state.subscriptions[recipient];
  entry.senders.clear();
  entry.updated_at = std::chrono::steady_clock::now();

  const std::size_t max_subs =
      config_.max_subscriptions == 0
          ? subs.size()
          : static_cast<std::size_t>(config_.max_subscriptions);
  for (const auto& sub : subs) {
    if (entry.senders.size() >= max_subs) {
      break;
    }
    if (sub.sender.empty() || sub.sender == recipient) {
      continue;
    }
    if (state.members.find(sub.sender) == state.members.end()) {
      continue;
    }
    const std::uint8_t flags =
        static_cast<std::uint8_t>(sub.media_flags &
                                  (kGroupCallMediaAudio | kGroupCallMediaVideo));
    if (flags == 0) {
      continue;
    }
    entry.senders[sub.sender] = flags;
  }
  return true;
}

bool GroupCallManager::IsSubscribed(
    const std::array<std::uint8_t, 16>& call_id,
    const std::string& recipient,
    const std::string& sender,
    std::uint8_t media_flag) const {
  if (recipient.empty() || sender.empty() || recipient == sender) {
    return false;
  }
  if ((media_flag & (kGroupCallMediaAudio | kGroupCallMediaVideo)) == 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  const std::string id_key = CallIdKey(call_id);
  auto it = calls_by_id_.find(id_key);
  if (it == calls_by_id_.end()) {
    return false;
  }
  const CallState& state = it->second;
  if (state.members.find(recipient) == state.members.end() ||
      state.members.find(sender) == state.members.end()) {
    return false;
  }
  auto sub_it = state.subscriptions.find(recipient);
  if (sub_it == state.subscriptions.end()) {
    return true;
  }
  const auto sender_it = sub_it->second.senders.find(sender);
  if (sender_it == sub_it->second.senders.end()) {
    return false;
  }
  return (sender_it->second & media_flag) != 0;
}

void GroupCallManager::EnqueueEvent(const std::string& recipient,
                                    GroupCallEvent event) {
  if (recipient.empty()) {
    return;
  }
  if (event.ts_ms == 0) {
    event.ts_ms = NowMs();
  }
  GroupCallManager::EventQueue::StoredEvent stored;
  stored.event = std::move(event);
  stored.created_at = std::chrono::steady_clock::now();
  auto& bucket = BucketForKey(recipient);
  {
    std::lock_guard<std::mutex> lock(bucket.mutex);
    auto& queue = bucket.queues[recipient];
    queue.last_seen = std::chrono::steady_clock::now();
    queue.events.push_back(std::move(stored));
    while (queue.events.size() > max_event_queue_) {
      queue.events.pop_front();
    }
  }
  bucket.cv.notify_all();
}

void GroupCallManager::EnqueueEventForMembers(
    const std::vector<std::string>& members,
    const GroupCallEvent& event) {
  for (const auto& member : members) {
    EnqueueEvent(member, event);
  }
}

void GroupCallManager::PullEvents(const std::string& recipient,
                                  std::size_t max_events,
                                  std::chrono::milliseconds wait,
                                  std::vector<GroupCallEvent>& out) {
  out.clear();
  if (recipient.empty() || max_events == 0) {
    return;
  }
  const auto deadline = std::chrono::steady_clock::now() + wait;
  auto& bucket = BucketForKey(recipient);
  std::unique_lock<std::mutex> lock(bucket.mutex);
  const auto has_data = [&]() {
    const auto it = bucket.queues.find(recipient);
    return it != bucket.queues.end() && !it->second.events.empty();
  };
  if (!has_data() && wait.count() > 0) {
    bucket.cv.wait_until(lock, deadline, has_data);
  }
  auto it = bucket.queues.find(recipient);
  if (it == bucket.queues.end() || it->second.events.empty()) {
    return;
  }
  auto& queue = it->second;
  const std::size_t count =
      std::min<std::size_t>(max_events, queue.events.size());
  out.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    out.push_back(std::move(queue.events.front().event));
    queue.events.pop_front();
  }
  queue.last_seen = std::chrono::steady_clock::now();
}

void GroupCallManager::Cleanup() {
  const auto now = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = calls_by_id_.begin(); it != calls_by_id_.end();) {
      CallState& state = it->second;
      const bool expired =
          (call_timeout_.count() > 0 &&
           now - state.created_at > call_timeout_) ||
          (idle_timeout_.count() > 0 &&
           now - state.last_active > idle_timeout_);
      if (expired) {
        for (const auto& member : state.members) {
          call_by_user_.erase(member);
        }
        call_by_group_.erase(state.group_id);
        it = calls_by_id_.erase(it);
        continue;
      }
      ++it;
    }
  }

  for (auto& bucket : buckets_) {
    std::lock_guard<std::mutex> lock(bucket.mutex);
    for (auto it = bucket.queues.begin(); it != bucket.queues.end();) {
      auto& queue = it->second;
      while (!queue.events.empty()) {
        const auto age = now - queue.events.front().created_at;
        if (age <= event_ttl_) {
          break;
        }
        queue.events.pop_front();
      }
      if (queue.events.empty() && now - queue.last_seen > event_ttl_) {
        it = bucket.queues.erase(it);
        continue;
      }
      ++it;
    }
  }
}

GroupCallStats GroupCallManager::GetStats() {
  GroupCallStats stats;
  std::lock_guard<std::mutex> lock(mutex_);
  stats.active_calls = calls_by_id_.size();
  std::uint64_t participants = 0;
  for (const auto& kv : calls_by_id_) {
    participants += kv.second.members.size();
  }
  stats.participants = participants;
  return stats;
}

}  // namespace mi::server
