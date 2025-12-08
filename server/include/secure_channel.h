#ifndef MI_E2EE_SERVER_SECURE_CHANNEL_H
#define MI_E2EE_SERVER_SECURE_CHANNEL_H

#include <array>
#include <cstdint>
#include <vector>

#include "crypto.h"
#include "pake.h"

namespace mi::server {

class SecureChannel {
 public:
  SecureChannel() = default;

  explicit SecureChannel(const DerivedKeys& keys);

  bool Encrypt(std::uint64_t seq,
               const std::vector<std::uint8_t>& plaintext,
               std::vector<std::uint8_t>& out);

  bool Decrypt(const std::vector<std::uint8_t>& input,
               std::uint64_t expected_seq,
               std::vector<std::uint8_t>& out_plain);

  //  ratchet_root + counter
  static bool DeriveMessageKey(const std::array<std::uint8_t, 32>& ratchet_root,
                               std::uint64_t counter,
                               std::array<std::uint8_t, 32>& out_key);

 private:
  bool DerivePerMessageKeys(std::uint64_t seq,
                            std::array<std::uint8_t, 32>& enc_key,
                            std::array<std::uint8_t, 32>& auth_key) const;
  bool HasRatchetRoot() const;

  std::array<std::uint8_t, 32> enc_key_{};
  std::array<std::uint8_t, 32> auth_key_{};
  std::array<std::uint8_t, 32> ratchet_root_{};
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_SECURE_CHANNEL_H
