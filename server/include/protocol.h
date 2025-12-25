#ifndef MI_E2EE_SERVER_PROTOCOL_H
#define MI_E2EE_SERVER_PROTOCOL_H

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace mi::server::proto {

bool WriteString(const std::string& s, std::vector<std::uint8_t>& out);
bool ReadString(const std::vector<std::uint8_t>& data, std::size_t& offset,
                std::string& out);

bool WriteUint32(std::uint32_t v, std::vector<std::uint8_t>& out);
bool ReadUint32(const std::vector<std::uint8_t>& data, std::size_t& offset,
                std::uint32_t& out);

bool WriteUint64(std::uint64_t v, std::vector<std::uint8_t>& out);
bool ReadUint64(const std::vector<std::uint8_t>& data, std::size_t& offset,
                std::uint64_t& out);

inline bool WriteBytes(const std::uint8_t* data, std::size_t len,
                       std::vector<std::uint8_t>& out) {
  if (!data && len != 0) {
    return false;
  }
  if (len > (std::numeric_limits<std::uint32_t>::max)()) {
    return false;
  }
  out.reserve(out.size() + 4 + len);
  return WriteUint32(static_cast<std::uint32_t>(len), out) &&
         (out.insert(out.end(), data, data + len), true);
}

inline bool WriteBytes(const std::vector<std::uint8_t>& buf,
                       std::vector<std::uint8_t>& out) {
  return WriteBytes(buf.data(), buf.size(), out);
}

inline bool ReadBytes(const std::vector<std::uint8_t>& data,
                      std::size_t& offset, std::vector<std::uint8_t>& out) {
  std::uint32_t len = 0;
  if (!ReadUint32(data, offset, len)) {
    return false;
  }
  if (offset + len > data.size()) {
    return false;
  }
  out.assign(data.begin() + offset, data.begin() + offset + len);
  offset += len;
  return true;
}

}  // namespace mi::server::proto

#endif  // MI_E2EE_SERVER_PROTOCOL_H
