#ifndef MI_E2EE_CLIENT_CORE_H
#define MI_E2EE_CLIENT_CORE_H

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../server/include/frame.h"
#include "../server/include/pake.h"
#include "../server/include/secure_channel.h"

#include "client_config.h"
#include "e2ee_engine.h"

struct mi_server_handle;

namespace mi::client {

class ChatHistoryStore;

class ClientCore {
 public:
  struct FriendEntry {
    std::string username;
    std::string remark;
  };

  struct FriendRequestEntry {
    std::string requester_username;
    std::string requester_remark;
  };

  struct ChatTextMessage {
    std::string from_username;
    std::string message_id_hex;
    std::string text_utf8;
  };

  struct ChatFileMessage {
    std::string from_username;
    std::string message_id_hex;
    std::string file_id;
    std::array<std::uint8_t, 32> file_key{};
    std::string file_name;
    std::uint64_t file_size{0};
  };

  struct ChatStickerMessage {
    std::string from_username;
    std::string message_id_hex;
    std::string sticker_id;
  };

  struct GroupChatTextMessage {
    std::string group_id;
    std::string from_username;
    std::string message_id_hex;
    std::string text_utf8;
  };

  struct GroupChatFileMessage {
    std::string group_id;
    std::string from_username;
    std::string message_id_hex;
    std::string file_id;
    std::array<std::uint8_t, 32> file_key{};
    std::string file_name;
    std::uint64_t file_size{0};
  };

  struct GroupInviteMessage {
    std::string group_id;
    std::string from_username;
    std::string message_id_hex;
  };

  enum class GroupMemberRole : std::uint8_t { kOwner = 0, kAdmin = 1, kMember = 2 };

  struct GroupMemberInfo {
    std::string username;
    GroupMemberRole role{GroupMemberRole::kMember};
  };

  struct DeviceEntry {
    std::string device_id;
    std::uint32_t last_seen_sec{0};
  };

  struct DevicePairingRequest {
    std::string device_id;
    std::string request_id_hex;
  };

  struct OutgoingChatTextMessage {
    std::string peer_username;
    std::string message_id_hex;
    std::string text_utf8;
  };

  struct OutgoingChatFileMessage {
    std::string peer_username;
    std::string message_id_hex;
    std::string file_id;
    std::array<std::uint8_t, 32> file_key{};
    std::string file_name;
    std::uint64_t file_size{0};
  };

  struct OutgoingChatStickerMessage {
    std::string peer_username;
    std::string message_id_hex;
    std::string sticker_id;
  };

  struct OutgoingGroupChatTextMessage {
    std::string group_id;
    std::string message_id_hex;
    std::string text_utf8;
  };

  struct OutgoingGroupChatFileMessage {
    std::string group_id;
    std::string message_id_hex;
    std::string file_id;
    std::array<std::uint8_t, 32> file_key{};
    std::string file_name;
    std::uint64_t file_size{0};
  };

  struct ChatDelivery {
    std::string from_username;
    std::string message_id_hex;
  };

  struct ChatReadReceipt {
    std::string from_username;
    std::string message_id_hex;
  };

  struct ChatTypingEvent {
    std::string from_username;
    bool typing{false};
  };

  struct ChatPresenceEvent {
    std::string from_username;
    bool online{false};
  };

  struct GroupNotice {
    std::string group_id;
    std::uint8_t kind{0};  // 1 join, 2 leave, 3 kick, 4 role_set
    std::string actor_username;
    std::string target_username;
    GroupMemberRole role{GroupMemberRole::kMember};
  };

  struct ChatPollResult {
    std::vector<ChatTextMessage> texts;
    std::vector<ChatFileMessage> files;
    std::vector<ChatStickerMessage> stickers;
    std::vector<GroupChatTextMessage> group_texts;
    std::vector<GroupChatFileMessage> group_files;
    std::vector<GroupInviteMessage> group_invites;
    std::vector<GroupNotice> group_notices;
    std::vector<OutgoingChatTextMessage> outgoing_texts;
    std::vector<OutgoingChatFileMessage> outgoing_files;
    std::vector<OutgoingChatStickerMessage> outgoing_stickers;
    std::vector<OutgoingGroupChatTextMessage> outgoing_group_texts;
    std::vector<OutgoingGroupChatFileMessage> outgoing_group_files;
    std::vector<ChatDelivery> deliveries;
    std::vector<ChatReadReceipt> read_receipts;
    std::vector<ChatTypingEvent> typing_events;
    std::vector<ChatPresenceEvent> presence_events;
  };

  enum class HistoryKind : std::uint8_t { kText = 1, kFile = 2, kSticker = 3, kSystem = 4 };
  enum class HistoryStatus : std::uint8_t { kSent = 0, kDelivered = 1, kRead = 2, kFailed = 3 };

  struct HistoryEntry {
    HistoryKind kind{HistoryKind::kText};
    HistoryStatus status{HistoryStatus::kSent};
    bool is_group{false};
    bool outgoing{false};
    std::uint64_t timestamp_sec{0};
    std::string conv_id;
    std::string sender;
    std::string message_id_hex;
    std::string text_utf8;

    // File fields (when kind == kFile)
    std::string file_id;
    std::array<std::uint8_t, 32> file_key{};
    std::string file_name;
    std::uint64_t file_size{0};

    // Sticker fields (when kind == kSticker)
    std::string sticker_id;
  };

  ClientCore();
  ~ClientCore();

  bool Init(const std::string& config_path = "config.ini");
  bool Register(const std::string& username, const std::string& password);
  bool Login(const std::string& username, const std::string& password);
  bool Relogin();
  bool Logout();

  bool JoinGroup(const std::string& group_id);
  bool LeaveGroup(const std::string& group_id);
  std::vector<std::string> ListGroupMembers(const std::string& group_id);
  std::vector<GroupMemberInfo> ListGroupMembersInfo(const std::string& group_id);
  bool SetGroupMemberRole(const std::string& group_id,
                          const std::string& target_username,
                          GroupMemberRole role);
  bool KickGroupMember(const std::string& group_id,
                       const std::string& target_username);
  bool SendGroupMessage(const std::string& group_id,
                        std::uint32_t threshold = 10000);
  bool CreateGroup(std::string& out_group_id);
  bool SendGroupChatText(const std::string& group_id,
                         const std::string& text_utf8,
                         std::string& out_message_id_hex);
  bool ResendGroupChatText(const std::string& group_id,
                           const std::string& message_id_hex,
                           const std::string& text_utf8);
  bool SendGroupChatFile(const std::string& group_id,
                         const std::filesystem::path& file_path,
                         std::string& out_message_id_hex);
  bool ResendGroupChatFile(const std::string& group_id,
                           const std::string& message_id_hex,
                           const std::filesystem::path& file_path);
  bool SendGroupInvite(const std::string& group_id,
                       const std::string& peer_username,
                       std::string& out_message_id_hex);
  bool SendOffline(const std::string& recipient,
                   const std::vector<std::uint8_t>& payload);
  std::vector<std::vector<std::uint8_t>> PullOffline();

  std::vector<FriendEntry> ListFriends();
  bool AddFriend(const std::string& friend_username,
                 const std::string& remark = "");
  bool SetFriendRemark(const std::string& friend_username,
                       const std::string& remark);
  bool SendFriendRequest(const std::string& target_username,
                         const std::string& requester_remark = "");
  std::vector<FriendRequestEntry> ListFriendRequests();
  bool RespondFriendRequest(const std::string& requester_username, bool accept);
  bool DeleteFriend(const std::string& friend_username);
  bool SetUserBlocked(const std::string& blocked_username, bool blocked);

  bool PublishPreKeyBundle();
  bool SendPrivateE2ee(const std::string& peer_username,
                       const std::vector<std::uint8_t>& plaintext);
  std::vector<mi::client::e2ee::PrivateMessage> PullPrivateE2ee();
  std::vector<mi::client::e2ee::PrivateMessage> DrainReadyPrivateE2ee();

  bool SendChatText(const std::string& peer_username,
                    const std::string& text_utf8,
                    std::string& out_message_id_hex);
  bool ResendChatText(const std::string& peer_username,
                      const std::string& message_id_hex,
                      const std::string& text_utf8);
  bool SendChatTextWithReply(const std::string& peer_username,
                             const std::string& text_utf8,
                             const std::string& reply_to_message_id_hex,
                             const std::string& reply_preview_utf8,
                             std::string& out_message_id_hex);
  bool ResendChatTextWithReply(const std::string& peer_username,
                               const std::string& message_id_hex,
                               const std::string& text_utf8,
                               const std::string& reply_to_message_id_hex,
                               const std::string& reply_preview_utf8);
  bool SendChatLocation(const std::string& peer_username,
                        std::int32_t lat_e7,
                        std::int32_t lon_e7,
                        const std::string& label_utf8,
                        std::string& out_message_id_hex);
  bool ResendChatLocation(const std::string& peer_username,
                          const std::string& message_id_hex,
                          std::int32_t lat_e7,
                          std::int32_t lon_e7,
                          const std::string& label_utf8);
  bool SendChatContactCard(const std::string& peer_username,
                           const std::string& card_username,
                           const std::string& card_display,
                           std::string& out_message_id_hex);
  bool ResendChatContactCard(const std::string& peer_username,
                             const std::string& message_id_hex,
                             const std::string& card_username,
                             const std::string& card_display);
  bool SendChatSticker(const std::string& peer_username,
                       const std::string& sticker_id,
                       std::string& out_message_id_hex);
  bool ResendChatSticker(const std::string& peer_username,
                         const std::string& message_id_hex,
                         const std::string& sticker_id);
  bool SendChatReadReceipt(const std::string& peer_username,
                           const std::string& message_id_hex);
  bool SendChatTyping(const std::string& peer_username, bool typing);
  bool SendChatPresence(const std::string& peer_username, bool online);
  bool SendChatFile(const std::string& peer_username,
                    const std::filesystem::path& file_path,
                    std::string& out_message_id_hex);
  bool ResendChatFile(const std::string& peer_username,
                      const std::string& message_id_hex,
                      const std::filesystem::path& file_path);
  ChatPollResult PollChat();
  bool Heartbeat();
  std::vector<DeviceEntry> ListDevices();
  bool KickDevice(const std::string& target_device_id);

  bool BeginDevicePairingPrimary(std::string& out_pairing_code);
  std::vector<DevicePairingRequest> PollDevicePairingRequests();
  bool ApproveDevicePairingRequest(const DevicePairingRequest& request);

  bool BeginDevicePairingLinked(const std::string& pairing_code);
  bool PollDevicePairingLinked(bool& out_completed);
  void CancelDevicePairing();

  bool DownloadChatFileToPath(const ChatFileMessage& file,
                              const std::filesystem::path& out_path,
                              bool wipe_after_read = true);
  bool DownloadChatFileToBytes(const ChatFileMessage& file,
                               std::vector<std::uint8_t>& out_bytes,
                               bool wipe_after_read = true);

  std::vector<HistoryEntry> LoadChatHistory(const std::string& conv_id,
                                            bool is_group,
                                            std::size_t limit = 200);
  bool AddHistorySystemMessage(const std::string& conv_id,
                               bool is_group,
                               const std::string& text_utf8);

  const std::string& token() const { return token_; }
  const std::string& last_error() const { return last_error_; }
  const std::string& device_id() const { return device_id_; }
  bool device_sync_enabled() const { return device_sync_enabled_; }
  bool device_sync_is_primary() const { return device_sync_is_primary_; }
  bool is_remote_mode() const { return remote_mode_; }
  bool remote_ok() const { return !remote_mode_ || remote_ok_; }
  const std::string& remote_error() const { return remote_error_; }
  bool HasPendingServerTrust() const { return !pending_server_pin_.empty(); }
  const std::string& pending_server_fingerprint() const {
    return pending_server_fingerprint_;
  }
  const std::string& pending_server_pin() const { return pending_server_pin_; }
  bool TrustPendingServer(const std::string& pin);

  bool HasPendingPeerTrust() const { return e2ee_.HasPendingPeerTrust(); }
  const mi::client::e2ee::PendingPeerTrust& pending_peer_trust() const {
    return e2ee_.pending_peer_trust();
  }
  bool TrustPendingPeer(const std::string& pin);

 private:
  struct CachedPeerIdentity {
    std::vector<std::uint8_t> id_sig_pk;
    std::array<std::uint8_t, 32> id_dh_pk{};
    std::string fingerprint_hex;
  };

  struct GroupSenderKeyState {
    std::string group_id;
    std::string sender_username;
    std::uint32_t version{0};
    std::uint32_t next_iteration{0};
    std::array<std::uint8_t, 32> ck{};
    std::string members_hash;
    std::uint64_t sent_count{0};
    std::unordered_map<std::uint32_t, std::array<std::uint8_t, 32>> skipped_mks;
    std::deque<std::uint32_t> skipped_order;
  };

  struct PendingSenderKeyDistribution {
    std::string group_id;
    std::uint32_t version{0};
    std::vector<std::uint8_t> envelope;
    std::unordered_set<std::string> pending_members;
    std::chrono::steady_clock::time_point last_sent{};
  };

  struct PendingGroupCipher {
    std::string group_id;
    std::string sender_username;
    std::vector<std::uint8_t> payload;
  };

  struct PendingGroupNotice {
    std::string group_id;
    std::string sender_username;
    std::vector<std::uint8_t> payload;
  };

  bool EnsureChannel();
  bool EnsureE2ee();
  bool LoadKtState();
  bool SaveKtState();
  bool EnsurePreKeyPublished();
  bool FetchPreKeyBundle(const std::string& peer_username,
                         std::vector<std::uint8_t>& out_bundle);
  bool FetchKtConsistency(std::uint64_t old_size, std::uint64_t new_size,
                          std::vector<std::array<std::uint8_t, 32>>& out_proof);
  bool ProcessEncrypted(mi::server::FrameType type,
                        const std::vector<std::uint8_t>& plain,
                        std::vector<std::uint8_t>& out_plain);
  bool ProcessRaw(const std::vector<std::uint8_t>& in_bytes,
                  std::vector<std::uint8_t>& out_bytes);

  bool SendGroupCipherMessage(const std::string& group_id,
                              const std::vector<std::uint8_t>& payload);
  std::vector<PendingGroupCipher> PullGroupCipherMessages();
  std::vector<PendingGroupNotice> PullGroupNoticeMessages();
  bool GetPeerIdentityCached(const std::string& peer_username,
                             CachedPeerIdentity& out,
                             bool require_trust);
  bool EnsureGroupSenderKeyForSend(const std::string& group_id,
                                   const std::vector<std::string>& members,
                                   GroupSenderKeyState*& out_sender_key,
                                   std::string& out_warn);

  bool LoadOrCreateDeviceId();
  bool LoadDeviceSyncKey();
  bool StoreDeviceSyncKey(const std::array<std::uint8_t, 32>& key);
  bool EncryptDeviceSync(const std::vector<std::uint8_t>& plaintext,
                         std::vector<std::uint8_t>& out_cipher);
  bool DecryptDeviceSync(const std::vector<std::uint8_t>& cipher,
                         std::vector<std::uint8_t>& out_plaintext);
  bool PushDeviceSyncCiphertext(const std::vector<std::uint8_t>& cipher);
  std::vector<std::vector<std::uint8_t>> PullDeviceSyncCiphertexts();

  void BestEffortBroadcastDeviceSyncMessage(
      bool is_group, bool outgoing, const std::string& conv_id,
      const std::string& sender, const std::vector<std::uint8_t>& envelope);
  void BestEffortBroadcastDeviceSyncDelivery(
      bool is_group, const std::string& conv_id,
      const std::array<std::uint8_t, 16>& msg_id, bool is_read);
  void BestEffortBroadcastDeviceSyncHistorySnapshot(
      const std::string& target_device_id);

  void BestEffortPersistHistoryEnvelope(
      bool is_group,
      bool outgoing,
      const std::string& conv_id,
      const std::string& sender,
      const std::vector<std::uint8_t>& envelope,
      HistoryStatus status,
      std::uint64_t timestamp_sec);
  void BestEffortPersistHistoryStatus(
      bool is_group,
      const std::string& conv_id,
      const std::array<std::uint8_t, 16>& msg_id,
      HistoryStatus status,
      std::uint64_t timestamp_sec);

  bool UploadE2eeFileBlob(const std::vector<std::uint8_t>& blob,
                          std::string& out_file_id);
  bool DownloadE2eeFileBlob(const std::string& file_id,
                            std::vector<std::uint8_t>& out_blob,
                            bool wipe_after_read);
  bool StartE2eeFileBlobUpload(std::uint64_t expected_size,
                               std::string& out_file_id,
                               std::string& out_upload_id);
  bool UploadE2eeFileBlobChunk(const std::string& file_id,
                               const std::string& upload_id,
                               std::uint64_t offset,
                               const std::vector<std::uint8_t>& chunk,
                               std::uint64_t& out_bytes_received);
  bool FinishE2eeFileBlobUpload(const std::string& file_id,
                                const std::string& upload_id,
                                std::uint64_t total_size);
  bool StartE2eeFileBlobDownload(const std::string& file_id,
                                 bool wipe_after_read,
                                 std::string& out_download_id,
                                 std::uint64_t& out_size);
  bool DownloadE2eeFileBlobChunk(const std::string& file_id,
                                 const std::string& download_id,
                                 std::uint64_t offset,
                                 std::uint32_t max_len,
                                 std::vector<std::uint8_t>& out_chunk,
                                 bool& out_eof);
  bool UploadE2eeFileBlobV3FromPath(const std::filesystem::path& file_path,
                                    std::uint64_t plaintext_size,
                                    const std::array<std::uint8_t, 32>& file_key,
                                    std::string& out_file_id);
  bool DownloadE2eeFileBlobV3ToPath(const std::string& file_id,
                                    const std::array<std::uint8_t, 32>& file_key,
                                    const std::filesystem::path& out_path,
                                    bool wipe_after_read);
  bool UploadChatFileFromPath(const std::filesystem::path& file_path,
                              std::uint64_t file_size,
                              const std::string& file_name,
                              std::array<std::uint8_t, 32>& out_file_key,
                              std::string& out_file_id);

  struct RemoteStream;
  void ResetRemoteStream();

  mi_server_handle* local_handle_{nullptr};
  bool remote_mode_{false};
  std::string server_ip_;
  std::uint16_t server_port_{0};
  bool use_tls_{false};
  AuthMode auth_mode_{AuthMode::kLegacy};
  ProxyConfig proxy_;
  std::mutex remote_stream_mutex_;
  std::unique_ptr<RemoteStream> remote_stream_;
  bool remote_ok_{true};
  std::string remote_error_;
  std::string trust_store_path_;
  std::string pinned_server_fingerprint_;
  std::string pending_server_fingerprint_;
  std::string pending_server_pin_;
  std::string config_path_{"config.ini"};
  std::string username_;
  std::string password_;
  std::string token_;
  std::string last_error_;
  mi::server::DerivedKeys keys_{};
  mi::server::SecureChannel channel_;
  std::uint64_t send_seq_{0};

  bool e2ee_inited_{false};
  bool prekey_published_{false};
  std::filesystem::path e2ee_state_dir_;
  std::unique_ptr<ChatHistoryStore> history_store_;
  std::filesystem::path kt_state_path_;
  std::uint64_t kt_tree_size_{0};
  std::array<std::uint8_t, 32> kt_root_{};
  bool device_sync_enabled_{false};
  bool device_sync_is_primary_{true};
  std::string device_id_;
  std::filesystem::path device_sync_key_path_;
  bool device_sync_key_loaded_{false};
  std::array<std::uint8_t, 32> device_sync_key_{};
   mi::client::e2ee::Engine e2ee_;

  std::unordered_map<std::string, CachedPeerIdentity> peer_id_cache_;
  std::unordered_map<std::string, GroupSenderKeyState> group_sender_keys_;
  std::unordered_map<std::string, PendingSenderKeyDistribution>
      pending_sender_key_dists_;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point>
      sender_key_req_last_sent_;
  std::deque<PendingGroupCipher> pending_group_cipher_;
  std::unordered_set<std::string> group_membership_dirty_;

  std::unordered_map<std::string, std::string> group_delivery_map_;
  std::deque<std::string> group_delivery_order_;

  bool pairing_active_{false};
  bool pairing_is_primary_{false};
  bool pairing_wait_response_{false};
  std::string pairing_id_hex_;
  std::array<std::uint8_t, 32> pairing_key_{};
  std::array<std::uint8_t, 16> pairing_request_id_{};

  std::unordered_set<std::string> chat_seen_ids_;
  std::deque<std::string> chat_seen_order_;
};

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_CORE_H
