#include "key_transparency.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "crypto.h"

namespace {

bool Check(bool cond) { return cond; }

std::filesystem::path TempDir(const std::string& name) {
  auto dir = std::filesystem::temp_directory_path() / name;
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  return dir;
}

constexpr std::uint8_t kLeafPrefix = 0x00;
constexpr std::uint8_t kNodePrefix = 0x01;

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

mi::server::Sha256Hash HashSha256(const std::uint8_t* data, std::size_t len) {
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(data, len, d);
  return d.bytes;
}

mi::server::Sha256Hash HashNode(const mi::server::Sha256Hash& left,
                                const mi::server::Sha256Hash& right) {
  std::uint8_t buf[1 + 32 + 32];
  buf[0] = kNodePrefix;
  std::memcpy(buf + 1, left.data(), left.size());
  std::memcpy(buf + 1 + 32, right.data(), right.size());
  return HashSha256(buf, sizeof(buf));
}

std::vector<std::uint8_t> BuildLeafData(
    const std::string& username,
    const std::array<std::uint8_t, mi::server::kKtIdentitySigPublicKeyBytes>&
        id_sig_pk,
    const std::array<std::uint8_t, mi::server::kKtIdentityDhPublicKeyBytes>&
        id_dh_pk) {
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

mi::server::Sha256Hash HashLeaf(const std::vector<std::uint8_t>& leaf_data) {
  std::vector<std::uint8_t> buf;
  buf.reserve(1 + leaf_data.size());
  buf.push_back(kLeafPrefix);
  buf.insert(buf.end(), leaf_data.begin(), leaf_data.end());
  return HashSha256(buf.data(), buf.size());
}

mi::server::Sha256Hash MerkleTreeHash(const std::vector<mi::server::Sha256Hash>& leaves,
                                      std::size_t start, std::size_t n) {
  if (n == 0) {
    static constexpr std::uint8_t kEmpty[1] = {0};
    return HashSha256(kEmpty, 0);
  }
  if (n == 1) {
    return leaves[start];
  }
  const std::size_t k = LargestPowerOfTwoLessThan(n);
  const auto left = MerkleTreeHash(leaves, start, k);
  const auto right = MerkleTreeHash(leaves, start + k, n - k);
  return HashNode(left, right);
}

std::vector<mi::server::Sha256Hash> MerkleAuditPath(
    std::size_t m, const std::vector<mi::server::Sha256Hash>& leaves,
    std::size_t start, std::size_t n) {
  if (n <= 1) {
    return {};
  }
  const std::size_t k = LargestPowerOfTwoLessThan(n);
  if (m < k) {
    auto path = MerkleAuditPath(m, leaves, start, k);
    path.push_back(MerkleTreeHash(leaves, start + k, n - k));
    return path;
  }
  auto path = MerkleAuditPath(m - k, leaves, start + k, n - k);
  path.push_back(MerkleTreeHash(leaves, start, k));
  return path;
}

std::vector<mi::server::Sha256Hash> MerkleSubProof(
    std::size_t m, const std::vector<mi::server::Sha256Hash>& leaves,
    std::size_t start, std::size_t n, bool b) {
  if (m == n) {
    if (b) {
      return {};
    }
    return {MerkleTreeHash(leaves, start, n)};
  }
  const std::size_t k = LargestPowerOfTwoLessThan(n);
  if (m <= k) {
    auto proof = MerkleSubProof(m, leaves, start, k, b);
    proof.push_back(MerkleTreeHash(leaves, start + k, n - k));
    return proof;
  }
  auto proof = MerkleSubProof(m - k, leaves, start + k, n - k, false);
  proof.push_back(MerkleTreeHash(leaves, start, k));
  return proof;
}

std::vector<mi::server::Sha256Hash> MerkleConsistencyProof(
    std::size_t m, const std::vector<mi::server::Sha256Hash>& leaves,
    std::size_t start, std::size_t n) {
  if (m == 0 || m == n) {
    return {};
  }
  return MerkleSubProof(m, leaves, start, n, true);
}

bool EqualHash(const mi::server::Sha256Hash& a, const mi::server::Sha256Hash& b) {
  return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

}  // namespace

int main() {
  const auto dir = TempDir("mi_e2ee_kt_incremental");
  const auto log_path = dir / "kt_log.bin";

  mi::server::KeyTransparencyLog log(log_path);
  std::string err;
  if (!Check(log.Load(err))) {
    return 1;
  }
  if (!Check(log.Head().tree_size == 0)) {
    return 1;
  }

  std::vector<mi::server::Sha256Hash> leaves;
  leaves.reserve(256);

  for (std::size_t i = 0; i < 256; ++i) {
    const std::string username = "user" + std::to_string(i);
    std::array<std::uint8_t, mi::server::kKtIdentitySigPublicKeyBytes> id_sig_pk{};
    std::array<std::uint8_t, mi::server::kKtIdentityDhPublicKeyBytes> id_dh_pk{};
    id_sig_pk.fill(static_cast<std::uint8_t>(i & 0xFF));
    for (std::size_t j = 0; j < id_dh_pk.size(); ++j) {
      id_dh_pk[j] = static_cast<std::uint8_t>((i + j) & 0xFF);
    }

    if (!log.UpdateIdentityKeys(username, id_sig_pk, id_dh_pk, err)) {
      return 1;
    }
    const auto leaf_hash = HashLeaf(BuildLeafData(username, id_sig_pk, id_dh_pk));
    leaves.push_back(leaf_hash);

    const auto sth = log.Head();
    if (!Check(sth.tree_size == leaves.size())) {
      return 1;
    }
    const auto expected_root = MerkleTreeHash(leaves, 0, leaves.size());
    if (!Check(EqualHash(sth.root, expected_root))) {
      return 1;
    }
  }

  {
    mi::server::KeyTransparencyProof proof;
    const std::string username = "user255";
    const std::uint64_t client_size = 255;
    if (!log.BuildProofForLatestKey(username, client_size, proof, err)) {
      return 1;
    }
    if (!Check(proof.sth.tree_size == 256)) {
      return 1;
    }
    if (!Check(proof.leaf_index == 255)) {
      return 1;
    }
    const auto expected_audit =
        MerkleAuditPath(255, leaves, 0, static_cast<std::size_t>(leaves.size()));
    if (!Check(proof.audit_path == expected_audit)) {
      return 1;
    }
    const auto expected_consistency = MerkleConsistencyProof(
        static_cast<std::size_t>(client_size), leaves, 0, leaves.size());
    if (!Check(proof.consistency_path == expected_consistency)) {
      return 1;
    }
  }

  {
    std::vector<mi::server::Sha256Hash> proof;
    if (!log.BuildConsistencyProof(128, 256, proof, err)) {
      return 1;
    }
    const auto expected =
        MerkleConsistencyProof(128, leaves, 0, static_cast<std::size_t>(leaves.size()));
    if (!Check(proof == expected)) {
      return 1;
    }
  }

  {
    mi::server::KeyTransparencyLog reloaded(log_path);
    if (!reloaded.Load(err)) {
      return 1;
    }
    const auto sth = reloaded.Head();
    if (!Check(sth.tree_size == 256)) {
      return 1;
    }
    const auto expected_root = MerkleTreeHash(leaves, 0, leaves.size());
    if (!Check(EqualHash(sth.root, expected_root))) {
      return 1;
    }
  }

  return 0;
}
