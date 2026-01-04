#ifndef MI_E2EE_SERVER_GROUP_CALL_MANAGER_H
#define MI_E2EE_SERVER_GROUP_CALL_MANAGER_H

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mi::server {

enum class GroupCallOp : std::uint8_t {
  kCreate = 1,
  kJoin = 2,
  kLeave = 3,
  kEnd = 4,
  kUpdate = 5,
  kPing = 6
};

constexpr std::uint8_t kGroupCallMediaAudio = 0x01;
constexpr std::uint8_t kGroupCallMediaVideo = 0x02;

struct GroupCallConfig {
  bool enable_group_call{false};
  std::uint32_t max_room_size{1000};
  std::uint32_t idle_timeout_sec{60};
  std::uint32_t call_timeout_sec{3600};
  std::uint32_t max_subscriptions{0};
};

struct GroupCallSubscription {
  std::string sender;
  std::uint8_t media_flags{0};
};

struct GroupCallEvent {
  GroupCallOp op{GroupCallOp::kCreate};
  std::string group_id;
  std::array<std::uint8_t, 16> call_id{};
  std::uint32_t key_id{0};
  std::string sender;
  std::uint8_t media_flags{0};
  std::uint64_t ts_ms{0};
};

struct GroupCallSnapshot {
  std::string group_id;
  std::string owner;
  std::array<std::uint8_t, 16> call_id{};
  std::uint32_t key_id{0};
  std::uint8_t media_flags{0};
  std::vector<std::string> members;
};

struct GroupCallStats {
  std::uint64_t active_calls{0};
  std::uint64_t participants{0};
};

class GroupCallManager {
 public:
  explicit GroupCallManager(GroupCallConfig config = {});

  bool enabled() const { return config_.enable_group_call; }
  const GroupCallConfig& config() const { return config_; }

  bool CreateCall(const std::string& group_id,
                  const std::string& owner,
                  std::uint8_t media_flags,
                  std::array<std::uint8_t, 16>& out_call_id,
                  GroupCallSnapshot& out_snapshot,
                  std::string& error);

  bool JoinCall(const std::string& group_id,
                const std::array<std::uint8_t, 16>& call_id,
                const std::string& username,
                std::uint8_t media_flags,
                GroupCallSnapshot& out_snapshot,
                std::string& error);

  bool LeaveCall(const std::string& group_id,
                 const std::array<std::uint8_t, 16>& call_id,
                 const std::string& username,
                 GroupCallSnapshot& out_snapshot,
                 bool& out_ended,
                 std::string& error);

  bool EndCall(const std::string& group_id,
               const std::array<std::uint8_t, 16>& call_id,
               const std::string& username,
               GroupCallSnapshot& out_snapshot,
               std::string& error);

  bool TouchCall(const std::array<std::uint8_t, 16>& call_id,
                 const std::string& username,
                 GroupCallSnapshot& out_snapshot,
                 std::string& error);

  bool GetCall(const std::array<std::uint8_t, 16>& call_id,
               GroupCallSnapshot& out_snapshot) const;

  bool GetUserCallId(const std::string& username,
                     std::array<std::uint8_t, 16>& out_call_id) const;

  bool UpdateSubscriptions(const std::array<std::uint8_t, 16>& call_id,
                           const std::string& recipient,
                           const std::vector<GroupCallSubscription>& subs,
                           std::string& error);
  bool IsSubscribed(const std::array<std::uint8_t, 16>& call_id,
                    const std::string& recipient,
                    const std::string& sender,
                    std::uint8_t media_flag) const;

  void EnqueueEvent(const std::string& recipient, GroupCallEvent event);
  void EnqueueEventForMembers(const std::vector<std::string>& members,
                              const GroupCallEvent& event);

  void PullEvents(const std::string& recipient,
                  std::size_t max_events,
                  std::chrono::milliseconds wait,
                  std::vector<GroupCallEvent>& out);

  void Cleanup();
  GroupCallStats GetStats();

 private:
  struct CallState {
    std::string group_id;
    std::string owner;
    std::array<std::uint8_t, 16> call_id{};
    std::uint32_t key_id{1};
    std::uint8_t media_flags{0};
    std::unordered_set<std::string> members;
    std::chrono::steady_clock::time_point created_at{};
    std::chrono::steady_clock::time_point last_active{};
    struct SubscriptionState {
      std::unordered_map<std::string, std::uint8_t> senders;
      std::chrono::steady_clock::time_point updated_at{};
    };
    std::unordered_map<std::string, SubscriptionState> subscriptions;
  };

  struct EventQueue {
    struct StoredEvent {
      GroupCallEvent event;
      std::chrono::steady_clock::time_point created_at{};
    };
    std::deque<StoredEvent> events;
    std::chrono::steady_clock::time_point last_seen{};
  };

  struct Bucket {
    std::mutex mutex;
    std::condition_variable cv;
    std::unordered_map<std::string, EventQueue> queues;
  };

  static constexpr std::size_t kBucketCount = 64;

  static std::string CallIdKey(const std::array<std::uint8_t, 16>& call_id);
  static bool IsAllZero(const std::array<std::uint8_t, 16>& call_id);

  bool GenerateUniqueCallId(std::array<std::uint8_t, 16>& out_call_id);
  GroupCallSnapshot BuildSnapshotLocked(const CallState& state) const;

  Bucket& BucketForKey(const std::string& key);

  GroupCallConfig config_;
  std::chrono::seconds call_timeout_{std::chrono::seconds(3600)};
  std::chrono::seconds idle_timeout_{std::chrono::seconds(60)};
  std::chrono::seconds event_ttl_{std::chrono::minutes(5)};
  std::size_t max_event_queue_{256};

  mutable std::mutex mutex_;
  std::unordered_map<std::string, CallState> calls_by_id_;
  std::unordered_map<std::string, std::string> call_by_group_;
  std::unordered_map<std::string, std::string> call_by_user_;

  std::array<Bucket, kBucketCount> buckets_{};
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_GROUP_CALL_MANAGER_H
