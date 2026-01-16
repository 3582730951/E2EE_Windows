#include "platform_sys.h"

#include <sys/resource.h>
#include <unistd.h>

namespace mi::platform {

std::uint64_t ProcessRssBytes() {
  struct rusage usage {};
  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    return 0;
  }
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__)
  return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
  return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024ull;
#endif
}

std::uint64_t SystemMemoryTotalBytes() {
  const long pages = sysconf(_SC_PHYS_PAGES);
  const long page_size = sysconf(_SC_PAGE_SIZE);
  if (pages <= 0 || page_size <= 0) {
    return 0;
  }
  return static_cast<std::uint64_t>(pages) *
         static_cast<std::uint64_t>(page_size);
}

}  // namespace mi::platform
