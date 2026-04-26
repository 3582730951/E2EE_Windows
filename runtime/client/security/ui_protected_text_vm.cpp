#include "protected_text_vm.h"

#include "secure_buffer.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <utility>

namespace mi::client {
namespace {

constexpr std::uint8_t kMagic[] = {'M', 'I', 'U', 'V'};
constexpr std::uint8_t kVersion = 1;
constexpr std::uint8_t kOpXor = 0x31;
constexpr std::uint8_t kOpRotXor = 0x32;
constexpr std::uint8_t kOpAdd = 0x33;
constexpr std::size_t kHeaderBytes = sizeof(kMagic) + 1 + 4 + 8;
constexpr std::uint64_t kSeedDomain = 0x9E3779B97F4A7C15ull;
constexpr std::uint64_t kHashDomain = 0x4D495F55495F564Dull;

std::uint64_t SplitMix64(std::uint64_t value) {
  value += 0x9E3779B97F4A7C15ull;
  value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
  value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
  return value ^ (value >> 31);
}

std::uint8_t Rotl8(std::uint8_t value, unsigned shift) {
  shift &= 7u;
  if (shift == 0) {
    return value;
  }
  return static_cast<std::uint8_t>((value << shift) | (value >> (8u - shift)));
}

std::uint8_t Rotr8(std::uint8_t value, unsigned shift) {
  shift &= 7u;
  if (shift == 0) {
    return value;
  }
  return static_cast<std::uint8_t>((value >> shift) | (value << (8u - shift)));
}

void WriteLe32(std::uint32_t value, std::vector<std::uint8_t>& out) {
  for (unsigned i = 0; i < 4; ++i) {
    out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFu));
  }
}

void WriteLe64(std::uint64_t value, std::vector<std::uint8_t>& out) {
  for (unsigned i = 0; i < 8; ++i) {
    out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFu));
  }
}

bool ReadLe32(const std::vector<std::uint8_t>& in,
              std::size_t& off,
              std::uint32_t& out) {
  if (off + 4 > in.size()) {
    return false;
  }
  out = 0;
  for (unsigned i = 0; i < 4; ++i) {
    out |= static_cast<std::uint32_t>(in[off++]) << (i * 8);
  }
  return true;
}

bool ReadLe64(const std::vector<std::uint8_t>& in,
              std::size_t& off,
              std::uint64_t& out) {
  if (off + 8 > in.size()) {
    return false;
  }
  out = 0;
  for (unsigned i = 0; i < 8; ++i) {
    out |= static_cast<std::uint64_t>(in[off++]) << (i * 8);
  }
  return true;
}

std::uint64_t PlaintextTag(std::string_view text, std::uint64_t seed) {
  std::uint64_t h = SplitMix64(seed ^ kHashDomain ^
                              static_cast<std::uint64_t>(text.size()));
  for (unsigned char ch : text) {
    h ^= static_cast<std::uint64_t>(ch) + 0x100ull;
    h = SplitMix64(h);
  }
  return h;
}

std::uint64_t RandomSeed() {
  std::random_device rd;
  std::uint64_t seed = (static_cast<std::uint64_t>(rd()) << 32) ^ rd();
  seed ^= static_cast<std::uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  return SplitMix64(seed);
}

std::uint64_t VmState(std::uint64_t seed, std::size_t len) {
  return SplitMix64(seed ^ kSeedDomain ^ static_cast<std::uint64_t>(len));
}

std::uint8_t SelectOp(std::uint64_t state) {
  switch (state % 3u) {
    case 0:
      return kOpXor;
    case 1:
      return kOpRotXor;
    default:
      return kOpAdd;
  }
}

std::uint8_t EncryptByte(std::uint8_t plain,
                         std::uint8_t op,
                         std::uint8_t key,
                         unsigned rot) {
  if (op == kOpXor) {
    return static_cast<std::uint8_t>(plain ^ key);
  }
  if (op == kOpRotXor) {
    return Rotl8(static_cast<std::uint8_t>(plain ^ key), rot);
  }
  return static_cast<std::uint8_t>(plain + key);
}

bool DecodeByte(std::uint8_t cipher,
                std::uint8_t op,
                std::uint8_t key,
                unsigned rot,
                char& out) {
  if (op == kOpXor) {
    out = static_cast<char>(cipher ^ key);
    return true;
  }
  if (op == kOpRotXor) {
    out = static_cast<char>(Rotr8(cipher, rot) ^ key);
    return true;
  }
  if (op == kOpAdd) {
    out = static_cast<char>(cipher - key);
    return true;
  }
  return false;
}

}  // namespace

UiProtectedTextLease::UiProtectedTextLease(std::vector<char> plain,
                                           std::chrono::milliseconds ttl)
    : plain_(std::move(plain)),
      expires_at_(std::chrono::steady_clock::now() + ttl),
      valid_(true),
      wiped_(plain_.empty()) {}

UiProtectedTextLease::UiProtectedTextLease(UiProtectedTextLease&& other) noexcept
    : plain_(std::move(other.plain_)),
      expires_at_(other.expires_at_),
      valid_(other.valid_),
      wiped_(other.wiped_) {
  other.valid_ = false;
  other.wiped_ = true;
}

UiProtectedTextLease& UiProtectedTextLease::operator=(
    UiProtectedTextLease&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  WipeNow();
  plain_ = std::move(other.plain_);
  expires_at_ = other.expires_at_;
  valid_ = other.valid_;
  wiped_ = other.wiped_;
  other.valid_ = false;
  other.wiped_ = true;
  return *this;
}

UiProtectedTextLease::~UiProtectedTextLease() {
  WipeNow();
}

bool UiProtectedTextLease::Expired() const {
  return valid_ && std::chrono::steady_clock::now() >= expires_at_;
}

std::string_view UiProtectedTextLease::View() {
  if (!valid_ || wiped_) {
    return {};
  }
  if (Expired()) {
    WipeNow();
    return {};
  }
  return std::string_view(plain_.data(), plain_.size());
}

void UiProtectedTextLease::WipeNow() {
  if (!plain_.empty()) {
    mi::common::SecureWipe(plain_.data(), plain_.size());
  }
  wiped_ = true;
  valid_ = false;
}

UiProtectedText UiProtectedText::Protect(std::string_view plaintext) {
  return ProtectForTest(plaintext, RandomSeed());
}

UiProtectedText UiProtectedText::ProtectForTest(std::string_view plaintext,
                                                std::uint64_t seed) {
  UiProtectedText out;
  out.seed_ = seed;
  out.bytecode_.reserve(kHeaderBytes + plaintext.size() * 2);
  out.bytecode_.insert(out.bytecode_.end(), std::begin(kMagic),
                       std::end(kMagic));
  out.bytecode_.push_back(kVersion);
  WriteLe32(static_cast<std::uint32_t>(plaintext.size()), out.bytecode_);
  WriteLe64(PlaintextTag(plaintext, seed), out.bytecode_);

  std::uint64_t state = VmState(seed, plaintext.size());
  for (std::size_t i = 0; i < plaintext.size(); ++i) {
    state = SplitMix64(state + i + 1);
    const std::uint8_t key = static_cast<std::uint8_t>(state & 0xFFu);
    const unsigned rot = static_cast<unsigned>((state >> 8) & 7u);
    const std::uint8_t op = SelectOp(state);
    out.bytecode_.push_back(op);
    out.bytecode_.push_back(EncryptByte(
        static_cast<std::uint8_t>(plaintext[i]), op, key, rot));
  }
  return out;
}

bool UiProtectedText::ImportForTest(const std::vector<std::uint8_t>& bytecode,
                                    std::uint64_t seed,
                                    UiProtectedText& out) {
  UiProtectedText candidate;
  candidate.bytecode_ = bytecode;
  candidate.seed_ = seed;
  std::vector<char> decoded;
  if (!candidate.Decode(decoded)) {
    mi::common::SecureWipe(decoded.data(), decoded.size());
    return false;
  }
  mi::common::SecureWipe(decoded.data(), decoded.size());
  out = std::move(candidate);
  return true;
}

UiProtectedTextLease UiProtectedText::Open(std::chrono::milliseconds ttl) const {
  std::vector<char> plain;
  if (ttl.count() <= 0 || !Decode(plain)) {
    mi::common::SecureWipe(plain.data(), plain.size());
    return {};
  }
  return UiProtectedTextLease(std::move(plain), ttl);
}

bool UiProtectedText::Decode(std::vector<char>& out) const {
  out.clear();
  if (bytecode_.size() < kHeaderBytes) {
    return false;
  }
  std::size_t off = 0;
  if (std::memcmp(bytecode_.data(), kMagic, sizeof(kMagic)) != 0) {
    return false;
  }
  off += sizeof(kMagic);
  if (bytecode_[off++] != kVersion) {
    return false;
  }
  std::uint32_t len = 0;
  std::uint64_t expected_tag = 0;
  if (!ReadLe32(bytecode_, off, len) || !ReadLe64(bytecode_, off, expected_tag)) {
    return false;
  }
  if (bytecode_.size() != kHeaderBytes + static_cast<std::size_t>(len) * 2) {
    return false;
  }

  out.assign(len, '\0');
  std::uint64_t state = VmState(seed_, len);
  for (std::size_t i = 0; i < len; ++i) {
    state = SplitMix64(state + i + 1);
    const std::uint8_t op = bytecode_[off++];
    const std::uint8_t cipher = bytecode_[off++];
    const std::uint8_t key = static_cast<std::uint8_t>(state & 0xFFu);
    const unsigned rot = static_cast<unsigned>((state >> 8) & 7u);
    if (!DecodeByte(cipher, op, key, rot, out[i])) {
      mi::common::SecureWipe(out.data(), out.size());
      out.clear();
      return false;
    }
  }

  const std::string_view view(out.data(), out.size());
  if (PlaintextTag(view, seed_) != expected_tag) {
    mi::common::SecureWipe(out.data(), out.size());
    out.clear();
    return false;
  }
  return true;
}

}  // namespace mi::client
