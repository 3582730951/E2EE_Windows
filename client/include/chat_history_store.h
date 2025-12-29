#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
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

struct ChatHistoryMessage {
  bool is_group{false};
  bool outgoing{false};
  bool is_system{false};
  ChatHistoryStatus status{ChatHistoryStatus::kSent};
  std::uint64_t timestamp_sec{0};
  std::string conv_id;
  std::string sender;
  std::vector<std::uint8_t> envelope;
  std::string system_text_utf8;
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

  bool LoadConversation(bool is_group,
                        const std::string& conv_id,
                        std::size_t limit,
                        std::vector<ChatHistoryMessage>& out_messages,
                        std::string& error) const;

  bool ExportRecentSnapshot(std::size_t max_conversations,
                            std::size_t max_messages_per_conversation,
                            std::vector<ChatHistoryMessage>& out_messages,
                            std::string& error) const;

 private:
  struct HistoryFileEntry {
    std::filesystem::path path;
    std::uint32_t seq{0};
    std::unordered_set<std::string> conv_keys;
  };

  bool EnsureKeyLoaded(std::string& error);

  bool DeriveConversationKey(bool is_group,
                             const std::string& conv_id,
                             std::array<std::uint8_t, 32>& out_key,
                             std::string& error) const;

  bool LoadHistoryFiles(std::string& error);

  bool EnsureHistoryFile(bool is_group,
                         const std::string& conv_id,
                         std::filesystem::path& out_path,
                         std::array<std::uint8_t, 32>& out_conv_key,
                         std::string& error);

  bool LoadLegacyConversation(bool is_group,
                              const std::string& conv_id,
                              std::size_t limit,
                              std::vector<ChatHistoryMessage>& out_messages,
                              std::string& error) const;

  std::filesystem::path e2ee_state_dir_;
  std::filesystem::path user_dir_;
  std::filesystem::path key_path_;
  std::filesystem::path legacy_conv_dir_;
  std::filesystem::path history_dir_;
  std::string user_tag_;
  std::vector<HistoryFileEntry> history_files_;
  std::unordered_map<std::string, std::size_t> conv_to_file_;
  std::uint32_t next_seq_{1};
  bool key_loaded_{false};
  std::array<std::uint8_t, 32> master_key_{};
};

}  // namespace mi::client
