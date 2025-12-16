#include <cassert>
#include <cstdint>
#include <vector>

#include "frame.h"

using mi::server::DecodeFrame;
using mi::server::DecodeFrameHeader;
using mi::server::EncodeFrame;
using mi::server::Frame;
using mi::server::FrameType;

int main() {
  Frame f;
  f.type = FrameType::kMessage;
  f.payload = {1, 2, 3, 4};
  auto encoded = EncodeFrame(f);
  assert(encoded.size() == mi::server::kFrameHeaderSize + f.payload.size());
  assert(encoded[4] == 1 && encoded[5] == 0);  // version le
  assert(encoded[6] == 3 && encoded[7] == 0);  // type le
  assert(encoded[8] == 4 && encoded[9] == 0 && encoded[10] == 0 &&
         encoded[11] == 0);  // payload len le

  FrameType header_type;
  std::uint32_t header_len = 0;
  bool ok = DecodeFrameHeader(encoded.data(), encoded.size(), header_type,
                              header_len);
  assert(ok);
  assert(header_type == FrameType::kMessage);
  assert(header_len == static_cast<std::uint32_t>(f.payload.size()));

  Frame parsed;
  ok = DecodeFrame(encoded.data(), encoded.size(), parsed);
  assert(ok);
  assert(parsed.type == FrameType::kMessage);
  assert(parsed.payload == f.payload);

  // Reject oversized payload len in header
  std::vector<std::uint8_t> huge;
  huge.resize(mi::server::kFrameHeaderSize);
  const std::uint32_t magic = mi::server::kFrameMagic;
  huge[0] = static_cast<std::uint8_t>(magic & 0xFF);
  huge[1] = static_cast<std::uint8_t>((magic >> 8) & 0xFF);
  huge[2] = static_cast<std::uint8_t>((magic >> 16) & 0xFF);
  huge[3] = static_cast<std::uint8_t>((magic >> 24) & 0xFF);
  huge[4] = 1;
  huge[5] = 0;
  huge[6] = 3;
  huge[7] = 0;
  const std::uint32_t too_big =
      static_cast<std::uint32_t>(mi::server::kMaxFramePayloadBytes + 1);
  huge[8] = static_cast<std::uint8_t>(too_big & 0xFF);
  huge[9] = static_cast<std::uint8_t>((too_big >> 8) & 0xFF);
  huge[10] = static_cast<std::uint8_t>((too_big >> 16) & 0xFF);
  huge[11] = static_cast<std::uint8_t>((too_big >> 24) & 0xFF);
  ok = DecodeFrameHeader(huge.data(), huge.size(), header_type, header_len);
  assert(!ok);

  // Corrupt magic
  encoded[0] ^= 0xFF;
  Frame bad;
  ok = DecodeFrame(encoded.data(), encoded.size(), bad);
  assert(!ok);

  return 0;
}
