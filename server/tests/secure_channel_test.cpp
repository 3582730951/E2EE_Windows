#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "pake.h"
#include "secure_channel.h"

using mi::server::DerivedKeys;
using mi::server::SecureChannel;

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
  SecureChannel ch(keys);

  // Without ratchet_root (zero), fallback to static keys
  DerivedKeys no_root{};
  SecureChannel ch2(no_root);

  std::vector<std::uint8_t> plain = {1, 2, 3, 4, 5};
  std::vector<std::uint8_t> cipher;
  bool ok = ch.Encrypt(7, plain, cipher);
  assert(ok);
  assert(cipher.size() == plain.size() + 12 + 32);

  std::vector<std::uint8_t> out;
  ok = ch.Decrypt(cipher, 7, out);
  assert(ok);
  assert(out == plain);

  // Tamper tag
  cipher.back() ^= 0xFF;
  ok = ch.Decrypt(cipher, 7, out);
  assert(!ok);

  // Fallback channel with zero root should still operate
  std::vector<std::uint8_t> cipher2;
  ok = ch2.Encrypt(1, plain, cipher2);
  assert(ok);
  std::vector<std::uint8_t> out2;
  ok = ch2.Decrypt(cipher2, 1, out2);
  assert(ok);
  assert(out2 == plain);

  return 0;
}
