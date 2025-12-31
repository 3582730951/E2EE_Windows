#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mi::client {

enum class ChatHistoryStatus : std::uint8_t {
  kSent = 0,
  kDelivered = 1,
  kRead = 2,
  kFailed = 3,
};

enum class ChatHistorySummaryKind : std::uint8_t {
  kNone = 0,
  kText = 1,
  kFile = 2,
  kSticker = 3,
  kLocation = 4,
  kContactCard = 5,
  kGroupInvite = 6,
  kUnknown = 255,
};

inline constexpr std::array<std::uint8_t, 4> kHistorySummaryMagic{
    {'M', 'I', 'H', 'S'}};
inline constexpr std::uint8_t kHistorySummaryVersion = 1;

struct ChatHistoryMessage {
  bool is_group{false};
  bool outgoing{false};
  bool is_system{false};
  ChatHistoryStatus status{ChatHistoryStatus::kSent};
  std::uint64_t timestamp_sec{0};
  std::string conv_id;
  std::string sender;
  std::vector<std::uint8_t> envelope;
  std::vector<std::uint8_t> summary;
  std::string system_text_utf8;
};

struct ChatHistoryConvStats {
  std::uint64_t min_ts{0};
  std::uint64_t max_ts{0};
  std::uint64_t record_count{0};
  std::uint64_t message_count{0};
};

class ChatHistoryStore {
 public:
  ChatHistoryStore();
  ~ChatHistoryStore();

  bool Init(const std::filesystem::path& e2ee_state_dir,
            const std::string& username,
            std::string& error);

  bool AppendEnvelope(bool is_group,
                      bool outgoing,
                      const std::string& conv_id,
                      const std::string& sender,
                      const std::vector<std::uint8_t>& envelope,
                      ChatHistoryStatus status,
                      std::uint64_t timestamp_sec,
                      std::string& error);

  bool AppendSystem(bool is_group,
                    const std::string& conv_id,
                    const std::string& text_utf8,
                    std::uint64_t timestamp_sec,
                    std::string& error);

  bool AppendStatusUpdate(bool is_group,
                          const std::string& conv_id,
                          const std::array<std::uint8_t, 16>& msg_id,
                          ChatHistoryStatus status,
                          std::uint64_t timestamp_sec,
                          std::string& error);

  bool StoreAttachmentPreview(const std::string& file_id,
                              const std::string& file_name,
                              std::uint64_t file_size,
                              const std::vector<std::uint8_t>& plain,
                              std::string& error);
  bool DeleteConversation(bool is_group,
                          const std::string& conv_id,
                          bool delete_attachments,
                          bool secure_wipe,
                          std::string& error);

  bool LoadConversation(bool is_group,
                        const std::string& conv_id,
                        std::size_t limit,
                        std::vector<ChatHistoryMessage>& out_messages,
                        std::string& error);

  bool ExportRecentSnapshot(std::size_t max_conversations,
                            std::size_t max_messages_per_conversation,
                            std::vector<ChatHistoryMessage>& out_messages,
                            std::string& error);

  bool Flush(std::string& error);

 private:
  struct HistoryFileEntry {
    std::filesystem::path path;
    std::uint32_t seq{0};
    std::uint8_t version{1};
    std::string tag;
    std::uint32_t internal_seq{0};
    bool has_internal_seq{false};
    std::array<std::uint8_t, 16> file_uuid{};
    std::array<std::uint8_t, 32> prev_hash{};
    bool has_prev_hash{false};
    bool chain_valid{true};
    std::uint64_t min_ts{0};
    std::uint64_t max_ts{0};
    std::uint64_t record_count{0};
    std::uint64_t message_count{0};
    bool conv_keys_complete{false};
    bool has_conv_hashes{false};
    std::vector<std::array<std::uint8_t, 16>> conv_hashes;
    std::unordered_set<std::string> conv_keys;
    std::unordered_map<std::string, ChatHistoryConvStats> conv_stats;
    bool conv_stats_complete{false};
  };
  static std::uint32_t EffectiveSeq(const HistoryFileEntry& entry);
  static void UpdateEntryStats(HistoryFileEntry& entry,
                               std::uint64_t ts,
                               bool is_message);
  static void UpdateConvStats(HistoryFileEntry& entry,
                              const std::string& conv_key,
                              std::uint64_t ts,
                              bool is_message);
  static void ValidateFileChain(std::vector<HistoryFileEntry>& files);

  bool EnsureKeyLoaded(std::string& error);
  bool EnsureTagKeyLoaded(std::string& error);
  bool EnsureProfileLoaded(const std::string& username, std::string& error);
  bool AcquireProfileLock(std::string& error);
  void ReleaseProfileLock();
  bool LoadHistoryIndex(std::string& error);
  bool SaveHistoryIndex(std::string& error);
  bool LoadHistoryJournal(std::string& error);
  bool AppendHistoryJournal(const std::vector<std::uint8_t>& plain,
                            std::string& error);
  void ClearHistoryJournal();
  bool DeriveIndexKey(std::array<std::uint8_t, 32>& out_key,
                      std::string& error) const;
  bool DeriveProfilesKey(std::array<std::uint8_t, 32>& out_key,
                         std::string& error) const;
  void RebuildConvHashIndex();
  bool EnsureConversationMapped(bool is_group,
                                const std::string& conv_id,
                                std::string& error);
  bool ScanFileForConversations(HistoryFileEntry& entry,
                                std::string& error);
  bool ScanFileForConvStats(HistoryFileEntry& entry,
                            std::string& error);
  bool EnsureAttachmentsLoaded(std::string& error);
  bool LoadAttachmentsIndex(std::string& error);
  bool SaveAttachmentsIndex(std::string& error);
  bool TouchAttachmentFromEnvelope(const std::vector<std::uint8_t>& envelope,
                                   std::uint64_t timestamp_sec,
                                   std::string& error);
  bool ReleaseAttachmentFromEnvelope(const std::vector<std::uint8_t>& envelope,
                                     std::string& error);
  bool UpdateAttachmentPreview(const std::string& file_id,
                               const std::string& file_name,
                               std::uint64_t file_size,
                               const std::vector<std::uint8_t>& plain,
                               std::string& error);

  bool DeriveConversationKey(bool is_group,
                             const std::string& conv_id,
                             std::array<std::uint8_t, 32>& out_key,
                             std::string& error) const;
  bool DeriveUserTag(const std::string& username,
                     std::string& out_tag,
                     std::string& error) const;
  bool MigrateLegacyHistoryFiles(const std::string& legacy_tag,
                                 const std::string& new_tag,
                                 std::string& error);

  bool LoadHistoryFiles(std::string& error);

  bool EnsureHistoryFile(bool is_group,
                         const std::string& conv_id,
                         std::filesystem::path& out_path,
                         std::array<std::uint8_t, 32>& out_conv_key,
                         std::uint8_t& out_version,
                         std::string& error);

  bool LoadLegacyConversation(bool is_group,
                              const std::string& conv_id,
                              std::size_t limit,
                              std::vector<ChatHistoryMessage>& out_messages,
                              std::string& error) const;

  std::filesystem::path e2ee_state_dir_;
  std::filesystem::path user_dir_;
  std::filesystem::path key_path_;
  std::filesystem::path tag_key_path_;
  std::filesystem::path index_path_;
  std::filesystem::path journal_path_;
  std::filesystem::path profiles_path_;
  std::filesystem::path profiles_lock_path_;
  std::filesystem::path profile_lock_path_;
  std::unique_ptr<struct ProfileLockState> profile_lock_;
  std::filesystem::path legacy_conv_dir_;
  std::filesystem::path history_dir_;
  std::filesystem::path attachments_dir_;
  std::filesystem::path attachments_index_path_;
  std::string user_tag_;
  std::string legacy_tag_alt_;
  std::array<std::uint8_t, 16> profile_id_{};
  std::string legacy_tag_;
  std::vector<HistoryFileEntry> history_files_;
  std::unordered_map<std::string, std::size_t> conv_to_file_;
  std::unordered_map<std::string, std::vector<std::size_t>> conv_hash_to_files_;
  struct AttachmentEntry {
    std::string file_name;
    std::uint64_t file_size{0};
    std::uint8_t kind{0};
    std::uint32_t ref_count{0};
    std::uint32_t preview_size{0};
    std::uint64_t last_ts{0};
  };
  std::unordered_map<std::string, AttachmentEntry> attachments_;
  std::uint32_t next_seq_{1};
  bool key_loaded_{false};
  bool tag_key_loaded_{false};
  bool index_dirty_{false};
  bool read_only_{false};
  bool attachments_loaded_{false};
  bool attachments_dirty_{false};
  std::array<std::uint8_t, 32> master_key_{};
  std::array<std::uint8_t, 32> tag_key_{};
};

}  // namespace mi::client
