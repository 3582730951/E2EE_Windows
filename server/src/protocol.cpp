#include "protocol.h"

#include <cstddef>

namespace mi::server::proto {

namespace {
constexpr std::size_t kMaxStringLen = 0xFFFFu;
}

bool WriteString(const std::string& s, std::vector<std::uint8_t>& out) {
  if (s.size() > kMaxStringLen) {
    return false;
  }
  out.reserve(out.size() + 2 + s.size());
  const std::uint16_t len = static_cast<std::uint16_t>(s.size());
  out.push_back(static_cast<std::uint8_t>(len & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
  out.insert(out.end(), s.begin(), s.begin() + len);
  return true;
}

bool ReadString(const std::vector<std::uint8_t>& data, std::size_t& offset,
                std::string& out) {
  if (offset + 2 > data.size()) {
    return false;
  }
  const std::uint16_t len =
      static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
  offset += 2;
  if (offset + len > data.size()) {
    return false;
  }
  out.assign(reinterpret_cast<const char*>(data.data() + offset), len);
  offset += len;
  return true;
}

bool WriteUint32(std::uint32_t v, std::vector<std::uint8_t>& out) {
  out.reserve(out.size() + 4);
  out.push_back(static_cast<std::uint8_t>(v & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  return true;
}

bool ReadUint32(const std::vector<std::uint8_t>& data, std::size_t& offset,
                std::uint32_t& out) {
  if (offset + 4 > data.size()) {
    return false;
  }
  out = static_cast<std::uint32_t>(data[offset]) |
        (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
        (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
        (static_cast<std::uint32_t>(data[offset + 3]) << 24);
  offset += 4;
  return true;
}

bool WriteUint64(std::uint64_t v, std::vector<std::uint8_t>& out) {
  out.reserve(out.size() + 8);
  for (int i = 0; i < 8; ++i) {
    out.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF));
  }
  return true;
}

bool ReadUint64(const std::vector<std::uint8_t>& data, std::size_t& offset,
                std::uint64_t& out) {
  if (offset + 8 > data.size()) {
    return false;
  }
  out = 0;
  for (int i = 0; i < 8; ++i) {
    out |= (static_cast<std::uint64_t>(data[offset + static_cast<std::size_t>(i)])
            << (i * 8));
  }
  offset += 8;
  return true;
}

}  // namespace mi::server::proto
