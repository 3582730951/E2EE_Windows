#include "pake.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>
#include <string>

#include "crypto.h"

namespace mi::server {

std::string_view TransportLabel(TransportKind transport) {
  switch (transport) {
    case TransportKind::kTls:
      return "tls";
    case TransportKind::kLocal:
      return "local";
    case TransportKind::kKcp:
      return "kcp";
    case TransportKind::kTcp:
    default:
      return "tcp";
  }
}

namespace {
void AppendWithNull(const std::string& value, std::vector<std::uint8_t>& out) {
  out.insert(out.end(), value.begin(), value.end());
  out.push_back(0);
}

void AppendWithNull(std::string_view value, std::vector<std::uint8_t>& out) {
  out.insert(out.end(), value.begin(), value.end());
  out.push_back(0);
}
}  // namespace

bool DeriveKeysFromHybridKeyExchange(
    const std::array<std::uint8_t, 32>& dh_shared,
    const std::array<std::uint8_t, 32>& kem_shared,
    const std::string& username,
    const std::string& token,
    TransportKind transport,
    DerivedKeys& out_keys,
    std::string& error) {
  if (username.empty() || token.empty()) {
    error = "invalid kex context";
    return false;
  }

  std::array<std::uint8_t, 64> ikm{};
  std::copy_n(dh_shared.begin(), dh_shared.size(), ikm.begin() + 0);
  std::copy_n(kem_shared.begin(), kem_shared.size(), ikm.begin() + 32);

  constexpr char kInfoPrefix[] = "mi_e2ee_login_hybrid_v2";
  const auto transport_label = TransportLabel(transport);
  std::vector<std::uint8_t> info;
  info.reserve(sizeof(kInfoPrefix) - 1 + 1 + username.size() + 1 + token.size() +
               1 + transport_label.size());
  info.insert(info.end(), kInfoPrefix, kInfoPrefix + sizeof(kInfoPrefix) - 1);
  info.push_back(0);
  AppendWithNull(username, info);
  AppendWithNull(token, info);
  info.insert(info.end(), transport_label.begin(), transport_label.end());

  std::array<std::uint8_t, 128> buf{};
  const bool ok = mi::server::crypto::HkdfSha256(
      ikm.data(), ikm.size(),
      nullptr, 0,
      info.data(), info.size(),
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

bool DeriveKeysFromPake(const std::string& pake_shared,
                        TransportKind transport,
                        DerivedKeys& out_keys,
                        std::string& error) {
  if (pake_shared.empty()) {
    error = "pake shared secret is empty";
    return false;
  }

  constexpr char kInfo[] = "mi_e2ee_pake_derive_v2";
  constexpr std::array<std::uint8_t, 32> kSalt = {
      0x5a, 0x12, 0x33, 0x97, 0xc1, 0x4f, 0x28, 0x0b, 0x91, 0x61, 0xaf,
      0x72, 0x4d, 0xf3, 0x86, 0x9b, 0x3c, 0x55, 0x6e, 0x21, 0xda, 0x01,
      0x44, 0x8f, 0xb7, 0x0a, 0xce, 0x19, 0x2e, 0x73, 0x58, 0xd4};

  const auto transport_label = TransportLabel(transport);
  std::vector<std::uint8_t> info;
  info.reserve(sizeof(kInfo) - 1 + 1 + transport_label.size());
  info.insert(info.end(), kInfo, kInfo + sizeof(kInfo) - 1);
  info.push_back(0);
  info.insert(info.end(), transport_label.begin(), transport_label.end());

  std::array<std::uint8_t, 128> buf{};
  const bool ok = mi::server::crypto::HkdfSha256(
      reinterpret_cast<const std::uint8_t*>(pake_shared.data()),
      pake_shared.size(), kSalt.data(), kSalt.size(),
      info.data(), info.size(),
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

bool DeriveKeysFromPakeHandshake(
    const std::array<std::uint8_t, 32>& handshake_key,
    const std::string& username,
    const std::string& token,
    TransportKind transport,
    DerivedKeys& out_keys,
    std::string& error) {
  if (username.empty() || token.empty()) {
    error = "invalid pake context";
    return false;
  }

  constexpr char kInfoPrefix[] = "mi_e2ee_pake_session_v2";
  const auto transport_label = TransportLabel(transport);
  std::vector<std::uint8_t> info;
  info.reserve(sizeof(kInfoPrefix) - 1 + 1 + username.size() + 1 + token.size() +
               1 + transport_label.size());
  info.insert(info.end(), kInfoPrefix, kInfoPrefix + sizeof(kInfoPrefix) - 1);
  info.push_back(0);
  AppendWithNull(username, info);
  AppendWithNull(token, info);
  info.insert(info.end(), transport_label.begin(), transport_label.end());

  std::array<std::uint8_t, 128> buf{};
  const bool ok = mi::server::crypto::HkdfSha256(
      handshake_key.data(), handshake_key.size(), nullptr, 0, info.data(),
      info.size(), buf.data(), buf.size());
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

bool DeriveKeysFromOpaqueSessionKey(const std::vector<std::uint8_t>& session_key,
                                    const std::string& username,
                                    const std::string& token,
                                    TransportKind transport,
                                    DerivedKeys& out_keys,
                                    std::string& error) {
  if (session_key.empty()) {
    error = "opaque session key empty";
    return false;
  }
  if (username.empty() || token.empty()) {
    error = "invalid opaque context";
    return false;
  }

  constexpr char kInfoPrefix[] = "mi_e2ee_opaque_session_v2";
  const auto transport_label = TransportLabel(transport);
  std::vector<std::uint8_t> info;
  info.reserve(sizeof(kInfoPrefix) - 1 + 1 + username.size() + 1 + token.size() +
               1 + transport_label.size());
  info.insert(info.end(), kInfoPrefix, kInfoPrefix + sizeof(kInfoPrefix) - 1);
  info.push_back(0);
  AppendWithNull(username, info);
  AppendWithNull(token, info);
  info.insert(info.end(), transport_label.begin(), transport_label.end());

  std::array<std::uint8_t, 128> buf{};
  const bool ok = mi::server::crypto::HkdfSha256(
      session_key.data(), session_key.size(), nullptr, 0, info.data(),
      info.size(), buf.data(), buf.size());
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
                               TransportKind transport,
                               DerivedKeys& out_keys,
                               std::string& error) {
  if (username.empty() || password.empty()) {
    error = "credentials empty";
    return false;
  }
  return DeriveKeysFromPake(username + ":" + password, transport, out_keys, error);
}

bool DeriveMessageKey(const std::array<std::uint8_t, 32>& ratchet_root,
                      std::uint64_t counter,
                      std::array<std::uint8_t, 32>& out_key) {
  constexpr char kInfo[] = "mi_e2ee_ratchet_msg_v1";
  std::uint8_t info[sizeof(kInfo) - 1 + 8];
  std::memcpy(info, kInfo, sizeof(kInfo) - 1);
  for (int i = 0; i < 8; ++i) {
    info[sizeof(kInfo) - 1 + i] =
        static_cast<std::uint8_t>((counter >> (i * 8)) & 0xFF);
  }
  return crypto::HkdfSha256(ratchet_root.data(), ratchet_root.size(),
                            nullptr, 0,
                            info, sizeof(info),
                            out_key.data(), out_key.size());
}

}  // namespace mi::server
