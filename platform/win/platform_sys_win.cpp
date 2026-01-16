#include "platform_sys.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>

namespace mi::platform {

std::uint64_t ProcessRssBytes() {
  PROCESS_MEMORY_COUNTERS_EX pmc{};
  if (GetProcessMemoryInfo(GetCurrentProcess(),
                           reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                           sizeof(pmc))) {
    return static_cast<std::uint64_t>(pmc.WorkingSetSize);
  }
  return 0;
}

std::uint64_t SystemMemoryTotalBytes() {
  MEMORYSTATUSEX ms{};
  ms.dwLength = sizeof(ms);
  if (!GlobalMemoryStatusEx(&ms)) {
    return 0;
  }
  return static_cast<std::uint64_t>(ms.ullTotalPhys);
}

}  // namespace mi::platform
