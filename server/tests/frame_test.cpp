#include <cassert>
#include <vector>

#include "frame.h"

using mi::server::DecodeFrame;
using mi::server::EncodeFrame;
using mi::server::Frame;
using mi::server::FrameType;

int main() {
  Frame f;
  f.type = FrameType::kMessage;
  f.payload = {1, 2, 3, 4};
  auto encoded = EncodeFrame(f);

  Frame parsed;
  bool ok = DecodeFrame(encoded.data(), encoded.size(), parsed);
  assert(ok);
  assert(parsed.type == FrameType::kMessage);
  assert(parsed.payload == f.payload);

  // Corrupt magic
  encoded[0] ^= 0xFF;
  Frame bad;
  ok = DecodeFrame(encoded.data(), encoded.size(), bad);
  assert(!ok);

  return 0;
}
