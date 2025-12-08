#ifndef MI_E2EE_SERVER_PROTOCOL_H
#define MI_E2EE_SERVER_PROTOCOL_H

#include <cstdint>
#include <string>
#include <vector>

namespace mi::server::proto {

bool WriteString(const std::string& s, std::vector<std::uint8_t>& out);
bool ReadString(const std::vector<std::uint8_t>& data, std::size_t& offset,
                std::string& out);

bool WriteUint32(std::uint32_t v, std::vector<std::uint8_t>& out);
bool ReadUint32(const std::vector<std::uint8_t>& data, std::size_t& offset,
                std::uint32_t& out);

inline bool WriteBytes(const std::vector<std::uint8_t>& buf,
                       std::vector<std::uint8_t>& out) {
  return WriteUint32(static_cast<std::uint32_t>(buf.size()), out) &&
         (out.insert(out.end(), buf.begin(), buf.end()), true);
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
