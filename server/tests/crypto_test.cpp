#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
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

  return 0;
}
