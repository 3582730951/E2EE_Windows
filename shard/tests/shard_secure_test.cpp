#include "media_frame.h"
#include "secure_types.h"

#include <array>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

int main() {
  shard::ScrambledValue<std::uint32_t> number(42);
  assert(number.get() == 42);
  number.set(1234);
  assert(number.get() == 1234);

  shard::ScrambledString text("hello");
  assert(text.get() == "hello");
  text.set("secure shard");
  assert(text.get() == "secure shard");

  shard_secure_i32* c_number = shard_secure_i32_create(-7);
  assert(c_number != nullptr);
  assert(shard_secure_i32_get(c_number) == -7);
  shard_secure_i32_set(c_number, 99);
  assert(shard_secure_i32_get(c_number) == 99);
  shard_secure_i32_destroy(c_number);

  const std::string payload = "native boundary";
  shard_secure_string* c_text =
      shard_secure_string_create_len(payload.data(), payload.size());
  assert(c_text != nullptr);
  assert(shard_secure_string_length(c_text) == payload.size());
  std::array<char, 32> out{};
  assert(shard_secure_string_get(c_text, out.data(), out.size()) ==
         payload.size());
  assert(std::strcmp(out.data(), payload.c_str()) == 0);
  shard_secure_string_destroy(c_text);

  mi::media::MediaFrame frame;
  frame.call_id.fill(0x42);
  frame.kind = mi::media::StreamKind::kVideo;
  frame.flags = mi::media::kFrameKey | mi::media::kFrameEnd;
  frame.timestamp_ms = 123456789;
  frame.payload = {1, 2, 3, 4, 5};

  std::vector<std::uint8_t> encoded;
  assert(mi::media::EncodeMediaFrame(frame, encoded));

  mi::media::MediaFrame decoded;
  assert(mi::media::DecodeMediaFrame(encoded, decoded));
  assert(decoded.call_id == frame.call_id);
  assert(decoded.kind == frame.kind);
  assert(decoded.flags == frame.flags);
  assert(decoded.timestamp_ms == frame.timestamp_ms);
  assert(decoded.payload == frame.payload);

  encoded[0] = 0xFF;
  assert(!mi::media::DecodeMediaFrame(encoded, decoded));
  return 0;
}
