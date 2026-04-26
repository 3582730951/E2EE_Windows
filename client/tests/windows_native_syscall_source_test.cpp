#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool ReadFile(const std::filesystem::path& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  out.assign(std::istreambuf_iterator<char>(in),
             std::istreambuf_iterator<char>());
  return true;
}

bool Contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

bool RequireContains(const std::string& body,
                     const char* needle,
                     const char* label) {
  if (Contains(body, needle)) {
    return true;
  }
  std::cerr << label << " missing required text: " << needle << "\n";
  return false;
}

bool RequireNotContains(const std::string& body,
                        const char* needle,
                        const char* label) {
  if (!Contains(body, needle)) {
    return true;
  }
  std::cerr << label << " contains forbidden text: " << needle << "\n";
  return false;
}

}  // namespace

int main() {
  const std::filesystem::path root = MI_E2EE_REPOSITORY_ROOT;
  const auto platform_security = root / "platform/include/platform_security.h";
  const auto native_probe_h = root / "platform/win/native_syscall_probe.h";
  const auto native_probe_cpp = root / "platform/win/native_syscall_probe.cpp";
  const auto platform_security_win =
      root / "platform/win/platform_security_win.cpp";
  const auto protected_text_vm =
      root / "runtime/client/security/ui_protected_text_vm.cpp";
  const auto platform_media_win = root / "platform/win/platform_media_win.cpp";

  std::string security_h;
  std::string probe_h;
  std::string probe_cpp;
  std::string security_win;
  std::string vm;
  std::string media_win;
  if (!ReadFile(platform_security, security_h) ||
      !ReadFile(native_probe_h, probe_h) ||
      !ReadFile(native_probe_cpp, probe_cpp) ||
      !ReadFile(platform_security_win, security_win) ||
      !ReadFile(protected_text_vm, vm) ||
      !ReadFile(platform_media_win, media_win)) {
    std::cerr << "missing Windows native syscall hardening source\n";
    return 1;
  }

  bool ok = true;
  ok &= RequireContains(security_h, "kApiHook", "platform_security.h");
  ok &= RequireContains(security_h, "kUnexpectedModule", "platform_security.h");
  ok &= RequireContains(security_h, "kPrivateExecutableMemory",
                        "platform_security.h");
  ok &= RequireContains(security_h, "CanRevealUiPlaintext()",
                        "platform_security.h");

  for (const char* name : {"NtQueryInformationProcess",
                           "NtQueryVirtualMemory",
                           "NtQuerySystemInformation",
                           "NtQueryInformationThread",
                           "NtSetInformationThread"}) {
    ok &= RequireContains(probe_h, name, "native_syscall_probe.h");
    ok &= RequireContains(probe_cpp, name, "native_syscall_probe.cpp");
  }
  ok &= RequireContains(probe_cpp, "AllowedNativeExport",
                        "native_syscall_probe.cpp");
  ok &= RequireContains(probe_cpp, "\".text\"", "native_syscall_probe.cpp");
  ok &= RequireContains(probe_cpp, "MapCleanNtdllImage",
                        "native_syscall_probe.cpp");
  ok &= RequireContains(probe_cpp, "NtdllIntegrityCheck",
                        "native_syscall_probe.cpp");
  ok &= RequireContains(probe_cpp, "ScanPrivateExecutableMemory",
                        "native_syscall_probe.cpp");
  ok &= RequireContains(probe_cpp, "ScanUnexpectedModules",
                        "native_syscall_probe.cpp");
  ok &= RequireContains(probe_cpp, "ScanImportAddressTable",
                        "native_syscall_probe.cpp");
  ok &= RequireContains(probe_cpp, "TamperSignal::kApiHook",
                        "native_syscall_probe.cpp");
  ok &= RequireContains(probe_cpp, "TamperSignal::kUnexpectedModule",
                        "native_syscall_probe.cpp");
  ok &= RequireContains(probe_cpp, "TamperSignal::kPrivateExecutableMemory",
                        "native_syscall_probe.cpp");
  ok &= RequireNotContains(probe_cpp, "SyscallNumber",
                           "native_syscall_probe.cpp");
  ok &= RequireNotContains(probe_cpp, "call arbitrary",
                           "native_syscall_probe.cpp");

  ok &= RequireContains(security_win, "#include \"native_syscall_probe.h\"",
                        "platform_security_win.cpp");
  ok &= RequireContains(security_win, "RunNativeSyscallProbe()",
                        "platform_security_win.cpp");
  ok &= RequireContains(security_win, "NativeDebuggerConflict",
                        "platform_security_win.cpp");
  ok &= RequireContains(security_win, "gLevel.load() == HardeningLevel::kOff",
                        "platform_security_win.cpp");
  ok &= RequireContains(security_win, "CanRevealUiPlaintext()",
                        "platform_security_win.cpp");

  ok &= RequireContains(vm, "#include \"platform_security.h\"",
                        "ui_protected_text_vm.cpp");
  ok &= RequireContains(vm, "mi::platform::CanRevealUiPlaintext()",
                        "ui_protected_text_vm.cpp");
  ok &= RequireNotContains(media_win, "LoadLibraryW(name)",
                           "platform_media_win.cpp");

  return ok ? 0 : 1;
}
