#include "platform_security.h"

#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <string>

#if defined(__linux__) && !defined(__ANDROID__)
#include <cstdio>
#include <cstring>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
#if defined(MI_E2EE_WITH_SECCOMP)
#include <seccomp.h>
#endif
#endif

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <unistd.h>
#endif

namespace mi::platform {

namespace {

enum class HardeningLevel : std::uint8_t {
  kOff = 0,
  kLow = 1,
  kMedium = 2,
  kHigh = 3
};

HardeningLevel ParseHardeningLevel() noexcept {
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
}

#if defined(__APPLE__)
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

bool IsTracedMac() noexcept {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  struct kinfo_proc info {};
  std::size_t size = sizeof(info);
  if (sysctl(mib, 4, &info, &size, nullptr, 0) != 0) {
    return false;
  }
  return (info.kp_proc.p_flag & P_TRACED) != 0;
}

bool CheckCodeSignature(bool require_signature) noexcept {
  SecCodeRef code = nullptr;
  OSStatus status = SecCodeCopySelf(kSecCSDefaultFlags, &code);
  if (status != errSecSuccess || !code) {
    if (code) {
      CFRelease(code);
    }
    return !require_signature;
  }
  status = SecCodeCheckValidity(code, kSecCSStrictValidate, nullptr);
  CFRelease(code);
  if (status == errSecSuccess) {
    return true;
  }
  if (status == errSecCSUnsigned) {
    return !require_signature;
  }
  return false;
}

bool HasAppSandboxEntitlement() noexcept {
  SecTaskRef task = SecTaskCreateFromSelf(kCFAllocatorDefault);
  if (!task) {
    return false;
  }
  CFTypeRef value = SecTaskCopyValueForEntitlement(
      task, CFSTR("com.apple.security.app-sandbox"), nullptr);
  CFRelease(task);
  if (!value) {
    return false;
  }
  bool enabled = false;
  if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
    enabled = CFBooleanGetValue(static_cast<CFBooleanRef>(value));
  }
  CFRelease(value);
  return enabled;
}

void ApplyAppleIntegrityBestEffort(HardeningLevel level) noexcept {
  if (level < HardeningLevel::kMedium) {
    return;
  }
  const bool require_signature =
      ParseEnvFlag("MI_E2EE_MAC_REQUIRE_SIGNATURE", false);
  const bool require_sandbox =
      ParseEnvFlag("MI_E2EE_MAC_REQUIRE_SANDBOX", false);
  if (level >= HardeningLevel::kHigh) {
    if (!CheckCodeSignature(require_signature)) {
      _exit(0xE2EE0004u);
    }
  } else if (require_signature) {
    if (!CheckCodeSignature(true)) {
      _exit(0xE2EE0004u);
    }
  }
  if (require_sandbox && !HasAppSandboxEntitlement()) {
    _exit(0xE2EE0005u);
  }
}
#endif

#if defined(__linux__) && !defined(__ANDROID__) && defined(MI_E2EE_WITH_SECCOMP)
enum class SeccompMode : std::uint8_t { kOff = 0, kBasic = 1 };

SeccompMode ParseSeccompMode() noexcept {
  const char* env = std::getenv("MI_E2EE_SECCOMP");
  if (!env || *env == '\0') {
    env = std::getenv("MI_E2EE_SECCOMP_MODE");
  }
  if (!env || *env == '\0') {
    return SeccompMode::kOff;
  }
  std::string v(env);
  for (auto& ch : v) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  if (v == "0" || v == "off" || v == "false" || v == "disable") {
    return SeccompMode::kOff;
  }
  if (v == "1" || v == "on" || v == "true" || v == "enable" ||
      v == "basic" || v == "deny") {
    return SeccompMode::kBasic;
  }
  return SeccompMode::kOff;
}
#endif

void ApplyBestEffortMitigations(HardeningLevel level) noexcept {
  if (level == HardeningLevel::kOff) {
    return;
  }
#if defined(__linux__) && !defined(__ANDROID__)
  rlimit lim{};
  lim.rlim_cur = 0;
  lim.rlim_max = 0;
  setrlimit(RLIMIT_CORE, &lim);
  prctl(PR_SET_DUMPABLE, 0);
  if (level >= HardeningLevel::kMedium) {
    prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
#ifdef PR_SET_PTRACER
    prctl(PR_SET_PTRACER, 0);
#endif
  }
#elif defined(__APPLE__)
  rlimit lim{};
  lim.rlim_cur = 0;
  lim.rlim_max = 0;
  setrlimit(RLIMIT_CORE, &lim);
  if (level >= HardeningLevel::kMedium) {
    ptrace(PT_DENY_ATTACH, 0, 0, 0);
  }
#else
  (void)level;
#endif
}

#if defined(__linux__) && !defined(__ANDROID__) && defined(MI_E2EE_WITH_SECCOMP)
void ApplySeccompBestEffort(HardeningLevel level) noexcept {
  if (level < HardeningLevel::kMedium) {
    return;
  }
  if (ParseSeccompMode() == SeccompMode::kOff) {
    return;
  }

  scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
  if (!ctx) {
    return;
  }
  seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(ptrace), 0);
  seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(process_vm_readv), 0);
  seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(process_vm_writev), 0);
  seccomp_load(ctx);
  seccomp_release(ctx);
}
#else
void ApplySeccompBestEffort(HardeningLevel /*level*/) noexcept {}
#endif

#if defined(__linux__) && !defined(__ANDROID__)
bool IsTracedLinux() noexcept {
  FILE* fp = std::fopen("/proc/self/status", "r");
  if (!fp) {
    return false;
  }
  char line[256];
  while (std::fgets(line, sizeof(line), fp)) {
    if (std::strncmp(line, "TracerPid:", 10) != 0) {
      continue;
    }
    const char* p = line + 10;
    while (*p == ' ' || *p == '\t') {
      ++p;
    }
    const long pid = std::strtol(p, nullptr, 10);
    std::fclose(fp);
    return pid > 0;
  }
  std::fclose(fp);
  return false;
}
#endif

}  // namespace

void StartEndpointHardening() noexcept {
  static std::atomic<bool> started{false};
  if (started.exchange(true)) {
    return;
  }
  const auto level = ParseHardeningLevel();
  ApplyBestEffortMitigations(level);
  ApplySeccompBestEffort(level);
#if defined(__APPLE__)
  ApplyAppleIntegrityBestEffort(level);
  if (level == HardeningLevel::kHigh && IsTracedMac()) {
    _exit(0xE2EE0002u);
  }
#endif
#if defined(__linux__) && !defined(__ANDROID__)
  if (level == HardeningLevel::kHigh && IsTracedLinux()) {
    _exit(0xE2EE0002u);
  }
#endif
}

}  // namespace mi::platform
