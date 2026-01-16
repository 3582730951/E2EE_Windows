#include "hex_utils.h"

#include <cctype>
#include <string_view>

#include "crypto.h"

namespace mi::common {

namespace {

int HexNibble(char c) {
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
}

}  // namespace

std::string Sha256Hex(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return {};
  }
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(data, len, d);
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(d.bytes.size() * 2);
  for (std::size_t i = 0; i < d.bytes.size(); ++i) {
    out[i * 2] = kHex[d.bytes[i] >> 4];
    out[i * 2 + 1] = kHex[d.bytes[i] & 0x0F];
  }
  return out;
}

bool HexToBytes(std::string_view hex, std::vector<std::uint8_t>& out) {
  out.clear();
  if (hex.empty() || (hex.size() % 2) != 0) {
    return false;
  }
  out.reserve(hex.size() / 2);
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    const int hi = HexNibble(hex[i]);
    const int lo = HexNibble(hex[i + 1]);
    if (hi < 0 || lo < 0) {
      out.clear();
      return false;
    }
    out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
  }
  return true;
}

bool HexToBytes(const std::string& hex, std::vector<std::uint8_t>& out) {
  return HexToBytes(std::string_view(hex), out);
}

std::string GroupHex4(const std::string& hex) {
  if (hex.empty()) {
    return {};
  }
  std::string out;
  out.reserve(hex.size() + (hex.size() / 4));
  for (std::size_t i = 0; i < hex.size(); ++i) {
    if (i != 0 && (i % 4) == 0) {
      out.push_back('-');
    }
    out.push_back(hex[i]);
  }
  return out;
}

}  // namespace mi::common
