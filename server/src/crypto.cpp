#include "crypto.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>


#include "platform_random.h"

namespace mi::server::crypto {

namespace {

constexpr std::uint32_t kInitState[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

constexpr std::uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline std::uint32_t RotR(std::uint32_t x, std::uint32_t n) {
  return (x >> n) | (x << (32U - n));
}

inline std::uint32_t Ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
  return (x & y) ^ ((~x) & z);
}

inline std::uint32_t Maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
  return (x & y) ^ (x & z) ^ (y & z);
}

inline std::uint32_t Sig0(std::uint32_t x) {
  return RotR(x, 2) ^ RotR(x, 13) ^ RotR(x, 22);
}

inline std::uint32_t Sig1(std::uint32_t x) {
  return RotR(x, 6) ^ RotR(x, 11) ^ RotR(x, 25);
}

inline std::uint32_t theta0(std::uint32_t x) {
  return RotR(x, 7) ^ RotR(x, 18) ^ (x >> 3);
}

inline std::uint32_t theta1(std::uint32_t x) {
  return RotR(x, 17) ^ RotR(x, 19) ^ (x >> 10);
}

void ProcessChunk(const std::uint8_t chunk[64], std::uint32_t state[8]) {
  std::uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<std::uint32_t>(chunk[i * 4]) << 24) |
           (static_cast<std::uint32_t>(chunk[i * 4 + 1]) << 16) |
           (static_cast<std::uint32_t>(chunk[i * 4 + 2]) << 8) |
           (static_cast<std::uint32_t>(chunk[i * 4 + 3]));
  }
  for (int i = 16; i < 64; ++i) {
    w[i] = theta1(w[i - 2]) + w[i - 7] + theta0(w[i - 15]) + w[i - 16];
  }

  std::uint32_t a = state[0];
  std::uint32_t b = state[1];
  std::uint32_t c = state[2];
  std::uint32_t d = state[3];
  std::uint32_t e = state[4];
  std::uint32_t f = state[5];
  std::uint32_t g = state[6];
  std::uint32_t h = state[7];

  for (int i = 0; i < 64; ++i) {
    const std::uint32_t t1 = h + Sig1(e) + Ch(e, f, g) + kK[i] + w[i];
    const std::uint32_t t2 = Sig0(a) + Maj(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

struct Sha256Ctx {
  std::uint32_t state[8]{};
  std::array<std::uint8_t, 64> buffer{};
  std::uint64_t total_len{0};
  std::size_t buffer_len{0};
};

void Sha256Init(Sha256Ctx& ctx) {
  std::memcpy(ctx.state, kInitState, sizeof(ctx.state));
  ctx.buffer.fill(0);
  ctx.total_len = 0;
  ctx.buffer_len = 0;
}

void Sha256Update(Sha256Ctx& ctx, const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return;
  }
  ctx.total_len += static_cast<std::uint64_t>(len);
  if (ctx.buffer_len != 0) {
    const std::size_t to_copy =
        std::min<std::size_t>(len, ctx.buffer.size() - ctx.buffer_len);
    std::memcpy(ctx.buffer.data() + ctx.buffer_len, data, to_copy);
    ctx.buffer_len += to_copy;
    data += to_copy;
    len -= to_copy;
    if (ctx.buffer_len == ctx.buffer.size()) {
      ProcessChunk(ctx.buffer.data(), ctx.state);
      ctx.buffer_len = 0;
    }
  }

  while (len >= ctx.buffer.size()) {
    ProcessChunk(data, ctx.state);
    data += ctx.buffer.size();
    len -= ctx.buffer.size();
  }

  if (len != 0) {
    std::memcpy(ctx.buffer.data(), data, len);
    ctx.buffer_len = len;
  }
}

void Sha256Final(Sha256Ctx& ctx, Sha256Digest& out) {
  std::array<std::uint8_t, 128> final_block{};
  std::memcpy(final_block.data(), ctx.buffer.data(), ctx.buffer_len);
  final_block[ctx.buffer_len] = 0x80;
  std::size_t total = ctx.buffer_len + 1;

  const std::uint64_t bit_len = ctx.total_len * 8ULL;
  const bool need_extra_block = total > 56;
  const std::size_t pad_len = need_extra_block ? (120 - total) : (56 - total);
  total += pad_len;

  final_block[total + 0] = static_cast<std::uint8_t>((bit_len >> 56) & 0xFF);
  final_block[total + 1] = static_cast<std::uint8_t>((bit_len >> 48) & 0xFF);
  final_block[total + 2] = static_cast<std::uint8_t>((bit_len >> 40) & 0xFF);
  final_block[total + 3] = static_cast<std::uint8_t>((bit_len >> 32) & 0xFF);
  final_block[total + 4] = static_cast<std::uint8_t>((bit_len >> 24) & 0xFF);
  final_block[total + 5] = static_cast<std::uint8_t>((bit_len >> 16) & 0xFF);
  final_block[total + 6] = static_cast<std::uint8_t>((bit_len >> 8) & 0xFF);
  final_block[total + 7] = static_cast<std::uint8_t>(bit_len & 0xFF);
  total += 8;

  const std::size_t chunk_count = total / 64;
  for (std::size_t i = 0; i < chunk_count; ++i) {
    ProcessChunk(final_block.data() + i * 64, ctx.state);
  }

  for (int i = 0; i < 8; ++i) {
    out.bytes[i * 4 + 0] =
        static_cast<std::uint8_t>((ctx.state[i] >> 24) & 0xFF);
    out.bytes[i * 4 + 1] =
        static_cast<std::uint8_t>((ctx.state[i] >> 16) & 0xFF);
    out.bytes[i * 4 + 2] =
        static_cast<std::uint8_t>((ctx.state[i] >> 8) & 0xFF);
    out.bytes[i * 4 + 3] = static_cast<std::uint8_t>(ctx.state[i] & 0xFF);
  }
}

void HmacSha256TwoPart(const std::uint8_t* key, std::size_t key_len,
                       const std::uint8_t* data1, std::size_t data1_len,
                       const std::uint8_t* data2, std::size_t data2_len,
                       const std::uint8_t* data3, std::size_t data3_len,
                       Sha256Digest& out) {
  constexpr std::size_t block_size = 64;
  std::uint8_t k_ipad[block_size];
  std::uint8_t k_opad[block_size];
  std::uint8_t key_block[block_size];
  std::memset(key_block, 0, block_size);

  if (key_len > block_size) {
    Sha256Digest hashed_key;
    Sha256(key, key_len, hashed_key);
    std::memcpy(key_block, hashed_key.bytes.data(), hashed_key.bytes.size());
  } else if (key && key_len != 0) {
    std::memcpy(key_block, key, key_len);
  }

  for (std::size_t i = 0; i < block_size; ++i) {
    k_ipad[i] = static_cast<std::uint8_t>(key_block[i] ^ 0x36);
    k_opad[i] = static_cast<std::uint8_t>(key_block[i] ^ 0x5c);
  }

  Sha256Ctx inner_ctx;
  Sha256Init(inner_ctx);
  Sha256Update(inner_ctx, k_ipad, block_size);
  Sha256Update(inner_ctx, data1, data1_len);
  Sha256Update(inner_ctx, data2, data2_len);
  Sha256Update(inner_ctx, data3, data3_len);
  Sha256Digest inner_hash;
  Sha256Final(inner_ctx, inner_hash);

  Sha256Ctx outer_ctx;
  Sha256Init(outer_ctx);
  Sha256Update(outer_ctx, k_opad, block_size);
  Sha256Update(outer_ctx, inner_hash.bytes.data(), inner_hash.bytes.size());
  Sha256Final(outer_ctx, out);
}

}  // namespace

void Sha256(const std::uint8_t* data, std::size_t len, Sha256Digest& out) {
  Sha256Ctx ctx;
  Sha256Init(ctx);
  Sha256Update(ctx, data, len);
  Sha256Final(ctx, out);
}

void HmacSha256(const std::uint8_t* key, std::size_t key_len,
                const std::uint8_t* data, std::size_t data_len,
                Sha256Digest& out) {
  HmacSha256TwoPart(key, key_len, data, data_len, nullptr, 0, nullptr, 0, out);
}

bool HkdfSha256(const std::uint8_t* ikm, std::size_t ikm_len,
                const std::uint8_t* salt, std::size_t salt_len,
                const std::uint8_t* info, std::size_t info_len,
                std::uint8_t* out_key, std::size_t out_len) {
  if (!out_key || out_len == 0) {
    return false;
  }
  const std::uint8_t zero_salt[32] = {0};
  const std::uint8_t* use_salt = salt;
  std::size_t use_salt_len = salt_len;
  if (!salt || salt_len == 0) {
    use_salt = zero_salt;
    use_salt_len = sizeof(zero_salt);
  }

  Sha256Digest prk;
  HmacSha256(use_salt, use_salt_len, ikm, ikm_len, prk);

  const std::size_t hash_len = prk.bytes.size();
  const std::size_t n_blocks = (out_len + hash_len - 1) / hash_len;
  if (n_blocks > 255) {
    return false;
  }

  std::array<std::uint8_t, 32> t{};
  std::size_t generated = 0;
  for (std::size_t i = 1; i <= n_blocks; ++i) {
    const std::uint8_t counter = static_cast<std::uint8_t>(i);
    Sha256Digest h;
    HmacSha256TwoPart(prk.bytes.data(), prk.bytes.size(),
                      generated > 0 ? t.data() : nullptr,
                      generated > 0 ? t.size() : 0,
                      info, info_len,
                      &counter, sizeof(counter), h);
    std::memcpy(t.data(), h.bytes.data(), hash_len);

    const std::size_t to_copy =
        (generated + hash_len > out_len) ? (out_len - generated) : hash_len;
    std::memcpy(out_key + generated, t.data(), to_copy);
    generated += to_copy;
  }

  return true;
}

}  // namespace mi::server::crypto
