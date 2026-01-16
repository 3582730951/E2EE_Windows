#ifndef MI_E2EE_CLIENT_STORAGE_SERVICE_H
#define MI_E2EE_CLIENT_STORAGE_SERVICE_H

#include "client_core.h"

namespace mi::client {

class StorageService {
 public:
  void BestEffortPersistHistoryEnvelope(ClientCore& core, bool is_group, bool outgoing, const std::string& conv_id, const std::string& sender, const std::vector<std::uint8_t>& envelope, ClientCore::HistoryStatus status, std::uint64_t timestamp_sec) const;
  void BestEffortPersistHistoryStatus(ClientCore& core, bool is_group, const std::string& conv_id, const std::array<std::uint8_t, 16>& msg_id, ClientCore::HistoryStatus status, std::uint64_t timestamp_sec) const;
  void BestEffortStoreAttachmentPreviewBytes(ClientCore& core, const std::string& file_id, const std::string& file_name, std::uint64_t file_size, const std::vector<std::uint8_t>& bytes) const;
  void BestEffortStoreAttachmentPreviewFromPath(ClientCore& core, const std::string& file_id, const std::string& file_name, std::uint64_t file_size, const std::filesystem::path& path) const;
  void WarmupHistoryOnStartup(ClientCore& core) const;
  void FlushHistoryOnShutdown(ClientCore& core) const;
  bool DeleteChatHistory(ClientCore& core, const std::string& conv_id, bool is_group, bool delete_attachments, bool secure_wipe) const;
  bool DownloadChatFileToPath(ClientCore& core, const ClientCore::ChatFileMessage& file, const std::filesystem::path& out_path, bool wipe_after_read, const std::function<void(std::uint64_t, std::uint64_t)>& on_progress) const;
  bool DownloadChatFileToBytes(ClientCore& core, const ClientCore::ChatFileMessage& file, std::vector<std::uint8_t>& out_bytes, bool wipe_after_read) const;
  std::vector<ClientCore::HistoryEntry> LoadChatHistory(ClientCore& core, const std::string& conv_id, bool is_group, std::size_t limit) const;
  bool AddHistorySystemMessage(ClientCore& core, const std::string& conv_id, bool is_group, const std::string& text_utf8) const;
  void SetHistoryEnabled(ClientCore& core, bool enabled) const;
  bool ClearAllHistory(ClientCore& core, bool delete_attachments, bool secure_wipe, std::string& error) const;
  bool UploadE2eeFileBlob(ClientCore& core, const std::vector<std::uint8_t>& blob, std::string& out_file_id) const;
  bool DownloadE2eeFileBlob(ClientCore& core, const std::string& file_id, std::vector<std::uint8_t>& out_blob, bool wipe_after_read, const std::function<void(std::uint64_t, std::uint64_t)>& on_progress) const;
  bool StartE2eeFileBlobUpload(ClientCore& core, std::uint64_t expected_size, std::string& out_file_id, std::string& out_upload_id) const;
  bool UploadE2eeFileBlobChunk(ClientCore& core, const std::string& file_id, const std::string& upload_id, std::uint64_t offset, const std::vector<std::uint8_t>& chunk, std::uint64_t& out_bytes_received) const;
  bool FinishE2eeFileBlobUpload(ClientCore& core, const std::string& file_id, const std::string& upload_id, std::uint64_t total_size) const;
  bool StartE2eeFileBlobDownload(ClientCore& core, const std::string& file_id, bool wipe_after_read, std::string& out_download_id, std::uint64_t& out_size) const;
  bool DownloadE2eeFileBlobChunk(ClientCore& core, const std::string& file_id, const std::string& download_id, std::uint64_t offset, std::uint32_t max_len, std::vector<std::uint8_t>& out_chunk, bool& out_eof) const;
  bool UploadE2eeFileBlobV3FromPath(ClientCore& core, const std::filesystem::path& file_path, std::uint64_t plaintext_size, const std::array<std::uint8_t, 32>& file_key, std::string& out_file_id) const;
  bool DownloadE2eeFileBlobV3ToPath(ClientCore& core, const std::string& file_id, const std::array<std::uint8_t, 32>& file_key, const std::filesystem::path& out_path, bool wipe_after_read, const std::function<void(std::uint64_t, std::uint64_t)>& on_progress) const;
  bool UploadChatFileFromPath(ClientCore& core, const std::filesystem::path& file_path, std::uint64_t file_size, const std::string& file_name, std::array<std::uint8_t, 32>& out_file_key, std::string& out_file_id) const;
};

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_STORAGE_SERVICE_H
