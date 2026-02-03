#ifndef MI_E2EE_PLATFORM_SECURITY_H
#define MI_E2EE_PLATFORM_SECURITY_H

#include <cstdint>

namespace mi::platform {

enum class TamperSignal : std::uint8_t {
  kNone = 0,
  kDebugger = 1,
  kCodeTamper = 2,
  kHardwareBreakpoint = 3,
  kSignatureInvalid = 4,
  kSandboxMissing = 5
};

using TamperHandler = void (*)(TamperSignal) noexcept;

void SetTamperHandler(TamperHandler handler) noexcept;
bool IsTamperDetected() noexcept;
TamperSignal LastTamperSignal() noexcept;

void StartEndpointHardening() noexcept;

}  // namespace mi::platform

#endif  // MI_E2EE_PLATFORM_SECURITY_H
