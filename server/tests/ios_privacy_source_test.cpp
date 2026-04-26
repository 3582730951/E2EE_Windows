#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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

bool IsIdentifierChar(char ch) {
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
         (ch >= '0' && ch <= '9') || ch == '_';
}

bool ContainsStandaloneCall(const std::string& haystack,
                            const std::string& call) {
  std::size_t pos = 0;
  while ((pos = haystack.find(call, pos)) != std::string::npos) {
    if (pos == 0 || !IsIdentifierChar(haystack[pos - 1])) {
      return true;
    }
    pos += call.size();
  }
  return false;
}

bool EndsWith(const std::string& value, const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

bool IsScannedSource(const std::filesystem::path& path) {
  const auto ext = path.extension().string();
  return ext == ".swift" || ext == ".m" || ext == ".mm";
}

bool CheckSecureClipboard(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing iOS secure clipboard source: " << path << "\n";
    return false;
  }
  const bool ok = Contains(body, "zerox://clip/") &&
                  Contains(body, "UIPasteboard.general.setItems") &&
                  Contains(body, "expirationDate") &&
                  Contains(body, "SecRandomCopyBytes");
  if (!ok) {
    std::cerr << "iOS SecureClipboard lacks tokenized expiring pasteboard design: "
              << path << "\n";
  }
  return ok;
}

bool CheckNoDirectPasteboardWrites(const std::filesystem::path& root) {
  std::error_code ec;
  for (std::filesystem::recursive_directory_iterator it(root, ec), end; it != end;
       it.increment(ec)) {
    if (ec) {
      std::cerr << "failed to iterate iOS source: " << ec.message() << "\n";
      return false;
    }
    if (!it->is_regular_file(ec) || !IsScannedSource(it->path())) {
      continue;
    }
    const std::string path = it->path().generic_string();
    if (EndsWith(path, "/SecureClipboard.swift")) {
      continue;
    }
    std::string body;
    if (!ReadFile(it->path(), body)) {
      std::cerr << "failed to read iOS source: " << path << "\n";
      return false;
    }
    if (Contains(body, "UIPasteboard.general.string") ||
        Contains(body, "UIPasteboard.general.setItems") ||
        Contains(body, "UIPasteboard.general.setValue")) {
      std::cerr << "direct iOS system pasteboard write outside SecureClipboard: "
                << path << "\n";
      return false;
    }
  }
  return true;
}

bool CheckNoReleaseLoggingOrTelemetry(const std::filesystem::path& root) {
  const std::vector<std::string> forbidden_calls = {
      "NSLog(",
      "debugPrint(",
      "print(",
      "os_log",
      "Logger("};
  const std::vector<std::string> forbidden_markers = {
      "Crashlytics",
      "Firebase",
      "Sentry",
      "Bugsnag",
      "Datadog",
      "NewRelic",
      "analytics",
      "telemetry"};
  std::error_code ec;
  for (std::filesystem::recursive_directory_iterator it(root, ec), end; it != end;
       it.increment(ec)) {
    if (ec) {
      std::cerr << "failed to iterate iOS source: " << ec.message() << "\n";
      return false;
    }
    if (!it->is_regular_file(ec) || !IsScannedSource(it->path())) {
      continue;
    }
    std::string body;
    if (!ReadFile(it->path(), body)) {
      std::cerr << "failed to read iOS source: "
                << it->path().generic_string() << "\n";
      return false;
    }
    for (const auto& needle : forbidden_calls) {
      if (ContainsStandaloneCall(body, needle)) {
        std::cerr << "iOS source contains logging marker " << needle << ": "
                  << it->path().generic_string() << "\n";
        return false;
      }
    }
    for (const auto& needle : forbidden_markers) {
      if (Contains(body, needle)) {
        std::cerr << "iOS source contains telemetry marker " << needle
                  << ": " << it->path().generic_string() << "\n";
        return false;
      }
    }
  }
  return true;
}

bool CheckEndpointThreatDetector(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing iOS endpoint threat detector source: " << path
              << "\n";
    return false;
  }
  const bool ok = Contains(body, "sysctl") &&
                  Contains(body, "KERN_PROC_PID") &&
                  Contains(body, "P_TRACED") &&
                  Contains(body, "_dyld_image_count") &&
                  Contains(body, "_dyld_get_image_name") &&
                  Contains(body, "frida") &&
                  Contains(body, "substrate") &&
                  Contains(body, "libhooker") &&
                  Contains(body, "substitute");
  if (!ok) {
    std::cerr << "iOS endpoint detector lacks debugger/dylib injection checks: "
              << path << "\n";
  }
  return ok;
}

bool CheckAppThreatGuard(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing iOS app entry source: " << path << "\n";
    return false;
  }
  const bool ok = Contains(body, "EndpointThreatDetector.evaluate") &&
                  Contains(body, "EndpointThreatBlockedView") &&
                  Contains(body, "SecureClipboard.clear()");
  if (!ok) {
    std::cerr << "iOS app entry is missing fail-closed endpoint threat guard: "
              << path << "\n";
  }
  return ok;
}

bool CheckInfoPlistProtection(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing iOS Info.plist: " << path << "\n";
    return false;
  }
  const bool ok = Contains(body, "NSFileProtectionKey") &&
                  Contains(body, "NSFileProtectionComplete");
  if (!ok) {
    std::cerr << "iOS Info.plist is missing complete file protection default: "
              << path << "\n";
  }
  return ok;
}

bool CheckBootstrapFileProtection(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing iOS bootstrap source: " << path << "\n";
    return false;
  }
  const bool ok = Contains(body, "FileAttributeKey.protectionKey") &&
                  Contains(body, "FileProtectionType.complete") &&
                  Contains(body, ".completeFileProtection");
  if (!ok) {
    std::cerr << "iOS config/data bootstrap lacks complete file protection: "
              << path << "\n";
  }
  return ok;
}

}  // namespace

int main() {
  const std::filesystem::path ios_root = MI_E2EE_IOS_SOURCE_ROOT;
  if (!std::filesystem::is_directory(ios_root)) {
    std::cerr << "iOS source root missing: " << ios_root << "\n";
    return 1;
  }
  if (!CheckSecureClipboard(ios_root / "RootAuthApp/SecureClipboard.swift")) {
    return 1;
  }
  if (!CheckNoDirectPasteboardWrites(ios_root)) {
    return 1;
  }
  if (!CheckNoReleaseLoggingOrTelemetry(ios_root)) {
    return 1;
  }
  if (!CheckEndpointThreatDetector(ios_root /
                                   "RootAuthApp/EndpointThreatDetector.swift")) {
    return 1;
  }
  if (!CheckAppThreatGuard(ios_root / "RootAuthApp/RootAuthAppApp.swift")) {
    return 1;
  }
  if (!CheckInfoPlistProtection(ios_root / "RootAuthApp/Info.plist")) {
    return 1;
  }
  if (!CheckBootstrapFileProtection(ios_root / "RootAuthApp/AppShell.swift")) {
    return 1;
  }
  return 0;
}
