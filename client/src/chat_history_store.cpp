#include "chat_history_store.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <utility>

#include "dpapi_util.h"
#include "monocypher.h"

#include "../server/include/crypto.h"
#include "../server/include/protocol.h"

namespace mi::client {

namespace {

constexpr std::uint8_t kContainerMagic[8] = {'M', 'I', 'H', 'D',
                                             'B', '0', '1', 0};
constexpr std::uint8_t kContainerVersion = 1;
constexpr std::size_t kPeStubSize = 512;
constexpr std::size_t kMaxConversationsPerFile = 3;
constexpr std::size_t kSeqWidth = 6;
constexpr std::uint8_t kLegacyMagic[8] = {'M', 'I', 'H', 'L',
                                          'O', 'G', '0', '1'};
constexpr std::uint8_t kLegacyVersion = 1;

constexpr std::uint8_t kRecordMeta = 1;
constexpr std::uint8_t kRecordMessage = 2;
constexpr std::uint8_t kRecordStatus = 3;

constexpr std::uint8_t kMessageKindEnvelope = 1;
constexpr std::uint8_t kMessageKindSystem = 2;

constexpr std::uint8_t kPadMagic[4] = {'M', 'I', 'P', 'D'};
constexpr std::size_t kPadHeaderBytes = 8;
constexpr std::size_t kPadBuckets[] = {256, 512, 1024, 2048,
                                       4096, 8192, 16384};

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

bool RandomUint32(std::uint32_t& out) {
  return mi::server::crypto::RandomBytes(reinterpret_cast<std::uint8_t*>(&out),
                                         sizeof(out));
}

std::size_t SelectPadTarget(std::size_t min_len) {
  for (const auto bucket : kPadBuckets) {
    if (bucket >= min_len) {
      if (bucket == min_len) {
        return bucket;
      }
      std::uint32_t r = 0;
      if (!RandomUint32(r)) {
        return bucket;
      }
      const std::size_t span = bucket - min_len;
      return min_len + (static_cast<std::size_t>(r) % (span + 1));
    }
  }
  const std::size_t round = ((min_len + 4095) / 4096) * 4096;
  if (round <= min_len) {
    return min_len;
  }
  std::uint32_t r = 0;
  if (!RandomUint32(r)) {
    return round;
  }
  const std::size_t span = round - min_len;
  return min_len + (static_cast<std::size_t>(r) % (span + 1));
}

bool PadPlain(const std::vector<std::uint8_t>& plain,
              std::vector<std::uint8_t>& out,
              std::string& error) {
  error.clear();
  out.clear();
  if (plain.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "pad size overflow";
    return false;
  }
  const std::size_t min_len = kPadHeaderBytes + plain.size();
  const std::size_t target_len = SelectPadTarget(min_len);
  out.reserve(target_len);
  out.insert(out.end(), kPadMagic, kPadMagic + sizeof(kPadMagic));
  const std::uint32_t len32 = static_cast<std::uint32_t>(plain.size());
  out.push_back(static_cast<std::uint8_t>(len32 & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len32 >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len32 >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len32 >> 24) & 0xFF));
  out.insert(out.end(), plain.begin(), plain.end());
  if (out.size() < target_len) {
    const std::size_t pad_len = target_len - out.size();
    const std::size_t offset = out.size();
    out.resize(target_len);
    if (!mi::server::crypto::RandomBytes(out.data() + offset, pad_len)) {
      error = "pad rng failed";
      return false;
    }
  }
  return true;
}

bool UnpadPlain(const std::vector<std::uint8_t>& plain,
                std::vector<std::uint8_t>& out,
                std::string& error) {
  error.clear();
  out.clear();
  if (plain.size() < kPadHeaderBytes ||
      std::memcmp(plain.data(), kPadMagic, sizeof(kPadMagic)) != 0) {
    out = plain;
    return true;
  }
  const std::uint32_t len =
      static_cast<std::uint32_t>(plain[4]) |
      (static_cast<std::uint32_t>(plain[5]) << 8) |
      (static_cast<std::uint32_t>(plain[6]) << 16) |
      (static_cast<std::uint32_t>(plain[7]) << 24);
  if (kPadHeaderBytes + len > plain.size()) {
    error = "pad size invalid";
    return false;
  }
  out.assign(plain.begin() + kPadHeaderBytes,
             plain.begin() + kPadHeaderBytes + len);
  return true;
}

std::string MakeConvKey(bool is_group, const std::string& conv_id) {
  std::string out;
  out.reserve(conv_id.size() + 2);
  out.push_back(is_group ? 'g' : 'p');
  out.push_back(':');
  out.append(conv_id);
  return out;
}

std::string PadSeq(std::uint32_t seq) {
  std::string s = std::to_string(seq);
  if (s.size() < kSeqWidth) {
    s.insert(0, kSeqWidth - s.size(), '0');
  }
  return s;
}

std::string BuildHistoryFileName(const std::string& user_tag,
                                 std::uint32_t seq) {
  return "main_" + user_tag + "_" + PadSeq(seq) + ".dll";
}

bool ParseHistoryFileName(const std::string& name,
                          const std::string& user_tag,
                          std::uint32_t& out_seq) {
  out_seq = 0;
  if (user_tag.empty()) {
    return false;
  }
  const std::string prefix = "main_" + user_tag + "_";
  if (name.size() <= prefix.size() + 4 || name.rfind(prefix, 0) != 0 ||
      name.substr(name.size() - 4) != ".dll") {
    return false;
  }
  const std::size_t num_len = name.size() - prefix.size() - 4;
  if (num_len == 0) {
    return false;
  }
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < num_len; ++i) {
    const char c = name[prefix.size() + i];
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return false;
    }
    value = value * 10u + static_cast<std::uint64_t>(c - '0');
    if (value > (std::numeric_limits<std::uint32_t>::max)()) {
      return false;
    }
  }
  out_seq = static_cast<std::uint32_t>(value);
  return out_seq != 0;
}

void WriteLe16(std::vector<std::uint8_t>& buf, std::size_t off,
               std::uint16_t v) {
  if (off + 2 > buf.size()) {
    return;
  }
  buf[off] = static_cast<std::uint8_t>(v & 0xFF);
  buf[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}

void WriteLe32(std::vector<std::uint8_t>& buf, std::size_t off,
               std::uint32_t v) {
  if (off + 4 > buf.size()) {
    return;
  }
  buf[off] = static_cast<std::uint8_t>(v & 0xFF);
  buf[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
  buf[off + 2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
  buf[off + 3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
}

std::vector<std::uint8_t> BuildPeStub() {
  std::vector<std::uint8_t> buf(kPeStubSize, 0);
  buf[0] = 'M';
  buf[1] = 'Z';
  WriteLe32(buf, 0x3C, 0x80);
  buf[0x80] = 'P';
  buf[0x81] = 'E';
  buf[0x82] = 0;
  buf[0x83] = 0;
  WriteLe16(buf, 0x84, 0x14c);
  WriteLe16(buf, 0x86, 1);
  WriteLe32(buf, 0x88, 0);
  WriteLe32(buf, 0x8C, 0);
  WriteLe32(buf, 0x90, 0);
  WriteLe16(buf, 0x94, 0xE0);
  WriteLe16(buf, 0x96, 0x2102);
  WriteLe16(buf, 0x98, 0x10B);
  buf[0x9A] = 0;
  buf[0x9B] = 0;
  WriteLe32(buf, 0x9C, 0);
  WriteLe32(buf, 0xA0, 0x200);
  WriteLe32(buf, 0xA4, 0);
  WriteLe32(buf, 0xA8, 0);
  WriteLe32(buf, 0xAC, 0x1000);
  WriteLe32(buf, 0xB0, 0x1000);
  WriteLe32(buf, 0xB4, 0x400000);
  WriteLe32(buf, 0xB8, 0x1000);
  WriteLe32(buf, 0xBC, 0x200);
  WriteLe16(buf, 0xC0, 6);
  WriteLe16(buf, 0xC2, 0);
  WriteLe16(buf, 0xC4, 0);
  WriteLe16(buf, 0xC6, 0);
  WriteLe16(buf, 0xC8, 6);
  WriteLe16(buf, 0xCA, 0);
  WriteLe32(buf, 0xCC, 0);
  WriteLe32(buf, 0xD0, 0x2000);
  WriteLe32(buf, 0xD4, 0x200);
  WriteLe32(buf, 0xD8, 0);
  WriteLe16(buf, 0xDC, 2);
  WriteLe16(buf, 0xDE, 0);
  WriteLe32(buf, 0xE0, 0x100000);
  WriteLe32(buf, 0xE4, 0x1000);
  WriteLe32(buf, 0xE8, 0x100000);
  WriteLe32(buf, 0xEC, 0x1000);
  WriteLe32(buf, 0xF0, 0);
  WriteLe32(buf, 0xF4, 16);
  std::size_t sec = 0x178;
  const char name[8] = {'.', 'r', 'd', 'a', 't', 'a', 0, 0};
  std::memcpy(buf.data() + sec, name, sizeof(name));
  WriteLe32(buf, sec + 8, 0x1000);
  WriteLe32(buf, sec + 12, 0x1000);
  WriteLe32(buf, sec + 16, 0x200);
  WriteLe32(buf, sec + 20, 0x200);
  WriteLe32(buf, sec + 36, 0x40000040);
  return buf;
}

const std::vector<std::uint8_t>& PeStubBytes() {
  static const std::vector<std::uint8_t> stub = BuildPeStub();
  return stub;
}

bool WriteContainerHeader(std::ofstream& out, std::string& error) {
  error.clear();
  if (!out) {
    error = "history write failed";
    return false;
  }
  out.write(reinterpret_cast<const char*>(kContainerMagic),
            sizeof(kContainerMagic));
  out.put(static_cast<char>(kContainerVersion));
  const char zero[3] = {0, 0, 0};
  out.write(zero, sizeof(zero));
  if (!out.good()) {
    error = "history write failed";
    return false;
  }
  return true;
}

bool ReadContainerHeader(std::ifstream& in, std::string& error) {
  error.clear();
  std::uint8_t magic[sizeof(kContainerMagic)]{};
  if (!ReadExact(in, magic, sizeof(magic))) {
    error = "history read failed";
    return false;
  }
  if (std::memcmp(magic, kContainerMagic, sizeof(kContainerMagic)) != 0) {
    error = "history magic mismatch";
    return false;
  }
  std::uint8_t version = 0;
  if (!ReadExact(in, &version, sizeof(version))) {
    error = "history read failed";
    return false;
  }
  std::uint8_t reserved[3]{};
  if (!ReadExact(in, reserved, sizeof(reserved))) {
    error = "history read failed";
    return false;
  }
  if (version != kContainerVersion) {
    error = "history version mismatch";
    return false;
  }
  return true;
}

bool WriteEncryptedRecord(std::ofstream& out,
                          const std::array<std::uint8_t, 32>& master_key,
                          const std::array<std::uint8_t, 32>& conv_key,
                          bool is_group,
                          const std::string& conv_id,
                          const std::vector<std::uint8_t>& inner_plain,
                          std::string& error) {
  error.clear();
  if (!out) {
    error = "history write failed";
    return false;
  }
  if (IsAllZero(master_key.data(), master_key.size()) ||
      IsAllZero(conv_key.data(), conv_key.size())) {
    error = "history key invalid";
    return false;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }

  std::vector<std::uint8_t> padded;
  if (!PadPlain(inner_plain, padded, error)) {
    return false;
  }

  std::array<std::uint8_t, 24> inner_nonce{};
  if (!mi::server::crypto::RandomBytes(inner_nonce.data(),
                                       inner_nonce.size())) {
    error = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> inner_cipher(padded.size());
  std::array<std::uint8_t, 16> inner_mac{};
  crypto_aead_lock(inner_cipher.data(), inner_mac.data(), conv_key.data(),
                   inner_nonce.data(), nullptr, 0, padded.data(),
                   padded.size());

  std::vector<std::uint8_t> outer_plain;
  outer_plain.reserve(1 + 2 + conv_id.size() + inner_nonce.size() + 4 +
                      inner_cipher.size() + inner_mac.size());
  outer_plain.push_back(is_group ? 1 : 0);
  if (!mi::server::proto::WriteString(conv_id, outer_plain)) {
    error = "conv id too long";
    return false;
  }
  outer_plain.insert(outer_plain.end(), inner_nonce.begin(),
                     inner_nonce.end());
  if (!mi::server::proto::WriteBytes(inner_cipher, outer_plain)) {
    error = "history record too large";
    return false;
  }
  outer_plain.insert(outer_plain.end(), inner_mac.begin(), inner_mac.end());

  std::array<std::uint8_t, 24> outer_nonce{};
  if (!mi::server::crypto::RandomBytes(outer_nonce.data(),
                                       outer_nonce.size())) {
    error = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> outer_cipher(outer_plain.size());
  std::array<std::uint8_t, 16> outer_mac{};
  crypto_aead_lock(outer_cipher.data(), outer_mac.data(), master_key.data(),
                   outer_nonce.data(), nullptr, 0, outer_plain.data(),
                   outer_plain.size());

  if (outer_cipher.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "history record too large";
    return false;
  }
  const std::uint32_t cipher_len =
      static_cast<std::uint32_t>(outer_cipher.size());
  if (!WriteUint32(out, cipher_len)) {
    error = "history write failed";
    return false;
  }
  out.write(reinterpret_cast<const char*>(outer_nonce.data()),
            static_cast<std::streamsize>(outer_nonce.size()));
  out.write(reinterpret_cast<const char*>(outer_cipher.data()),
            static_cast<std::streamsize>(outer_cipher.size()));
  out.write(reinterpret_cast<const char*>(outer_mac.data()),
            static_cast<std::streamsize>(outer_mac.size()));
  if (!out.good()) {
    error = "history write failed";
    return false;
  }
  return true;
}

bool ReadOuterRecord(std::ifstream& in,
                     const std::array<std::uint8_t, 32>& master_key,
                     bool& out_has_record,
                     bool& out_is_group,
                     std::string& out_conv_id,
                     std::array<std::uint8_t, 24>& out_inner_nonce,
                     std::vector<std::uint8_t>& out_inner_cipher,
                     std::array<std::uint8_t, 16>& out_inner_mac,
                     std::string& error) {
  error.clear();
  out_has_record = false;
  out_is_group = false;
  out_conv_id.clear();
  out_inner_nonce.fill(0);
  out_inner_cipher.clear();
  out_inner_mac.fill(0);

  if (IsAllZero(master_key.data(), master_key.size())) {
    error = "history key invalid";
    return false;
  }
  if (!in) {
    error = "history read failed";
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

  std::vector<std::uint8_t> outer_plain(cipher.size());
  const int ok = crypto_aead_unlock(outer_plain.data(), mac.data(),
                                    master_key.data(), nonce.data(), nullptr,
                                    0, cipher.data(), cipher.size());
  if (ok != 0) {
    error = "history auth failed";
    return false;
  }

  std::size_t off = 0;
  if (outer_plain.empty()) {
    error = "history record empty";
    return false;
  }
  out_is_group = outer_plain[off++] != 0;
  if (!mi::server::proto::ReadString(outer_plain, off, out_conv_id) ||
      out_conv_id.empty()) {
    error = "history read failed";
    return false;
  }
  if (off + out_inner_nonce.size() > outer_plain.size()) {
    error = "history read failed";
    return false;
  }
  std::memcpy(out_inner_nonce.data(), outer_plain.data() + off,
              out_inner_nonce.size());
  off += out_inner_nonce.size();
  if (!mi::server::proto::ReadBytes(outer_plain, off, out_inner_cipher)) {
    error = "history read failed";
    return false;
  }
  if (off + out_inner_mac.size() != outer_plain.size()) {
    error = "history read failed";
    return false;
  }
  std::memcpy(out_inner_mac.data(), outer_plain.data() + off,
              out_inner_mac.size());
  out_has_record = true;
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

std::filesystem::path LegacyConversationPath(const std::filesystem::path& conv_dir,
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

bool ReadLegacyRecord(std::ifstream& in,
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
  key_path_.clear();
  legacy_conv_dir_.clear();
  history_dir_.clear();
  user_tag_.clear();
  history_files_.clear();
  conv_to_file_.clear();
  next_seq_ = 1;
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
  legacy_conv_dir_ = user_dir_ / "conversations";
  key_path_ = user_dir_ / "history_key.bin";
  user_tag_ = user_hash.substr(0, std::min<std::size_t>(16, user_hash.size()));
  history_dir_ = e2ee_state_dir_.parent_path();
  if (history_dir_.empty()) {
    history_dir_ = e2ee_state_dir_;
  }

  std::error_code ec;
  std::filesystem::create_directories(legacy_conv_dir_, ec);
  std::filesystem::create_directories(history_dir_, ec);
  if (!EnsureKeyLoaded(error)) {
    return false;
  }
  std::string scan_err;
  (void)LoadHistoryFiles(scan_err);
  return true;
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

bool ChatHistoryStore::LoadHistoryFiles(std::string& error) {
  error.clear();
  history_files_.clear();
  conv_to_file_.clear();
  next_seq_ = 1;
  if (history_dir_.empty() || user_tag_.empty()) {
    return true;
  }

  std::error_code ec;
  if (!std::filesystem::exists(history_dir_, ec)) {
    return true;
  }

  std::vector<HistoryFileEntry> files;
  for (const auto& entry : std::filesystem::directory_iterator(history_dir_, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto name = entry.path().filename().string();
    std::uint32_t seq = 0;
    if (!ParseHistoryFileName(name, user_tag_, seq)) {
      continue;
    }
    HistoryFileEntry file;
    file.path = entry.path();
    file.seq = seq;

    std::ifstream in(file.path, std::ios::binary);
    if (!in.is_open()) {
      continue;
    }
    std::vector<std::uint8_t> stub(kPeStubSize);
    if (!ReadExact(in, stub.data(), stub.size())) {
      continue;
    }
    if (stub.size() < 2 || stub[0] != 'M' || stub[1] != 'Z') {
      continue;
    }
    std::string hdr_err;
    if (!ReadContainerHeader(in, hdr_err)) {
      continue;
    }

    while (true) {
      bool has_record = false;
      bool is_group = false;
      std::string conv_id;
      std::array<std::uint8_t, 24> inner_nonce{};
      std::vector<std::uint8_t> inner_cipher;
      std::array<std::uint8_t, 16> inner_mac{};
      std::string rec_err;
      if (!ReadOuterRecord(in, master_key_, has_record, is_group, conv_id,
                           inner_nonce, inner_cipher, inner_mac, rec_err)) {
        break;
      }
      if (!has_record) {
        break;
      }
      if (!conv_id.empty()) {
        file.conv_keys.insert(MakeConvKey(is_group, conv_id));
      }
    }

    files.push_back(std::move(file));
    if (seq >= next_seq_) {
      next_seq_ = seq + 1;
    }
  }

  std::sort(files.begin(), files.end(),
            [](const HistoryFileEntry& a, const HistoryFileEntry& b) {
              return a.seq < b.seq;
            });
  history_files_ = std::move(files);
  for (std::size_t i = 0; i < history_files_.size(); ++i) {
    for (const auto& key : history_files_[i].conv_keys) {
      conv_to_file_[key] = i;
    }
  }
  return true;
}

bool ChatHistoryStore::EnsureHistoryFile(
    bool is_group,
    const std::string& conv_id,
    std::filesystem::path& out_path,
    std::array<std::uint8_t, 32>& out_conv_key,
    std::string& error) {
  error.clear();
  out_path.clear();
  out_conv_key.fill(0);
  if (history_dir_.empty()) {
    error = "history dir empty";
    return false;
  }
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }
  if (!DeriveConversationKey(is_group, conv_id, out_conv_key, error)) {
    return false;
  }

  const std::string conv_key = MakeConvKey(is_group, conv_id);
  auto it = conv_to_file_.find(conv_key);
  if (it != conv_to_file_.end() && it->second < history_files_.size()) {
    out_path = history_files_[it->second].path;
    return true;
  }

  std::size_t target = history_files_.size();
  for (std::size_t i = history_files_.size(); i > 0; --i) {
    if (history_files_[i - 1].conv_keys.size() < kMaxConversationsPerFile) {
      target = i - 1;
      break;
    }
  }
  if (target == history_files_.size()) {
    const std::uint32_t seq = next_seq_++;
    const std::string name = BuildHistoryFileName(user_tag_, seq);
    if (name.empty()) {
      error = "history create failed";
      return false;
    }
    const std::filesystem::path path = history_dir_ / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
      error = "history create failed";
      return false;
    }
    const auto& stub = PeStubBytes();
    out.write(reinterpret_cast<const char*>(stub.data()),
              static_cast<std::streamsize>(stub.size()));
    if (!out.good()) {
      error = "history create failed";
      return false;
    }
    if (!WriteContainerHeader(out, error)) {
      return false;
    }
    out.flush();
    if (!out.good()) {
      error = "history create failed";
      return false;
    }
    HistoryFileEntry entry;
    entry.path = path;
    entry.seq = seq;
    history_files_.push_back(std::move(entry));
    target = history_files_.size() - 1;
  }

  history_files_[target].conv_keys.insert(conv_key);
  conv_to_file_[conv_key] = target;
  out_path = history_files_[target].path;

  std::vector<ChatHistoryMessage> legacy;
  std::string legacy_err;
  if (LoadLegacyConversation(is_group, conv_id, 0, legacy, legacy_err) &&
      !legacy.empty()) {
    std::ofstream out(out_path, std::ios::binary | std::ios::app);
    if (out) {
      for (const auto& m : legacy) {
        std::vector<std::uint8_t> rec;
        std::string rec_err;
        bool ok = false;
        if (m.is_system) {
          rec.reserve(5 + 8 + 2 + m.system_text_utf8.size());
          rec.push_back(kRecordMessage);
          rec.push_back(kMessageKindSystem);
          rec.push_back(m.is_group ? 1 : 0);
          rec.push_back(0);
          rec.push_back(static_cast<std::uint8_t>(ChatHistoryStatus::kSent));
          mi::server::proto::WriteUint64(m.timestamp_sec, rec);
          ok = mi::server::proto::WriteString(m.system_text_utf8, rec);
        } else {
          rec.reserve(5 + 8 + 2 + m.sender.size() + 4 + m.envelope.size());
          rec.push_back(kRecordMessage);
          rec.push_back(kMessageKindEnvelope);
          rec.push_back(m.is_group ? 1 : 0);
          rec.push_back(m.outgoing ? 1 : 0);
          rec.push_back(static_cast<std::uint8_t>(m.status));
          mi::server::proto::WriteUint64(m.timestamp_sec, rec);
          ok = mi::server::proto::WriteString(m.sender, rec) &&
               mi::server::proto::WriteBytes(m.envelope, rec);
        }
        if (!ok) {
          continue;
        }
        std::string write_err;
        if (!WriteEncryptedRecord(out, master_key_, out_conv_key, is_group,
                                  conv_id, rec, write_err)) {
          break;
        }
      }
    }
  }
  return true;
}

bool ChatHistoryStore::LoadLegacyConversation(
    bool is_group,
    const std::string& conv_id,
    std::size_t limit,
    std::vector<ChatHistoryMessage>& out_messages,
    std::string& error) const {
  error.clear();
  out_messages.clear();
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    return true;
  }
  if (legacy_conv_dir_.empty()) {
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
  const auto path = LegacyConversationPath(legacy_conv_dir_, is_group, conv_id);
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
  std::uint8_t hdr[sizeof(kLegacyMagic) + 1];
  if (!ReadExact(in, hdr, sizeof(hdr))) {
    return true;
  }
  if (std::memcmp(hdr, kLegacyMagic, sizeof(kLegacyMagic)) != 0 ||
      hdr[sizeof(kLegacyMagic)] != kLegacyVersion) {
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
    if (!ReadLegacyRecord(in, conv_key, master_key_, plain, rec_err)) {
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
      if (!mi::server::proto::ReadString(plain, off, text) ||
          off != plain.size()) {
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
  if (!EnsureHistoryFile(is_group, conv_id, path, conv_key, error)) {
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
  if (!mi::server::proto::WriteString(sender, rec) ||
      !mi::server::proto::WriteBytes(envelope, rec)) {
    error = "history write failed";
    return false;
  }
  if (!WriteEncryptedRecord(out, master_key_, conv_key, is_group, conv_id, rec,
                            error)) {
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
  if (!EnsureHistoryFile(is_group, conv_id, path, conv_key, error)) {
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
  if (!mi::server::proto::WriteString(text_utf8, rec)) {
    error = "history write failed";
    return false;
  }
  if (!WriteEncryptedRecord(out, master_key_, conv_key, is_group, conv_id, rec,
                            error)) {
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
  if (!EnsureHistoryFile(is_group, conv_id, path, conv_key, error)) {
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
  if (!WriteEncryptedRecord(out, master_key_, conv_key, is_group, conv_id, rec,
                            error)) {
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

  const std::string conv_key_id = MakeConvKey(is_group, conv_id);
  auto it = conv_to_file_.find(conv_key_id);
  if (it == conv_to_file_.end() || it->second >= history_files_.size()) {
    return LoadLegacyConversation(is_group, conv_id, limit, out_messages, error);
  }

  std::array<std::uint8_t, 32> conv_key{};
  if (!DeriveConversationKey(is_group, conv_id, conv_key, error)) {
    return false;
  }
  const auto path = history_files_[it->second].path;
  if (path.empty()) {
    error = "history path failed";
    return false;
  }

  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    error = "history open failed";
    return false;
  }

  std::vector<std::uint8_t> stub(kPeStubSize);
  if (!ReadExact(in, stub.data(), stub.size())) {
    return true;
  }
  if (stub.size() < 2 || stub[0] != 'M' || stub[1] != 'Z') {
    error = "history magic mismatch";
    return false;
  }
  if (!ReadContainerHeader(in, error)) {
    return false;
  }

  if (limit > 0) {
    out_messages.reserve(limit);
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
    bool has_record = false;
    bool rec_group = false;
    std::string rec_conv;
    std::array<std::uint8_t, 24> inner_nonce{};
    std::vector<std::uint8_t> inner_cipher;
    std::array<std::uint8_t, 16> inner_mac{};
    std::string rec_err;
    if (!ReadOuterRecord(in, master_key_, has_record, rec_group, rec_conv,
                         inner_nonce, inner_cipher, inner_mac, rec_err)) {
      error = rec_err.empty() ? "history read failed" : rec_err;
      return false;
    }
    if (!has_record) {
      break;
    }
    if (rec_group != is_group || rec_conv != conv_id) {
      continue;
    }
    if (inner_cipher.empty()) {
      continue;
    }
    std::vector<std::uint8_t> plain(inner_cipher.size());
    const int ok = crypto_aead_unlock(plain.data(), inner_mac.data(),
                                      conv_key.data(), inner_nonce.data(),
                                      nullptr, 0, inner_cipher.data(),
                                      inner_cipher.size());
    if (ok != 0) {
      error = "history auth failed";
      return false;
    }
    std::vector<std::uint8_t> unpadded;
    std::string pad_err;
    if (!UnpadPlain(plain, unpadded, pad_err)) {
      error = pad_err.empty() ? "history read failed" : pad_err;
      return false;
    }
    if (unpadded.empty()) {
      continue;
    }
    std::size_t off = 0;
    const std::uint8_t type = unpadded[off++];
    if (type == kRecordMeta) {
      continue;
    }
    if (type == kRecordStatus) {
      if (off + 1 + 1 + 8 + 16 > unpadded.size()) {
        continue;
      }
      const bool rec_is_group = unpadded[off++] != 0;
      const std::uint8_t raw_st = unpadded[off++];
      if (rec_is_group != is_group) {
        continue;
      }
      ChatHistoryStatus st;
      if (!tryParseStatus(raw_st, st)) {
        continue;
      }
      std::uint64_t ts = 0;
      if (!mi::server::proto::ReadUint64(unpadded, off, ts) ||
          off + 16 != unpadded.size()) {
        continue;
      }
      std::array<std::uint8_t, 16> msg_id{};
      std::memcpy(msg_id.data(), unpadded.data() + off, msg_id.size());
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
    if (off + 1 + 1 + 1 + 1 + 8 > unpadded.size()) {
      continue;
    }
    const std::uint8_t kind = unpadded[off++];
    const bool rec_is_group = unpadded[off++] != 0;
    const bool outgoing = unpadded[off++] != 0;
    const std::uint8_t raw_st = unpadded[off++];
    if (rec_is_group != is_group) {
      continue;
    }
    ChatHistoryStatus st;
    if (!tryParseStatus(raw_st, st)) {
      continue;
    }
    std::uint64_t ts = 0;
    if (!mi::server::proto::ReadUint64(unpadded, off, ts)) {
      continue;
    }

    ChatHistoryMessage m;
    m.is_group = rec_is_group;
    m.outgoing = outgoing;
    m.status = st;
    m.timestamp_sec = ts;
    m.conv_id = conv_id;

    if (kind == kMessageKindEnvelope) {
      if (!mi::server::proto::ReadString(unpadded, off, m.sender) ||
          !mi::server::proto::ReadBytes(unpadded, off, m.envelope) ||
          off != unpadded.size()) {
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
          existing.is_group = rec_is_group;
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
      if (!mi::server::proto::ReadString(unpadded, off, text) ||
          off != unpadded.size()) {
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
  if (conv_to_file_.empty()) {
    return true;
  }

  struct ConvSnapshot {
    bool is_group{false};
    std::string conv_id;
    std::uint64_t last_ts{0};
    std::vector<ChatHistoryMessage> msgs;
  };

  std::vector<ConvSnapshot> convs;
  convs.reserve(conv_to_file_.size());
  for (const auto& kv : conv_to_file_) {
    const std::string& key = kv.first;
    if (key.size() < 3 || key[1] != ':') {
      continue;
    }
    const bool is_group = key[0] == 'g';
    const std::string conv_id = key.substr(2);
    if (conv_id.empty()) {
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
