#include "platform_security.h"
#include "native_syscall_probe.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cctype>
#include <string>
#include <thread>

#include "monocypher.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <cstring>

namespace mi::platform {

namespace {

std::atomic<bool> gStarted{false};
enum class HardeningLevel : std::uint8_t { kOff = 0, kLow = 1, kMedium = 2, kHigh = 3 };
std::atomic<HardeningLevel> gLevel{HardeningLevel::kHigh};
std::atomic<bool> gTamperDetected{false};
std::atomic<TamperSignal> gLastTamper{TamperSignal::kNone};
std::atomic<TamperHandler> gTamperHandler{nullptr};

HardeningLevel ParseHardeningLevel() noexcept;
std::uint32_t ParseHardeningPollMs() noexcept;

using SetProcessMitigationPolicyFn = BOOL(WINAPI*)(int, PVOID, SIZE_T);
using NtQueryInformationProcessFn =
    LONG(WINAPI*)(HANDLE, int, PVOID, ULONG, PULONG);
using NtSetInformationThreadFn =
    LONG(WINAPI*)(HANDLE, int, PVOID, ULONG);
using SetDefaultDllDirectoriesFn = BOOL(WINAPI*)(DWORD);

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif
#ifndef LOAD_LIBRARY_SEARCH_APPLICATION_DIR
#define LOAD_LIBRARY_SEARCH_APPLICATION_DIR 0x00000200
#endif

constexpr DWORD kNoRemoteImagesFlag = 0x1u;
constexpr DWORD kNoLowMandatoryLabelImagesFlag = 0x2u;
constexpr DWORD kPreferSystem32ImagesFlag = 0x4u;

constexpr int kProcessExtensionPointDisablePolicy = 6;
constexpr int kProcessImageLoadPolicy = 10;
constexpr int kProcessDebugPort = 7;
constexpr int kProcessDebugObjectHandle = 0x1e;
constexpr int kProcessDebugFlags = 0x1f;
constexpr int kThreadHideFromDebugger = 0x11;

struct ExtensionPointDisablePolicy {
  std::uint32_t flags{0};
};
static_assert(sizeof(ExtensionPointDisablePolicy) == sizeof(std::uint32_t));

struct ImageLoadPolicy {
  std::uint32_t flags{0};
};
static_assert(sizeof(ImageLoadPolicy) == sizeof(std::uint32_t));

struct TextRegion {
  const std::uint8_t* base{nullptr};
  std::size_t size{0};
};

bool ParseEnvFlag(const char* name, bool default_value) noexcept {
  const char* env = std::getenv(name);
  if (!env || *env == '\0') {
    return default_value;
  }
  std::string v(env);
  for (auto& ch : v) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  if (v == "1" || v == "true" || v == "on" || v == "yes") {
    return true;
  }
  if (v == "0" || v == "false" || v == "off" || v == "no") {
    return false;
  }
  return default_value;
}

bool FailCloseEnabled() noexcept {
#if defined(MI_E2EE_SECURE_RELEASE)
  return true;
#else
  if (ParseHardeningLevel() == HardeningLevel::kOff) {
    return false;
  }
  // Fail-close is secure-by-default. Set either env to 0/off for local debug.
  const bool hardening = ParseEnvFlag("MI_E2EE_HARDENING_FAILCLOSE", true);
  const bool tamper = ParseEnvFlag("MI_E2EE_TAMPER_FAILCLOSE", true);
  return hardening && tamper;
#endif
}

void FailClose(TamperSignal signal) noexcept {
  if (!FailCloseEnabled()) {
    return;
  }
  std::uint32_t code = 0xE2EE00FFu;
  switch (signal) {
    case TamperSignal::kDebugger:
      code = 0xE2EE0002u;
      break;
    case TamperSignal::kCodeTamper:
      code = 0xE2EE0001u;
      break;
    case TamperSignal::kHardwareBreakpoint:
      code = 0xE2EE0003u;
      break;
    case TamperSignal::kSignatureInvalid:
      code = 0xE2EE0004u;
      break;
    case TamperSignal::kSandboxMissing:
      code = 0xE2EE0005u;
      break;
    case TamperSignal::kApiHook:
      code = 0xE2EE0006u;
      break;
    case TamperSignal::kUnexpectedModule:
      code = 0xE2EE0007u;
      break;
    case TamperSignal::kPrivateExecutableMemory:
      code = 0xE2EE0008u;
      break;
    default:
      break;
  }
  TerminateProcess(GetCurrentProcess(), static_cast<UINT>(code));
}

void ReportTamper(TamperSignal signal) noexcept {
  if (signal == TamperSignal::kNone) {
    return;
  }
  const bool first = !gTamperDetected.exchange(true);
  gLastTamper.store(signal);
  if (first) {
    if (auto handler = gTamperHandler.load()) {
      handler(signal);
    }
  }
  FailClose(signal);
}

void ApplyBestEffortMitigations(HardeningLevel level) noexcept {
  if (level == HardeningLevel::kOff) {
    return;
  }
  HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

  SetDllDirectoryW(L"");

  const auto kernel32 = GetModuleHandleW(L"kernel32.dll");
  if (!kernel32) {
    return;
  }
  const auto setDefaultDllDirectories =
      reinterpret_cast<SetDefaultDllDirectoriesFn>(
          GetProcAddress(kernel32, "SetDefaultDllDirectories"));
  if (setDefaultDllDirectories) {
    setDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 |
                             LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
  }
  if (level == HardeningLevel::kLow) {
    return;
  }

  const auto ntdll = GetModuleHandleW(L"ntdll.dll");
  if (ntdll) {
    const auto setInfoThread =
        reinterpret_cast<NtSetInformationThreadFn>(
            GetProcAddress(ntdll, "NtSetInformationThread"));
    if (setInfoThread) {
      setInfoThread(GetCurrentThread(), kThreadHideFromDebugger, nullptr, 0);
    }
  }

  const auto setProcessMitigationPolicy =
      reinterpret_cast<SetProcessMitigationPolicyFn>(
          GetProcAddress(kernel32, "SetProcessMitigationPolicy"));
  if (!setProcessMitigationPolicy) {
    return;
  }

  ExtensionPointDisablePolicy ext{};
  ext.flags = 0x1u;  // DisableExtensionPoints
  setProcessMitigationPolicy(kProcessExtensionPointDisablePolicy, &ext,
                             sizeof(ext));

  ImageLoadPolicy img{};
  img.flags = kNoRemoteImagesFlag | kNoLowMandatoryLabelImagesFlag |
              kPreferSystem32ImagesFlag;
  setProcessMitigationPolicy(kProcessImageLoadPolicy, &img, sizeof(img));
}

bool GetMainModuleTextRegion(TextRegion& out) noexcept {
  const auto exe = GetModuleHandleW(nullptr);
  if (!exe) {
    return false;
  }
  const auto* base = reinterpret_cast<const std::uint8_t*>(exe);
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
    return false;
  }
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
      base + static_cast<std::size_t>(dos->e_lfanew));
  if (nt->Signature != IMAGE_NT_SIGNATURE) {
    return false;
  }
  const auto* sections = IMAGE_FIRST_SECTION(nt);
  const std::uint16_t count = nt->FileHeader.NumberOfSections;
  for (std::uint16_t i = 0; i < count; ++i) {
    const auto& sec = sections[i];
    if (std::memcmp(sec.Name, ".text", 5) != 0) {
      continue;
    }
    out.base = base + static_cast<std::size_t>(sec.VirtualAddress);
    out.size = static_cast<std::size_t>(sec.Misc.VirtualSize);
    return out.base != nullptr && out.size != 0;
  }
  return false;
}

std::array<std::uint8_t, 32> HashText(const TextRegion& region) noexcept {
  std::array<std::uint8_t, 32> hash{};
  crypto_blake2b(hash.data(), hash.size(), region.base, region.size);
  return hash;
}

bool IsDebuggerPresentFast() noexcept {
  if (IsDebuggerPresent()) {
    return true;
  }
  BOOL remote = FALSE;
  if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &remote) && remote) {
    return true;
  }
  return false;
}

bool IsDebuggerPresentNt() noexcept {
  const auto ntdll = GetModuleHandleW(L"ntdll.dll");
  if (!ntdll) {
    return false;
  }
  const auto query =
      reinterpret_cast<NtQueryInformationProcessFn>(
          GetProcAddress(ntdll, "NtQueryInformationProcess"));
  if (!query) {
    return false;
  }

  ULONG_PTR debug_port = 0;
  if (query(GetCurrentProcess(), kProcessDebugPort, &debug_port,
            sizeof(debug_port), nullptr) >= 0 &&
      debug_port != 0) {
    return true;
  }

  ULONG debug_flags = 0;
  if (query(GetCurrentProcess(), kProcessDebugFlags, &debug_flags,
            sizeof(debug_flags), nullptr) >= 0 &&
      debug_flags == 0) {
    return true;
  }

  HANDLE debug_object = nullptr;
  if (query(GetCurrentProcess(), kProcessDebugObjectHandle, &debug_object,
            sizeof(debug_object), nullptr) >= 0 &&
      debug_object != nullptr) {
    return true;
  }

  return false;
}

TamperSignal NativeUnavailableSignal() noexcept {
#if defined(MI_E2EE_SECURE_RELEASE)
  return TamperSignal::kApiHook;
#else
  return TamperSignal::kNone;
#endif
}

bool NativeDebuggerConflict(
    bool win32_debugger,
    const mi::platform::win::NativeProbeResult& native) noexcept {
  return native.native_query_succeeded &&
         native.debugger_present != win32_debugger;
}

TamperSignal EvaluateNativeProbe(
    bool win32_debugger,
    const mi::platform::win::NativeProbeResult& native) noexcept {
  if (native.signal != TamperSignal::kNone) {
    return native.signal;
  }
  if (win32_debugger) {
    return TamperSignal::kDebugger;
  }
  if (NativeDebuggerConflict(win32_debugger, native)) {
    return native.debugger_present ? TamperSignal::kDebugger
                                   : TamperSignal::kApiHook;
  }
  if (!native.available || !native.ntdll_stubs_verified) {
    return NativeUnavailableSignal();
  }
  return TamperSignal::kNone;
}

bool HasHardwareBreakpoints() noexcept {
  const DWORD pid = GetCurrentProcessId();
  const DWORD self_tid = GetCurrentThreadId();
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return false;
  }
  THREADENTRY32 entry{};
  entry.dwSize = sizeof(entry);
  bool hit = false;
  if (Thread32First(snapshot, &entry)) {
    do {
      if (entry.th32OwnerProcessID != pid ||
          entry.th32ThreadID == self_tid) {
        entry.dwSize = sizeof(entry);
        continue;
      }
      HANDLE thread = OpenThread(
          THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME |
              THREAD_QUERY_INFORMATION,
          FALSE, entry.th32ThreadID);
      if (!thread) {
        entry.dwSize = sizeof(entry);
        continue;
      }
      const DWORD suspend = SuspendThread(thread);
      if (suspend != static_cast<DWORD>(-1)) {
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (GetThreadContext(thread, &ctx)) {
          if (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3 || ctx.Dr7) {
            hit = true;
          }
        }
        ResumeThread(thread);
      }
      CloseHandle(thread);
      if (hit) {
        break;
      }
      entry.dwSize = sizeof(entry);
    } while (Thread32Next(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return hit;
}

void ScanThreadMain(TextRegion region,
                    std::array<std::uint8_t, 32> baseline) noexcept {
  for (;;) {
    const auto now = HashText(region);
    if (now != baseline) {
      ReportTamper(TamperSignal::kCodeTamper);
      return;
    }
    Sleep(1000);
  }
}

void MonitorThreadMain(HardeningLevel level, std::uint32_t poll_ms) noexcept {
  if (level == HardeningLevel::kOff || level == HardeningLevel::kLow) {
    return;
  }
  const bool check_hw_breakpoints = (level == HardeningLevel::kHigh);
  const bool hw_break = check_hw_breakpoints && HasHardwareBreakpoints();
  const bool win32_debugger = IsDebuggerPresentFast() || IsDebuggerPresentNt();
  const auto native_probe = mi::platform::win::RunNativeSyscallProbe();
  const auto native_signal = EvaluateNativeProbe(win32_debugger, native_probe);
  if (native_signal != TamperSignal::kNone || hw_break) {
    ReportTamper(hw_break ? TamperSignal::kHardwareBreakpoint
                          : native_signal);
    return;
  }
  std::uint32_t tick = 0;
  for (;;) {
    ApplyBestEffortMitigations(level);
    const bool loop_win32_debugger =
        IsDebuggerPresentFast() || IsDebuggerPresentNt();
    const auto loop_native_probe = mi::platform::win::RunNativeSyscallProbe();
    const auto loop_native_signal =
        EvaluateNativeProbe(loop_win32_debugger, loop_native_probe);
    if (loop_native_signal != TamperSignal::kNone) {
      ReportTamper(loop_native_signal);
      return;
    }
    if (check_hw_breakpoints && (++tick % 3u) == 0u) {
      if (HasHardwareBreakpoints()) {
        ReportTamper(TamperSignal::kHardwareBreakpoint);
        return;
      }
    }
    Sleep(static_cast<DWORD>(poll_ms));
  }
}

void StartThreadsBestEffort(HardeningLevel level) noexcept {
  if (level == HardeningLevel::kOff) {
    return;
  }
  TextRegion region{};
  if (!GetMainModuleTextRegion(region)) {
    return;
  }
  const auto baseline = HashText(region);

  try {
    if (level == HardeningLevel::kHigh) {
      std::thread(ScanThreadMain, region, baseline).detach();
    }
    if (level >= HardeningLevel::kMedium) {
      std::thread(MonitorThreadMain, level, ParseHardeningPollMs()).detach();
    }
  } catch (...) {
  }
}

HardeningLevel ParseHardeningLevel() noexcept {
#if defined(MI_E2EE_SECURE_RELEASE)
  return HardeningLevel::kHigh;
#else
  const char* env = std::getenv("MI_E2EE_HARDENING");
  if (!env || *env == '\0') {
    env = std::getenv("MI_E2EE_HARDENING_LEVEL");
  }
  if (!env || *env == '\0') {
    return HardeningLevel::kHigh;
  }
  std::string v(env);
  for (auto& ch : v) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  if (v == "0" || v == "off" || v == "false" || v == "disable") {
    return HardeningLevel::kOff;
  }
  if (v == "1" || v == "low") {
    return HardeningLevel::kLow;
  }
  if (v == "2" || v == "medium" || v == "med") {
    return HardeningLevel::kMedium;
  }
  if (v == "3" || v == "high" || v == "on" || v == "true") {
    return HardeningLevel::kHigh;
  }
  return HardeningLevel::kHigh;
#endif
}

std::uint32_t ParseHardeningPollMs() noexcept {
  const char* env = std::getenv("MI_E2EE_HARDENING_POLL_MS");
  if (!env || *env == '\0') {
    env = std::getenv("MI_E2EE_HARDENING_INTERVAL_MS");
  }
  if (!env || *env == '\0') {
    return 5000;
  }
  char* end = nullptr;
  long value = std::strtol(env, &end, 10);
  if (end == env || value <= 0) {
    return 5000;
  }
  if (value < 500) {
    value = 500;
  }
  if (value > 600000) {
    value = 600000;
  }
  return static_cast<std::uint32_t>(value);
}

}  // namespace

void StartEndpointHardening() noexcept {
  if (gStarted.exchange(true)) {
    return;
  }
  const auto level = ParseHardeningLevel();
  gLevel.store(level);
  ApplyBestEffortMitigations(level);
  StartThreadsBestEffort(level);
}

void SetTamperHandler(TamperHandler handler) noexcept {
  gTamperHandler.store(handler);
}

bool IsTamperDetected() noexcept {
  return gTamperDetected.load();
}

TamperSignal LastTamperSignal() noexcept {
  return gLastTamper.load();
}

bool CanRevealUiPlaintext() noexcept {
  if (gLevel.load() == HardeningLevel::kOff ||
      ParseHardeningLevel() == HardeningLevel::kOff) {
    return true;
  }
  if (IsTamperDetected()) {
    return false;
  }
  const bool win32_debugger = IsDebuggerPresentFast() || IsDebuggerPresentNt();
  const auto native_probe = mi::platform::win::RunNativeSyscallProbe();
  const auto native_signal = EvaluateNativeProbe(win32_debugger, native_probe);
  if (native_signal != TamperSignal::kNone) {
    ReportTamper(native_signal);
    return false;
  }
  if (HasHardwareBreakpoints()) {
    ReportTamper(TamperSignal::kHardwareBreakpoint);
    return false;
  }
  return !IsTamperDetected();
}

}  // namespace mi::platform
