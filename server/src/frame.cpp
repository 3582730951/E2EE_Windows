#include "frame.h"

#include <cstring>
#include <limits>

namespace mi::server {

namespace {
constexpr std::size_t kFrameLengthOffset = 8;

std::uint16_t ReadUint16Le(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[0]) |
                                    (static_cast<std::uint16_t>(p[1]) << 8));
}

std::uint32_t ReadUint32Le(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(static_cast<std::uint32_t>(p[0]) |
                                    (static_cast<std::uint32_t>(p[1]) << 8) |
                                    (static_cast<std::uint32_t>(p[2]) << 16) |
                                    (static_cast<std::uint32_t>(p[3]) << 24));
}

void WriteUint16Le(std::uint16_t v, std::uint8_t* out) {
  out[0] = static_cast<std::uint8_t>(v & 0xFF);
  out[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}

void WriteUint32Le(std::uint32_t v, std::uint8_t* out) {
  out[0] = static_cast<std::uint8_t>(v & 0xFF);
  out[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
  out[2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
  out[3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
}
}  // namespace

std::vector<std::uint8_t> EncodeFrame(const Frame& frame) {
  if (frame.payload.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    return {};
  }
  if (frame.payload.size() > kMaxFramePayloadBytes) {
    return {};
  }

  std::vector<std::uint8_t> buffer;
  buffer.resize(kFrameHeaderSize + frame.payload.size());
  WriteUint32Le(kFrameMagic, buffer.data());
  WriteUint16Le(kFrameVersion, buffer.data() + 4);
  WriteUint16Le(static_cast<std::uint16_t>(frame.type), buffer.data() + 6);
  WriteUint32Le(static_cast<std::uint32_t>(frame.payload.size()),
                buffer.data() + kFrameLengthOffset);
  if (!frame.payload.empty()) {
    std::memcpy(buffer.data() + kFrameHeaderSize, frame.payload.data(),
                frame.payload.size());
  }
  return buffer;
}

bool DecodeFrameHeader(const std::uint8_t* data, std::size_t len,
                       FrameType& out_type, std::uint32_t& out_payload_len) {
  if (!data || len < kFrameHeaderSize) {
    return false;
  }

  const std::uint32_t magic = ReadUint32Le(data);
  const std::uint16_t version = ReadUint16Le(data + 4);
  if (magic != kFrameMagic || version != kFrameVersion) {
    return false;
  }

  out_type = static_cast<FrameType>(ReadUint16Le(data + 6));
  out_payload_len = ReadUint32Le(data + kFrameLengthOffset);
  if (out_payload_len > kMaxFramePayloadBytes) {
    return false;
  }
  return true;
}

bool DecodeFrame(const std::uint8_t* data, std::size_t len, Frame& out) {
  FrameType type;
  std::uint32_t payload_len = 0;
  if (!DecodeFrameHeader(data, len, type, payload_len)) {
    return false;
  }
  const std::size_t total = kFrameHeaderSize + payload_len;
  if (total > len) {
    return false;
  }
  out.type = type;
  out.payload.resize(static_cast<std::size_t>(payload_len));
  if (payload_len > 0) {
    std::memcpy(out.payload.data(), data + kFrameHeaderSize, payload_len);
  }
  return true;
}

}  // namespace mi::server
