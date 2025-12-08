#include "pake.h"

#include <algorithm>
#include <array>
#include <string>

#include "crypto.h"

namespace mi::server {

bool DeriveKeysFromPake(const std::string& pake_shared,
                        DerivedKeys& out_keys,
                        std::string& error) {
  if (pake_shared.empty()) {
    error = "pake shared secret is empty";
    return false;
  }

  constexpr char kInfo[] = "mi_e2ee_pake_derive_v1";
  constexpr std::array<std::uint8_t, 32> kSalt = {
      0x5a, 0x12, 0x33, 0x97, 0xc1, 0x4f, 0x28, 0x0b, 0x91, 0x61, 0xaf,
      0x72, 0x4d, 0xf3, 0x86, 0x9b, 0x3c, 0x55, 0x6e, 0x21, 0xda, 0x01,
      0x44, 0x8f, 0xb7, 0x0a, 0xce, 0x19, 0x2e, 0x73, 0x58, 0xd4};

  std::array<std::uint8_t, 128> buf{};
  const bool ok = mi::server::crypto::HkdfSha256(
      reinterpret_cast<const std::uint8_t*>(pake_shared.data()),
      pake_shared.size(), kSalt.data(), kSalt.size(),
      reinterpret_cast<const std::uint8_t*>(kInfo), sizeof(kInfo) - 1,
      buf.data(), buf.size());
  if (!ok) {
    error = "hkdf derivation failed";
    return false;
  }

  std::copy_n(buf.begin() + 0, out_keys.root_key.size(),
              out_keys.root_key.begin());
  std::copy_n(buf.begin() + 32, out_keys.header_key.size(),
              out_keys.header_key.begin());
  std::copy_n(buf.begin() + 64, out_keys.kcp_key.size(),
              out_keys.kcp_key.begin());
  std::copy_n(buf.begin() + 96, out_keys.ratchet_root.size(),
              out_keys.ratchet_root.begin());
  error.clear();
  return true;
}

bool DeriveKeysFromCredentials(const std::string& username,
                               const std::string& password,
                               DerivedKeys& out_keys,
                               std::string& error) {
  if (username.empty() || password.empty()) {
    error = "credentials empty";
    return false;
  }
  return DeriveKeysFromPake(username + ":" + password, out_keys, error);
}

bool DeriveMessageKey(const std::array<std::uint8_t, 32>& ratchet_root,
                      std::uint64_t counter,
                      std::array<std::uint8_t, 32>& out_key) {
  constexpr char kInfo[] = "mi_e2ee_ratchet_msg_v1";
  return crypto::HkdfSha256(ratchet_root.data(), ratchet_root.size(),
                            nullptr, 0,
                            reinterpret_cast<const std::uint8_t*>(kInfo),
                            sizeof(kInfo) - 1,
                            out_key.data(), out_key.size());
}

}  // namespace mi::server
