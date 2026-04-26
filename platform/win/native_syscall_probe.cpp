#include "native_syscall_probe.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

namespace mi::platform::win {
namespace {

using NtQueryInformationProcessFn =
    LONG(WINAPI*)(HANDLE, int, PVOID, ULONG, PULONG);
using NtQueryVirtualMemoryFn =
    LONG(WINAPI*)(HANDLE, PVOID, int, PVOID, SIZE_T, PSIZE_T);
using NtQuerySystemInformationFn = LONG(WINAPI*)(int, PVOID, ULONG, PULONG);
using NtQueryInformationThreadFn =
    LONG(WINAPI*)(HANDLE, int, PVOID, ULONG, PULONG);
using NtSetInformationThreadFn = LONG(WINAPI*)(HANDLE, int, PVOID, ULONG);

constexpr int kProcessDebugPort = 7;
constexpr int kProcessDebugObjectHandle = 0x1e;
constexpr int kProcessDebugFlags = 0x1f;
constexpr int kMemoryBasicInformation = 0;
constexpr std::size_t kStubCompareBytes = 16;

struct NativeApi {
  NtQueryInformationProcessFn NtQueryInformationProcess{nullptr};
  NtQueryVirtualMemoryFn NtQueryVirtualMemory{nullptr};
  NtQuerySystemInformationFn NtQuerySystemInformation{nullptr};
  NtQueryInformationThreadFn NtQueryInformationThread{nullptr};
  NtSetInformationThreadFn NtSetInformationThread{nullptr};
};

struct ImageRange {
  std::uintptr_t begin{0};
  std::uintptr_t end{0};
};

struct MappedImage {
  HANDLE file{INVALID_HANDLE_VALUE};
  HANDLE mapping{nullptr};
  const std::uint8_t* view{nullptr};

  ~MappedImage() {
    if (view) {
      UnmapViewOfFile(view);
    }
    if (mapping) {
      CloseHandle(mapping);
    }
    if (file != INVALID_HANDLE_VALUE) {
      CloseHandle(file);
    }
  }

  MappedImage() = default;
  MappedImage(const MappedImage&) = delete;
  MappedImage& operator=(const MappedImage&) = delete;
};

bool IsNtSuccess(LONG status) noexcept {
  return status >= 0;
}

bool IsExecutableProtect(DWORD protect) noexcept {
  if ((protect & PAGE_GUARD) || (protect & PAGE_NOACCESS)) {
    return false;
  }
  const DWORD base = protect & 0xffu;
  return base == PAGE_EXECUTE || base == PAGE_EXECUTE_READ ||
         base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

bool AddressInRange(std::uintptr_t address, ImageRange range) noexcept {
  return range.begin != 0 && address >= range.begin && address < range.end;
}

bool ReadableMemory(const void* address, std::size_t size) noexcept {
  MEMORY_BASIC_INFORMATION mbi{};
  if (!VirtualQuery(address, &mbi, sizeof(mbi))) {
    return false;
  }
  if (mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD) ||
      (mbi.Protect & PAGE_NOACCESS)) {
    return false;
  }
  const auto begin = reinterpret_cast<std::uintptr_t>(address);
  const auto end = begin + size;
  const auto region_begin = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
  const auto region_end = region_begin + mbi.RegionSize;
  return end >= begin && begin >= region_begin && end <= region_end;
}

bool PrivateExecutableAddress(std::uintptr_t address) noexcept {
  MEMORY_BASIC_INFORMATION mbi{};
  if (!VirtualQuery(reinterpret_cast<const void*>(address), &mbi,
                    sizeof(mbi))) {
    return false;
  }
  return mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE &&
         IsExecutableProtect(mbi.Protect);
}

bool GetImageHeaders(const std::uint8_t* base,
                     const IMAGE_NT_HEADERS*& nt) noexcept {
  if (!base) {
    return false;
  }
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
    return false;
  }
  nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
      base + static_cast<std::size_t>(dos->e_lfanew));
  return nt->Signature == IMAGE_NT_SIGNATURE;
}

bool FindTextRange(const std::uint8_t* image, ImageRange& out) noexcept {
  const IMAGE_NT_HEADERS* nt = nullptr;
  if (!GetImageHeaders(image, nt)) {
    return false;
  }
  const auto* section = IMAGE_FIRST_SECTION(nt);
  for (std::uint16_t i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
    if (std::memcmp(section[i].Name, ".text", 5) != 0) {
      continue;
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(
        image + static_cast<std::size_t>(section[i].VirtualAddress));
    const auto size = std::max<std::uint32_t>(
        section[i].Misc.VirtualSize, section[i].SizeOfRawData);
    if (begin == 0 || size == 0) {
      return false;
    }
    out.begin = begin;
    out.end = begin + size;
    return out.end > out.begin;
  }
  return false;
}

void* FindImageExport(const std::uint8_t* image, const char* name) noexcept {
  const IMAGE_NT_HEADERS* nt = nullptr;
  if (!GetImageHeaders(image, nt) || !name) {
    return nullptr;
  }
  const auto& dir =
      nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if (dir.VirtualAddress == 0 || dir.Size == 0) {
    return nullptr;
  }
  const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
      image + static_cast<std::size_t>(dir.VirtualAddress));
  const auto* names = reinterpret_cast<const DWORD*>(
      image + static_cast<std::size_t>(exports->AddressOfNames));
  const auto* ordinals = reinterpret_cast<const WORD*>(
      image + static_cast<std::size_t>(exports->AddressOfNameOrdinals));
  const auto* functions = reinterpret_cast<const DWORD*>(
      image + static_cast<std::size_t>(exports->AddressOfFunctions));
  for (DWORD i = 0; i < exports->NumberOfNames; ++i) {
    const auto* export_name = reinterpret_cast<const char*>(
        image + static_cast<std::size_t>(names[i]));
    if (std::strcmp(export_name, name) != 0) {
      continue;
    }
    const DWORD rva = functions[ordinals[i]];
    if (rva >= dir.VirtualAddress && rva < dir.VirtualAddress + dir.Size) {
      return nullptr;
    }
    return const_cast<std::uint8_t*>(image + static_cast<std::size_t>(rva));
  }
  return nullptr;
}

bool MapCleanNtdllImage(MappedImage& out) noexcept {
  wchar_t system_dir[MAX_PATH]{};
  const UINT len = GetSystemDirectoryW(system_dir, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return false;
  }
  std::wstring path(system_dir, system_dir + len);
  if (!path.empty() && path.back() != L'\\') {
    path.push_back(L'\\');
  }
  path += L"ntdll.dll";

  out.file = CreateFileW(path.c_str(), GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (out.file == INVALID_HANDLE_VALUE) {
    return false;
  }
  out.mapping =
      CreateFileMappingW(out.file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0,
                         nullptr);
  if (!out.mapping) {
    return false;
  }
  out.view = static_cast<const std::uint8_t*>(
      MapViewOfFile(out.mapping, FILE_MAP_READ, 0, 0, 0));
  return out.view != nullptr;
}

bool DecodeInlineBranch(const std::uint8_t* stub,
                        std::uintptr_t address,
                        std::uintptr_t& target) noexcept {
  target = 0;
  if (!stub) {
    return false;
  }
  if (stub[0] == 0xE9 || stub[0] == 0xE8) {
    std::int32_t rel = 0;
    std::memcpy(&rel, stub + 1, sizeof(rel));
    target = static_cast<std::uintptr_t>(
        static_cast<std::intptr_t>(address + 5) + rel);
    return true;
  }
  if (stub[0] == 0xEB) {
    const auto rel = static_cast<std::int8_t>(stub[1]);
    target = static_cast<std::uintptr_t>(
        static_cast<std::intptr_t>(address + 2) + rel);
    return true;
  }
#if defined(_M_X64) || defined(__x86_64__)
  if (stub[0] == 0xFF && stub[1] == 0x25) {
    std::int32_t rel = 0;
    std::memcpy(&rel, stub + 2, sizeof(rel));
    const auto slot = static_cast<std::uintptr_t>(
        static_cast<std::intptr_t>(address + 6) + rel);
    if (!ReadableMemory(reinterpret_cast<const void*>(slot),
                        sizeof(std::uintptr_t))) {
      return true;
    }
    std::memcpy(&target, reinterpret_cast<const void*>(slot), sizeof(target));
    return true;
  }
  if (stub[0] == 0x48 && stub[1] == 0xB8 && stub[10] == 0xFF &&
      stub[11] == 0xE0) {
    std::memcpy(&target, stub + 2, sizeof(target));
    return true;
  }
#else
  if (stub[0] == 0xB8 && stub[5] == 0xFF && stub[6] == 0xE0) {
    std::uint32_t absolute = 0;
    std::memcpy(&absolute, stub + 1, sizeof(absolute));
    target = absolute;
    return true;
  }
  if (stub[0] == 0x68 && stub[5] == 0xC3) {
    std::uint32_t absolute = 0;
    std::memcpy(&absolute, stub + 1, sizeof(absolute));
    target = absolute;
    return true;
  }
#endif
  return false;
}

TamperSignal CompareStubWithCleanImage(const std::uint8_t* current,
                                       const std::uint8_t* clean,
                                       ImageRange ntdll_text) noexcept {
  const auto address = reinterpret_cast<std::uintptr_t>(current);
  if (!AddressInRange(address, ntdll_text)) {
    return TamperSignal::kApiHook;
  }

  std::uintptr_t target = 0;
  if (DecodeInlineBranch(current, address, target)) {
    if (!AddressInRange(target, ntdll_text) || PrivateExecutableAddress(target)) {
      return TamperSignal::kApiHook;
    }
    return TamperSignal::kNone;
  }

  if (!clean) {
    return TamperSignal::kNone;
  }
  if (std::memcmp(current, clean, kStubCompareBytes) != 0) {
    return TamperSignal::kApiHook;
  }
  return TamperSignal::kNone;
}

struct ResolveResult {
  bool available{false};
  bool verified{false};
  TamperSignal signal{TamperSignal::kNone};
};

ResolveResult ResolveAndVerifyNativeApi(NativeApi& api) noexcept {
  ResolveResult result;
  const auto ntdll = GetModuleHandleW(L"ntdll.dll");
  if (!ntdll) {
    return result;
  }

  const auto* loaded = reinterpret_cast<const std::uint8_t*>(ntdll);
  ImageRange loaded_text{};
  if (!FindTextRange(loaded, loaded_text)) {
    result.signal = TamperSignal::kApiHook;
    return result;
  }

  MappedImage clean_image;
  if (!MapCleanNtdllImage(clean_image)) {
    return result;
  }

  ImageRange clean_text{};
  if (!FindTextRange(clean_image.view, clean_text) || clean_text.begin == 0) {
    return result;
  }

  std::array<void*, 5> exports{};
  std::size_t index = 0;
  for (const char* name : kNativeExportWhitelist) {
    if (!AllowedNativeExport(name)) {
      result.signal = TamperSignal::kApiHook;
      return result;
    }
    auto* current = static_cast<std::uint8_t*>(
        FindImageExport(loaded, name));
    auto* clean = static_cast<std::uint8_t*>(
        FindImageExport(clean_image.view, name));
    if (!current || !clean) {
      return result;
    }
    result.signal = CompareStubWithCleanImage(current, clean, loaded_text);
    if (result.signal != TamperSignal::kNone) {
      return result;
    }
    exports[index++] = current;
  }

  api.NtQueryInformationProcess =
      reinterpret_cast<NtQueryInformationProcessFn>(exports[0]);
  api.NtQueryVirtualMemory =
      reinterpret_cast<NtQueryVirtualMemoryFn>(exports[1]);
  api.NtQuerySystemInformation =
      reinterpret_cast<NtQuerySystemInformationFn>(exports[2]);
  api.NtQueryInformationThread =
      reinterpret_cast<NtQueryInformationThreadFn>(exports[3]);
  api.NtSetInformationThread =
      reinterpret_cast<NtSetInformationThreadFn>(exports[4]);
  result.available = api.NtQueryInformationProcess &&
                     api.NtQueryVirtualMemory &&
                     api.NtQuerySystemInformation &&
                     api.NtQueryInformationThread &&
                     api.NtSetInformationThread;
  result.verified = result.available;
  return result;
}

ResolveResult NtdllIntegrityCheck(NativeApi& api) noexcept {
  return ResolveAndVerifyNativeApi(api);
}

bool QueryDebuggerWithNative(const NativeApi& api,
                             bool& debugger_present) noexcept {
  debugger_present = false;
  if (!api.NtQueryInformationProcess) {
    return false;
  }

  bool any_success = false;
  ULONG_PTR debug_port = 0;
  if (IsNtSuccess(api.NtQueryInformationProcess(
          GetCurrentProcess(), kProcessDebugPort, &debug_port,
          sizeof(debug_port), nullptr))) {
    any_success = true;
    if (debug_port != 0) {
      debugger_present = true;
    }
  }

  ULONG debug_flags = 0;
  if (IsNtSuccess(api.NtQueryInformationProcess(
          GetCurrentProcess(), kProcessDebugFlags, &debug_flags,
          sizeof(debug_flags), nullptr))) {
    any_success = true;
    if (debug_flags == 0) {
      debugger_present = true;
    }
  }

  HANDLE debug_object = nullptr;
  if (IsNtSuccess(api.NtQueryInformationProcess(
          GetCurrentProcess(), kProcessDebugObjectHandle, &debug_object,
          sizeof(debug_object), nullptr))) {
    any_success = true;
    if (debug_object) {
      debugger_present = true;
      CloseHandle(debug_object);
    }
  }
  return any_success;
}

std::wstring LowerPath(std::wstring value) {
  std::replace(value.begin(), value.end(), L'/', L'\\');
  for (auto& ch : value) {
    ch = static_cast<wchar_t>(std::towlower(ch));
  }
  while (!value.empty() && value.back() == L'\\') {
    value.pop_back();
  }
  return value;
}

bool PathSameOrChild(const std::wstring& root,
                     const std::wstring& path) noexcept {
  if (root.empty() || path.empty()) {
    return false;
  }
  if (path == root) {
    return true;
  }
  if (path.size() <= root.size() || path.compare(0, root.size(), root) != 0) {
    return false;
  }
  return path[root.size()] == L'\\';
}

std::wstring DirectoryOfModule(HMODULE module) {
  std::vector<wchar_t> buffer(MAX_PATH);
  DWORD len = 0;
  for (;;) {
    len = GetModuleFileNameW(module, buffer.data(),
                             static_cast<DWORD>(buffer.size()));
    if (len == 0) {
      return {};
    }
    if (len + 1 < buffer.size()) {
      break;
    }
    buffer.resize(buffer.size() * 2);
  }
  std::wstring path(buffer.data(), buffer.data() + len);
  const auto pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos) {
    return {};
  }
  return LowerPath(path.substr(0, pos));
}

std::vector<std::wstring> AllowedModuleRoots() {
  std::vector<std::wstring> roots;
  roots.push_back(DirectoryOfModule(nullptr));

  wchar_t system_dir[MAX_PATH]{};
  const UINT system_len = GetSystemDirectoryW(system_dir, MAX_PATH);
  if (system_len > 0 && system_len < MAX_PATH) {
    roots.emplace_back(LowerPath(std::wstring(system_dir, system_len)));
  }

  wchar_t windows_dir[MAX_PATH]{};
  const UINT windows_len = GetWindowsDirectoryW(windows_dir, MAX_PATH);
  if (windows_len > 0 && windows_len < MAX_PATH) {
    std::wstring winsxs(windows_dir, windows_dir + windows_len);
    if (!winsxs.empty() && winsxs.back() != L'\\') {
      winsxs.push_back(L'\\');
    }
    winsxs += L"WinSxS";
    roots.emplace_back(LowerPath(winsxs));
  }
  return roots;
}

TamperSignal ScanUnexpectedModules() noexcept {
  const auto roots = AllowedModuleRoots();
  const HANDLE snapshot = CreateToolhelp32Snapshot(
      TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
  if (snapshot == INVALID_HANDLE_VALUE) {
    return TamperSignal::kNone;
  }

  MODULEENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  TamperSignal signal = TamperSignal::kNone;
  if (Module32FirstW(snapshot, &entry)) {
    do {
      const std::wstring path = LowerPath(entry.szExePath);
      bool allowed = false;
      for (const auto& root : roots) {
        if (PathSameOrChild(root, path)) {
          allowed = true;
          break;
        }
      }
      if (!allowed) {
        signal = TamperSignal::kUnexpectedModule;
        break;
      }
      entry.dwSize = sizeof(entry);
    } while (Module32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return signal;
}

TamperSignal ScanPrivateExecutableMemory(const NativeApi& api) noexcept {
  if (!api.NtQueryVirtualMemory) {
    return TamperSignal::kNone;
  }
  SYSTEM_INFO si{};
  GetSystemInfo(&si);
  auto address = reinterpret_cast<std::uintptr_t>(si.lpMinimumApplicationAddress);
  const auto max_address =
      reinterpret_cast<std::uintptr_t>(si.lpMaximumApplicationAddress);
  const std::uintptr_t page_size =
      std::max<std::uintptr_t>(si.dwPageSize, 0x1000u);

  while (address < max_address) {
    MEMORY_BASIC_INFORMATION mbi{};
    SIZE_T returned = 0;
    const LONG status = api.NtQueryVirtualMemory(
        GetCurrentProcess(), reinterpret_cast<PVOID>(address),
        kMemoryBasicInformation, &mbi, sizeof(mbi), &returned);
    if (!IsNtSuccess(status) || mbi.RegionSize == 0) {
      const auto next = address + page_size;
      if (next <= address) {
        break;
      }
      address = next;
      continue;
    }
    if (mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE &&
        IsExecutableProtect(mbi.Protect)) {
      return TamperSignal::kPrivateExecutableMemory;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
    const auto next = base + mbi.RegionSize;
    if (next <= address) {
      break;
    }
    address = next;
  }
  return TamperSignal::kNone;
}

TamperSignal ScanImportAddressTable() noexcept {
  const auto module = GetModuleHandleW(nullptr);
  if (!module) {
    return TamperSignal::kNone;
  }
  const auto* image = reinterpret_cast<const std::uint8_t*>(module);
  const IMAGE_NT_HEADERS* nt = nullptr;
  if (!GetImageHeaders(image, nt)) {
    return TamperSignal::kNone;
  }
  const auto& dir =
      nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (dir.VirtualAddress == 0 || dir.Size == 0) {
    return TamperSignal::kNone;
  }
  const auto* imports = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(
      image + static_cast<std::size_t>(dir.VirtualAddress));
  for (; imports->Name != 0; ++imports) {
    auto* thunk = reinterpret_cast<const IMAGE_THUNK_DATA*>(
        image + static_cast<std::size_t>(imports->FirstThunk));
    if (!thunk) {
      continue;
    }
    for (; thunk->u1.Function != 0; ++thunk) {
      const auto target =
          static_cast<std::uintptr_t>(thunk->u1.Function);
      if (PrivateExecutableAddress(target)) {
        return TamperSignal::kApiHook;
      }
    }
  }
  return TamperSignal::kNone;
}

}  // namespace

bool AllowedNativeExport(const char* name) noexcept {
  if (!name) {
    return false;
  }
  for (const char* allowed : kNativeExportWhitelist) {
    if (std::strcmp(name, allowed) == 0) {
      return true;
    }
  }
  return false;
}

NativeProbeResult RunNativeSyscallProbe() noexcept {
  NativeProbeResult result;
  NativeApi api;
  const ResolveResult resolve = NtdllIntegrityCheck(api);
  result.available = resolve.available;
  result.ntdll_stubs_verified = resolve.verified;
  if (resolve.signal != TamperSignal::kNone) {
    result.signal = resolve.signal;
    return result;
  }
  if (!resolve.available) {
    return result;
  }

  if (!QueryDebuggerWithNative(api, result.debugger_present)) {
    result.available = false;
    return result;
  }
  result.native_query_succeeded = true;

  result.signal = ScanUnexpectedModules();
  if (result.signal != TamperSignal::kNone) {
    return result;
  }

  result.signal = ScanPrivateExecutableMemory(api);
  if (result.signal != TamperSignal::kNone) {
    return result;
  }

  result.signal = ScanImportAddressTable();
  return result;
}

}  // namespace mi::platform::win
