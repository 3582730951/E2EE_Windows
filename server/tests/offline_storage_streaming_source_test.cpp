#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifndef MI_E2EE_OFFLINE_STORAGE_SOURCE
#define MI_E2EE_OFFLINE_STORAGE_SOURCE ""
#endif

namespace {

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    return {};
  }
  std::ostringstream oss;
  oss << ifs.rdbuf();
  return oss.str();
}

bool BodyContains(const std::string& source, const std::string& function,
                  const std::string& needle) {
  const auto start = source.find(function);
  if (start == std::string::npos) {
    return false;
  }
  const auto next = source.find("\n}\n\n", start);
  if (next == std::string::npos) {
    return false;
  }
  return source.substr(start, next - start).find(needle) != std::string::npos;
}

}  // namespace

int main() {
  const std::string source = ReadFile(MI_E2EE_OFFLINE_STORAGE_SOURCE);
  if (source.empty()) {
    std::cerr << "failed to read offline storage source\n";
    return 1;
  }
  if (!BodyContains(source, "OfflineStorage::PutBlob", "BeginBlobUpload") ||
      !BodyContains(source, "OfflineStorage::PutBlob", "AppendBlobUploadChunk") ||
      !BodyContains(source, "OfflineStorage::PutBlob", "FinishBlobUpload")) {
    std::cerr << "PutBlob does not use streaming upload path\n";
    return 1;
  }
  if (!BodyContains(source, "OfflineStorage::FetchBlob", "BeginBlobDownload") ||
      !BodyContains(source, "OfflineStorage::FetchBlob",
                    "ReadBlobDownloadChunk")) {
    std::cerr << "FetchBlob does not use streaming download path\n";
    return 1;
  }
  return 0;
}
