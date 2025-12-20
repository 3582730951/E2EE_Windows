#ifndef MI_E2EE_CLIENT_E2EE_ENGINE_H
#define MI_E2EE_CLIENT_E2EE_ENGINE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace mi::client::e2ee {

struct PendingPeerTrust {
  std::string peer_username;
  std::string fingerprint_hex;
  std::string pin6;
};

struct PrivateMessage {
  std::string from_username;
  std::vector<std::uint8_t> plaintext;
};

struct IdentityPolicy {
  std::uint32_t rotation_days{90};
  std::uint32_t legacy_retention_days{180};
  bool tpm_enable{true};
  bool tpm_require{false};
};

class Engine {
 public:
  Engine();
  ~Engine();

  void SetIdentityPolicy(IdentityPolicy policy);

  bool Init(const std::filesystem::path& state_dir, std::string& error);
  void SetLocalUsername(std::string username);

  bool MaybeRotatePreKeys(bool& out_rotated, std::string& error);

  bool BuildPublishBundle(std::vector<std::uint8_t>& out_bundle,
                          std::string& error) const;

  bool HasPendingPeerTrust() const { return !pending_.peer_username.empty(); }
  const PendingPeerTrust& pending_peer_trust() const { return pending_; }
  bool TrustPendingPeer(const std::string& pin, std::string& error);

  std::vector<PrivateMessage> DrainReadyMessages();

  bool EncryptToPeer(const std::string& peer_username,
                     const std::vector<std::uint8_t>& peer_bundle,
                     const std::vector<std::uint8_t>& plaintext,
                     std::vector<std::uint8_t>& out_payload,
                     std::string& error);

  bool DecryptFromPayload(const std::string& peer_username,
                          const std::vector<std::uint8_t>& payload,
                          PrivateMessage& out_message,
                          std::string& error);

  bool SignDetached(const std::vector<std::uint8_t>& message,
                    std::vector<std::uint8_t>& out_sig,
                    std::string& error) const;

  static bool VerifyDetached(const std::vector<std::uint8_t>& message,
                             const std::vector<std::uint8_t>& sig,
                             const std::vector<std::uint8_t>& pk,
                             std::string& error);

  bool ExtractPeerIdentityFromBundle(
      const std::vector<std::uint8_t>& peer_bundle,
      std::vector<std::uint8_t>& out_id_sig_pk,
      std::array<std::uint8_t, 32>& out_id_dh_pk,
      std::string& out_fingerprint_hex,
      std::string& error) const;

  bool EnsurePeerTrusted(const std::string& peer_username,
                         const std::string& fingerprint_hex,
                         std::string& error);

 private:
   static constexpr std::uint8_t kIdentityVersion = 4;
   static constexpr std::uint8_t kProtocolVersion = 5;
   static constexpr std::size_t kSigPublicKeyBytes = 1952;
   static constexpr std::size_t kSigSecretKeyBytes = 4032;
   static constexpr std::size_t kSigBytes = 3309;
   static constexpr std::size_t kKemPublicKeyBytes = 1184;
   static constexpr std::size_t kKemSecretKeyBytes = 2400;
   static constexpr std::size_t kKemCiphertextBytes = 1088;
   static constexpr std::size_t kKemSharedSecretBytes = 32;

   static constexpr std::uint8_t kMsgPreKey = 1;
   static constexpr std::uint8_t kMsgRatchet = 2;

   struct PeerBundle {
     std::array<std::uint8_t, kSigPublicKeyBytes> id_sig_pk{};
     std::array<std::uint8_t, 32> id_dh_pk{};
     std::uint32_t spk_id{0};
     std::array<std::uint8_t, 32> spk_pk{};
     std::array<std::uint8_t, kKemPublicKeyBytes> kem_pk{};
     std::array<std::uint8_t, kSigBytes> spk_sig{};
   };

   struct SkippedKeyId {
     std::array<std::uint8_t, 32> dh{};
     std::uint32_t n{0};

     bool operator==(const SkippedKeyId& other) const {
       return dh == other.dh && n == other.n;
     }
   };

   struct SkippedKeyIdHash {
     std::size_t operator()(const SkippedKeyId& v) const noexcept;
   };

 struct Session {
    std::string peer_username;
    std::string peer_fingerprint_hex;
    std::array<std::uint8_t, 32> rk{};
    std::array<std::uint8_t, 32> ck_s{};
    std::array<std::uint8_t, 32> ck_r{};
    bool has_ck_s{false};
    bool has_ck_r{false};
    std::array<std::uint8_t, 32> dhs_sk{};
    std::array<std::uint8_t, 32> dhs_pk{};
    std::array<std::uint8_t, 32> dhr_pk{};
    std::array<std::uint8_t, kKemSecretKeyBytes> kem_s_sk{};
    std::array<std::uint8_t, kKemPublicKeyBytes> kem_s_pk{};
    std::array<std::uint8_t, kKemPublicKeyBytes> kem_r_pk{};
    std::unordered_map<SkippedKeyId, std::array<std::uint8_t, 32>, SkippedKeyIdHash>
        skipped_mks;
    std::deque<SkippedKeyId> skipped_order;
    std::uint32_t ns{0};
    std::uint32_t nr{0};
    std::uint32_t pn{0};
  };

  struct LegacyKeyset {
    std::uint32_t spk_id{0};
    std::uint64_t retired_at{0};
    std::array<std::uint8_t, 32> spk_sk{};
    std::array<std::uint8_t, kKemSecretKeyBytes> kem_sk{};
  };

  bool LoadOrCreateIdentity(std::string& error);
  bool SaveIdentity(std::string& error) const;
  bool DeriveIdentity(std::string& error);

  bool LoadTrustStore(std::string& error);
  bool SaveTrustStore(std::string& error) const;

  bool ParsePeerBundle(const std::vector<std::uint8_t>& peer_bundle,
                       PeerBundle& out,
                       std::string& error) const;

  bool CheckTrustedForSend(const std::string& peer_username,
                           const std::string& fingerprint_hex,
                           std::string& error);

   void EnforceSkippedMkLimit(Session& session);

   bool TryDecryptWithSkippedMk(Session& session,
                               const std::array<std::uint8_t, 32>& dh,
                               std::uint32_t n,
                               const std::vector<std::uint8_t>& header_ad,
                               const std::array<std::uint8_t, 24>& nonce,
                               const std::vector<std::uint8_t>& cipher_text,
                               const std::array<std::uint8_t, 16>& mac,
                               std::vector<std::uint8_t>& out_plain);

   void SetPendingTrust(const std::string& peer_username,
                        const std::string& fingerprint_hex);

   bool InitSessionAsInitiator(const std::string& peer_username,
                               const PeerBundle& peer,
                               std::array<std::uint8_t, kKemCiphertextBytes>& out_kem_ct,
                               Session& out_session,
                               std::string& error);

  bool InitSessionAsResponder(const std::string& peer_username,
                              const PeerBundle& peer,
                              const std::array<std::uint8_t, 32>& sender_eph_pk,
                              const std::array<std::uint8_t, kKemPublicKeyBytes>& sender_ratchet_kem_pk,
                              const std::array<std::uint8_t, kKemCiphertextBytes>& kem_ct,
                              Session& out_session,
                              std::string& error);

  bool MaybeRotatePreKeysLocked(bool& out_rotated, std::string& error);
  bool PruneLegacyKeys(std::uint64_t now_sec);
  const LegacyKeyset* FindLegacyKey(std::uint32_t spk_id) const;

  bool EncryptMessage(Session& session, std::uint8_t msg_type,
                      const std::vector<std::uint8_t>& header_ad,
                      const std::vector<std::uint8_t>& plaintext,
                      std::vector<std::uint8_t>& out_payload,
                      std::string& error);

  bool RatchetOnReceive(Session& session,
                        const std::array<std::uint8_t, 32>& new_dhr,
                        const std::array<std::uint8_t, kKemPublicKeyBytes>& new_kem_r_pk,
                        const std::array<std::uint8_t, kKemCiphertextBytes>& kem_ct,
                        std::string& error);

   bool DecryptWithSession(Session& session, std::uint8_t msg_type,
                           const std::vector<std::uint8_t>& header_ad,
                           std::uint32_t n, const std::array<std::uint8_t, 24>& nonce,
                           const std::vector<std::uint8_t>& cipher_text,
                           const std::array<std::uint8_t, 16>& mac,
                           std::vector<std::uint8_t>& out_plain,
                           std::string& error);

   std::filesystem::path state_dir_;
   std::filesystem::path identity_path_;
  std::filesystem::path trust_path_;
  std::string local_username_;

  std::array<std::uint8_t, kSigSecretKeyBytes> id_sig_sk_{};
  std::array<std::uint8_t, kSigPublicKeyBytes> id_sig_pk_{};
  std::array<std::uint8_t, 32> id_dh_sk_{};
  std::array<std::uint8_t, 32> id_dh_pk_{};
   std::uint32_t spk_id_{0};
   std::array<std::uint8_t, 32> spk_sk_{};
   std::array<std::uint8_t, 32> spk_pk_{};
   std::array<std::uint8_t, kSigBytes> spk_sig_{};
   std::array<std::uint8_t, kKemSecretKeyBytes> kem_sk_{};
   std::array<std::uint8_t, kKemPublicKeyBytes> kem_pk_{};
   std::uint64_t identity_created_at_{0};
   std::uint64_t identity_rotated_at_{0};
   std::vector<LegacyKeyset> legacy_keys_;

   std::unordered_map<std::string, std::string> trusted_peers_;
   PendingPeerTrust pending_{};
   std::unordered_map<std::string, Session> sessions_;
  std::unordered_map<std::string, std::vector<std::vector<std::uint8_t>>>
      pending_payloads_;
  std::vector<PrivateMessage> ready_messages_;
  IdentityPolicy identity_policy_{};
};

}  // namespace mi::client::e2ee

#endif  // MI_E2EE_CLIENT_E2EE_ENGINE_H
