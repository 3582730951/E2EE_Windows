#ifndef MI_E2EE_WIN_NATIVE_SYSCALL_PROBE_H
#define MI_E2EE_WIN_NATIVE_SYSCALL_PROBE_H

#include "platform_security.h"

#include <cstdint>

namespace mi::platform::win {

inline constexpr const char* kNativeExportWhitelist[] = {
    "NtQueryInformationProcess",
    "NtQueryVirtualMemory",
    "NtQuerySystemInformation",
    "NtQueryInformationThread",
    "NtSetInformationThread",
};

struct NativeProbeResult {
  bool available{false};
  bool native_query_succeeded{false};
  bool debugger_present{false};
  bool ntdll_stubs_verified{false};
  TamperSignal signal{TamperSignal::kNone};
};

bool AllowedNativeExport(const char* name) noexcept;
NativeProbeResult RunNativeSyscallProbe() noexcept;

}  // namespace mi::platform::win

#endif  // MI_E2EE_WIN_NATIVE_SYSCALL_PROBE_H
