#include "frame.h"

#include <cstring>

namespace mi::server {

namespace {
constexpr std::uint32_t kMagic = 0x4D495746;  // 'MIWF'
constexpr std::uint16_t kVersion = 1;

struct RawHeader {
  std::uint32_t magic;
  std::uint16_t version;
  std::uint16_t type;
  std::uint32_t length;
};
}  // namespace

std::vector<std::uint8_t> EncodeFrame(const Frame& frame) {
  RawHeader header;
  header.magic = kMagic;
  header.version = kVersion;
  header.type = static_cast<std::uint16_t>(frame.type);
  header.length = static_cast<std::uint32_t>(frame.payload.size());

  std::vector<std::uint8_t> buffer;
  buffer.resize(sizeof(RawHeader) + frame.payload.size());
  std::memcpy(buffer.data(), &header, sizeof(RawHeader));
  if (!frame.payload.empty()) {
    std::memcpy(buffer.data() + sizeof(RawHeader), frame.payload.data(),
                frame.payload.size());
  }
  return buffer;
}

bool DecodeFrame(const std::uint8_t* data, std::size_t len, Frame& out) {
  if (!data || len < sizeof(RawHeader)) {
    return false;
  }
  RawHeader header;
  std::memcpy(&header, data, sizeof(RawHeader));
  if (header.magic != kMagic || header.version != kVersion) {
    return false;
  }
  const std::size_t payload_len = header.length;
  if (sizeof(RawHeader) + payload_len > len) {
    return false;
  }
  out.type = static_cast<FrameType>(header.type);
  out.payload.resize(payload_len);
  if (payload_len > 0) {
    std::memcpy(out.payload.data(), data + sizeof(RawHeader), payload_len);
  }
  return true;
}

}  // namespace mi::server
