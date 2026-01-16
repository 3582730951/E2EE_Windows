#include "platform_time.h"

#include <chrono>
#include <thread>

namespace mi::platform {

std::uint64_t NowSteadyMs() {
  static const auto kStart = std::chrono::steady_clock::now();
  const auto now = std::chrono::steady_clock::now();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now - kStart)
          .count());
}

std::uint64_t NowUnixSeconds() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto sec = std::chrono::duration_cast<std::chrono::seconds>(now).count();
  return sec <= 0 ? 0 : static_cast<std::uint64_t>(sec);
}

void SleepMs(std::uint32_t ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

}  // namespace mi::platform
