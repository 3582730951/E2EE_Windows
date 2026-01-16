#ifndef MI_E2EE_PLATFORM_SYS_H
#define MI_E2EE_PLATFORM_SYS_H

#include <cstdint>

namespace mi::platform {

std::uint64_t ProcessRssBytes();
std::uint64_t SystemMemoryTotalBytes();

}  // namespace mi::platform

#endif  // MI_E2EE_PLATFORM_SYS_H
