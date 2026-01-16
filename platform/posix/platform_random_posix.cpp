#include "platform_random.h"

#include <fcntl.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/random.h>
#endif

namespace mi::platform {

namespace {

bool OsRandomBytes(std::uint8_t* out, std::size_t len) {
  if (!out || len == 0) {
    return false;
  }
#if defined(__linux__)
  std::size_t done = 0;
  while (done < len) {
    const ssize_t got = getrandom(out + done, len - done, 0);
    if (got <= 0) {
      break;
    }
    done += static_cast<std::size_t>(got);
  }
  if (done == len) {
    return true;
  }
#endif

  int fd = ::open("/dev/urandom", O_RDONLY);
  if (fd < 0) {
    return false;
  }
  std::size_t done = 0;
  while (done < len) {
    const ssize_t got = ::read(fd, out + done, len - done);
    if (got <= 0) {
      break;
    }
    done += static_cast<std::size_t>(got);
  }
  ::close(fd);
  return done == len;
}

}  // namespace

bool RandomBytes(std::uint8_t* out, std::size_t len) {
  return OsRandomBytes(out, len);
}

bool RandomUint32(std::uint32_t& out) {
  return RandomBytes(reinterpret_cast<std::uint8_t*>(&out), sizeof(out));
}

}  // namespace mi::platform
