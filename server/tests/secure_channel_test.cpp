#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "pake.h"
#include "secure_channel.h"

using mi::server::DerivedKeys;
using mi::server::SecureChannel;
using mi::server::SecureChannelRole;
using mi::server::FrameType;

static DerivedKeys MakeKeys() {
  DerivedKeys k;
  for (std::size_t i = 0; i < 32; ++i) {
    k.kcp_key[i] = static_cast<std::uint8_t>(i);
    k.header_key[i] = static_cast<std::uint8_t>(0xFF - i);
    k.ratchet_root[i] = static_cast<std::uint8_t>(0xAA ^ i);
  }
  return k;
}

int main() {
  auto keys = MakeKeys();
  SecureChannel client(keys, SecureChannelRole::kClient);
  SecureChannel server(keys, SecureChannelRole::kServer);

  DerivedKeys no_root{};
  SecureChannel client2(no_root, SecureChannelRole::kClient);
  SecureChannel server2(no_root, SecureChannelRole::kServer);

  std::vector<std::uint8_t> plain = {1, 2, 3, 4, 5};
  std::vector<std::uint8_t> cipher;
  bool ok = client.Encrypt(7, FrameType::kMessage, plain, cipher);
  assert(ok);
  assert(cipher.size() == plain.size() + 8 + 16);

  std::vector<std::uint8_t> out;
  ok = server.Decrypt(cipher, FrameType::kMessage, out);
  assert(ok);
  assert(out == plain);

  // Tamper tag
  cipher.back() ^= 0xFF;
  ok = server.Decrypt(cipher, FrameType::kMessage, out);
  assert(!ok);

  // Replay should fail
  std::vector<std::uint8_t> cipher_replay;
  ok = client.Encrypt(8, FrameType::kMessage, plain, cipher_replay);
  assert(ok);
  ok = server.Decrypt(cipher_replay, FrameType::kMessage, out);
  assert(ok);
  ok = server.Decrypt(cipher_replay, FrameType::kMessage, out);
  assert(!ok);

  // Zero keys should still operate (insecure but functional)
  std::vector<std::uint8_t> cipher2;
  ok = client2.Encrypt(1, FrameType::kMessage, plain, cipher2);
  assert(ok);
  std::vector<std::uint8_t> out2;
  ok = server2.Decrypt(cipher2, FrameType::kMessage, out2);
  assert(ok);
  assert(out2 == plain);

  return 0;
}
