#include "platform_security.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
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

void ApplyBestEffortMitigations() noexcept {
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

void TerminateFailClosed(std::uint32_t code) noexcept {
  TerminateProcess(GetCurrentProcess(), static_cast<UINT>(code));
}

void ScanThreadMain(TextRegion region,
                    std::array<std::uint8_t, 32> baseline) noexcept {
  for (;;) {
    const auto now = HashText(region);
    if (now != baseline) {
      TerminateFailClosed(0xE2EE0001u);
    }
    Sleep(1000);
  }
}

void MonitorThreadMain() noexcept {
  if (IsDebuggerPresentFast() || IsDebuggerPresentNt() ||
      HasHardwareBreakpoints()) {
    TerminateFailClosed(0xE2EE0002u);
  }
  std::uint32_t tick = 0;
  for (;;) {
    ApplyBestEffortMitigations();
    if (IsDebuggerPresentFast() || IsDebuggerPresentNt()) {
      TerminateFailClosed(0xE2EE0002u);
    }
    if ((++tick % 3u) == 0u) {
      if (HasHardwareBreakpoints()) {
        TerminateFailClosed(0xE2EE0003u);
      }
    }
    Sleep(5000);
  }
}

void StartThreadsBestEffort() noexcept {
  TextRegion region{};
  if (!GetMainModuleTextRegion(region)) {
    return;
  }
  const auto baseline = HashText(region);

  try {
    std::thread(ScanThreadMain, region, baseline).detach();
    std::thread(MonitorThreadMain).detach();
  } catch (...) {
  }
}

}  // namespace

void StartEndpointHardening() noexcept {
  if (gStarted.exchange(true)) {
    return;
  }
  ApplyBestEffortMitigations();
  StartThreadsBestEffort();
}

}  // namespace mi::platform
