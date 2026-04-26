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

bool IsSkippedFlutterPath(const std::string& path_text) {
  return path_text.find("/.dart_tool/") != std::string::npos ||
         path_text.find("/build/") != std::string::npos ||
         path_text.find("/.symlinks/") != std::string::npos ||
         path_text.find("/ios/Flutter/") != std::string::npos;
}

std::size_t FindCallOpenParen(const std::string& body,
                              std::size_t token_pos,
                              std::size_t token_size) {
  std::size_t pos = token_pos + token_size;
  while (pos < body.size() &&
         (body[pos] == ' ' || body[pos] == '\t' || body[pos] == '\n' ||
          body[pos] == '\r')) {
    ++pos;
  }
  if (pos < body.size() && body[pos] == '.') {
    ++pos;
    while (pos < body.size() &&
           (IsIdentifierChar(body[pos]) || body[pos] == '.')) {
      ++pos;
    }
    while (pos < body.size() &&
           (body[pos] == ' ' || body[pos] == '\t' || body[pos] == '\n' ||
            body[pos] == '\r')) {
      ++pos;
    }
  }
  return pos < body.size() && body[pos] == '(' ? pos : std::string::npos;
}

bool ExtractBalancedBlock(const std::string& body,
                          std::size_t open_paren,
                          std::string& out) {
  int depth = 0;
  bool in_single = false;
  bool in_double = false;
  bool in_line_comment = false;
  bool in_block_comment = false;
  for (std::size_t i = open_paren; i < body.size(); ++i) {
    const char ch = body[i];
    const char next = i + 1 < body.size() ? body[i + 1] : '\0';
    if (in_line_comment) {
      if (ch == '\n') {
        in_line_comment = false;
      }
      continue;
    }
    if (in_block_comment) {
      if (ch == '*' && next == '/') {
        in_block_comment = false;
        ++i;
      }
      continue;
    }
    if (in_single || in_double) {
      if (ch == '\\') {
        ++i;
        continue;
      }
      if ((in_single && ch == '\'') || (in_double && ch == '"')) {
        in_single = false;
        in_double = false;
      }
      continue;
    }
    if (ch == '/' && next == '/') {
      in_line_comment = true;
      ++i;
      continue;
    }
    if (ch == '/' && next == '*') {
      in_block_comment = true;
      ++i;
      continue;
    }
    if (ch == '\'') {
      in_single = true;
      continue;
    }
    if (ch == '"') {
      in_double = true;
      continue;
    }
    if (ch == '(') {
      ++depth;
    } else if (ch == ')') {
      --depth;
      if (depth == 0) {
        out = body.substr(open_paren, i - open_paren + 1);
        return true;
      }
    }
  }
  return false;
}

bool CheckTextInputPrivacyBlock(const std::string& block,
                                const std::string& path_text,
                                const std::string& widget_name) {
  const std::vector<std::string> required = {
      "enableSuggestions: false",
      "autocorrect: false",
      "smartDashesType: SmartDashesType.disabled",
      "smartQuotesType: SmartQuotesType.disabled"};
  for (const auto& needle : required) {
    if (!Contains(block, needle)) {
      std::cerr << "Flutter " << widget_name
                << " missing input privacy flag " << needle << ": "
                << path_text << "\n";
      return false;
    }
  }
  return true;
}

bool CheckFlutterTextInputPrivacy(const std::filesystem::path& root) {
  const std::vector<std::string> widget_names = {"TextField",
                                                "CupertinoTextField"};
  std::error_code ec;
  for (std::filesystem::recursive_directory_iterator it(root, ec), end;
       it != end; it.increment(ec)) {
    if (ec) {
      std::cerr << "failed to iterate Flutter source: " << ec.message()
                << "\n";
      return false;
    }
    if (!it->is_regular_file(ec) || it->path().extension() != ".dart") {
      continue;
    }
    const std::string path_text = it->path().generic_string();
    if (IsSkippedFlutterPath(path_text)) {
      continue;
    }
    std::string body;
    if (!ReadFile(it->path(), body)) {
      std::cerr << "failed to read Flutter source: " << path_text << "\n";
      return false;
    }
    for (const auto& widget_name : widget_names) {
      std::size_t pos = 0;
      while ((pos = body.find(widget_name, pos)) != std::string::npos) {
        if ((pos > 0 && IsIdentifierChar(body[pos - 1])) ||
            (pos + widget_name.size() < body.size() &&
             IsIdentifierChar(body[pos + widget_name.size()]))) {
          pos += widget_name.size();
          continue;
        }
        const std::size_t open_paren =
            FindCallOpenParen(body, pos, widget_name.size());
        if (open_paren == std::string::npos) {
          pos += widget_name.size();
          continue;
        }
        std::string block;
        if (!ExtractBalancedBlock(body, open_paren, block)) {
          std::cerr << "failed to parse Flutter " << widget_name << ": "
                    << path_text << "\n";
          return false;
        }
        if (!CheckTextInputPrivacyBlock(block, path_text, widget_name)) {
          return false;
        }
        pos = open_paren + block.size();
      }
    }
  }
  return true;
}

bool CheckAndroidManifestPrivacy(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing Flutter Android manifest: " << path << "\n";
    return false;
  }
  const std::vector<std::string> required = {
      "android:allowBackup=\"false\"",
      "android:fullBackupContent=\"@xml/no_backup_rules\"",
      "android:dataExtractionRules=\"@xml/no_data_extraction\"",
      "android:usesCleartextTraffic=\"false\""};
  for (const auto& needle : required) {
    if (!Contains(body, needle)) {
      std::cerr << "Flutter Android manifest missing privacy policy "
                << needle << "\n";
      return false;
    }
  }
  return true;
}

bool CheckBackupRules(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing Flutter Android backup rule: " << path << "\n";
    return false;
  }
  const std::vector<std::string> required = {
      "domain=\"file\"",
      "domain=\"database\"",
      "domain=\"sharedpref\"",
      "domain=\"external\"",
      "path=\".\""};
  for (const auto& needle : required) {
    if (!Contains(body, needle)) {
      std::cerr << "Flutter Android backup rule missing " << needle
                << ": " << path << "\n";
      return false;
    }
  }
  return true;
}

bool CheckAndroidActivityHardening(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing Flutter Android activity: " << path << "\n";
    return false;
  }
  const bool ok = Contains(body, "WindowManager.LayoutParams.FLAG_SECURE") &&
                  Contains(body, "IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS") &&
                  Contains(body, "IMPORTANT_FOR_CONTENT_CAPTURE_NO_EXCLUDE_DESCENDANTS");
  if (!ok) {
    std::cerr << "Flutter Android activity lacks privacy window hardening\n";
  }
  return ok;
}

bool CheckAndroidEndpointThreatDetector(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing Flutter Android endpoint detector: " << path
              << "\n";
    return false;
  }
  const bool ok =
      Contains(body, "Debug.isDebuggerConnected()") &&
      Contains(body, "/proc/self/status") &&
      Contains(body, "TracerPid") &&
      Contains(body, "/proc/self/maps") &&
      Contains(body, "frida") &&
      Contains(body, "xposed") &&
      Contains(body, "magisk") &&
      Contains(body, "/system/xbin/su") &&
      Contains(body, "ro.debuggable");
  if (!ok) {
    std::cerr << "Flutter Android endpoint detector lacks debugger/tracer/injection/root checks\n";
  }
  return ok;
}

bool CheckAndroidActivityThreatGuard(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing Flutter Android activity: " << path << "\n";
    return false;
  }
  const bool ok = Contains(body, "EndpointThreatDetector.evaluate") &&
                  Contains(body, "finishAndRemoveTask()");
  if (!ok) {
    std::cerr << "Flutter Android activity lacks fail-closed endpoint guard\n";
  }
  return ok;
}

bool CheckIosInfoPlistPrivacy(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing Flutter iOS Info.plist: " << path << "\n";
    return false;
  }
  const bool has_protection = Contains(body, "NSFileProtectionKey") &&
                              Contains(body, "NSFileProtectionComplete");
  const bool exposes_files = Contains(body, "UIFileSharingEnabled") ||
                             Contains(body, "LSSupportsOpeningDocumentsInPlace");
  const bool arbitrary_loads = Contains(body, "NSAllowsArbitraryLoads") &&
                               Contains(body, "<true/>");
  if (!has_protection || exposes_files || arbitrary_loads) {
    std::cerr << "Flutter iOS Info.plist lacks privacy defaults\n";
    return false;
  }
  return true;
}

bool CheckIosEndpointThreatDetector(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing Flutter iOS endpoint detector: " << path << "\n";
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
    std::cerr << "Flutter iOS endpoint detector lacks debugger/dylib injection checks\n";
  }
  return ok;
}

bool CheckIosAppThreatGuard(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing Flutter iOS app delegate: " << path << "\n";
    return false;
  }
  const bool ok = Contains(body, "EndpointThreatDetector.evaluate") &&
                  Contains(body, "EndpointThreatBlocker.install");
  if (!ok) {
    std::cerr << "Flutter iOS app delegate lacks fail-closed endpoint guard\n";
  }
  return ok;
}

bool CheckNoReleaseLoggingOrTelemetry(const std::filesystem::path& root) {
  const std::vector<std::string> forbidden_calls = {
      "debugPrint(",
      "print(",
      "NSLog(",
      "os_log",
      "Logger("};
  const std::vector<std::string> forbidden_markers = {
      "dart:developer",
      "developer.log",
      "android.util.Log",
      "Log.",
      "println(",
      "printStackTrace(",
      "AutofillGroup(",
      "AutofillHints",
      "autofillHints:",
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
      std::cerr << "failed to iterate Flutter source: " << ec.message()
                << "\n";
      return false;
    }
    if (!it->is_regular_file(ec)) {
      continue;
    }
    const std::string path_text = it->path().generic_string();
    if (IsSkippedFlutterPath(path_text)) {
      continue;
    }
    const auto ext = it->path().extension().string();
    if (ext != ".dart" && ext != ".kt" && ext != ".swift" && ext != ".xml" &&
        ext != ".plist") {
      continue;
    }
    std::string body;
    if (!ReadFile(it->path(), body)) {
      std::cerr << "failed to read Flutter source: "
                << it->path().generic_string() << "\n";
      return false;
    }
    for (const auto& needle : forbidden_calls) {
      if (ContainsStandaloneCall(body, needle)) {
        std::cerr << "Flutter source contains logging call " << needle
                << ": " << path_text << "\n";
        return false;
      }
    }
    for (const auto& needle : forbidden_markers) {
      if (Contains(body, needle)) {
        std::cerr << "Flutter source contains logging/telemetry marker "
                  << needle << ": " << path_text << "\n";
        return false;
      }
    }
  }
  return true;
}

}  // namespace

int main() {
  const std::filesystem::path flutter_root = MI_E2EE_FLUTTER_SOURCE_ROOT;
  if (!std::filesystem::is_directory(flutter_root)) {
    std::cerr << "Flutter source root missing: " << flutter_root << "\n";
    return 1;
  }
  if (!CheckAndroidManifestPrivacy(
          flutter_root / "android/app/src/main/AndroidManifest.xml")) {
    return 1;
  }
  if (!CheckBackupRules(
          flutter_root / "android/app/src/main/res/xml/no_backup_rules.xml")) {
    return 1;
  }
  if (!CheckBackupRules(
          flutter_root / "android/app/src/main/res/xml/no_data_extraction.xml")) {
    return 1;
  }
  if (!CheckAndroidActivityHardening(
          flutter_root /
          "android/app/src/main/kotlin/com/mi/e2ee/mi_e2ee_im_app/MainActivity.kt")) {
    return 1;
  }
  if (!CheckAndroidEndpointThreatDetector(
          flutter_root /
          "android/app/src/main/kotlin/com/mi/e2ee/mi_e2ee_im_app/EndpointThreatDetector.kt")) {
    return 1;
  }
  if (!CheckAndroidActivityThreatGuard(
          flutter_root /
          "android/app/src/main/kotlin/com/mi/e2ee/mi_e2ee_im_app/MainActivity.kt")) {
    return 1;
  }
  if (!CheckIosInfoPlistPrivacy(flutter_root / "ios/Runner/Info.plist")) {
    return 1;
  }
  if (!CheckIosEndpointThreatDetector(
          flutter_root / "ios/Runner/EndpointThreatDetector.swift")) {
    return 1;
  }
  if (!CheckIosAppThreatGuard(flutter_root / "ios/Runner/AppDelegate.swift")) {
    return 1;
  }
  if (!CheckFlutterTextInputPrivacy(flutter_root)) {
    return 1;
  }
  if (!CheckNoReleaseLoggingOrTelemetry(flutter_root)) {
    return 1;
  }
  return 0;
}
