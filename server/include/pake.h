#ifndef MI_E2EE_SERVER_PAKE_H
#define MI_E2EE_SERVER_PAKE_H

#include <array>
#include <cstdint>
#include <string>

namespace mi::server {

struct DerivedKeys {
  std::array<std::uint8_t, 32> root_key{};
  std::array<std::uint8_t, 32> header_key{};
  std::array<std::uint8_t, 32> kcp_key{};
  std::array<std::uint8_t, 32> ratchet_root{};
};

// Placeholder: hook real PAKE (OPAQUE/SPA KE2+) then derive keys via HKDF.
bool DeriveKeysFromPake(const std::string& pake_shared,
                        DerivedKeys& out_keys,
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
