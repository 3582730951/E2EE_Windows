#include "chat_history_store.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <utility>

#include "dpapi_util.h"
#include "miniz.h"
#include "monocypher.h"

#include "../server/include/crypto.h"
#include "../server/include/protocol.h"

namespace mi::client {

namespace {

constexpr std::uint8_t kContainerMagic[8] = {'M', 'I', 'H', 'D',
                                             'B', '0', '1', 0};
constexpr std::uint8_t kContainerVersionV1 = 1;
constexpr std::uint8_t kContainerVersionV2 = 2;
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
constexpr std::uint8_t kCompressMagic[4] = {'M', 'I', 'C', 'M'};
constexpr std::uint8_t kCompressVersion = 1;
constexpr std::uint8_t kCompressMethodDeflate = 1;
constexpr int kCompressLevel = 1;
constexpr std::size_t kCompressHeaderBytes =
    sizeof(kCompressMagic) + 1 + 1 + 2 + 4;
constexpr std::uint8_t kAesLayerMagic[8] = {'M', 'I', 'A', 'E',
                                            'S', '0', '1', 0};
constexpr std::uint8_t kAesLayerVersion = 1;
constexpr std::size_t kAesNonceBytes = 12;
constexpr std::size_t kAesTagBytes = 16;
constexpr std::size_t kAesLayerHeaderBytes =
    sizeof(kAesLayerMagic) + 1 + kAesNonceBytes + kAesTagBytes + 4;
constexpr std::uint8_t kWrapMagic[4] = {'M', 'I', 'H', '2'};
constexpr std::uint8_t kWrapVersion = 1;
constexpr std::size_t kWrapKeyBytes = 32;
constexpr std::size_t kWrapSlotCount = 3;
constexpr std::size_t kWrapSlotNonceBytes = 24;
constexpr std::size_t kWrapSlotCipherBytes = kWrapKeyBytes;
constexpr std::size_t kWrapSlotMacBytes = 16;
constexpr std::size_t kWrapHeaderBytes = 8;
constexpr std::size_t kWrapNonceBytes = 24;
constexpr std::size_t kWrapMacBytes = 16;

constexpr std::size_t kMaxRecordCipherLen = 2u * 1024u * 1024u;
constexpr std::size_t kMaxWrapRecordBytes = kMaxRecordCipherLen + 4096;
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

std::uint64_t NowUnixSeconds() {
  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::seconds>(
          now.time_since_epoch())
          .count());
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

bool EncodeCompressionLayer(const std::vector<std::uint8_t>& plain,
                            std::vector<std::uint8_t>& out,
                            std::string& error) {
  error.clear();
  out.clear();
  if (plain.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "history record too large";
    return false;
  }

  mz_ulong bound = mz_compressBound(static_cast<mz_ulong>(plain.size()));
  if (bound == 0 || bound > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "history compress failed";
    return false;
  }
  std::vector<std::uint8_t> comp;
  comp.resize(static_cast<std::size_t>(bound));
  mz_ulong comp_len = bound;
  const int rc = mz_compress2(comp.data(), &comp_len, plain.data(),
                              static_cast<mz_ulong>(plain.size()),
                              kCompressLevel);
  if (rc != MZ_OK) {
    error = "history compress failed";
    return false;
  }
  comp.resize(static_cast<std::size_t>(comp_len));

  out.reserve(kCompressHeaderBytes + 4 + comp.size());
  out.insert(out.end(), kCompressMagic,
             kCompressMagic + sizeof(kCompressMagic));
  out.push_back(kCompressVersion);
  out.push_back(kCompressMethodDeflate);
  out.push_back(0);
  out.push_back(0);
  if (!mi::server::proto::WriteUint32(
          static_cast<std::uint32_t>(plain.size()), out)) {
    error = "history record too large";
    return false;
  }
  if (!mi::server::proto::WriteBytes(comp, out)) {
    error = "history record too large";
    return false;
  }
  return true;
}

bool DecodeCompressionLayer(const std::vector<std::uint8_t>& input,
                            std::vector<std::uint8_t>& out_plain,
                            bool& out_used_compress,
                            std::string& error) {
  error.clear();
  out_plain.clear();
  out_used_compress = false;
  if (input.size() < kCompressHeaderBytes) {
    out_plain = input;
    return true;
  }
  if (std::memcmp(input.data(), kCompressMagic,
                  sizeof(kCompressMagic)) != 0) {
    out_plain = input;
    return true;
  }
  std::size_t off = sizeof(kCompressMagic);
  const std::uint8_t version = input[off++];
  const std::uint8_t method = input[off++];
  off += 2;
  if (version != kCompressVersion || method != kCompressMethodDeflate) {
    error = "history version mismatch";
    return false;
  }
  std::uint32_t plain_len = 0;
  if (!mi::server::proto::ReadUint32(input, off, plain_len)) {
    error = "history read failed";
    return false;
  }
  if (plain_len > kMaxRecordCipherLen) {
    error = "history record size invalid";
    return false;
  }
  std::vector<std::uint8_t> comp;
  if (!mi::server::proto::ReadBytes(input, off, comp) || off != input.size()) {
    error = "history read failed";
    return false;
  }
  std::vector<std::uint8_t> plain;
  plain.resize(plain_len);
  mz_ulong dest_len = static_cast<mz_ulong>(plain_len);
  const int rc = mz_uncompress(plain.data(), &dest_len, comp.data(),
                               static_cast<mz_ulong>(comp.size()));
  if (rc != MZ_OK || dest_len != plain_len) {
    error = "history compress failed";
    return false;
  }
  out_plain = std::move(plain);
  out_used_compress = true;
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

bool ParseConvKey(const std::string& key,
                  bool& out_is_group,
                  std::string& out_conv_id) {
  out_is_group = false;
  out_conv_id.clear();
  if (key.size() < 3 || key[1] != ':') {
    return false;
  }
  const char prefix = key[0];
  if (prefix != 'g' && prefix != 'p') {
    return false;
  }
  out_is_group = (prefix == 'g');
  out_conv_id = key.substr(2);
  return !out_conv_id.empty();
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

bool WriteContainerHeader(std::ofstream& out,
                          std::uint8_t version,
                          std::string& error) {
  error.clear();
  if (!out) {
    error = "history write failed";
    return false;
  }
  out.write(reinterpret_cast<const char*>(kContainerMagic),
            sizeof(kContainerMagic));
  out.put(static_cast<char>(version));
  const char zero[3] = {0, 0, 0};
  out.write(zero, sizeof(zero));
  if (!out.good()) {
    error = "history write failed";
    return false;
  }
  return true;
}

bool ReadContainerHeader(std::ifstream& in,
                         std::uint8_t& out_version,
                         std::string& error) {
  error.clear();
  out_version = 0;
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
  out_version = version;
  return true;
}

bool ParseOuterPlain(const std::vector<std::uint8_t>& outer_plain,
                     bool& out_is_group,
                     std::string& out_conv_id,
                     std::array<std::uint8_t, 24>& out_inner_nonce,
                     std::vector<std::uint8_t>& out_inner_cipher,
                     std::array<std::uint8_t, 16>& out_inner_mac,
                     std::string& error) {
  error.clear();
  out_is_group = false;
  out_conv_id.clear();
  out_inner_nonce.fill(0);
  out_inner_cipher.clear();
  out_inner_mac.fill(0);

  if (outer_plain.empty()) {
    error = "history record empty";
    return false;
  }
  std::size_t off = 0;
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
  return true;
}

bool DecryptOuterBlob(const std::array<std::uint8_t, 32>& master_key,
                      const std::vector<std::uint8_t>& blob,
                      bool& out_is_group,
                      std::string& out_conv_id,
                      std::array<std::uint8_t, 24>& out_inner_nonce,
                      std::vector<std::uint8_t>& out_inner_cipher,
                      std::array<std::uint8_t, 16>& out_inner_mac,
                      std::string& error) {
  error.clear();
  out_is_group = false;
  out_conv_id.clear();
  out_inner_nonce.fill(0);
  out_inner_cipher.clear();
  out_inner_mac.fill(0);

  if (IsAllZero(master_key.data(), master_key.size())) {
    error = "history key invalid";
    return false;
  }
  if (blob.size() < (kWrapNonceBytes + kWrapMacBytes)) {
    error = "history record size invalid";
    return false;
  }
  const std::size_t cipher_len =
      blob.size() - kWrapNonceBytes - kWrapMacBytes;
  if (cipher_len == 0 || cipher_len > kMaxRecordCipherLen) {
    error = "history record size invalid";
    return false;
  }
  std::array<std::uint8_t, 24> nonce{};
  std::memcpy(nonce.data(), blob.data(), nonce.size());
  std::vector<std::uint8_t> cipher(cipher_len);
  std::memcpy(cipher.data(), blob.data() + nonce.size(), cipher.size());
  std::array<std::uint8_t, 16> mac{};
  std::memcpy(mac.data(), blob.data() + nonce.size() + cipher.size(),
              mac.size());

  std::vector<std::uint8_t> outer_plain(cipher.size());
  const int ok = crypto_aead_unlock(outer_plain.data(), mac.data(),
                                    master_key.data(), nonce.data(), nullptr,
                                    0, cipher.data(), cipher.size());
  if (ok != 0) {
    error = "history auth failed";
    return false;
  }
  return ParseOuterPlain(outer_plain, out_is_group, out_conv_id,
                         out_inner_nonce, out_inner_cipher, out_inner_mac,
                         error);
}

std::array<std::uint8_t, 32> DeriveMaskFromLabel(const char* label) {
  mi::server::crypto::Sha256Digest d;
  const std::size_t len = label ? std::strlen(label) : 0u;
  mi::server::crypto::Sha256(reinterpret_cast<const std::uint8_t*>(label), len,
                             d);
  return d.bytes;
}

const std::array<std::uint8_t, 32>& WhiteboxMask1() {
  static const std::array<std::uint8_t, 32> mask =
      DeriveMaskFromLabel("MI_E2EE_WB_MASK1_V1");
  return mask;
}

const std::array<std::uint8_t, 32>& WhiteboxMask2() {
  static const std::array<std::uint8_t, 32> mask =
      DeriveMaskFromLabel("MI_E2EE_WB_MASK2_V1");
  return mask;
}

const std::array<std::uint8_t, 32>& WhiteboxMask3() {
  static const std::array<std::uint8_t, 32> mask =
      DeriveMaskFromLabel("MI_E2EE_WB_MASK3_V1");
  return mask;
}

void WhiteboxMixKey(std::array<std::uint8_t, 32>& key) {
  const auto& m1 = WhiteboxMask1();
  const auto& m2 = WhiteboxMask2();
  const auto& m3 = WhiteboxMask3();
  for (std::size_t i = 0; i < key.size(); ++i) {
    key[i] = static_cast<std::uint8_t>(key[i] ^ m1[i]);
  }
  for (std::size_t i = 0; i < key.size(); ++i) {
    key[i] = static_cast<std::uint8_t>(key[i] + m2[i]);
  }
  for (std::size_t i = 0; i < key.size(); ++i) {
    const std::uint8_t src = m3[i];
    const std::uint8_t shift = static_cast<std::uint8_t>(i & 7u);
    std::uint8_t rot = src;
    if (shift != 0) {
      rot = static_cast<std::uint8_t>(
          static_cast<std::uint8_t>(src << shift) |
          static_cast<std::uint8_t>(src >> (8u - shift)));
    }
    key[i] = static_cast<std::uint8_t>(key[i] ^ rot ^
                                       key[(i + 13u) % key.size()]);
  }
}

bool DeriveWhiteboxAesKey(const std::array<std::uint8_t, 32>& conv_key,
                          bool is_group,
                          const std::string& conv_id,
                          std::array<std::uint8_t, 32>& out_key,
                          std::string& error) {
  error.clear();
  out_key.fill(0);
  if (conv_id.empty()) {
    error = "conv id empty";
    return false;
  }
  if (IsAllZero(conv_key.data(), conv_key.size())) {
    error = "history key invalid";
    return false;
  }
  std::vector<std::uint8_t> info;
  static constexpr char kPrefix[] = "MI_E2EE_HISTORY_AESGCM_WB_V1";
  info.insert(info.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  info.push_back(0);
  info.push_back(is_group ? 1 : 0);
  info.push_back(0);
  info.insert(info.end(), conv_id.begin(), conv_id.end());

  std::array<std::uint8_t, 32> salt{};
  static constexpr char kSalt[] = "MI_E2EE_HISTORY_AESGCM_WB_SALT_V1";
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(reinterpret_cast<const std::uint8_t*>(kSalt),
                             sizeof(kSalt) - 1, d);
  salt = d.bytes;

  if (!mi::server::crypto::HkdfSha256(conv_key.data(), conv_key.size(),
                                      salt.data(), salt.size(), info.data(),
                                      info.size(), out_key.data(),
                                      out_key.size())) {
    error = "history hkdf failed";
    return false;
  }
  WhiteboxMixKey(out_key);
  return true;
}

constexpr std::uint8_t kAesSbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
    0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
    0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
    0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
    0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
    0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
    0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
    0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
    0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
    0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
    0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
    0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
    0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
    0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
    0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

constexpr std::uint8_t kAesRcon[15] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,
    0x80, 0x1B, 0x36, 0x6C, 0xD8, 0xAB, 0x4D,
};

struct Aes256KeySchedule {
  std::array<std::uint8_t, 240> bytes;
};

void RotWord(std::uint8_t w[4]) {
  const std::uint8_t tmp = w[0];
  w[0] = w[1];
  w[1] = w[2];
  w[2] = w[3];
  w[3] = tmp;
}

void SubWord(std::uint8_t w[4]) {
  w[0] = kAesSbox[w[0]];
  w[1] = kAesSbox[w[1]];
  w[2] = kAesSbox[w[2]];
  w[3] = kAesSbox[w[3]];
}

void Aes256KeyExpand(const std::array<std::uint8_t, 32>& key,
                     Aes256KeySchedule& ks) {
  ks.bytes.fill(0);
  std::memcpy(ks.bytes.data(), key.data(), key.size());
  std::size_t bytes_generated = key.size();
  std::size_t rcon_iter = 1;
  std::uint8_t temp[4];
  while (bytes_generated < ks.bytes.size()) {
    std::memcpy(temp, ks.bytes.data() + bytes_generated - 4, sizeof(temp));
    if (bytes_generated % 32 == 0) {
      RotWord(temp);
      SubWord(temp);
      temp[0] = static_cast<std::uint8_t>(temp[0] ^ kAesRcon[rcon_iter]);
      ++rcon_iter;
    } else if (bytes_generated % 32 == 16) {
      SubWord(temp);
    }
    for (std::size_t i = 0; i < 4; ++i) {
      ks.bytes[bytes_generated] =
          static_cast<std::uint8_t>(ks.bytes[bytes_generated - 32] ^ temp[i]);
      ++bytes_generated;
    }
  }
}

std::uint8_t Xtime(std::uint8_t v) {
  return static_cast<std::uint8_t>((v << 1) ^ ((v & 0x80u) ? 0x1Bu : 0));
}

using ByteBijection = std::array<std::uint8_t, 256>;
using RoundBijections = std::array<std::array<ByteBijection, 16>, 15>;

struct WhiteboxAesTables {
  std::array<std::array<std::array<std::array<std::uint32_t, 256>, 4>, 4>, 13>
      rounds{};
  std::array<std::array<std::array<std::uint32_t, 256>, 4>, 4> final{};
  RoundBijections enc_a{};
  RoundBijections dec_a{};
  RoundBijections enc_b{};
  RoundBijections dec_b{};
};

constexpr std::array<std::array<int, 4>, 4> kAesTboxInputIndex = {{
    {{0, 5, 10, 15}},
    {{4, 9, 14, 3}},
    {{8, 13, 2, 7}},
    {{12, 1, 6, 11}},
}};

void WordsToBytes(const std::array<std::uint32_t, 4>& words,
                  std::array<std::uint8_t, 16>& out) {
  for (std::size_t i = 0; i < words.size(); ++i) {
    const std::uint32_t w = words[i];
    out[i * 4 + 0] = static_cast<std::uint8_t>((w >> 24) & 0xFF);
    out[i * 4 + 1] = static_cast<std::uint8_t>((w >> 16) & 0xFF);
    out[i * 4 + 2] = static_cast<std::uint8_t>((w >> 8) & 0xFF);
    out[i * 4 + 3] = static_cast<std::uint8_t>(w & 0xFF);
  }
}

std::uint32_t LoadBe32(const std::uint8_t* ptr) {
  return (static_cast<std::uint32_t>(ptr[0]) << 24) |
         (static_cast<std::uint32_t>(ptr[1]) << 16) |
         (static_cast<std::uint32_t>(ptr[2]) << 8) |
         static_cast<std::uint32_t>(ptr[3]);
}

void LoadRoundKeys(const Aes256KeySchedule& ks,
                   std::array<std::uint32_t, 60>& out) {
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = LoadBe32(ks.bytes.data() + i * 4);
  }
}

void BuildBaseTables(
    std::array<std::array<std::uint32_t, 256>, 4>& te,
    std::array<std::array<std::uint32_t, 256>, 4>& fe) {
  for (std::size_t i = 0; i < 256; ++i) {
    const std::uint8_t s = kAesSbox[i];
    const std::uint8_t s2 = Xtime(s);
    const std::uint8_t s3 = static_cast<std::uint8_t>(s2 ^ s);
    te[0][i] =
        (static_cast<std::uint32_t>(s2) << 24) |
        (static_cast<std::uint32_t>(s) << 16) |
        (static_cast<std::uint32_t>(s) << 8) |
        static_cast<std::uint32_t>(s3);
    te[1][i] =
        (static_cast<std::uint32_t>(s3) << 24) |
        (static_cast<std::uint32_t>(s2) << 16) |
        (static_cast<std::uint32_t>(s) << 8) |
        static_cast<std::uint32_t>(s);
    te[2][i] =
        (static_cast<std::uint32_t>(s) << 24) |
        (static_cast<std::uint32_t>(s3) << 16) |
        (static_cast<std::uint32_t>(s2) << 8) |
        static_cast<std::uint32_t>(s);
    te[3][i] =
        (static_cast<std::uint32_t>(s) << 24) |
        (static_cast<std::uint32_t>(s) << 16) |
        (static_cast<std::uint32_t>(s3) << 8) |
        static_cast<std::uint32_t>(s2);
    fe[0][i] = static_cast<std::uint32_t>(s) << 24;
    fe[1][i] = static_cast<std::uint32_t>(s) << 16;
    fe[2][i] = static_cast<std::uint32_t>(s) << 8;
    fe[3][i] = static_cast<std::uint32_t>(s);
  }
}

std::uint8_t EncodeByte(const WhiteboxAesTables& tables,
                        std::size_t round,
                        std::size_t pos,
                        std::uint8_t value) {
  return tables.enc_b[round][pos][tables.enc_a[round][pos][value]];
}

std::uint8_t DecodeByte(const WhiteboxAesTables& tables,
                        std::size_t round,
                        std::size_t pos,
                        std::uint8_t value) {
  return tables.dec_a[round][pos][tables.dec_b[round][pos][value]];
}

std::uint32_t EncodeWord(const WhiteboxAesTables& tables,
                         std::size_t round,
                         std::uint32_t word,
                         std::size_t word_index) {
  const std::uint8_t b0 = static_cast<std::uint8_t>((word >> 24) & 0xFF);
  const std::uint8_t b1 = static_cast<std::uint8_t>((word >> 16) & 0xFF);
  const std::uint8_t b2 = static_cast<std::uint8_t>((word >> 8) & 0xFF);
  const std::uint8_t b3 = static_cast<std::uint8_t>(word & 0xFF);
  const std::size_t base = word_index * 4;
  const std::uint8_t e0 = EncodeByte(tables, round, base + 0, b0);
  const std::uint8_t e1 = EncodeByte(tables, round, base + 1, b1);
  const std::uint8_t e2 = EncodeByte(tables, round, base + 2, b2);
  const std::uint8_t e3 = EncodeByte(tables, round, base + 3, b3);
  return (static_cast<std::uint32_t>(e0) << 24) |
         (static_cast<std::uint32_t>(e1) << 16) |
         (static_cast<std::uint32_t>(e2) << 8) |
         static_cast<std::uint32_t>(e3);
}

std::array<std::uint8_t, 32> Sha256Bytes(const std::vector<std::uint8_t>& in) {
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(in.data(), in.size(), d);
  return d.bytes;
}

std::uint8_t Parity8(std::uint8_t v) {
  v ^= static_cast<std::uint8_t>(v >> 4);
  v ^= static_cast<std::uint8_t>(v >> 2);
  v ^= static_cast<std::uint8_t>(v >> 1);
  return static_cast<std::uint8_t>(v & 1u);
}

std::uint8_t ApplyMatrix(const std::array<std::uint8_t, 8>& mat,
                         std::uint8_t value) {
  std::uint8_t out = 0;
  for (std::size_t row = 0; row < mat.size(); ++row) {
    const std::uint8_t bits = static_cast<std::uint8_t>(mat[row] & value);
    const std::uint8_t bit = Parity8(bits);
    if (bit) {
      out = static_cast<std::uint8_t>(out | (1u << (7u - row)));
    }
  }
  return out;
}

bool InvertMatrix(const std::array<std::uint8_t, 8>& mat,
                  std::array<std::uint8_t, 8>& inv) {
  std::array<std::uint16_t, 8> rows{};
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const std::uint16_t left = static_cast<std::uint16_t>(mat[i]) << 8;
    const std::uint16_t right =
        static_cast<std::uint16_t>(1u << (7u - i));
    rows[i] = static_cast<std::uint16_t>(left | right);
  }

  for (std::size_t col = 0; col < 8; ++col) {
    const std::uint16_t mask =
        static_cast<std::uint16_t>(1u << (15u - col));
    std::size_t pivot = col;
    while (pivot < 8 && (rows[pivot] & mask) == 0) {
      ++pivot;
    }
    if (pivot == 8) {
      return false;
    }
    if (pivot != col) {
      std::swap(rows[pivot], rows[col]);
    }
    for (std::size_t r = 0; r < 8; ++r) {
      if (r != col && (rows[r] & mask)) {
        rows[r] = static_cast<std::uint16_t>(rows[r] ^ rows[col]);
      }
    }
  }

  for (std::size_t i = 0; i < 8; ++i) {
    inv[i] = static_cast<std::uint8_t>(rows[i] & 0xFFu);
  }
  return true;
}

bool BuildLinearBijection(const std::array<std::uint8_t, 32>& key,
                          std::uint32_t round,
                          std::uint32_t pos,
                          const char* label,
                          ByteBijection& enc,
                          ByteBijection& dec,
                          std::string& error) {
  error.clear();
  std::array<std::uint8_t, 8> mat{};
  std::array<std::uint8_t, 8> inv{};
  bool ok = false;
  for (std::uint32_t attempt = 0; attempt < 1024; ++attempt) {
    std::vector<std::uint8_t> seed;
    if (label && *label) {
      const std::size_t len = std::strlen(label);
      seed.insert(seed.end(), label, label + len);
    }
    seed.insert(seed.end(), key.begin(), key.end());
    seed.push_back(static_cast<std::uint8_t>(round & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((round >> 8) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((round >> 16) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((round >> 24) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>(pos & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((pos >> 8) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((pos >> 16) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((pos >> 24) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>(attempt & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((attempt >> 8) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((attempt >> 16) & 0xFF));
    seed.push_back(static_cast<std::uint8_t>((attempt >> 24) & 0xFF));
    const auto hash = Sha256Bytes(seed);
    for (std::size_t i = 0; i < mat.size(); ++i) {
      mat[i] = hash[i];
    }
    if (InvertMatrix(mat, inv)) {
      ok = true;
      break;
    }
  }
  if (!ok) {
    error = "history whitebox linear map failed";
    return false;
  }

  for (std::size_t v = 0; v < 256; ++v) {
    const auto byte = static_cast<std::uint8_t>(v);
    enc[v] = ApplyMatrix(mat, byte);
    dec[v] = ApplyMatrix(inv, byte);
  }
  return true;
}

bool BuildRoundBijections(const std::array<std::uint8_t, 32>& key,
                          const char* label,
                          RoundBijections& enc,
                          RoundBijections& dec,
                          std::string& error) {
  error.clear();
  for (std::uint32_t round = 0; round < enc.size(); ++round) {
    for (std::uint32_t pos = 0; pos < 16; ++pos) {
      if (!BuildLinearBijection(key, round, pos, label,
                                enc[round][pos], dec[round][pos], error)) {
        return false;
      }
    }
  }
  return true;
}

std::array<std::uint32_t, 4> DeriveRoundMask(
    const std::array<std::uint8_t, 32>& key,
    std::uint32_t round) {
  std::vector<std::uint8_t> buf;
  static constexpr char kLabel[] = "MI_E2EE_WB_AES_OUTMASK_V1";
  buf.insert(buf.end(), kLabel, kLabel + sizeof(kLabel) - 1);
  buf.insert(buf.end(), key.begin(), key.end());
  buf.push_back(static_cast<std::uint8_t>(round & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((round >> 8) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((round >> 16) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((round >> 24) & 0xFF));
  const auto hash = Sha256Bytes(buf);
  std::array<std::uint32_t, 4> out{};
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = LoadBe32(hash.data() + i * 4);
  }
  return out;
}

std::array<std::uint32_t, 3> DeriveShareMask(
    const std::array<std::uint8_t, 32>& key,
    std::uint32_t round,
    std::uint32_t word,
    bool final_round) {
  std::vector<std::uint8_t> buf;
  static constexpr char kLabel[] = "MI_E2EE_WB_AES_SHARE_V1";
  static constexpr char kFinalLabel[] = "MI_E2EE_WB_AES_FSHARE_V1";
  const char* label = final_round ? kFinalLabel : kLabel;
  const std::size_t label_len =
      final_round ? (sizeof(kFinalLabel) - 1) : (sizeof(kLabel) - 1);
  buf.insert(buf.end(), label, label + label_len);
  buf.insert(buf.end(), key.begin(), key.end());
  buf.push_back(static_cast<std::uint8_t>(round & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((round >> 8) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((round >> 16) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((round >> 24) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>(word & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((word >> 8) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((word >> 16) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((word >> 24) & 0xFF));
  const auto hash = Sha256Bytes(buf);
  std::array<std::uint32_t, 3> out{};
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = LoadBe32(hash.data() + i * 4);
  }
  return out;
}

bool BuildWhiteboxTables(const std::array<std::uint8_t, 32>& key,
                         WhiteboxAesTables& out,
                         std::string& error) {
  error.clear();
  if (IsAllZero(key.data(), key.size())) {
    error = "history key invalid";
    return false;
  }
  if (!BuildRoundBijections(key, "MI_E2EE_WB_LIN_A_V1",
                            out.enc_a, out.dec_a, error)) {
    return false;
  }
  if (!BuildRoundBijections(key, "MI_E2EE_WB_LIN_B_V1",
                            out.enc_b, out.dec_b, error)) {
    return false;
  }

  Aes256KeySchedule ks;
  Aes256KeyExpand(key, ks);
  std::array<std::uint32_t, 60> round_keys{};
  LoadRoundKeys(ks, round_keys);

  std::array<std::array<std::uint32_t, 256>, 4> te{};
  std::array<std::array<std::uint32_t, 256>, 4> fe{};
  BuildBaseTables(te, fe);

  std::array<std::uint8_t, 16> in_mask{};
  {
    std::array<std::uint32_t, 4> rk0{};
    for (std::size_t w = 0; w < 4; ++w) {
      rk0[w] = round_keys[w];
    }
    WordsToBytes(rk0, in_mask);
  }

  for (std::uint32_t round = 0; round < 13; ++round) {
    const auto out_mask_words = DeriveRoundMask(key, round);
    std::array<std::uint8_t, 16> next_mask{};
    WordsToBytes(out_mask_words, next_mask);

    for (std::uint32_t word = 0; word < 4; ++word) {
      const auto shares = DeriveShareMask(key, round, word, false);
      const std::uint32_t rk = round_keys[(round + 1) * 4 + word];
      const std::uint32_t const_word = rk ^ out_mask_words[word];
      const std::uint32_t enc_const =
          EncodeWord(out, round + 1, const_word, word);
      const std::uint32_t share0 = shares[0];
      const std::uint32_t share1 = shares[1];
      const std::uint32_t share2 = shares[2];
      const std::uint32_t share3 =
          enc_const ^ shares[0] ^ shares[1] ^ shares[2];
      const std::uint32_t share[4] = {share0, share1, share2, share3};

      for (std::size_t table = 0; table < 4; ++table) {
        const int idx = kAesTboxInputIndex[word][table];
        const std::uint8_t mask = in_mask[static_cast<std::size_t>(idx)];
        for (std::size_t b = 0; b < 256; ++b) {
          const std::uint8_t unmasked =
              static_cast<std::uint8_t>(
                  DecodeByte(out, round, static_cast<std::size_t>(idx),
                             static_cast<std::uint8_t>(b)) ^
                  mask);
          out.rounds[round][word][table][b] =
              EncodeWord(out, round + 1, te[table][unmasked], word) ^
              share[table];
        }
      }
    }
    in_mask = next_mask;
  }

  const std::uint32_t final_round = 13;
  for (std::uint32_t word = 0; word < 4; ++word) {
    const auto shares = DeriveShareMask(key, final_round, word, true);
    const std::uint32_t rk = round_keys[14 * 4 + word];
    const std::uint32_t enc_const =
        EncodeWord(out, 14, rk, word);
    const std::uint32_t share0 = shares[0];
    const std::uint32_t share1 = shares[1];
    const std::uint32_t share2 = shares[2];
    const std::uint32_t share3 =
        enc_const ^ shares[0] ^ shares[1] ^ shares[2];
    const std::uint32_t share[4] = {share0, share1, share2, share3};

    for (std::size_t table = 0; table < 4; ++table) {
      const int idx = kAesTboxInputIndex[word][table];
      const std::uint8_t mask = in_mask[static_cast<std::size_t>(idx)];
      for (std::size_t b = 0; b < 256; ++b) {
        const std::uint8_t unmasked =
            static_cast<std::uint8_t>(
                DecodeByte(out, final_round,
                           static_cast<std::size_t>(idx),
                           static_cast<std::uint8_t>(b)) ^
                mask);
        out.final[word][table][b] =
            EncodeWord(out, 14, fe[table][unmasked], word) ^
            share[table];
      }
    }
  }

  crypto_wipe(ks.bytes.data(), ks.bytes.size());
  crypto_wipe(round_keys.data(), round_keys.size() * sizeof(round_keys[0]));
  return true;
}

void WipeWhiteboxTables(WhiteboxAesTables& tables) {
  crypto_wipe(&tables, sizeof(tables));
}

void WhiteboxAesEncryptBlock(const WhiteboxAesTables& tables,
                             const std::uint8_t in[16],
                             std::uint8_t out[16]) {
  std::array<std::uint8_t, 16> encoded{};
  for (std::size_t i = 0; i < encoded.size(); ++i) {
    encoded[i] = EncodeByte(tables, 0, i, in[i]);
  }
  std::uint32_t s0 = LoadBe32(encoded.data() + 0);
  std::uint32_t s1 = LoadBe32(encoded.data() + 4);
  std::uint32_t s2 = LoadBe32(encoded.data() + 8);
  std::uint32_t s3 = LoadBe32(encoded.data() + 12);

  for (std::size_t round = 0; round < 13; ++round) {
    const auto& r = tables.rounds[round];
    const std::uint32_t t0 =
        r[0][0][s0 >> 24] ^
        r[0][1][(s1 >> 16) & 0xFF] ^
        r[0][2][(s2 >> 8) & 0xFF] ^
        r[0][3][s3 & 0xFF];
    const std::uint32_t t1 =
        r[1][0][s1 >> 24] ^
        r[1][1][(s2 >> 16) & 0xFF] ^
        r[1][2][(s3 >> 8) & 0xFF] ^
        r[1][3][s0 & 0xFF];
    const std::uint32_t t2 =
        r[2][0][s2 >> 24] ^
        r[2][1][(s3 >> 16) & 0xFF] ^
        r[2][2][(s0 >> 8) & 0xFF] ^
        r[2][3][s1 & 0xFF];
    const std::uint32_t t3 =
        r[3][0][s3 >> 24] ^
        r[3][1][(s0 >> 16) & 0xFF] ^
        r[3][2][(s1 >> 8) & 0xFF] ^
        r[3][3][s2 & 0xFF];
    s0 = t0;
    s1 = t1;
    s2 = t2;
    s3 = t3;
  }

  const auto& f = tables.final;
  const std::uint32_t t0 =
      f[0][0][s0 >> 24] ^
      f[0][1][(s1 >> 16) & 0xFF] ^
      f[0][2][(s2 >> 8) & 0xFF] ^
      f[0][3][s3 & 0xFF];
  const std::uint32_t t1 =
      f[1][0][s1 >> 24] ^
      f[1][1][(s2 >> 16) & 0xFF] ^
      f[1][2][(s3 >> 8) & 0xFF] ^
      f[1][3][s0 & 0xFF];
  const std::uint32_t t2 =
      f[2][0][s2 >> 24] ^
      f[2][1][(s3 >> 16) & 0xFF] ^
      f[2][2][(s0 >> 8) & 0xFF] ^
      f[2][3][s1 & 0xFF];
  const std::uint32_t t3 =
      f[3][0][s3 >> 24] ^
      f[3][1][(s0 >> 16) & 0xFF] ^
      f[3][2][(s1 >> 8) & 0xFF] ^
      f[3][3][s2 & 0xFF];

  const std::uint8_t raw[16] = {
      static_cast<std::uint8_t>(t0 >> 24),
      static_cast<std::uint8_t>(t0 >> 16),
      static_cast<std::uint8_t>(t0 >> 8),
      static_cast<std::uint8_t>(t0),
      static_cast<std::uint8_t>(t1 >> 24),
      static_cast<std::uint8_t>(t1 >> 16),
      static_cast<std::uint8_t>(t1 >> 8),
      static_cast<std::uint8_t>(t1),
      static_cast<std::uint8_t>(t2 >> 24),
      static_cast<std::uint8_t>(t2 >> 16),
      static_cast<std::uint8_t>(t2 >> 8),
      static_cast<std::uint8_t>(t2),
      static_cast<std::uint8_t>(t3 >> 24),
      static_cast<std::uint8_t>(t3 >> 16),
      static_cast<std::uint8_t>(t3 >> 8),
      static_cast<std::uint8_t>(t3),
  };
  for (std::size_t i = 0; i < 16; ++i) {
    out[i] = DecodeByte(tables, 14, i, raw[i]);
  }
}

void StoreUint64Be(std::uint8_t* out, std::uint64_t v) {
  out[0] = static_cast<std::uint8_t>(v >> 56);
  out[1] = static_cast<std::uint8_t>(v >> 48);
  out[2] = static_cast<std::uint8_t>(v >> 40);
  out[3] = static_cast<std::uint8_t>(v >> 32);
  out[4] = static_cast<std::uint8_t>(v >> 24);
  out[5] = static_cast<std::uint8_t>(v >> 16);
  out[6] = static_cast<std::uint8_t>(v >> 8);
  out[7] = static_cast<std::uint8_t>(v);
}

void GcmXorBlock(std::uint8_t out[16],
                 const std::uint8_t a[16],
                 const std::uint8_t b[16]) {
  for (std::size_t i = 0; i < 16; ++i) {
    out[i] = static_cast<std::uint8_t>(a[i] ^ b[i]);
  }
}

void GcmShiftRightOne(std::uint8_t v[16]) {
  const bool lsb = (v[15] & 1u) != 0;
  for (int i = 15; i > 0; --i) {
    v[i] = static_cast<std::uint8_t>((v[i] >> 1) | ((v[i - 1] & 1u) << 7));
  }
  v[0] = static_cast<std::uint8_t>(v[0] >> 1);
  if (lsb) {
    v[0] = static_cast<std::uint8_t>(v[0] ^ 0xE1u);
  }
}

void GcmMul(const std::uint8_t x[16],
            const std::uint8_t h[16],
            std::uint8_t out[16]) {
  std::uint8_t z[16]{};
  std::uint8_t v[16];
  std::memcpy(v, h, 16);
  for (int i = 0; i < 128; ++i) {
    const std::uint8_t bit =
        static_cast<std::uint8_t>((x[i / 8] >> (7 - (i % 8))) & 1u);
    if (bit) {
      for (std::size_t j = 0; j < 16; ++j) {
        z[j] = static_cast<std::uint8_t>(z[j] ^ v[j]);
      }
    }
    GcmShiftRightOne(v);
  }
  std::memcpy(out, z, 16);
}

void GcmGhash(const std::uint8_t h[16],
              const std::uint8_t* aad,
              std::size_t aad_len,
              const std::uint8_t* cipher,
              std::size_t cipher_len,
              std::uint8_t out[16]) {
  std::uint8_t y[16]{};
  std::uint8_t block[16]{};
  std::size_t offset = 0;
  while (offset < aad_len) {
    const std::size_t take =
        std::min<std::size_t>(16, aad_len - offset);
    std::memset(block, 0, sizeof(block));
    if (aad) {
      std::memcpy(block, aad + offset, take);
    }
    std::uint8_t tmp[16];
    GcmXorBlock(tmp, y, block);
    GcmMul(tmp, h, y);
    offset += take;
  }
  offset = 0;
  while (offset < cipher_len) {
    const std::size_t take =
        std::min<std::size_t>(16, cipher_len - offset);
    std::memset(block, 0, sizeof(block));
    if (cipher) {
      std::memcpy(block, cipher + offset, take);
    }
    std::uint8_t tmp[16];
    GcmXorBlock(tmp, y, block);
    GcmMul(tmp, h, y);
    offset += take;
  }
  std::uint8_t len_block[16]{};
  StoreUint64Be(len_block, static_cast<std::uint64_t>(aad_len) * 8u);
  StoreUint64Be(len_block + 8, static_cast<std::uint64_t>(cipher_len) * 8u);
  std::uint8_t tmp[16];
  GcmXorBlock(tmp, y, len_block);
  GcmMul(tmp, h, y);
  std::memcpy(out, y, 16);
}

void Increment32(std::uint8_t counter[16]) {
  for (int i = 15; i >= 12; --i) {
    counter[i] = static_cast<std::uint8_t>(counter[i] + 1u);
    if (counter[i] != 0) {
      break;
    }
  }
}

bool Aes256GcmEncrypt(const std::array<std::uint8_t, 32>& key,
                      const std::array<std::uint8_t, kAesNonceBytes>& nonce,
                      const std::vector<std::uint8_t>& plain,
                      std::vector<std::uint8_t>& out_cipher,
                      std::array<std::uint8_t, kAesTagBytes>& out_tag,
                      std::string& error) {
  error.clear();
  out_cipher.clear();
  out_tag.fill(0);
  if (IsAllZero(key.data(), key.size())) {
    error = "history key invalid";
    return false;
  }

  WhiteboxAesTables tables;
  if (!BuildWhiteboxTables(key, tables, error)) {
    return false;
  }
  auto wipe = [&]() { WipeWhiteboxTables(tables); };

  std::uint8_t h[16]{};
  std::uint8_t zero[16]{};
  WhiteboxAesEncryptBlock(tables, zero, h);

  std::uint8_t j0[16]{};
  std::memcpy(j0, nonce.data(), nonce.size());
  j0[15] = 0x01;

  out_cipher.resize(plain.size());
  std::uint8_t counter[16];
  std::memcpy(counter, j0, sizeof(counter));
  std::size_t offset = 0;
  while (offset < plain.size()) {
    Increment32(counter);
    std::uint8_t stream[16];
    WhiteboxAesEncryptBlock(tables, counter, stream);
    const std::size_t take =
        std::min<std::size_t>(16, plain.size() - offset);
    for (std::size_t i = 0; i < take; ++i) {
      out_cipher[offset + i] =
          static_cast<std::uint8_t>(plain[offset + i] ^ stream[i]);
    }
    offset += take;
  }

  std::uint8_t ghash[16]{};
  GcmGhash(h, nullptr, 0, out_cipher.data(), out_cipher.size(), ghash);

  std::uint8_t s[16];
  WhiteboxAesEncryptBlock(tables, j0, s);
  for (std::size_t i = 0; i < out_tag.size(); ++i) {
    out_tag[i] = static_cast<std::uint8_t>(s[i] ^ ghash[i]);
  }
  wipe();
  return true;
}

bool Aes256GcmDecrypt(const std::array<std::uint8_t, 32>& key,
                      const std::array<std::uint8_t, kAesNonceBytes>& nonce,
                      const std::vector<std::uint8_t>& cipher,
                      const std::array<std::uint8_t, kAesTagBytes>& tag,
                      std::vector<std::uint8_t>& out_plain,
                      std::string& error) {
  error.clear();
  out_plain.clear();
  if (IsAllZero(key.data(), key.size())) {
    error = "history key invalid";
    return false;
  }

  WhiteboxAesTables tables;
  if (!BuildWhiteboxTables(key, tables, error)) {
    return false;
  }
  auto wipe = [&]() { WipeWhiteboxTables(tables); };

  std::uint8_t h[16]{};
  std::uint8_t zero[16]{};
  WhiteboxAesEncryptBlock(tables, zero, h);

  std::uint8_t j0[16]{};
  std::memcpy(j0, nonce.data(), nonce.size());
  j0[15] = 0x01;

  std::uint8_t ghash[16]{};
  GcmGhash(h, nullptr, 0, cipher.data(), cipher.size(), ghash);

  std::uint8_t s[16];
  WhiteboxAesEncryptBlock(tables, j0, s);
  std::uint8_t expected[16];
  for (std::size_t i = 0; i < sizeof(expected); ++i) {
    expected[i] = static_cast<std::uint8_t>(s[i] ^ ghash[i]);
  }

  if (crypto_verify16(expected, tag.data()) != 0) {
    wipe();
    error = "history auth failed";
    return false;
  }

  out_plain.resize(cipher.size());
  std::uint8_t counter[16];
  std::memcpy(counter, j0, sizeof(counter));
  std::size_t offset = 0;
  while (offset < cipher.size()) {
    Increment32(counter);
    std::uint8_t stream[16];
    WhiteboxAesEncryptBlock(tables, counter, stream);
    const std::size_t take =
        std::min<std::size_t>(16, cipher.size() - offset);
    for (std::size_t i = 0; i < take; ++i) {
      out_plain[offset + i] =
          static_cast<std::uint8_t>(cipher[offset + i] ^ stream[i]);
    }
    offset += take;
  }
  wipe();
  return true;
}

bool EncodeAesLayer(const std::array<std::uint8_t, 32>& conv_key,
                    bool is_group,
                    const std::string& conv_id,
                    const std::vector<std::uint8_t>& plain,
                    std::vector<std::uint8_t>& out,
                    std::string& error) {
  error.clear();
  out.clear();
  std::array<std::uint8_t, 32> aes_key{};
  if (!DeriveWhiteboxAesKey(conv_key, is_group, conv_id, aes_key, error)) {
    return false;
  }
  std::array<std::uint8_t, kAesNonceBytes> nonce{};
  if (!mi::server::crypto::RandomBytes(nonce.data(), nonce.size())) {
    error = "rng failed";
    crypto_wipe(aes_key.data(), aes_key.size());
    return false;
  }
  std::vector<std::uint8_t> cipher;
  std::array<std::uint8_t, kAesTagBytes> tag{};
  if (!Aes256GcmEncrypt(aes_key, nonce, plain, cipher, tag, error)) {
    crypto_wipe(aes_key.data(), aes_key.size());
    return false;
  }
  if (cipher.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "history record too large";
    crypto_wipe(aes_key.data(), aes_key.size());
    return false;
  }

  out.reserve(kAesLayerHeaderBytes + cipher.size());
  out.insert(out.end(), kAesLayerMagic,
             kAesLayerMagic + sizeof(kAesLayerMagic));
  out.push_back(kAesLayerVersion);
  out.insert(out.end(), nonce.begin(), nonce.end());
  out.insert(out.end(), tag.begin(), tag.end());
  if (!mi::server::proto::WriteBytes(cipher, out)) {
    error = "history record too large";
    crypto_wipe(aes_key.data(), aes_key.size());
    return false;
  }
  crypto_wipe(aes_key.data(), aes_key.size());
  return true;
}

bool DecodeAesLayer(const std::array<std::uint8_t, 32>& conv_key,
                    bool is_group,
                    const std::string& conv_id,
                    const std::vector<std::uint8_t>& input,
                    std::vector<std::uint8_t>& out_plain,
                    bool& out_used_aes,
                    std::string& error) {
  error.clear();
  out_plain.clear();
  out_used_aes = false;
  if (input.size() < kAesLayerHeaderBytes) {
    out_plain = input;
    return true;
  }
  if (std::memcmp(input.data(), kAesLayerMagic,
                  sizeof(kAesLayerMagic)) != 0) {
    out_plain = input;
    return true;
  }
  std::size_t off = sizeof(kAesLayerMagic);
  const std::uint8_t version = input[off++];
  if (version != kAesLayerVersion) {
    error = "history version mismatch";
    return false;
  }
  if (off + kAesNonceBytes + kAesTagBytes > input.size()) {
    error = "history read failed";
    return false;
  }
  std::array<std::uint8_t, kAesNonceBytes> nonce{};
  std::memcpy(nonce.data(), input.data() + off, nonce.size());
  off += nonce.size();
  std::array<std::uint8_t, kAesTagBytes> tag{};
  std::memcpy(tag.data(), input.data() + off, tag.size());
  off += tag.size();
  std::vector<std::uint8_t> cipher;
  if (!mi::server::proto::ReadBytes(input, off, cipher) ||
      off != input.size()) {
    error = "history read failed";
    return false;
  }
  std::array<std::uint8_t, 32> aes_key{};
  if (!DeriveWhiteboxAesKey(conv_key, is_group, conv_id, aes_key, error)) {
    return false;
  }
  std::vector<std::uint8_t> plain;
  if (!Aes256GcmDecrypt(aes_key, nonce, cipher, tag, plain, error)) {
    crypto_wipe(aes_key.data(), aes_key.size());
    return false;
  }
  crypto_wipe(aes_key.data(), aes_key.size());
  out_plain = std::move(plain);
  out_used_aes = true;
  return true;
}

bool WriteMultiWrappedRecord(std::ofstream& out,
                             const std::array<std::uint8_t, 32>& master_key,
                             const std::vector<std::uint8_t>& payload,
                             std::string& error);

bool WriteEncryptedRecord(std::ofstream& out,
                          const std::array<std::uint8_t, 32>& master_key,
                          const std::array<std::uint8_t, 32>& conv_key,
                          bool is_group,
                          const std::string& conv_id,
                          const std::vector<std::uint8_t>& inner_plain,
                          std::uint8_t format_version,
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

  std::vector<std::uint8_t> compressed;
  if (!EncodeCompressionLayer(inner_plain, compressed, error)) {
    return false;
  }

  std::vector<std::uint8_t> padded;
  if (!PadPlain(compressed, padded, error)) {
    return false;
  }

  std::vector<std::uint8_t> aes_layer;
  if (!EncodeAesLayer(conv_key, is_group, conv_id, padded, aes_layer, error)) {
    return false;
  }

  std::array<std::uint8_t, 24> inner_nonce{};
  if (!mi::server::crypto::RandomBytes(inner_nonce.data(),
                                       inner_nonce.size())) {
    error = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> inner_cipher(aes_layer.size());
  std::array<std::uint8_t, 16> inner_mac{};
  crypto_aead_lock(inner_cipher.data(), inner_mac.data(), conv_key.data(),
                   inner_nonce.data(), nullptr, 0, aes_layer.data(),
                   aes_layer.size());

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

  std::vector<std::uint8_t> outer_blob;
  outer_blob.reserve(outer_nonce.size() + outer_cipher.size() + outer_mac.size());
  outer_blob.insert(outer_blob.end(), outer_nonce.begin(), outer_nonce.end());
  outer_blob.insert(outer_blob.end(), outer_cipher.begin(), outer_cipher.end());
  outer_blob.insert(outer_blob.end(), outer_mac.begin(), outer_mac.end());

  if (format_version >= kContainerVersionV2) {
    return WriteMultiWrappedRecord(out, master_key, outer_blob, error);
  }

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

  std::string parse_err;
  if (!ParseOuterPlain(outer_plain, out_is_group, out_conv_id,
                       out_inner_nonce, out_inner_cipher, out_inner_mac,
                       parse_err)) {
    error = parse_err.empty() ? "history read failed" : parse_err;
    return false;
  }
  out_has_record = true;
  return true;
}

bool DeriveWrapSlotKey(const std::array<std::uint8_t, 32>& master_key,
                       std::uint32_t slot,
                       std::array<std::uint8_t, 32>& out_key,
                       std::string& error) {
  error.clear();
  out_key.fill(0);
  if (IsAllZero(master_key.data(), master_key.size())) {
    error = "history key invalid";
    return false;
  }
  std::vector<std::uint8_t> info;
  static constexpr char kPrefix[] = "MI_E2EE_HISTORY_WRAP_SLOT_V1";
  info.insert(info.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  info.push_back(0);
  info.push_back(static_cast<std::uint8_t>(slot & 0xFF));
  info.push_back(static_cast<std::uint8_t>((slot >> 8) & 0xFF));
  info.push_back(static_cast<std::uint8_t>((slot >> 16) & 0xFF));
  info.push_back(static_cast<std::uint8_t>((slot >> 24) & 0xFF));

  std::array<std::uint8_t, 32> salt{};
  static constexpr char kSalt[] = "MI_E2EE_HISTORY_WRAP_SALT_V1";
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(reinterpret_cast<const std::uint8_t*>(kSalt),
                             sizeof(kSalt) - 1, d);
  salt = d.bytes;

  if (!mi::server::crypto::HkdfSha256(master_key.data(), master_key.size(),
                                      salt.data(), salt.size(), info.data(),
                                      info.size(), out_key.data(),
                                      out_key.size())) {
    error = "history hkdf failed";
    return false;
  }
  return true;
}

bool WriteMultiWrappedRecord(std::ofstream& out,
                             const std::array<std::uint8_t, 32>& master_key,
                             const std::vector<std::uint8_t>& payload,
                             std::string& error) {
  error.clear();
  if (!out) {
    error = "history write failed";
    return false;
  }
  if (IsAllZero(master_key.data(), master_key.size())) {
    error = "history key invalid";
    return false;
  }
  if (payload.empty()) {
    error = "history record empty";
    return false;
  }
  if (payload.size() > (kMaxRecordCipherLen + 64u)) {
    error = "history record too large";
    return false;
  }

  std::array<std::uint8_t, kWrapKeyBytes> wrap_key{};
  if (!mi::server::crypto::RandomBytes(wrap_key.data(), wrap_key.size())) {
    error = "rng failed";
    return false;
  }

  struct WrapSlot {
    std::array<std::uint8_t, kWrapSlotNonceBytes> nonce{};
    std::array<std::uint8_t, kWrapSlotCipherBytes> cipher{};
    std::array<std::uint8_t, kWrapSlotMacBytes> mac{};
  };

  std::array<WrapSlot, kWrapSlotCount> slots{};
  for (std::size_t i = 0; i < slots.size(); ++i) {
    std::array<std::uint8_t, 32> slot_key{};
    std::string slot_err;
    if (!DeriveWrapSlotKey(master_key, static_cast<std::uint32_t>(i),
                           slot_key, slot_err)) {
      error = slot_err.empty() ? "history hkdf failed" : slot_err;
      return false;
    }
    if (!mi::server::crypto::RandomBytes(slots[i].nonce.data(),
                                         slots[i].nonce.size())) {
      error = "rng failed";
      return false;
    }
    crypto_aead_lock(slots[i].cipher.data(), slots[i].mac.data(),
                     slot_key.data(), slots[i].nonce.data(), nullptr, 0,
                     wrap_key.data(), wrap_key.size());
    crypto_wipe(slot_key.data(), slot_key.size());
  }

  std::array<std::uint8_t, kWrapNonceBytes> wrap_nonce{};
  if (!mi::server::crypto::RandomBytes(wrap_nonce.data(),
                                       wrap_nonce.size())) {
    error = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> wrap_cipher(payload.size());
  std::array<std::uint8_t, kWrapMacBytes> wrap_mac{};
  crypto_aead_lock(wrap_cipher.data(), wrap_mac.data(), wrap_key.data(),
                   wrap_nonce.data(), nullptr, 0, payload.data(),
                   payload.size());

  std::vector<std::uint8_t> record;
  record.reserve(kWrapHeaderBytes +
                 slots.size() * (kWrapSlotNonceBytes + kWrapSlotCipherBytes +
                                 kWrapSlotMacBytes) +
                 kWrapNonceBytes + 4 + wrap_cipher.size() + kWrapMacBytes);
  record.insert(record.end(), kWrapMagic, kWrapMagic + sizeof(kWrapMagic));
  record.push_back(kWrapVersion);
  record.push_back(static_cast<std::uint8_t>(slots.size()));
  record.push_back(0);
  record.push_back(0);
  for (const auto& slot : slots) {
    record.insert(record.end(), slot.nonce.begin(), slot.nonce.end());
    record.insert(record.end(), slot.cipher.begin(), slot.cipher.end());
    record.insert(record.end(), slot.mac.begin(), slot.mac.end());
  }
  record.insert(record.end(), wrap_nonce.begin(), wrap_nonce.end());
  if (!mi::server::proto::WriteBytes(wrap_cipher, record)) {
    error = "history record too large";
    return false;
  }
  record.insert(record.end(), wrap_mac.begin(), wrap_mac.end());

  if (record.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "history record too large";
    return false;
  }
  const std::uint32_t record_len =
      static_cast<std::uint32_t>(record.size());
  if (!WriteUint32(out, record_len)) {
    error = "history write failed";
    return false;
  }
  out.write(reinterpret_cast<const char*>(record.data()),
            static_cast<std::streamsize>(record.size()));
  if (!out.good()) {
    error = "history write failed";
    return false;
  }
  return true;
}

bool ReadOuterRecordV2(std::ifstream& in,
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

  std::uint32_t record_len = 0;
  if (!ReadUint32(in, record_len)) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }
  if (record_len == 0 || record_len > kMaxWrapRecordBytes) {
    error = "history record size invalid";
    return false;
  }

  std::vector<std::uint8_t> record;
  record.resize(record_len);
  if (!ReadExact(in, record.data(), record.size())) {
    if (in.eof()) {
      return true;
    }
    error = "history read failed";
    return false;
  }

  std::size_t off = 0;
  if (record.size() < kWrapHeaderBytes ||
      std::memcmp(record.data(), kWrapMagic, sizeof(kWrapMagic)) != 0) {
    error = "history magic mismatch";
    return false;
  }
  off += sizeof(kWrapMagic);
  const std::uint8_t version = record[off++];
  const std::uint8_t slot_count = record[off++];
  off += 2;
  if (version != kWrapVersion || slot_count == 0 ||
      slot_count > static_cast<std::uint8_t>(kWrapSlotCount)) {
    error = "history version mismatch";
    return false;
  }
  const std::size_t slot_bytes =
      kWrapSlotNonceBytes + kWrapSlotCipherBytes + kWrapSlotMacBytes;
  const std::size_t slot_block = static_cast<std::size_t>(slot_count) * slot_bytes;
  if (off + slot_block + kWrapNonceBytes + 4 + kWrapMacBytes > record.size()) {
    error = "history record size invalid";
    return false;
  }

  struct SlotView {
    std::array<std::uint8_t, kWrapSlotNonceBytes> nonce{};
    std::array<std::uint8_t, kWrapSlotCipherBytes> cipher{};
    std::array<std::uint8_t, kWrapSlotMacBytes> mac{};
  };
  std::vector<SlotView> slots;
  slots.resize(slot_count);
  for (std::size_t i = 0; i < slots.size(); ++i) {
    std::memcpy(slots[i].nonce.data(), record.data() + off,
                slots[i].nonce.size());
    off += slots[i].nonce.size();
    std::memcpy(slots[i].cipher.data(), record.data() + off,
                slots[i].cipher.size());
    off += slots[i].cipher.size();
    std::memcpy(slots[i].mac.data(), record.data() + off,
                slots[i].mac.size());
    off += slots[i].mac.size();
  }

  std::array<std::uint8_t, kWrapNonceBytes> wrap_nonce{};
  std::memcpy(wrap_nonce.data(), record.data() + off, wrap_nonce.size());
  off += wrap_nonce.size();
  std::vector<std::uint8_t> wrap_cipher;
  if (!mi::server::proto::ReadBytes(record, off, wrap_cipher)) {
    error = "history read failed";
    return false;
  }
  if (wrap_cipher.size() > (kMaxRecordCipherLen + 64u)) {
    error = "history record size invalid";
    return false;
  }
  if (off + kWrapMacBytes > record.size()) {
    error = "history read failed";
    return false;
  }
  std::array<std::uint8_t, kWrapMacBytes> wrap_mac{};
  std::memcpy(wrap_mac.data(), record.data() + off, wrap_mac.size());
  off += wrap_mac.size();
  if (off != record.size()) {
    error = "history read failed";
    return false;
  }

  std::array<std::uint8_t, kWrapKeyBytes> wrap_key{};
  bool slot_ok = false;
  for (std::size_t i = 0; i < slots.size(); ++i) {
    std::array<std::uint8_t, 32> slot_key{};
    std::string slot_err;
    if (!DeriveWrapSlotKey(master_key, static_cast<std::uint32_t>(i),
                           slot_key, slot_err)) {
      error = slot_err.empty() ? "history hkdf failed" : slot_err;
      return false;
    }
    std::array<std::uint8_t, kWrapKeyBytes> candidate{};
    const int ok = crypto_aead_unlock(
        candidate.data(), slots[i].mac.data(), slot_key.data(),
        slots[i].nonce.data(), nullptr, 0, slots[i].cipher.data(),
        slots[i].cipher.size());
    crypto_wipe(slot_key.data(), slot_key.size());
    if (ok == 0) {
      wrap_key = candidate;
      slot_ok = true;
      break;
    }
  }
  if (!slot_ok) {
    error = "history auth failed";
    return false;
  }

  std::vector<std::uint8_t> outer_blob(wrap_cipher.size());
  const int ok = crypto_aead_unlock(outer_blob.data(), wrap_mac.data(),
                                    wrap_key.data(), wrap_nonce.data(), nullptr,
                                    0, wrap_cipher.data(), wrap_cipher.size());
  if (ok != 0) {
    error = "history auth failed";
    return false;
  }

  std::string parse_err;
  if (!DecryptOuterBlob(master_key, outer_blob, out_is_group, out_conv_id,
                        out_inner_nonce, out_inner_cipher, out_inner_mac,
                        parse_err)) {
    error = parse_err.empty() ? "history read failed" : parse_err;
    return false;
  }
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
  std::filesystem::path base_dir = e2ee_state_dir_.parent_path();
  if (base_dir.empty()) {
    base_dir = e2ee_state_dir_;
  }
  history_dir_ = base_dir / "database";

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
    std::uint8_t version = 0;
    std::string hdr_err;
    if (!ReadContainerHeader(in, version, hdr_err)) {
      continue;
    }
    if (version != kContainerVersionV1 && version != kContainerVersionV2) {
      continue;
    }
    file.version = version;

    while (true) {
      bool has_record = false;
      bool is_group = false;
      std::string conv_id;
      std::array<std::uint8_t, 24> inner_nonce{};
      std::vector<std::uint8_t> inner_cipher;
      std::array<std::uint8_t, 16> inner_mac{};
      std::string rec_err;
      const bool ok = (version >= kContainerVersionV2)
                          ? ReadOuterRecordV2(in, master_key_, has_record,
                                              is_group, conv_id, inner_nonce,
                                              inner_cipher, inner_mac, rec_err)
                          : ReadOuterRecord(in, master_key_, has_record,
                                            is_group, conv_id, inner_nonce,
                                            inner_cipher, inner_mac, rec_err);
      if (!ok) {
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
    std::uint8_t& out_version,
    std::string& error) {
  error.clear();
  out_path.clear();
  out_conv_key.fill(0);
  out_version = kContainerVersionV2;
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
  bool had_existing = false;
  bool loaded_existing = false;
  std::size_t old_index = history_files_.size();
  std::vector<ChatHistoryMessage> migrate_messages;
  auto it = conv_to_file_.find(conv_key);
  if (it != conv_to_file_.end() && it->second < history_files_.size()) {
    HistoryFileEntry& entry = history_files_[it->second];
    had_existing = true;
    old_index = it->second;
    if (entry.version >= kContainerVersionV2) {
      out_path = entry.path;
      out_version = entry.version;
      return true;
    }
    std::string load_err;
    loaded_existing =
        LoadConversation(is_group, conv_id, 0, migrate_messages, load_err);
  }

  std::size_t target = history_files_.size();
  for (std::size_t i = history_files_.size(); i > 0; --i) {
    if (history_files_[i - 1].version >= kContainerVersionV2 &&
        history_files_[i - 1].conv_keys.size() < kMaxConversationsPerFile) {
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
    if (!WriteContainerHeader(out, kContainerVersionV2, error)) {
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
    entry.version = kContainerVersionV2;
    history_files_.push_back(std::move(entry));
    target = history_files_.size() - 1;
  }

  if (had_existing && old_index < history_files_.size()) {
    history_files_[old_index].conv_keys.erase(conv_key);
  }

  history_files_[target].conv_keys.insert(conv_key);
  conv_to_file_[conv_key] = target;
  out_path = history_files_[target].path;
  out_version = history_files_[target].version;

  const auto append_messages =
      [&](const std::vector<ChatHistoryMessage>& messages) {
        if (messages.empty()) {
          return;
        }
        std::ofstream out(out_path, std::ios::binary | std::ios::app);
        if (!out) {
          return;
        }
        for (const auto& m : messages) {
          if (m.is_group != is_group) {
            continue;
          }
          std::vector<std::uint8_t> rec;
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
                                    conv_id, rec, out_version, write_err)) {
            break;
          }
        }
      };

  if (had_existing) {
    append_messages(migrate_messages);
  }

  if (!had_existing || (!loaded_existing && migrate_messages.empty())) {
    std::vector<ChatHistoryMessage> legacy;
    std::string legacy_err;
    if (LoadLegacyConversation(is_group, conv_id, 0, legacy, legacy_err) &&
        !legacy.empty()) {
      append_messages(legacy);
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
  std::uint8_t file_version = kContainerVersionV2;
  if (!EnsureHistoryFile(is_group, conv_id, path, conv_key, file_version,
                         error)) {
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
                            file_version, error)) {
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
  std::uint8_t file_version = kContainerVersionV2;
  if (!EnsureHistoryFile(is_group, conv_id, path, conv_key, file_version,
                         error)) {
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
                            file_version, error)) {
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
  std::uint8_t file_version = kContainerVersionV2;
  if (!EnsureHistoryFile(is_group, conv_id, path, conv_key, file_version,
                         error)) {
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
                            file_version, error)) {
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
  std::uint8_t version = 0;
  if (!ReadContainerHeader(in, version, error)) {
    return false;
  }
  if (version != kContainerVersionV1 && version != kContainerVersionV2) {
    error = "history version mismatch";
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
    const bool record_ok =
        (version >= kContainerVersionV2)
            ? ReadOuterRecordV2(in, master_key_, has_record, rec_group, rec_conv,
                                inner_nonce, inner_cipher, inner_mac, rec_err)
            : ReadOuterRecord(in, master_key_, has_record, rec_group, rec_conv,
                              inner_nonce, inner_cipher, inner_mac, rec_err);
    if (!record_ok) {
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
    std::vector<std::uint8_t> padded;
    bool used_aes = false;
    std::string aes_err;
    if (!DecodeAesLayer(conv_key, is_group, conv_id, plain, padded, used_aes,
                        aes_err)) {
      error = aes_err.empty() ? "history read failed" : aes_err;
      return false;
    }
    (void)used_aes;
    std::vector<std::uint8_t> unpadded;
    std::string pad_err;
    if (!UnpadPlain(padded, unpadded, pad_err)) {
      error = pad_err.empty() ? "history read failed" : pad_err;
      return false;
    }
    std::vector<std::uint8_t> record_plain;
    bool used_compress = false;
    std::string comp_err;
    if (!DecodeCompressionLayer(unpadded, record_plain, used_compress,
                                comp_err)) {
      error = comp_err.empty() ? "history read failed" : comp_err;
      return false;
    }
    (void)used_compress;
    if (record_plain.empty()) {
      continue;
    }
    std::size_t off = 0;
    const std::uint8_t type = record_plain[off++];
    if (type == kRecordMeta) {
      continue;
    }
    if (type == kRecordStatus) {
      if (off + 1 + 1 + 8 + 16 > record_plain.size()) {
        continue;
      }
      const bool rec_is_group = record_plain[off++] != 0;
      const std::uint8_t raw_st = record_plain[off++];
      if (rec_is_group != is_group) {
        continue;
      }
      ChatHistoryStatus st;
      if (!tryParseStatus(raw_st, st)) {
        continue;
      }
      std::uint64_t ts = 0;
      if (!mi::server::proto::ReadUint64(record_plain, off, ts) ||
          off + 16 != record_plain.size()) {
        continue;
      }
      std::array<std::uint8_t, 16> msg_id{};
      std::memcpy(msg_id.data(), record_plain.data() + off, msg_id.size());
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
    if (off + 1 + 1 + 1 + 1 + 8 > record_plain.size()) {
      continue;
    }
    const std::uint8_t kind = record_plain[off++];
    const bool rec_is_group = record_plain[off++] != 0;
    const bool outgoing = record_plain[off++] != 0;
    const std::uint8_t raw_st = record_plain[off++];
    if (rec_is_group != is_group) {
      continue;
    }
    ChatHistoryStatus st;
    if (!tryParseStatus(raw_st, st)) {
      continue;
    }
    std::uint64_t ts = 0;
    if (!mi::server::proto::ReadUint64(record_plain, off, ts)) {
      continue;
    }

    ChatHistoryMessage m;
    m.is_group = rec_is_group;
    m.outgoing = outgoing;
    m.status = st;
    m.timestamp_sec = ts;
    m.conv_id = conv_id;

    if (kind == kMessageKindEnvelope) {
      if (!mi::server::proto::ReadString(record_plain, off, m.sender) ||
          !mi::server::proto::ReadBytes(record_plain, off, m.envelope) ||
          off != record_plain.size()) {
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
      if (!mi::server::proto::ReadString(record_plain, off, text) ||
          off != record_plain.size()) {
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

bool ChatHistoryStore::Flush(std::string& error) {
  error.clear();
  if (!key_loaded_ || IsAllZero(master_key_.data(), master_key_.size())) {
    return true;
  }
  if (history_files_.empty()) {
    return true;
  }

  const std::uint64_t now_ts = NowUnixSeconds();
  for (const auto& entry : history_files_) {
    if (entry.path.empty() || entry.conv_keys.empty()) {
      continue;
    }
    const std::string& conv_key = *entry.conv_keys.begin();
    bool is_group = false;
    std::string conv_id;
    if (!ParseConvKey(conv_key, is_group, conv_id)) {
      continue;
    }

    std::array<std::uint8_t, 32> conv_key_bytes{};
    std::string key_err;
    if (!DeriveConversationKey(is_group, conv_id, conv_key_bytes, key_err)) {
      continue;
    }

    std::vector<std::uint8_t> rec;
    rec.reserve(1 + 8);
    rec.push_back(kRecordMeta);
    mi::server::proto::WriteUint64(now_ts, rec);

    std::ofstream out(entry.path, std::ios::binary | std::ios::app);
    if (!out) {
      error = "history write failed";
      return false;
    }
    std::string write_err;
    if (!WriteEncryptedRecord(out, master_key_, conv_key_bytes, is_group,
                              conv_id, rec, entry.version, write_err)) {
      error = write_err.empty() ? "history write failed" : write_err;
      return false;
    }
  }
  return true;
}

}  // namespace mi::client
