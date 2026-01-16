#ifndef MI_E2EE_PLATFORM_TIME_H
#define MI_E2EE_PLATFORM_TIME_H

#include <cstdint>

namespace mi::platform {

std::uint64_t NowSteadyMs();
std::uint64_t NowUnixSeconds();
void SleepMs(std::uint32_t ms);

}  // namespace mi::platform

#endif  // MI_E2EE_PLATFORM_TIME_H
