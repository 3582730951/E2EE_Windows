#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "client_core.h"

namespace {

constexpr std::size_t kChatHeaderSize = 4 + 1 + 1 + 16;

std::array<std::uint8_t, 16> MakeId(std::uint8_t seed) {
  std::array<std::uint8_t, 16> out{};
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<std::uint8_t>(seed + i);
  }
  return out;
}

}  // namespace

int main() {
  using mi::client::ClientCore;

  const std::string group_id = "g1";
  const auto msg_id = MakeId(1);
  const auto call_id = MakeId(9);
  std::array<std::uint8_t, 32> call_key{};
  call_key.fill(0xAB);
  const std::vector<std::uint8_t> sig = {0x01, 0x02, 0x03};

  std::vector<std::uint8_t> dist_payload;
  assert(ClientCore::EncodeGroupCallKeyDist(msg_id, group_id, call_id, 7,
                                            call_key, sig, dist_payload));
  assert(dist_payload.size() > kChatHeaderSize);
  assert(dist_payload[0] == 'M');
  assert(dist_payload[1] == 'I');
  assert(dist_payload[2] == 'C');
  assert(dist_payload[3] == 'H');
  assert(dist_payload[4] == 1);
  assert(dist_payload[5] == 14);

  std::size_t off = kChatHeaderSize;
  std::string out_group;
  std::array<std::uint8_t, 16> out_call{};
  std::uint32_t out_key_id = 0;
  std::array<std::uint8_t, 32> out_key{};
  std::vector<std::uint8_t> out_sig;
  assert(ClientCore::DecodeGroupCallKeyDist(dist_payload, off, out_group,
                                            out_call, out_key_id, out_key,
                                            out_sig));
  assert(off == dist_payload.size());
  assert(out_group == group_id);
  assert(out_call == call_id);
  assert(out_key_id == 7);
  assert(out_key == call_key);
  assert(out_sig == sig);

  std::vector<std::uint8_t> req_payload;
  assert(ClientCore::EncodeGroupCallKeyReq(msg_id, group_id, call_id, 9,
                                           req_payload));
  assert(req_payload.size() > kChatHeaderSize);
  assert(req_payload[4] == 1);
  assert(req_payload[5] == 15);

  off = kChatHeaderSize;
  std::string req_group;
  std::array<std::uint8_t, 16> req_call{};
  std::uint32_t want_key = 0;
  assert(ClientCore::DecodeGroupCallKeyReq(req_payload, off, req_group,
                                           req_call, want_key));
  assert(off == req_payload.size());
  assert(req_group == group_id);
  assert(req_call == call_id);
  assert(want_key == 9);

  return 0;
}
