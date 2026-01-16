#include "client_core.h"

#include "storage_service.h"

namespace mi::client {

void ClientCore::BestEffortPersistHistoryEnvelope(
    bool is_group,
    bool outgoing,
    const std::string& conv_id,
    const std::string& sender,
    const std::vector<std::uint8_t>& envelope,
    HistoryStatus status,
    std::uint64_t timestamp_sec) {
  StorageService().BestEffortPersistHistoryEnvelope(*this, is_group, outgoing, conv_id, sender, envelope, status, timestamp_sec);
}

void ClientCore::BestEffortPersistHistoryStatus(
    bool is_group,
    const std::string& conv_id,
    const std::array<std::uint8_t, 16>& msg_id,
    HistoryStatus status,
    std::uint64_t timestamp_sec) {
  StorageService().BestEffortPersistHistoryStatus(*this, is_group, conv_id, msg_id, status, timestamp_sec);
}

void ClientCore::BestEffortStoreAttachmentPreviewBytes(
    const std::string& file_id,
    const std::string& file_name,
    std::uint64_t file_size,
    const std::vector<std::uint8_t>& bytes) {
  StorageService().BestEffortStoreAttachmentPreviewBytes(*this, file_id, file_name, file_size, bytes);
}

void ClientCore::BestEffortStoreAttachmentPreviewFromPath(
    const std::string& file_id,
    const std::string& file_name,
    std::uint64_t file_size,
    const std::filesystem::path& path) {
  StorageService().BestEffortStoreAttachmentPreviewFromPath(*this, file_id, file_name, file_size, path);
}

void ClientCore::StoreAttachmentPreviewBytes(
    const std::string& file_id,
    const std::string& file_name,
    std::uint64_t file_size,
    const std::vector<std::uint8_t>& bytes) {
  BestEffortStoreAttachmentPreviewBytes(file_id, file_name, file_size, bytes);
}

void ClientCore::StoreAttachmentPreviewFromPath(
    const std::string& file_id,
    const std::string& file_name,
    std::uint64_t file_size,
    const std::filesystem::path& path) {
  BestEffortStoreAttachmentPreviewFromPath(file_id, file_name, file_size, path);
}

void ClientCore::WarmupHistoryOnStartup() {
  StorageService().WarmupHistoryOnStartup(*this);
}

void ClientCore::FlushHistoryOnShutdown() {
  StorageService().FlushHistoryOnShutdown(*this);
}

bool ClientCore::DeleteChatHistory(const std::string& conv_id,
                                   bool is_group,
                                   bool delete_attachments,
                                   bool secure_wipe) {
  return StorageService().DeleteChatHistory(*this, conv_id, is_group, delete_attachments, secure_wipe);
}

bool ClientCore::DownloadChatFileToPath(
    const ChatFileMessage& file,
    const std::filesystem::path& out_path,
    bool wipe_after_read,
    const std::function<void(std::uint64_t, std::uint64_t)>& on_progress) {
  return StorageService().DownloadChatFileToPath(*this, file, out_path, wipe_after_read, on_progress);
}

bool ClientCore::DownloadChatFileToBytes(const ChatFileMessage& file,
                                        std::vector<std::uint8_t>& out_bytes,
                                        bool wipe_after_read) {
  return StorageService().DownloadChatFileToBytes(*this, file, out_bytes, wipe_after_read);
}

std::vector<ClientCore::HistoryEntry> ClientCore::LoadChatHistory(
    const std::string& conv_id, bool is_group, std::size_t limit) {
  return StorageService().LoadChatHistory(*this, conv_id, is_group, limit);
}

bool ClientCore::AddHistorySystemMessage(const std::string& conv_id,
                                        bool is_group,
                                        const std::string& text_utf8) {
  return StorageService().AddHistorySystemMessage(*this, conv_id, is_group, text_utf8);
}

void ClientCore::SetHistoryEnabled(bool enabled) {
  StorageService().SetHistoryEnabled(*this, enabled);
}

bool ClientCore::ClearAllHistory(bool delete_attachments,
                                 bool secure_wipe,
                                 std::string& error) {
  return StorageService().ClearAllHistory(*this, delete_attachments, secure_wipe, error);
}

bool ClientCore::UploadE2eeFileBlob(const std::vector<std::uint8_t>& blob,
                                    std::string& out_file_id) {
  return StorageService().UploadE2eeFileBlob(*this, blob, out_file_id);
}

bool ClientCore::DownloadE2eeFileBlob(
    const std::string& file_id,
    std::vector<std::uint8_t>& out_blob,
    bool wipe_after_read,
    const std::function<void(std::uint64_t, std::uint64_t)>& on_progress) {
  return StorageService().DownloadE2eeFileBlob(*this, file_id, out_blob, wipe_after_read, on_progress);
}

bool ClientCore::StartE2eeFileBlobUpload(std::uint64_t expected_size,
                                        std::string& out_file_id,
                                        std::string& out_upload_id) {
  return StorageService().StartE2eeFileBlobUpload(*this, expected_size, out_file_id, out_upload_id);
}

bool ClientCore::UploadE2eeFileBlobChunk(const std::string& file_id,
                                        const std::string& upload_id,
                                        std::uint64_t offset,
                                        const std::vector<std::uint8_t>& chunk,
                                        std::uint64_t& out_bytes_received) {
  return StorageService().UploadE2eeFileBlobChunk(*this, file_id, upload_id, offset, chunk, out_bytes_received);
}

bool ClientCore::FinishE2eeFileBlobUpload(const std::string& file_id,
                                         const std::string& upload_id,
                                         std::uint64_t total_size) {
  return StorageService().FinishE2eeFileBlobUpload(*this, file_id, upload_id, total_size);
}

bool ClientCore::StartE2eeFileBlobDownload(const std::string& file_id,
                                          bool wipe_after_read,
                                          std::string& out_download_id,
                                          std::uint64_t& out_size) {
  return StorageService().StartE2eeFileBlobDownload(*this, file_id, wipe_after_read, out_download_id, out_size);
}

bool ClientCore::DownloadE2eeFileBlobChunk(const std::string& file_id,
                                          const std::string& download_id,
                                          std::uint64_t offset,
                                          std::uint32_t max_len,
                                          std::vector<std::uint8_t>& out_chunk,
                                          bool& out_eof) {
  return StorageService().DownloadE2eeFileBlobChunk(*this, file_id, download_id, offset, max_len, out_chunk, out_eof);
}

bool ClientCore::UploadE2eeFileBlobV3FromPath(
    const std::filesystem::path& file_path, std::uint64_t plaintext_size,
    const std::array<std::uint8_t, 32>& file_key, std::string& out_file_id) {
  return StorageService().UploadE2eeFileBlobV3FromPath(*this, file_path, plaintext_size, file_key, out_file_id);
}

bool ClientCore::DownloadE2eeFileBlobV3ToPath(
    const std::string& file_id, const std::array<std::uint8_t, 32>& file_key,
    const std::filesystem::path& out_path, bool wipe_after_read,
    const std::function<void(std::uint64_t, std::uint64_t)>& on_progress) {
  return StorageService().DownloadE2eeFileBlobV3ToPath(*this, file_id, file_key, out_path, wipe_after_read, on_progress);
}

bool ClientCore::UploadChatFileFromPath(const std::filesystem::path& file_path,
                                       std::uint64_t file_size,
                                       const std::string& file_name,
                                       std::array<std::uint8_t, 32>& out_file_key,
                                       std::string& out_file_id) {
  return StorageService().UploadChatFileFromPath(*this, file_path, file_size, file_name, out_file_key, out_file_id);
}

}  // namespace mi::client
