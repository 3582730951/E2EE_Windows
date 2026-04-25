#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "auth_provider.h"
#include "monocypher.h"

namespace mi::server::test {

inline std::string HexEncode(const std::uint8_t* data, std::size_t len) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    out[i * 2] = kHex[data[i] >> 4];
    out[i * 2 + 1] = kHex[data[i] & 0x0F];
  }
  return out;
}

inline std::string StrongPasswordRecord(const std::string& password,
                                        const std::string& salt_label) {
  constexpr std::uint32_t kBlocks = 8;
  constexpr std::uint32_t kPasses = 1;
  const std::string salt_text = "mi-e2ee-test-auth:" + salt_label;
  std::vector<std::uint8_t> salt(salt_text.begin(), salt_text.end());
  std::vector<std::uint8_t> work_area(kBlocks * 1024);
  std::array<std::uint8_t, 32> hash{};

  crypto_argon2_config cfg;
  cfg.algorithm = CRYPTO_ARGON2_ID;
  cfg.nb_blocks = kBlocks;
  cfg.nb_passes = kPasses;
  cfg.nb_lanes = 1;

  crypto_argon2_inputs in;
  in.pass = reinterpret_cast<const std::uint8_t*>(password.data());
  in.pass_size = static_cast<std::uint32_t>(password.size());
  in.salt = salt.data();
  in.salt_size = static_cast<std::uint32_t>(salt.size());

  crypto_argon2(hash.data(), static_cast<std::uint32_t>(hash.size()),
                work_area.data(), cfg, in, crypto_argon2_no_extras);
  return "argon2id$" + std::to_string(kBlocks) + "$" +
         std::to_string(kPasses) + "$" + HexEncode(salt.data(), salt.size()) +
         "$" + HexEncode(hash.data(), hash.size());
}

inline DemoUser MakeDemoUser(const std::string& username,
                             const std::string& password) {
  DemoUser user;
  const std::string record = StrongPasswordRecord(password, username);
  user.username.set(username);
  user.password.set(record);
  user.username_plain = username;
  user.password_plain = record;
  return user;
}

inline std::string DemoUserFileLine(const std::string& username,
                                    const std::string& password) {
  return username + ":" + StrongPasswordRecord(password, username) + "\n";
}

}  // namespace mi::server::test
