#include "key_transparency.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string_view>
#include <utility>

#include "crypto.h"

namespace mi::server {

namespace {

constexpr std::uint8_t kLeafPrefix = 0x00;
constexpr std::uint8_t kNodePrefix = 0x01;

bool IsPowerOfTwo(std::size_t n) {
  return n != 0 && ((n & (n - 1)) == 0);
}

std::size_t Log2PowerOfTwo(std::size_t n) {
  std::size_t out = 0;
  while (n > 1) {
    n >>= 1;
    out++;
  }
  return out;
}

std::size_t LargestPowerOfTwoLessThan(std::size_t n) {
  if (n <= 1) {
    return 0;
  }
  std::size_t k = 1;
  while ((k << 1) < n) {
    k <<= 1;
  }
  return k;
}

Sha256Hash HashSha256(const std::uint8_t* data, std::size_t len) {
  crypto::Sha256Digest d;
  crypto::Sha256(data, len, d);
  return d.bytes;
}

Sha256Hash HashLeaf(const std::vector<std::uint8_t>& leaf_data) {
  std::vector<std::uint8_t> buf;
  buf.reserve(1 + leaf_data.size());
  buf.push_back(kLeafPrefix);
  buf.insert(buf.end(), leaf_data.begin(), leaf_data.end());
  return HashSha256(buf.data(), buf.size());
}

Sha256Hash HashNode(const Sha256Hash& left, const Sha256Hash& right) {
  std::uint8_t buf[1 + 32 + 32];
  buf[0] = kNodePrefix;
  std::memcpy(buf + 1, left.data(), left.size());
  std::memcpy(buf + 1 + 32, right.data(), right.size());
  return HashSha256(buf, sizeof(buf));
}

Sha256Hash MerkleTreeHash(const std::vector<Sha256Hash>& leaves,
                          const std::vector<std::vector<Sha256Hash>>& pow2_levels,
                          std::size_t start, std::size_t n) {
  if (n == 0) {
    static constexpr std::uint8_t kEmpty[1] = {0};
    return HashSha256(kEmpty, 0);
  }
  if (n == 1) {
    return leaves[start];
  }
  if (IsPowerOfTwo(n)) {
    const std::size_t level = Log2PowerOfTwo(n);
    if (level == 0) {
      return leaves[start];
    }
    const std::size_t idx = start >> level;
    const std::size_t vec_idx = level - 1;
    if (vec_idx < pow2_levels.size() && idx < pow2_levels[vec_idx].size()) {
      return pow2_levels[vec_idx][idx];
    }
  }
  const std::size_t k = LargestPowerOfTwoLessThan(n);
  const auto left = MerkleTreeHash(leaves, pow2_levels, start, k);
  const auto right = MerkleTreeHash(leaves, pow2_levels, start + k, n - k);
  return HashNode(left, right);
}

std::vector<Sha256Hash> MerkleAuditPath(std::size_t m,
                                        const std::vector<Sha256Hash>& leaves,
                                        const std::vector<std::vector<Sha256Hash>>& pow2_levels,
                                        std::size_t start, std::size_t n) {
  if (n <= 1) {
    return {};
  }
  const std::size_t k = LargestPowerOfTwoLessThan(n);
  if (m < k) {
    auto path = MerkleAuditPath(m, leaves, pow2_levels, start, k);
    path.push_back(MerkleTreeHash(leaves, pow2_levels, start + k, n - k));
    return path;
  }
  auto path = MerkleAuditPath(m - k, leaves, pow2_levels, start + k, n - k);
  path.push_back(MerkleTreeHash(leaves, pow2_levels, start, k));
  return path;
}

std::vector<Sha256Hash> MerkleSubProof(std::size_t m,
                                       const std::vector<Sha256Hash>& leaves,
                                       const std::vector<std::vector<Sha256Hash>>& pow2_levels,
                                       std::size_t start, std::size_t n,
                                       bool b) {
  if (m == n) {
    if (b) {
      return {};
    }
    return {MerkleTreeHash(leaves, pow2_levels, start, n)};
  }
  const std::size_t k = LargestPowerOfTwoLessThan(n);
  if (m <= k) {
    auto proof = MerkleSubProof(m, leaves, pow2_levels, start, k, b);
    proof.push_back(MerkleTreeHash(leaves, pow2_levels, start + k, n - k));
    return proof;
  }
  auto proof =
      MerkleSubProof(m - k, leaves, pow2_levels, start + k, n - k, false);
  proof.push_back(MerkleTreeHash(leaves, pow2_levels, start, k));
  return proof;
}

std::vector<Sha256Hash> MerkleConsistencyProof(
    std::size_t m, const std::vector<Sha256Hash>& leaves, std::size_t start,
    std::size_t n, const std::vector<std::vector<Sha256Hash>>& pow2_levels) {
  if (m == 0 || m == n) {
    return {};
  }
  return MerkleSubProof(m, leaves, pow2_levels, start, n, true);
}

std::vector<std::uint8_t> BuildLeafData(
    const std::string& username,
    const std::array<std::uint8_t, kKtIdentitySigPublicKeyBytes>& id_sig_pk,
    const std::array<std::uint8_t, kKtIdentityDhPublicKeyBytes>& id_dh_pk) {
  std::vector<std::uint8_t> out;
  constexpr char kPrefix[] = "mi_e2ee_kt_leaf_v1";
  out.reserve(sizeof(kPrefix) - 1 + 1 + username.size() + 1 + id_sig_pk.size() +
              id_dh_pk.size());
  out.insert(out.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  out.push_back(0);
  out.insert(out.end(), username.begin(), username.end());
  out.push_back(0);
  out.insert(out.end(), id_sig_pk.begin(), id_sig_pk.end());
  out.insert(out.end(), id_dh_pk.begin(), id_dh_pk.end());
  return out;
}

bool ReadExact(std::ifstream& in, void* dst, std::size_t len) {
  if (len == 0) {
    return true;
  }
  in.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(len));
  return in.good();
}

void WriteUint16(std::ofstream& out, std::uint16_t v) {
  const std::uint8_t b[2] = {
      static_cast<std::uint8_t>(v & 0xFF),
      static_cast<std::uint8_t>((v >> 8) & 0xFF),
  };
  out.write(reinterpret_cast<const char*>(b), sizeof(b));
}

bool ReadUint16(std::ifstream& in, std::uint16_t& v) {
  std::uint8_t b[2];
  if (!ReadExact(in, b, sizeof(b))) {
    return false;
  }
  v = static_cast<std::uint16_t>(b[0] | (static_cast<std::uint16_t>(b[1]) << 8));
  return true;
}

}  // namespace

KeyTransparencyLog::KeyTransparencyLog(std::filesystem::path log_path)
    : log_path_(std::move(log_path)) {}

bool KeyTransparencyLog::Load(std::string& error) {
  error.clear();
  std::lock_guard<std::mutex> lock(mutex_);
  leaves_.clear();
  pow2_levels_.clear();
  latest_by_user_.clear();
  root_ = Sha256Hash{};

  if (log_path_.empty()) {
    error = "kt log path empty";
    return false;
  }

  std::error_code ec;
  if (!std::filesystem::exists(log_path_, ec)) {
    RecomputeRootLocked();
    return true;
  }

  std::ifstream in(log_path_, std::ios::binary);
  if (!in.is_open()) {
    error = "open kt log failed";
    return false;
  }

  char magic[8];
  if (!ReadExact(in, magic, sizeof(magic))) {
    RecomputeRootLocked();
    return true;
  }
  if (std::string_view(magic, sizeof(magic)) != "MIKTLOG1") {
    error = "kt log magic mismatch";
    return false;
  }

  while (true) {
    std::uint16_t user_len = 0;
    if (!ReadUint16(in, user_len)) {
      break;
    }
    if (user_len == 0 || user_len > 4096) {
      error = "kt log username length invalid";
      return false;
    }
    std::string username;
    username.resize(user_len);
    if (!ReadExact(in, username.data(), username.size())) {
      break;
    }

    std::array<std::uint8_t, kKtIdentitySigPublicKeyBytes> id_sig_pk{};
    std::array<std::uint8_t, kKtIdentityDhPublicKeyBytes> id_dh_pk{};
    if (!ReadExact(in, id_sig_pk.data(), id_sig_pk.size()) ||
        !ReadExact(in, id_dh_pk.data(), id_dh_pk.size())) {
      break;
    }

    const auto leaf_data = BuildLeafData(username, id_sig_pk, id_dh_pk);
    const auto leaf_hash = HashLeaf(leaf_data);
    const std::uint64_t idx = static_cast<std::uint64_t>(leaves_.size());
    leaves_.push_back(leaf_hash);
    latest_by_user_[username] = LatestKey{idx, leaf_hash};
  }

  RebuildPow2LevelsLocked();
  RecomputeRootLocked();
  return true;
}

bool KeyTransparencyLog::UpdateIdentityKeys(
    const std::string& username,
    const std::array<std::uint8_t, kKtIdentitySigPublicKeyBytes>& id_sig_pk,
    const std::array<std::uint8_t, kKtIdentityDhPublicKeyBytes>& id_dh_pk,
    std::string& error) {
  error.clear();
  if (username.empty()) {
    error = "username empty";
    return false;
  }
  if (log_path_.empty()) {
    error = "kt log path empty";
    return false;
  }

  const auto leaf_data = BuildLeafData(username, id_sig_pk, id_dh_pk);
  const auto leaf_hash = HashLeaf(leaf_data);

  std::lock_guard<std::mutex> lock(mutex_);
  auto it = latest_by_user_.find(username);
  if (it != latest_by_user_.end() && it->second.leaf_hash == leaf_hash) {
    return true;
  }
  if (!AppendEntryLocked(username, id_sig_pk, id_dh_pk, leaf_hash, error)) {
    return false;
  }
  latest_by_user_[username] = LatestKey{
      static_cast<std::uint64_t>(leaves_.size() - 1), leaf_hash};
  RecomputeRootLocked();
  return true;
}

void KeyTransparencyLog::RebuildPow2LevelsLocked() {
  pow2_levels_.clear();
  const std::vector<Sha256Hash>* prev = &leaves_;
  while (prev->size() >= 2) {
    std::vector<Sha256Hash> level;
    level.reserve(prev->size() / 2);
    for (std::size_t i = 0; i + 1 < prev->size(); i += 2) {
      level.push_back(HashNode((*prev)[i], (*prev)[i + 1]));
    }
    pow2_levels_.push_back(std::move(level));
    prev = &pow2_levels_.back();
  }
}

void KeyTransparencyLog::AppendLeafHashLocked(const Sha256Hash& leaf_hash) {
  leaves_.push_back(leaf_hash);
  const std::size_t n = leaves_.size();
  if (n < 2) {
    return;
  }

  std::size_t block = 2;
  std::size_t level_idx = 0;
  while (block != 0 && (n % block) == 0) {
    if (pow2_levels_.size() <= level_idx) {
      pow2_levels_.resize(level_idx + 1);
    }

    const auto& prev =
        level_idx == 0 ? leaves_ : pow2_levels_[level_idx - 1];
    if (prev.size() < 2) {
      break;
    }
    const auto node = HashNode(prev[prev.size() - 2], prev[prev.size() - 1]);
    pow2_levels_[level_idx].push_back(node);

    level_idx++;
    if (block > (std::numeric_limits<std::size_t>::max)() / 2u) {
      break;
    }
    block *= 2;
  }
}

KeyTransparencySth KeyTransparencyLog::Head() const {
  std::lock_guard<std::mutex> lock(mutex_);
  KeyTransparencySth sth;
  sth.tree_size = static_cast<std::uint64_t>(leaves_.size());
  sth.root = root_;
  return sth;
}

bool KeyTransparencyLog::BuildProofForLatestKey(
    const std::string& username, std::uint64_t client_tree_size,
    KeyTransparencyProof& out_proof, std::string& error) const {
  error.clear();
  out_proof = KeyTransparencyProof{};
  if (username.empty()) {
    error = "username empty";
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = latest_by_user_.find(username);
  if (it == latest_by_user_.end()) {
    error = "kt entry not found";
    return false;
  }
  const std::size_t n = leaves_.size();
  if (n == 0) {
    error = "kt empty";
    return false;
  }

  out_proof.sth.tree_size = static_cast<std::uint64_t>(n);
  out_proof.sth.root = root_;
  out_proof.leaf_index = it->second.leaf_index;
  out_proof.audit_path =
      MerkleAuditPath(static_cast<std::size_t>(it->second.leaf_index), leaves_,
                      pow2_levels_, 0, n);
  if (client_tree_size > 0 &&
      client_tree_size < static_cast<std::uint64_t>(n)) {
    out_proof.consistency_path =
        MerkleConsistencyProof(static_cast<std::size_t>(client_tree_size),
                               leaves_, 0, n, pow2_levels_);
  }
  return true;
}

bool KeyTransparencyLog::BuildConsistencyProof(
    std::uint64_t old_size, std::uint64_t new_size,
    std::vector<Sha256Hash>& out_proof, std::string& error) const {
  error.clear();
  out_proof.clear();
  std::lock_guard<std::mutex> lock(mutex_);
  const std::uint64_t current = static_cast<std::uint64_t>(leaves_.size());
  if (old_size == 0 || new_size == 0 || old_size > new_size) {
    error = "invalid sizes";
    return false;
  }
  if (new_size > current) {
    error = "new size beyond head";
    return false;
  }
  if (old_size == new_size) {
    return true;
  }
  out_proof =
      MerkleConsistencyProof(static_cast<std::size_t>(old_size), leaves_, 0,
                             static_cast<std::size_t>(new_size), pow2_levels_);
  return true;
}

bool KeyTransparencyLog::AppendEntryLocked(
    const std::string& username,
    const std::array<std::uint8_t, kKtIdentitySigPublicKeyBytes>& id_sig_pk,
    const std::array<std::uint8_t, kKtIdentityDhPublicKeyBytes>& id_dh_pk,
    const Sha256Hash& leaf_hash, std::string& error) {
  error.clear();
  std::error_code ec;
  const auto dir = log_path_.has_parent_path() ? log_path_.parent_path()
                                               : std::filesystem::path{};
  if (!dir.empty()) {
    std::filesystem::create_directories(dir, ec);
  }

  const bool exists = std::filesystem::exists(log_path_, ec);
  std::ofstream out(log_path_, std::ios::binary | std::ios::app);
  if (!out) {
    error = "open kt log for append failed";
    return false;
  }
  if (!exists) {
    out.write("MIKTLOG1", 8);
  }

  const std::uint16_t user_len = static_cast<std::uint16_t>(
      username.size() > 0xFFFF ? 0xFFFF : username.size());
  if (user_len == 0 || user_len != username.size()) {
    error = "username too long";
    return false;
  }
  WriteUint16(out, user_len);
  out.write(username.data(), static_cast<std::streamsize>(username.size()));
  out.write(reinterpret_cast<const char*>(id_sig_pk.data()),
            static_cast<std::streamsize>(id_sig_pk.size()));
  out.write(reinterpret_cast<const char*>(id_dh_pk.data()),
            static_cast<std::streamsize>(id_dh_pk.size()));
  out.flush();
  if (!out.good()) {
    error = "write kt log failed";
    return false;
  }

  AppendLeafHashLocked(leaf_hash);
  return true;
}

void KeyTransparencyLog::RecomputeRootLocked() {
  root_ = MerkleTreeHash(leaves_, pow2_levels_, 0, leaves_.size());
}

}  // namespace mi::server
