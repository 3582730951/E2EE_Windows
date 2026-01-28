#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>

#include "crypto.h"

using mi::server::crypto::HmacSha256;
using mi::server::crypto::Sha256;
using mi::server::crypto::Sha256Digest;

namespace {

std::string ToHex(const Sha256Digest& digest) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(digest.bytes.size() * 2);
  for (std::size_t i = 0; i < digest.bytes.size(); ++i) {
    const std::uint8_t byte = digest.bytes[i];
    out[i * 2] = kHex[(byte >> 4) & 0x0F];
    out[i * 2 + 1] = kHex[byte & 0x0F];
  }
  return out;
}

std::string ToHexBytes(const std::uint8_t* data, std::size_t len) {
  static constexpr char kHex[] = "0123456789abcdef";
  if (!data || len == 0) {
    return {};
  }
  std::string out;
  out.resize(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    out[i * 2] = kHex[(data[i] >> 4) & 0x0F];
    out[i * 2 + 1] = kHex[data[i] & 0x0F];
  }
  return out;
}

std::vector<std::uint8_t> HexToBytes(const std::string& hex) {
  std::vector<std::uint8_t> out;
  if (hex.empty() || (hex.size() % 2) != 0) {
    return out;
  }
  out.reserve(hex.size() / 2);
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    auto nibble = [](char c) -> int {
      if (c >= '0' && c <= '9') {
        return c - '0';
      }
      if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
      }
      if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
      }
      return -1;
    };
    const int hi = nibble(hex[i]);
    const int lo = nibble(hex[i + 1]);
    if (hi < 0 || lo < 0) {
      out.clear();
      return out;
    }
    out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
  }
  return out;
}

void ExpectSha256(const std::string& input, const std::string& expected_hex) {
  Sha256Digest digest;
  Sha256(reinterpret_cast<const std::uint8_t*>(input.data()), input.size(),
         digest);
  assert(ToHex(digest) == expected_hex);
}

void ExpectHmacSha256(const std::string& key, const std::string& message,
                      const std::string& expected_hex) {
  Sha256Digest digest;
  HmacSha256(reinterpret_cast<const std::uint8_t*>(key.data()), key.size(),
             reinterpret_cast<const std::uint8_t*>(message.data()),
             message.size(), digest);
  assert(ToHex(digest) == expected_hex);
}

void ExpectHkdfSha256(const std::string& ikm_hex,
                      const std::string& salt_hex,
                      const std::string& info_hex,
                      std::size_t out_len,
                      const std::string& expected_hex) {
  const auto ikm = HexToBytes(ikm_hex);
  const auto salt = HexToBytes(salt_hex);
  const auto info = HexToBytes(info_hex);
  std::vector<std::uint8_t> out;
  out.resize(out_len);
  const bool ok = mi::server::crypto::HkdfSha256(
      ikm.data(), ikm.size(),
      salt.data(), salt.size(),
      info.data(), info.size(),
      out.data(), out.size());
  assert(ok);
  assert(ToHexBytes(out.data(), out.size()) == expected_hex);
}

}  // namespace

int main() {
  ExpectSha256(
      "",
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  ExpectSha256(
      "abc",
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

  ExpectHmacSha256(
      "key", "The quick brown fox jumps over the lazy dog",
      "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");

  ExpectHkdfSha256(
      "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
      "000102030405060708090a0b0c",
      "f0f1f2f3f4f5f6f7f8f9",
      42,
      "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
      "34007208d5b887185865");

  ExpectHkdfSha256(
      "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
      "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
      "404142434445464748494a4b4c4d4e4f",
      "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f"
      "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
      "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf",
      "b0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
      "d0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeef"
      "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
      82,
      "b11e398dc80327a1c8e7f78c596a49344f012eda2d4efad8a050cc4c19afa97c"
      "59045a99cac7827271cb41c65e590e09da3275600c2f09b8367793a9aca3db71"
      "cc30c58179ec3e87c14c01d5c1f3434f1d87");

  return 0;
}
