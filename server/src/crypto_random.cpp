#include "crypto.h"

#include "platform_random.h"

namespace mi::server::crypto {

bool RandomBytes(std::uint8_t* out, std::size_t len) {
  if (!out || len == 0) {
    return false;
  }
  return mi::platform::RandomBytes(out, len);
}

}  // namespace mi::server::crypto
