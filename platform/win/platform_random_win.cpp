#include "platform_random.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

namespace mi::platform {

bool RandomBytes(std::uint8_t* out, std::size_t len) {
  if (!out || len == 0) {
    return false;
  }
  return BCryptGenRandom(nullptr, out, static_cast<ULONG>(len),
                         BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}

bool RandomUint32(std::uint32_t& out) {
  return RandomBytes(reinterpret_cast<std::uint8_t*>(&out), sizeof(out));
}

}  // namespace mi::platform
