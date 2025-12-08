#include "offline_storage.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <random>

#include "crypto.h"

namespace mi::server {

namespace {

std::array<std::uint8_t, 16> RandomNonce() {
  std::array<std::uint8_t, 16> nonce{};
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(0, 255);
  for (auto& b : nonce) {
    b = static_cast<std::uint8_t>(dist(gen));
  }
  return nonce;
}

void DeriveBlock(const std::array<std::uint8_t, 32>& key,
                 const std::array<std::uint8_t, 16>& nonce, std::uint64_t counter,
                 std::array<std::uint8_t, 32>& out) {
  std::array<std::uint8_t, 24> buf{};
  std::memcpy(buf.data(), nonce.data(), nonce.size());
  for (int i = 0; i < 8; ++i) {
    buf[16 + i] = static_cast<std::uint8_t>((counter >> (56 - 8 * i)) & 0xFF);
  }
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::HmacSha256(key.data(), key.size(), buf.data(),
                                 buf.size(), d);
  std::memcpy(out.data(), d.bytes.data(), out.size());
}

}  // namespace

OfflineStorage::OfflineStorage(std::filesystem::path base_dir,
                               std::chrono::seconds ttl)
    : base_dir_(std::move(base_dir)), ttl_(ttl) {
  std::error_code ec;
  std::filesystem::create_directories(base_dir_, ec);
}

PutResult OfflineStorage::Put(const std::string& owner,
                              const std::vector<std::uint8_t>& plaintext) {
  PutResult result;
  if (plaintext.empty()) {
    result.error = "empty payload";
    return result;
  }

  const std::string id = GenerateId();
  const std::array<std::uint8_t, 32> key = GenerateKey();
  const std::array<std::uint8_t, 16> nonce = RandomNonce();

  std::vector<std::uint8_t> cipher;
  std::array<std::uint8_t, 32> tag{};
  if (!Encrypt(plaintext, key, nonce, cipher, tag)) {
    result.error = "encrypt failed";
    return result;
  }

  const auto path = ResolvePath(id);
  std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    result.error = "open file failed";
    return result;
  }
  ofs.write(reinterpret_cast<const char*>(nonce.data()),
            static_cast<std::streamsize>(nonce.size()));
  ofs.write(reinterpret_cast<const char*>(cipher.data()),
            static_cast<std::streamsize>(cipher.size()));
  ofs.write(reinterpret_cast<const char*>(tag.data()),
            static_cast<std::streamsize>(tag.size()));
  ofs.close();

  StoredFileMeta meta;
  meta.id = id;
  meta.owner = owner;
  meta.size = static_cast<std::uint64_t>(plaintext.size());
  meta.created_at = std::chrono::steady_clock::now();

  {
    std::lock_guard<std::mutex> lock(mutex_);
    metadata_[id] = meta;
  }

  result.success = true;
  result.file_id = id;
  result.file_key = key;
  result.meta = meta;
  return result;
}

std::optional<std::vector<std::uint8_t>> OfflineStorage::Fetch(
    const std::string& file_id, const std::array<std::uint8_t, 32>& file_key,
    bool wipe_after_read, std::string& error) {
  const auto path = ResolvePath(file_id);
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    error = "file not found";
    return std::nullopt;
  }
  std::vector<std::uint8_t> content((std::istreambuf_iterator<char>(ifs)),
                                    std::istreambuf_iterator<char>());
  ifs.close();

  if (content.size() < (16 + 32)) {
    error = "file truncated";
    return std::nullopt;
  }

  const std::array<std::uint8_t, 16> nonce = [&]() {
    std::array<std::uint8_t, 16> n{};
    std::memcpy(n.data(), content.data(), n.size());
    return n;
  }();

  const std::size_t cipher_len = content.size() - nonce.size() - 32;
  if (cipher_len == 0) {
    error = "cipher empty";
    return std::nullopt;
  }

  const std::array<std::uint8_t, 32> tag = [&]() {
    std::array<std::uint8_t, 32> t{};
    std::memcpy(t.data(), content.data() + nonce.size() + cipher_len,
                t.size());
    return t;
  }();

  std::vector<std::uint8_t> cipher(cipher_len);
  std::memcpy(cipher.data(), content.data() + nonce.size(), cipher_len);

  std::vector<std::uint8_t> plaintext;
  if (!Decrypt(cipher, file_key, nonce, tag, plaintext)) {
    error = "auth failed";
    return std::nullopt;
  }

  if (wipe_after_read) {
    WipeFile(path);
    std::lock_guard<std::mutex> lock(mutex_);
    metadata_.erase(file_id);
  }

  error.clear();
  return plaintext;
}

std::optional<StoredFileMeta> OfflineStorage::Meta(
    const std::string& file_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = metadata_.find(file_id);
  if (it == metadata_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void OfflineStorage::CleanupExpired() {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = metadata_.begin(); it != metadata_.end();) {
    if (now - it->second.created_at > ttl_) {
      const auto path = ResolvePath(it->first);
      WipeFile(path);
      it = metadata_.erase(it);
    } else {
      ++it;
    }
  }
}

std::filesystem::path OfflineStorage::ResolvePath(
    const std::string& file_id) const {
  return base_dir_ / (file_id + ".bin");
}

std::string OfflineStorage::GenerateId() const {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(0, 255);
  const char* hex = "0123456789abcdef";
  std::string out;
  out.resize(32);
  for (int i = 0; i < 16; ++i) {
    const std::uint8_t v = static_cast<std::uint8_t>(dist(gen));
    out[i * 2] = hex[v >> 4];
    out[i * 2 + 1] = hex[v & 0x0F];
  }
  return out;
}

std::array<std::uint8_t, 32> OfflineStorage::GenerateKey() const {
  std::array<std::uint8_t, 32> key{};
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(0, 255);
  for (auto& b : key) {
    b = static_cast<std::uint8_t>(dist(gen));
  }
  return key;
}

bool OfflineStorage::Encrypt(const std::vector<std::uint8_t>& plaintext,
                             const std::array<std::uint8_t, 32>& key,
                             const std::array<std::uint8_t, 16>& nonce,
                             std::vector<std::uint8_t>& cipher,
                             std::array<std::uint8_t, 32>& tag) const {
  cipher.resize(plaintext.size());
  std::array<std::uint8_t, 32> block{};
  std::uint64_t counter = 0;
  std::size_t offset = 0;
  while (offset < plaintext.size()) {
    DeriveBlock(key, nonce, counter, block);
    const std::size_t to_copy =
        std::min(block.size(), plaintext.size() - offset);
    for (std::size_t i = 0; i < to_copy; ++i) {
      cipher[offset + i] = static_cast<std::uint8_t>(
          plaintext[offset + i] ^ block[i]);
    }
    ++counter;
    offset += to_copy;
  }

  std::vector<std::uint8_t> mac_buf;
  mac_buf.reserve(nonce.size() + cipher.size());
  mac_buf.insert(mac_buf.end(), nonce.begin(), nonce.end());
  mac_buf.insert(mac_buf.end(), cipher.begin(), cipher.end());

  crypto::Sha256Digest digest;
  crypto::HmacSha256(key.data(), key.size(), mac_buf.data(),
                     mac_buf.size(), digest);
  std::memcpy(tag.data(), digest.bytes.data(), tag.size());
  return true;
}

bool OfflineStorage::Decrypt(const std::vector<std::uint8_t>& cipher,
                             const std::array<std::uint8_t, 32>& key,
                             const std::array<std::uint8_t, 16>& nonce,
                             const std::array<std::uint8_t, 32>& tag,
                             std::vector<std::uint8_t>& plaintext) const {
  std::vector<std::uint8_t> mac_buf;
  mac_buf.reserve(nonce.size() + cipher.size());
  mac_buf.insert(mac_buf.end(), nonce.begin(), nonce.end());
  mac_buf.insert(mac_buf.end(), cipher.begin(), cipher.end());

  crypto::Sha256Digest digest;
  crypto::HmacSha256(key.data(), key.size(), mac_buf.data(),
                     mac_buf.size(), digest);
  if (std::memcmp(tag.data(), digest.bytes.data(), tag.size()) != 0) {
    return false;
  }

  plaintext.resize(cipher.size());
  std::array<std::uint8_t, 32> block{};
  std::uint64_t counter = 0;
  std::size_t offset = 0;
  while (offset < cipher.size()) {
    DeriveBlock(key, nonce, counter, block);
    const std::size_t to_copy = std::min(block.size(), cipher.size() - offset);
    for (std::size_t i = 0; i < to_copy; ++i) {
      plaintext[offset + i] = static_cast<std::uint8_t>(
          cipher[offset + i] ^ block[i]);
    }
    ++counter;
    offset += to_copy;
  }
  return true;
}

void OfflineStorage::WipeFile(const std::filesystem::path& path) const {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return;
  }
  const auto size = std::filesystem::file_size(path, ec);
  if (ec) {
    std::filesystem::remove(path, ec);
    return;
  }
  std::fstream fs(path, std::ios::binary | std::ios::in | std::ios::out);
  if (!fs) {
    std::filesystem::remove(path, ec);
    return;
  }
  const std::size_t wipe_len = size < 16 ? static_cast<std::size_t>(size) : 16;
  const std::vector<std::uint8_t> ff(wipe_len, 0xFF);
  fs.seekp(0);
  fs.write(reinterpret_cast<const char*>(ff.data()),
           static_cast<std::streamsize>(wipe_len));
  if (size > wipe_len) {
    const std::size_t mid = static_cast<std::size_t>(size / 2);
    fs.seekp(static_cast<std::streamoff>(mid));
    fs.write(reinterpret_cast<const char*>(ff.data()),
             static_cast<std::streamsize>(
                 std::min(wipe_len, static_cast<std::size_t>(size - mid))));
    if (size > wipe_len * 2) {
      const auto tail_pos =
          static_cast<std::streamoff>(size > wipe_len ? size - wipe_len : 0);
      fs.seekp(tail_pos);
      fs.write(reinterpret_cast<const char*>(ff.data()),
               static_cast<std::streamsize>(wipe_len));
    }
  }
  fs.flush();
  fs.close();
  std::filesystem::remove(path, ec);
}

OfflineQueue::OfflineQueue(std::chrono::seconds default_ttl)
    : default_ttl_(default_ttl) {}

void OfflineQueue::Enqueue(const std::string& recipient,
                           std::vector<std::uint8_t> payload,
                           std::chrono::seconds ttl) {
  OfflineMessage msg;
  msg.recipient = recipient;
  msg.payload = std::move(payload);
  msg.created_at = std::chrono::steady_clock::now();
  msg.ttl = (ttl == std::chrono::seconds::zero()) ? default_ttl_ : ttl;

  std::lock_guard<std::mutex> lock(mutex_);
  messages_[recipient].push_back(std::move(msg));
}

std::vector<std::vector<std::uint8_t>> OfflineQueue::Drain(
    const std::string& recipient) {
  std::vector<std::vector<std::uint8_t>> out;
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = messages_.find(recipient);
  if (it == messages_.end()) {
    return out;
  }
  auto& vec = it->second;
  for (auto msg_it = vec.begin(); msg_it != vec.end();) {
    if (now - msg_it->created_at > msg_it->ttl) {
      msg_it = vec.erase(msg_it);
      continue;
    }
    out.push_back(msg_it->payload);
    msg_it = vec.erase(msg_it);
  }
  if (vec.empty()) {
    messages_.erase(it);
  }
  return out;
}

void OfflineQueue::CleanupExpired() {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = messages_.begin(); it != messages_.end();) {
    auto& vec = it->second;
    vec.erase(std::remove_if(vec.begin(), vec.end(),
                             [&](const OfflineMessage& msg) {
                               return now - msg.created_at > msg.ttl;
                             }),
              vec.end());
    if (vec.empty()) {
      it = messages_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace mi::server
