#ifndef MI_E2EE_SERVER_PAKE_H
#define MI_E2EE_SERVER_PAKE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mi::server {

constexpr std::uint32_t kLoginKeyExchangeV1 = 1;
constexpr std::size_t kX25519PublicKeyBytes = 32;
constexpr std::size_t kMlKem768PublicKeyBytes = 1184;
constexpr std::size_t kMlKem768CiphertextBytes = 1088;
constexpr std::size_t kMlKem768SharedSecretBytes = 32;

enum class PakePwScheme : std::uint8_t {
  kSha256 = 1,
  kSaltedSha256 = 2,
  kArgon2id = 3,
};

struct DerivedKeys {
  std::array<std::uint8_t, 32> root_key{};
  std::array<std::uint8_t, 32> header_key{};
  std::array<std::uint8_t, 32> kcp_key{};
  std::array<std::uint8_t, 32> ratchet_root{};
};

bool DeriveKeysFromHybridKeyExchange(
    const std::array<std::uint8_t, 32>& dh_shared,
    const std::array<std::uint8_t, 32>& kem_shared,
    const std::string& username,
    const std::string& token,
    DerivedKeys& out_keys,
    std::string& error);

bool DeriveKeysFromPakeHandshake(
    const std::array<std::uint8_t, 32>& handshake_key,
    const std::string& username,
    const std::string& token,
    DerivedKeys& out_keys,
    std::string& error);

bool DeriveKeysFromOpaqueSessionKey(const std::vector<std::uint8_t>& session_key,
                                    const std::string& username,
                                    const std::string& token,
                                    DerivedKeys& out_keys,
                                    std::string& error);

// Legacy: username+password input until real PAKE is wired.
bool DeriveKeysFromPake(const std::string& pake_shared, DerivedKeys& out_keys,
                        std::string& error);

// Simplified: username+password input until real PAKE is wired.
bool DeriveKeysFromCredentials(const std::string& username,
                               const std::string& password,
                               DerivedKeys& out_keys,
                               std::string& error);

// Double-ratchet placeholder: derive message key from root+counter.
bool DeriveMessageKey(const std::array<std::uint8_t, 32>& ratchet_root,
                      std::uint64_t counter,
                      std::array<std::uint8_t, 32>& out_key);

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_PAKE_H
