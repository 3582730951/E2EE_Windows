#ifndef MI_E2EE_SERVER_KEY_TRANSPARENCY_H
#define MI_E2EE_SERVER_KEY_TRANSPARENCY_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mi::server {

constexpr std::size_t kKtIdentitySigPublicKeyBytes = 1952;
constexpr std::size_t kKtIdentityDhPublicKeyBytes = 32;
constexpr std::size_t kKtSthSigPublicKeyBytes = 1952;
constexpr std::size_t kKtSthSigSecretKeyBytes = 4032;
constexpr std::size_t kKtSthSigBytes = 3309;

using Sha256Hash = std::array<std::uint8_t, 32>;

struct KeyTransparencySth {
  std::uint64_t tree_size{0};
  Sha256Hash root{};
  std::vector<std::uint8_t> signature;
};

inline std::vector<std::uint8_t> BuildKtSthSignatureMessage(
    const KeyTransparencySth& sth) {
  std::vector<std::uint8_t> msg;
  static constexpr char kPrefix[] = "MI_KT_STH_V1";
  msg.insert(msg.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  for (int i = 0; i < 8; ++i) {
    msg.push_back(static_cast<std::uint8_t>((sth.tree_size >> (i * 8)) & 0xFF));
  }
  msg.insert(msg.end(), sth.root.begin(), sth.root.end());
  return msg;
}

struct KeyTransparencyProof {
  KeyTransparencySth sth{};
  std::uint64_t leaf_index{0};
  std::vector<Sha256Hash> audit_path;
  std::vector<Sha256Hash> consistency_path;
};

class KeyTransparencyLog {
 public:
  explicit KeyTransparencyLog(std::filesystem::path log_path);

  bool Load(std::string& error);

  bool UpdateIdentityKeys(
      const std::string& username,
      const std::array<std::uint8_t, kKtIdentitySigPublicKeyBytes>& id_sig_pk,
      const std::array<std::uint8_t, kKtIdentityDhPublicKeyBytes>& id_dh_pk,
      std::string& error);

  KeyTransparencySth Head() const;

  bool BuildProofForLatestKey(const std::string& username,
                              std::uint64_t client_tree_size,
                              KeyTransparencyProof& out_proof,
                              std::string& error) const;

  bool BuildConsistencyProof(std::uint64_t old_size, std::uint64_t new_size,
                             std::vector<Sha256Hash>& out_proof,
                             std::string& error) const;

 private:
  struct LatestKey {
    std::uint64_t leaf_index{0};
    Sha256Hash leaf_hash{};
  };

  void RebuildPow2LevelsLocked();
  void AppendLeafHashLocked(const Sha256Hash& leaf_hash);

  bool AppendEntryLocked(const std::string& username,
                         const std::array<std::uint8_t,
                                          kKtIdentitySigPublicKeyBytes>& id_sig_pk,
                         const std::array<std::uint8_t,
                                          kKtIdentityDhPublicKeyBytes>& id_dh_pk,
                         const Sha256Hash& leaf_hash,
                         std::string& error);

  void RecomputeRootLocked();

  std::filesystem::path log_path_;
  mutable std::mutex mutex_;
  std::vector<Sha256Hash> leaves_;
  std::vector<std::vector<Sha256Hash>> pow2_levels_;
  Sha256Hash root_{};
  std::unordered_map<std::string, LatestKey> latest_by_user_;
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_KEY_TRANSPARENCY_H
