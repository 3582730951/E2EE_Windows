#ifndef MI_E2EE_SERVER_CRYPTO_H
#define MI_E2EE_SERVER_CRYPTO_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace mi::server::crypto {

struct Sha256Digest {
  std::array<std::uint8_t, 32> bytes{};
};

void Sha256(const std::uint8_t* data, std::size_t len, Sha256Digest& out);

void HmacSha256(const std::uint8_t* key, std::size_t key_len,
                const std::uint8_t* data, std::size_t data_len,
                Sha256Digest& out);

bool HkdfSha256(const std::uint8_t* ikm, std::size_t ikm_len,
                const std::uint8_t* salt, std::size_t salt_len,
                const std::uint8_t* info, std::size_t info_len,
                std::uint8_t* out_key, std::size_t out_len);

}  // namespace mi::server::crypto

#endif  // MI_E2EE_SERVER_CRYPTO_H
