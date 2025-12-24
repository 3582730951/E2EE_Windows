#include "offline_storage.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>
#include <random>

#include "crypto.h"

namespace {

bool Check(bool cond) {
  return cond;
}

std::filesystem::path TempDir(const std::string& name) {
  auto dir = std::filesystem::temp_directory_path() / name;
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  return dir;
}

}  // namespace

int main() {
  {
    const auto dir = TempDir("mi_e2ee_offline_put_fetch");
    mi::server::OfflineStorage storage(dir, std::chrono::seconds(60));

    std::vector<std::uint8_t> payload = {1, 2, 3, 4, 5, 6, 7};
    auto put = storage.Put("alice", payload);
    if (!Check(put.success && !put.file_id.empty() && put.meta.owner == "alice")) {
      return 1;
    }
    {
      const auto path = dir / (put.file_id + ".bin");
      std::ifstream ifs(path, std::ios::binary);
      if (!ifs) {
        return 1;
      }
      std::array<std::uint8_t, 9> hdr{};
      ifs.read(reinterpret_cast<char*>(hdr.data()),
               static_cast<std::streamsize>(hdr.size()));
      if (ifs.gcount() != static_cast<std::streamsize>(hdr.size())) {
        return 1;
      }
      static constexpr std::array<std::uint8_t, 8> kMagic = {
          'M', 'I', 'O', 'F', 'A', 'E', 'A', 'D'};
      if (!std::equal(kMagic.begin(), kMagic.end(), hdr.begin())) {
        return 1;
      }
      if (hdr[8] != 2) {
        return 1;
      }
    }
    {
      const auto key_path = dir / (put.file_id + ".key");
      std::error_code ec;
      if (!std::filesystem::exists(key_path, ec) || ec) {
        return 1;
      }
      if (std::filesystem::file_size(key_path, ec) != 32 || ec) {
        return 1;
      }
    }

    std::string err;
    auto fetched = storage.Fetch(put.file_id, put.file_key, true, err);
    if (!fetched.has_value() || payload != fetched.value()) {
      return 1;
    }
    if (std::filesystem::exists(dir / (put.file_id + ".bin"))) {
      return 1;
    }
    if (std::filesystem::exists(dir / (put.file_id + ".key"))) {
      return 1;
    }
    if (storage.Meta(put.file_id).has_value()) {
      return 1;
    }
  }

  {
    const auto dir = TempDir("mi_e2ee_offline_key_delete");
    mi::server::OfflineStorage storage(dir, std::chrono::seconds(60));

    std::vector<std::uint8_t> payload = {0xAA, 0xBB, 0xCC};
    auto put = storage.Put("alice", payload);
    if (!put.success) {
      return 1;
    }
    std::error_code ec;
    std::filesystem::remove(dir / (put.file_id + ".key"), ec);
    std::string err;
    auto fetched = storage.Fetch(put.file_id, put.file_key, false, err);
    if (fetched.has_value()) {
      return 1;
    }
  }

  {
    const auto dir = TempDir("mi_e2ee_offline_roundtrip_random");
    mi::server::OfflineStorage storage(dir, std::chrono::seconds(60));
    std::mt19937 rng(0x4D495F32u);
    std::uniform_int_distribution<std::size_t> len_dist(1, 1024);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    for (int i = 0; i < 64; ++i) {
      const std::size_t len = len_dist(rng);
      std::vector<std::uint8_t> payload(len);
      for (auto& b : payload) {
        b = static_cast<std::uint8_t>(byte_dist(rng));
      }
      auto put = storage.Put("alice", payload);
      if (!put.success) {
        return 1;
      }
      std::string err;
      auto fetched = storage.Fetch(put.file_id, put.file_key, true, err);
      if (!fetched.has_value() || fetched.value() != payload) {
        return 1;
      }
      if (std::filesystem::exists(dir / (put.file_id + ".bin"))) {
        return 1;
      }
      if (std::filesystem::exists(dir / (put.file_id + ".key"))) {
        return 1;
      }
    }
  }

  {
    const auto dir = TempDir("mi_e2ee_offline_legacy_compat");
    mi::server::OfflineStorage storage(dir, std::chrono::seconds(60));

    static constexpr std::array<std::uint8_t, 32> kKey = []() {
      std::array<std::uint8_t, 32> out{};
      for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<std::uint8_t>(i);
      }
      return out;
    }();

    static constexpr std::array<std::uint8_t, 16> kNonce = []() {
      std::array<std::uint8_t, 16> out{};
      for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<std::uint8_t>(0xA0u + i);
      }
      return out;
    }();

    const std::string file_id = "0123456789abcdef0123456789abcdef";
    const std::vector<std::uint8_t> payload = {9, 8, 7, 6, 5, 4, 3, 2, 1};

    auto deriveBlock = [](std::uint64_t counter,
                          std::array<std::uint8_t, 32>& out) {
      std::array<std::uint8_t, 24> buf{};
      std::memcpy(buf.data(), kNonce.data(), kNonce.size());
      for (int i = 0; i < 8; ++i) {
        buf[16 + i] =
            static_cast<std::uint8_t>((counter >> (56 - 8 * i)) & 0xFF);
      }
      mi::server::crypto::Sha256Digest digest;
      mi::server::crypto::HmacSha256(kKey.data(), kKey.size(), buf.data(),
                                     buf.size(), digest);
      std::memcpy(out.data(), digest.bytes.data(), out.size());
    };

    std::vector<std::uint8_t> cipher;
    cipher.resize(payload.size());
    std::array<std::uint8_t, 32> block{};
    std::uint64_t counter = 0;
    std::size_t offset = 0;
    while (offset < payload.size()) {
      deriveBlock(counter, block);
      const std::size_t to_copy =
          std::min(block.size(), payload.size() - offset);
      for (std::size_t i = 0; i < to_copy; ++i) {
        cipher[offset + i] = static_cast<std::uint8_t>(payload[offset + i] ^
                                                      block[i]);
      }
      ++counter;
      offset += to_copy;
    }

    std::vector<std::uint8_t> mac_buf;
    mac_buf.reserve(kNonce.size() + cipher.size());
    mac_buf.insert(mac_buf.end(), kNonce.begin(), kNonce.end());
    mac_buf.insert(mac_buf.end(), cipher.begin(), cipher.end());
    mi::server::crypto::Sha256Digest digest;
    mi::server::crypto::HmacSha256(kKey.data(), kKey.size(), mac_buf.data(),
                                   mac_buf.size(), digest);

    const auto path = dir / (file_id + ".bin");
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      return 1;
    }
    ofs.write(reinterpret_cast<const char*>(kNonce.data()),
              static_cast<std::streamsize>(kNonce.size()));
    ofs.write(reinterpret_cast<const char*>(cipher.data()),
              static_cast<std::streamsize>(cipher.size()));
    ofs.write(reinterpret_cast<const char*>(digest.bytes.data()),
              static_cast<std::streamsize>(digest.bytes.size()));
    ofs.close();

    std::string err;
    auto fetched = storage.Fetch(file_id, kKey, true, err);
    if (!fetched.has_value() || fetched.value() != payload) {
      return 1;
    }
    if (std::filesystem::exists(path)) {
      return 1;
    }
  }

  {
    const auto dir = TempDir("mi_e2ee_offline_blob_stream");
    mi::server::OfflineStorage storage(dir, std::chrono::seconds(60));

    auto started = storage.BeginBlobUpload("alice", 0);
    if (!Check(started.success && !started.file_id.empty() &&
               !started.upload_id.empty())) {
      return 1;
    }

    const std::vector<std::uint8_t> chunk1 = {1, 2, 3};
    const std::vector<std::uint8_t> chunk2 = {4, 5, 6, 7};

    auto a1 = storage.AppendBlobUploadChunk("alice", started.file_id,
                                            started.upload_id, 0, chunk1);
    if (!a1.success || a1.bytes_received != chunk1.size()) {
      return 1;
    }

    auto bad = storage.AppendBlobUploadChunk("alice", started.file_id,
                                             started.upload_id, 0, chunk2);
    if (bad.success) {
      return 1;
    }

    auto a2 = storage.AppendBlobUploadChunk(
        "alice", started.file_id, started.upload_id,
        static_cast<std::uint64_t>(chunk1.size()), chunk2);
    if (!a2.success ||
        a2.bytes_received != static_cast<std::uint64_t>(chunk1.size() + chunk2.size())) {
      return 1;
    }

    auto finished = storage.FinishBlobUpload(
        "alice", started.file_id, started.upload_id,
        static_cast<std::uint64_t>(chunk1.size() + chunk2.size()));
    if (!finished.success || finished.meta.size != chunk1.size() + chunk2.size()) {
      return 1;
    }
    if (!std::filesystem::exists(dir / (started.file_id + ".bin"))) {
      return 1;
    }

    auto dl = storage.BeginBlobDownload("bob", started.file_id, true);
    if (!dl.success || dl.download_id.empty()) {
      return 1;
    }

    auto c1 = storage.ReadBlobDownloadChunk("bob", started.file_id, dl.download_id,
                                            0, 3);
    if (!c1.success || c1.offset != 0 || c1.chunk != chunk1 || c1.eof) {
      return 1;
    }

    auto c2 = storage.ReadBlobDownloadChunk(
        "bob", started.file_id, dl.download_id,
        static_cast<std::uint64_t>(chunk1.size()), 1024);
    if (!c2.success || c2.offset != chunk1.size() || c2.chunk != chunk2 ||
        !c2.eof) {
      return 1;
    }

    if (std::filesystem::exists(dir / (started.file_id + ".bin"))) {
      return 1;
    }
    if (storage.Meta(started.file_id).has_value()) {
      return 1;
    }
  }

  {
    const auto dir = TempDir("mi_e2ee_offline_cleanup");
    mi::server::OfflineStorage storage(dir, std::chrono::seconds(1));
    std::vector<std::uint8_t> payload(64, 0xAB);
    auto put = storage.Put("bob", payload);
    if (!put.success || !std::filesystem::exists(dir / (put.file_id + ".bin"))) {
      return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    storage.CleanupExpired();
    if (std::filesystem::exists(dir / (put.file_id + ".bin"))) {
      return 1;
    }
    if (std::filesystem::exists(dir / (put.file_id + ".key"))) {
      return 1;
    }
    if (storage.Meta(put.file_id).has_value()) {
      return 1;
    }
  }

  {
    mi::server::OfflineQueue queue(std::chrono::seconds(1));
    queue.Enqueue("alice", {1, 2, 3});
    queue.Enqueue("alice", {4, 5, 6});
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    queue.Enqueue("alice", {7, 8, 9}, std::chrono::seconds(2));

    queue.CleanupExpired();
    auto msgs = queue.Drain("alice");
    if (msgs.size() != 1u || msgs[0] != std::vector<std::uint8_t>({7, 8, 9})) {
      return 1;
    }
    if (!queue.Drain("alice").empty()) {
      return 1;
    }
  }

  return 0;
}
