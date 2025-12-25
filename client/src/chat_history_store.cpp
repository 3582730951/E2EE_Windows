#include "chat_history_store.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>

#include "dpapi_util.h"
#include "monocypher.h"

#include "../server/include/crypto.h"
#include "../server/include/protocol.h"

namespace mi::client {

namespace {

constexpr std::uint8_t kMagic[8] = {'M', 'I', 'H', 'L', 'O', 'G', '0', '1'};
constexpr std::uint8_t kVersion = 1;

constexpr std::uint8_t kRecordMeta = 1;
constexpr std::uint8_t kRecordMessage = 2;
constexpr std::uint8_t kRecordStatus = 3;

constexpr std::uint8_t kMessageKindEnvelope = 1;
constexpr std::uint8_t kMessageKindSystem = 2;

constexpr std::size_t kMaxRecordCipherLen = 2u * 1024u * 1024u;
constexpr std::size_t kMaxHistoryKeyFileBytes = 64u * 1024u;

bool IsAllZero(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return true;
  }
  std::uint8_t acc = 0;
  for (std::size_t i = 0; i < len; ++i) {
    acc |= data[i];
  }
  return acc == 0;
}

std::string BytesToHexLower(const std::uint8_t* data, std::size_t len) {
  static constexpr char kHex[] = "0123456789abcdef";
  if (!data || len == 0) {
    return {};
  }
  std::string out;
  out.resize(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    out[i * 2] = kHex[data[i] >> 4];
    out[i * 2 + 1] = kHex[data[i] & 0x0F];
  }
  return out;
}

std::string Sha256HexLower(const std::vector<std::uint8_t>& in) {
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(in.data(), in.size(), d);
  return BytesToHexLower(d.bytes.data(), d.bytes.size());
}

bool ReadExact(std::ifstream& in, void* dst, std::size_t len) {
  if (len == 0) {
    return true;
  }
  in.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(len));
  return in.good();
}

bool WriteUint32(std::ofstream& out, std::uint32_t v) {
  const std::uint8_t b[4] = {
      static_cast<std::uint8_t>(v & 0xFF),
      static_cast<std::uint8_t>((v >> 8) & 0xFF),
      static_cast<std::uint8_t>((v >> 16) & 0xFF),
      static_cast<std::uint8_t>((v >> 24) & 0xFF),
  };
  out.write(reinterpret_cast<const char*>(b), sizeof(b));
  return out.good();
}

bool ReadUint32(std::ifstream& in, std::uint32_t& v) {
  std::uint8_t b[4];
  if (!ReadExact(in, b, sizeof(b))) {
    return false;
  }
  v = static_cast<std::uint32_t>(b[0]) |
      (static_cast<std::uint32_t>(b[1]) << 8) |
      (static_cast<std::uint32_t>(b[2]) << 16) |
      (static_cast<std::uint32_t>(b[3]) << 24);
  return true;
}

bool LooksLikeChatEnvelopeId(const std::vector<std::uint8_t>& envelope,
                             std::array<std::uint8_t, 16>& out_msg_id) {
  out_msg_id.fill(0);
  if (envelope.size() < (4 + 1 + 1 + out_msg_id.size())) {
    return false;
  }
  static constexpr std::uint8_t kChatMagic[4] = {'M', 'I', 'C', 'H'};
  if (std::memcmp(envelope.data(), kChatMagic, sizeof(kChatMagic)) != 0) {
    return false;
  }
  const std::size_t off = sizeof(kChatMagic) + 1 + 1;
  std::memcpy(out_msg_id.data(), envelope.data() + off, out_msg_id.size());
  return true;
}

std::filesystem::path ConversationPath(const std::filesystem::path& conv_dir,
                                       bool is_group,
                                       const std::string& conv_id) {
  std::vector<std::uint8_t> buf;
  buf.reserve(3 + conv_id.size());
  buf.push_back('m');
  buf.push_back(is_group ? 'g' : 'p');
  buf.push_back(0);
  buf.insert(buf.end(), conv_id.begin(), conv_id.end());
  const std::string hex = Sha256HexLower(buf);
  const std::string name =
      std::string(is_group ? "g_" : "p_") + hex.substr(0, 32) + ".bin";
  return conv_dir / name;
}

}  // namespace

ChatHistoryStore::ChatHistoryStore() = default;

ChatHistoryStore::~ChatHistoryStore() {
  if (key_loaded_) {
    crypto_wipe(master_key_.data(), master_key_.size());
  }
}

bool ChatHistoryStore::Init(const std::filesystem::path& e2ee_state_dir,
                            const std::string& username,
                            std::string& error) {
  error.clear();
  e2ee_state_dir_ = e2ee_state_dir;
  user_dir_.clear();
  conv_dir_.clear();
  key_path_.clear();
  key_loaded_ = false;
  master_key_.fill(0);

  if (e2ee_state_dir_.empty()) {
    error = "state dir empty";
    return false;
  }
  if (username.empty()) {
    error = "username empty";
    return false;
  }

  std::vector<std::uint8_t> user_bytes(username.begin(), username.end());
  const std::string user_hash = Sha256HexLower(user_bytes);
  if (user_hash.empty()) {
    error = "username hash failed";
    return false;
  }

  user_dir_ = e2ee_state_dir_ / "history" / user_hash.substr(0, 32);
  conv_dir_ = user_dir_ / "conversations";
  key_path_ = user_dir_ / "history_key.bin";

  std::error_code ec;
  std::filesystem::create_directories(conv_dir_, ec);
  return EnsureKeyLoaded(error);
}

bool ChatHistoryStore::EnsureKeyLoaded(std::string& error) {
  error.clear();
  if (key_loaded_) {
    return true;
  }
  if (key_path_.empty()) {
    error = "history key path empty";
    return false;
  }

  std::error_code ec;
  if (std::filesystem::exists(key_path_, ec)) {
    if (ec) {
      error = "history key path error";
      return false;
    }
    const auto size = std::filesystem::file_size(key_path_, ec);
    if (ec) {
      error = "history key size stat failed";
      return false;
    }
    if (size > kMaxHistoryKeyFileBytes) {
      error = "history key too large";
      return false;
    }
  } else if (ec) {
    error = "history key path error";
    return false;
  }

  std::vector<std::uint8_t> bytes;
  {
    std::ifstream f(key_path_, std::ios::binary);
    if (f.is_open()) {
      bytes.assign(std::istreambuf_iterator<char>(f),
                   std::istreambuf_iterator<char>());
    }
  }

  if (!bytes.empty()) {
    std::vector<std::uint8_t> plain;
    bool was_dpapi = false;
    static constexpr char kMagic[] = "MI_E2EE_HISTORY_KEY_DPAPI1";
    static constexpr char kEntropy[] = "MI_E2EE_HISTORY_KEY_ENTROPY_V1";
    std::string dpapi_err;
    if (!MaybeUnprotectDpapi(bytes, kMagic, kEntropy, plain, was_dpapi,
                             dpapi_err)) {
      error = dpapi_err.empty() ? "history key unprotect failed" : dpapi_err;
      return false;
    }

    if (plain.size() != master_key_.size()) {
      error = "history key size invalid";
      return false;
    }
    std::memcpy(master_key_.data(), plain.data(), master_key_.size());
    key_loaded_ = true;

#ifdef _WIN32
    if (!was_dpapi) {
      std::vector<std::uint8_t> wrapped;
      std::string wrap_err;
      if (ProtectDpapi(plain, kMagic, kEntropy, wrapped, wrap_err)) {
        std::error_code ec;
        const auto tmp = key_path_.string() + ".tmp";
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (out) {
          out.write(reinterpret_cast<const char*>(wrapped.data()),
                    static_cast<std::streamsize>(wrapped.size()));
          out.close();
          if (out) {
            std::filesystem::rename(tmp, key_path_, ec);
            if (ec) {
              std::filesystem::remove(tmp, ec);
            }
          } else {
            std::filesystem::remove(tmp, ec);
          }
        }
      }
    }
#endif
    return true;
  }

  std::array<std::uint8_t, 32> k{};
  if (!mi::server::crypto::RandomBytes(k.data(), k.size())) {
    error = "rng failed";
    return false;
  }

  std::vector<std::uint8_t> plain(k.begin(), k.end());
  std::vector<std::uint8_t> out_bytes = plain;
#ifdef _WIN32
  static constexpr char kMagic[] = "MI_E2EE_HISTORY_KEY_DPAPI1";
  static constexpr char kEntropy[] = "MI_E2EE_HISTORY_KEY_ENTROPY_V1";
  std::string wrap_err;
  std::vector<std::uint8_t> wrapped;
  if (!ProtectDpapi(plain, kMagic, kEntropy, wrapped, wrap_err)) {
    error = wrap_err.empty() ? "history key protect failed" : wrap_err;
    return false;
  }
  out_bytes = std::move(wrapped);
#endif

  std::error_code ec2;
  std::filesystem::create_directories(user_dir_, ec2);
  const auto tmp = key_path_.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      error = "history key write failed";
      return false;
    }
    out.write(reinterpret_cast<const char*>(out_bytes.data()),
              static_cast<std::streamsize>(out_bytes.size()));
    out.close();
    if (!out) {
      error = "history key write failed";
      std::filesystem::remove(tmp, ec2);
      return false;
    }
  }
  std::filesystem::rename(tmp, key_path_, ec2);
  if (ec2) {
    std::filesystem::remove(tmp, ec2);
    error = "history key write failed";
    return false;
  }

  master_key_ = k;
  key_loaded_ = true;
  return true;
}

bool ChatHistoryStore::DeriveConversationKey(
    bool is_group,
    const std::string& conv_id,
    std::array<std::uint8_t, 32>& out_key,
    std::string& error) const {
  error.clear();
  out_key.fill(0);
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    error = "history key missing";
    return false;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }

  std::vector<std::uint8_t> info;
  static constexpr char kPrefix[] = "MI_E2EE_HISTORY_CONV_KEY_V1";
  info.insert(info.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  info.push_back(0);
  info.push_back(is_group ? 1 : 0);
  info.push_back(0);
  info.insert(info.end(), conv_id.begin(), conv_id.end());

  std::array<std::uint8_t, 32> salt{};
  static constexpr char kSalt[] = "MI_E2EE_HISTORY_SALT_V1";
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(reinterpret_cast<const std::uint8_t*>(kSalt),
                             sizeof(kSalt) - 1, d);
  salt = d.bytes;

  if (!mi::server::crypto::HkdfSha256(master_key_.data(), master_key_.size(),
                                      salt.data(), salt.size(), info.data(),
                                      info.size(), out_key.data(),
                                      out_key.size())) {
    error = "history hkdf failed";
    return false;
  }
  return true;
}

bool ChatHistoryStore::EnsureConversationFile(
    bool is_group,
    const std::string& conv_id,
    std::filesystem::path& out_path,
    std::array<std::uint8_t, 32>& out_conv_key,
    std::string& error) const {
  error.clear();
  out_path.clear();
  out_conv_key.fill(0);
  if (conv_dir_.empty()) {
    error = "history conv dir empty";
    return false;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }
  if (!DeriveConversationKey(is_group, conv_id, out_conv_key, error)) {
    return false;
  }

  out_path = ConversationPath(conv_dir_, is_group, conv_id);
  if (out_path.empty()) {
    error = "history conv path failed";
    return false;
  }

  std::error_code ec;
  if (std::filesystem::exists(out_path, ec)) {
    return true;
  }
  std::filesystem::create_directories(out_path.parent_path(), ec);

  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "history create failed";
    return false;
  }
  out.write(reinterpret_cast<const char*>(kMagic), sizeof(kMagic));
  out.put(static_cast<char>(kVersion));
  if (!out.good()) {
    error = "history create failed";
    return false;
  }

  std::vector<std::uint8_t> meta;
  meta.reserve(4 + conv_id.size());
  meta.push_back(kRecordMeta);
  meta.push_back(is_group ? 1 : 0);
  if (!mi::server::proto::WriteString(conv_id, meta)) {
    error = "conv id too long";
    return false;
  }
  if (!WriteRecord(out, master_key_, meta, error)) {
    return false;
  }
  out.flush();
  if (!out.good()) {
    error = "history create failed";
    return false;
  }
  return true;
}

bool ChatHistoryStore::WriteRecord(std::ofstream& out,
                                  const std::array<std::uint8_t, 32>& conv_key,
                                  const std::vector<std::uint8_t>& plain,
                                  std::string& error) {
  error.clear();
  if (!out) {
    error = "history write failed";
    return false;
  }
  if (plain.empty()) {
    error = "history record empty";
    return false;
  }
  if (IsAllZero(conv_key.data(), conv_key.size())) {
    error = "history key invalid";
    return false;
  }

  std::array<std::uint8_t, 24> nonce{};
  if (!mi::server::crypto::RandomBytes(nonce.data(), nonce.size())) {
    error = "rng failed";
    return false;
  }

  std::vector<std::uint8_t> cipher(plain.size());
  std::array<std::uint8_t, 16> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), conv_key.data(), nonce.data(),
                   nullptr, 0, plain.data(), plain.size());

  const std::uint32_t cipher_len = static_cast<std::uint32_t>(
      cipher.size() > std::numeric_limits<std::uint32_t>::max()
          ? std::numeric_limits<std::uint32_t>::max()
          : cipher.size());
  if (cipher_len != cipher.size()) {
    error = "history record too large";
    return false;
  }

  if (!WriteUint32(out, cipher_len)) {
    error = "history write failed";
    return false;
  }
  out.write(reinterpret_cast<const char*>(nonce.data()),
            static_cast<std::streamsize>(nonce.size()));
  out.write(reinterpret_cast<const char*>(cipher.data()),
            static_cast<std::streamsize>(cipher.size()));
  out.write(reinterpret_cast<const char*>(mac.data()),
            static_cast<std::streamsize>(mac.size()));
  if (!out.good()) {
    error = "history write failed";
    return false;
  }
  return true;
}

bool ChatHistoryStore::ReadRecord(std::ifstream& in,
                                 const std::array<std::uint8_t, 32>& conv_key,
                                 const std::array<std::uint8_t, 32>& master_key,
                                 std::vector<std::uint8_t>& out_plain,
                                 std::string& error) {
  error.clear();
  out_plain.clear();
  if (!in) {
    error = "history read failed";
    return false;
  }
  if (IsAllZero(conv_key.data(), conv_key.size()) &&
      IsAllZero(master_key.data(), master_key.size())) {
    error = "history key invalid";
    return false;
  }

  std::uint32_t cipher_len = 0;
  if (!ReadUint32(in, cipher_len)) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }
  if (cipher_len == 0 || cipher_len > kMaxRecordCipherLen) {
    error = "history record size invalid";
    return false;
  }

  std::array<std::uint8_t, 24> nonce{};
  if (!ReadExact(in, nonce.data(), nonce.size())) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }

  std::vector<std::uint8_t> cipher;
  cipher.resize(cipher_len);
  if (!ReadExact(in, cipher.data(), cipher.size())) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }

  std::array<std::uint8_t, 16> mac{};
  if (!ReadExact(in, mac.data(), mac.size())) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }

  const auto try_dec = [&](const std::array<std::uint8_t, 32>& key) -> bool {
    if (IsAllZero(key.data(), key.size())) {
      return false;
    }
    std::vector<std::uint8_t> plain(cipher.size());
    const int ok = crypto_aead_unlock(plain.data(), mac.data(), key.data(),
                                      nonce.data(), nullptr, 0, cipher.data(),
                                      cipher.size());
    if (ok != 0) {
      return false;
    }
    out_plain = std::move(plain);
    return true;
  };

  if (try_dec(conv_key)) {
    return true;
  }
  if (try_dec(master_key)) {
    return true;
  }
  error = "history auth failed";
  out_plain.clear();
  return false;
}

bool ChatHistoryStore::AppendEnvelope(bool is_group,
                                      bool outgoing,
                                      const std::string& conv_id,
                                      const std::string& sender,
                                      const std::vector<std::uint8_t>& envelope,
                                      ChatHistoryStatus status,
                                      std::uint64_t timestamp_sec,
                                      std::string& error) {
  error.clear();
  if (!EnsureKeyLoaded(error)) {
    return false;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }
  if (envelope.empty()) {
    error = "envelope empty";
    return false;
  }

  std::filesystem::path path;
  std::array<std::uint8_t, 32> conv_key{};
  if (!EnsureConversationFile(is_group, conv_id, path, conv_key, error)) {
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out) {
    error = "history write failed";
    return false;
  }

  std::vector<std::uint8_t> rec;
  rec.reserve(5 + 8 + 2 + sender.size() + 4 + envelope.size());
  rec.push_back(kRecordMessage);
  rec.push_back(kMessageKindEnvelope);
  rec.push_back(is_group ? 1 : 0);
  rec.push_back(outgoing ? 1 : 0);
  rec.push_back(static_cast<std::uint8_t>(status));
  mi::server::proto::WriteUint64(timestamp_sec, rec);
  mi::server::proto::WriteString(sender, rec);
  mi::server::proto::WriteBytes(envelope, rec);
  if (!WriteRecord(out, conv_key, rec, error)) {
    return false;
  }
  return true;
}

bool ChatHistoryStore::AppendSystem(bool is_group,
                                    const std::string& conv_id,
                                    const std::string& text_utf8,
                                    std::uint64_t timestamp_sec,
                                    std::string& error) {
  error.clear();
  if (!EnsureKeyLoaded(error)) {
    return false;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }
  if (text_utf8.empty()) {
    error = "system text empty";
    return false;
  }

  std::filesystem::path path;
  std::array<std::uint8_t, 32> conv_key{};
  if (!EnsureConversationFile(is_group, conv_id, path, conv_key, error)) {
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out) {
    error = "history write failed";
    return false;
  }

  std::vector<std::uint8_t> rec;
  rec.reserve(5 + 8 + 2 + text_utf8.size());
  rec.push_back(kRecordMessage);
  rec.push_back(kMessageKindSystem);
  rec.push_back(is_group ? 1 : 0);
  rec.push_back(0);
  rec.push_back(static_cast<std::uint8_t>(ChatHistoryStatus::kSent));
  mi::server::proto::WriteUint64(timestamp_sec, rec);
  mi::server::proto::WriteString(text_utf8, rec);
  if (!WriteRecord(out, conv_key, rec, error)) {
    return false;
  }
  return true;
}

bool ChatHistoryStore::AppendStatusUpdate(
    bool is_group,
    const std::string& conv_id,
    const std::array<std::uint8_t, 16>& msg_id,
    ChatHistoryStatus status,
    std::uint64_t timestamp_sec,
    std::string& error) {
  error.clear();
  if (!EnsureKeyLoaded(error)) {
    return false;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }
  if (IsAllZero(msg_id.data(), msg_id.size())) {
    error = "msg id empty";
    return false;
  }

  std::filesystem::path path;
  std::array<std::uint8_t, 32> conv_key{};
  if (!EnsureConversationFile(is_group, conv_id, path, conv_key, error)) {
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out) {
    error = "history write failed";
    return false;
  }

  std::vector<std::uint8_t> rec;
  rec.reserve(1 + 1 + 1 + 8 + 16);
  rec.push_back(kRecordStatus);
  rec.push_back(is_group ? 1 : 0);
  rec.push_back(static_cast<std::uint8_t>(status));
  mi::server::proto::WriteUint64(timestamp_sec, rec);
  rec.insert(rec.end(), msg_id.begin(), msg_id.end());
  if (!WriteRecord(out, conv_key, rec, error)) {
    return false;
  }
  return true;
}

bool ChatHistoryStore::LoadConversation(bool is_group,
                                        const std::string& conv_id,
                                        std::size_t limit,
                                        std::vector<ChatHistoryMessage>& out_messages,
                                        std::string& error) const {
  error.clear();
  out_messages.clear();
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    return true;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }

  std::array<std::uint8_t, 32> conv_key{};
  if (!DeriveConversationKey(is_group, conv_id, conv_key, error)) {
    return false;
  }
  const auto path = ConversationPath(conv_dir_, is_group, conv_id);
  if (path.empty()) {
    error = "history path failed";
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return true;
  }

  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    error = "history open failed";
    return false;
  }

  if (limit > 0) {
    out_messages.reserve(limit);
  }
  std::uint8_t hdr[sizeof(kMagic) + 1];
  if (!ReadExact(in, hdr, sizeof(hdr))) {
    return true;
  }
  if (std::memcmp(hdr, kMagic, sizeof(kMagic)) != 0 || hdr[sizeof(kMagic)] != kVersion) {
    error = "history magic mismatch";
    return false;
  }

  std::unordered_map<std::string, ChatHistoryStatus> status_by_id;
  std::unordered_map<std::string, std::size_t> index_by_id;
  std::size_t reserve_hint = 512;
  if (limit > 0) {
    reserve_hint = std::min<std::size_t>(limit * 2, 8192);
  }
  status_by_id.reserve(reserve_hint);
  index_by_id.reserve(reserve_hint);

  const auto statusRank = [](ChatHistoryStatus status) -> int {
    switch (status) {
      case ChatHistoryStatus::kFailed:
        return 0;
      case ChatHistoryStatus::kSent:
        return 1;
      case ChatHistoryStatus::kDelivered:
        return 2;
      case ChatHistoryStatus::kRead:
        return 3;
    }
    return 0;
  };
  const auto betterStatus = [&](ChatHistoryStatus a,
                                ChatHistoryStatus b) -> ChatHistoryStatus {
    return statusRank(a) >= statusRank(b) ? a : b;
  };
  const auto tryParseStatus = [](std::uint8_t raw,
                                 ChatHistoryStatus& out) -> bool {
    if (raw > static_cast<std::uint8_t>(ChatHistoryStatus::kFailed)) {
      return false;
    }
    out = static_cast<ChatHistoryStatus>(raw);
    return true;
  };

  while (true) {
    std::vector<std::uint8_t> plain;
    std::string rec_err;
    if (!ReadRecord(in, conv_key, master_key_, plain, rec_err)) {
      error = rec_err.empty() ? "history read failed" : rec_err;
      return false;
    }
    if (plain.empty()) {
      break;
    }
    std::size_t off = 0;
    const std::uint8_t type = plain[off++];
    if (type == kRecordMeta) {
      continue;
    }
    if (type == kRecordStatus) {
      if (off + 1 + 1 + 8 + 16 > plain.size()) {
        continue;
      }
      const bool rec_group = plain[off++] != 0;
      const std::uint8_t raw_st = plain[off++];
      if (rec_group != is_group) {
        continue;
      }
      ChatHistoryStatus st;
      if (!tryParseStatus(raw_st, st)) {
        continue;
      }
      std::uint64_t ts = 0;
      if (!mi::server::proto::ReadUint64(plain, off, ts) ||
          off + 16 != plain.size()) {
        continue;
      }
      std::array<std::uint8_t, 16> msg_id{};
      std::memcpy(msg_id.data(), plain.data() + off, msg_id.size());
      const std::string id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
      auto st_it = status_by_id.find(id_hex);
      if (st_it == status_by_id.end()) {
        st_it = status_by_id.emplace(id_hex, st).first;
      } else {
        st_it->second = betterStatus(st_it->second, st);
      }

      const auto it = index_by_id.find(id_hex);
      if (it != index_by_id.end() && it->second < out_messages.size()) {
        out_messages[it->second].status =
            betterStatus(out_messages[it->second].status, st_it->second);
      }
      continue;
    }
    if (type != kRecordMessage) {
      continue;
    }
    if (off + 1 + 1 + 1 + 1 + 8 > plain.size()) {
      continue;
    }
    const std::uint8_t kind = plain[off++];
    const bool rec_group = plain[off++] != 0;
    const bool outgoing = plain[off++] != 0;
    const std::uint8_t raw_st = plain[off++];
    if (rec_group != is_group) {
      continue;
    }
    ChatHistoryStatus st;
    if (!tryParseStatus(raw_st, st)) {
      continue;
    }
    std::uint64_t ts = 0;
    if (!mi::server::proto::ReadUint64(plain, off, ts)) {
      continue;
    }

    ChatHistoryMessage m;
    m.is_group = rec_group;
    m.outgoing = outgoing;
    m.status = st;
    m.timestamp_sec = ts;
    m.conv_id = conv_id;

    if (kind == kMessageKindEnvelope) {
      if (!mi::server::proto::ReadString(plain, off, m.sender) ||
          !mi::server::proto::ReadBytes(plain, off, m.envelope) ||
          off != plain.size()) {
        continue;
      }
      m.is_system = false;
      std::array<std::uint8_t, 16> msg_id{};
      if (LooksLikeChatEnvelopeId(m.envelope, msg_id)) {
        const std::string id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
        const auto it = status_by_id.find(id_hex);
        if (it != status_by_id.end()) {
          m.status = betterStatus(m.status, it->second);
        }
        const auto prev = index_by_id.find(id_hex);
        if (prev != index_by_id.end() && prev->second < out_messages.size()) {
          ChatHistoryMessage& existing = out_messages[prev->second];
          existing.is_group = rec_group;
          existing.outgoing = outgoing;
          existing.is_system = false;
          existing.status = betterStatus(existing.status, m.status);
          existing.sender = std::move(m.sender);
          existing.envelope = std::move(m.envelope);
          continue;
        }
        index_by_id.emplace(id_hex, out_messages.size());
      }
      out_messages.push_back(std::move(m));
      continue;
    }
    if (kind == kMessageKindSystem) {
      std::string text;
      if (!mi::server::proto::ReadString(plain, off, text) || off != plain.size()) {
        continue;
      }
      m.is_system = true;
      m.system_text_utf8 = std::move(text);
      out_messages.push_back(std::move(m));
      continue;
    }
  }

  if (limit > 0 && out_messages.size() > limit) {
    out_messages.erase(out_messages.begin(),
                       out_messages.end() - static_cast<std::ptrdiff_t>(limit));
  }
  return true;
}

bool ChatHistoryStore::ExportRecentSnapshot(
    std::size_t max_conversations,
    std::size_t max_messages_per_conversation,
    std::vector<ChatHistoryMessage>& out_messages,
    std::string& error) const {
  error.clear();
  out_messages.clear();
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    return true;
  }
  if (conv_dir_.empty()) {
    return true;
  }

  struct ConvSnapshot {
    bool is_group{false};
    std::string conv_id;
    std::uint64_t last_ts{0};
    std::vector<ChatHistoryMessage> msgs;
  };

  std::vector<ConvSnapshot> convs;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(conv_dir_, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto path = entry.path();
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
      continue;
    }
    std::uint8_t hdr[sizeof(kMagic) + 1];
    if (!ReadExact(in, hdr, sizeof(hdr))) {
      continue;
    }
    if (std::memcmp(hdr, kMagic, sizeof(kMagic)) != 0 ||
        hdr[sizeof(kMagic)] != kVersion) {
      continue;
    }

    std::vector<std::uint8_t> meta_plain;
    std::string meta_err;
    if (!ReadRecord(in, master_key_, master_key_, meta_plain, meta_err)) {
      continue;
    }
    if (meta_plain.size() < 2 || meta_plain[0] != kRecordMeta) {
      continue;
    }
    std::size_t off = 1;
    const bool is_group = meta_plain[off++] != 0;
    std::string conv_id;
    if (!mi::server::proto::ReadString(meta_plain, off, conv_id) ||
        off != meta_plain.size() || conv_id.empty()) {
      continue;
    }

    std::vector<ChatHistoryMessage> msgs;
    std::string load_err;
    if (!LoadConversation(is_group, conv_id, max_messages_per_conversation, msgs,
                          load_err) ||
        msgs.empty()) {
      continue;
    }

    ConvSnapshot s;
    s.is_group = is_group;
    s.conv_id = conv_id;
    s.msgs = std::move(msgs);
    s.last_ts = 0;
    for (const auto& m : s.msgs) {
      s.last_ts = std::max(s.last_ts, m.timestamp_sec);
    }
    convs.push_back(std::move(s));
  }

  std::sort(convs.begin(), convs.end(),
            [](const ConvSnapshot& a, const ConvSnapshot& b) {
              return a.last_ts > b.last_ts;
            });
  if (max_conversations > 0 && convs.size() > max_conversations) {
    convs.resize(max_conversations);
  }

  for (auto& c : convs) {
    out_messages.insert(out_messages.end(),
                        std::make_move_iterator(c.msgs.begin()),
                        std::make_move_iterator(c.msgs.end()));
  }
  return true;
}

}  // namespace mi::client
