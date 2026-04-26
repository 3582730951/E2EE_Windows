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

bool EndsWith(const std::string& value, const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

bool IsIdentifierChar(char ch) {
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
         (ch >= '0' && ch <= '9') || ch == '_';
}

bool IsGeneratedAndroidPath(const std::filesystem::path& path) {
  const std::string value = path.generic_string();
  return Contains(value, "/build/") || Contains(value, "/.gradle/") ||
         Contains(value, "/.cxx/");
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

bool CheckComposeTextInputPrivacy(const std::filesystem::path& root) {
  const std::vector<std::string> text_fields = {
      "OutlinedTextField",
      "BasicTextField",
      "TextField"};
  std::error_code ec;
  bool ok = true;
  for (std::filesystem::recursive_directory_iterator it(root, ec), end;
       it != end; it.increment(ec)) {
    if (ec) {
      std::cerr << "failed to iterate Android source: " << ec.message()
                << "\n";
      return false;
    }
    if (!it->is_regular_file(ec) || it->path().extension() != ".kt") {
      continue;
    }
    if (IsGeneratedAndroidPath(it->path())) {
      continue;
    }
    const std::string path = it->path().generic_string();
    std::string body;
    if (!ReadFile(it->path(), body)) {
      std::cerr << "failed to read Android source: " << path << "\n";
      return false;
    }
    std::size_t keyboard_pos = 0;
    while ((keyboard_pos = body.find("KeyboardOptions", keyboard_pos)) !=
           std::string::npos) {
      constexpr std::size_t kKeyboardOptionsSize = 15;
      if ((keyboard_pos > 0 && IsIdentifierChar(body[keyboard_pos - 1])) ||
          (keyboard_pos + kKeyboardOptionsSize < body.size() &&
           IsIdentifierChar(body[keyboard_pos + kKeyboardOptionsSize]))) {
        keyboard_pos += kKeyboardOptionsSize;
        continue;
      }
      const std::size_t open_paren =
          FindCallOpenParen(body, keyboard_pos, kKeyboardOptionsSize);
      if (open_paren == std::string::npos) {
        keyboard_pos += kKeyboardOptionsSize;
        continue;
      }
      std::string block;
      if (!ExtractBalancedBlock(body, open_paren, block)) {
        std::cerr << "failed to parse KeyboardOptions: " << path << "\n";
        return false;
      }
      if (!Contains(block, "autoCorrectEnabled = false")) {
        std::cerr << "KeyboardOptions missing autoCorrectEnabled = false: "
                  << path << "\n";
        ok = false;
      }
      keyboard_pos = open_paren + block.size();
    }
    for (const auto& widget : text_fields) {
      std::size_t pos = 0;
      while ((pos = body.find(widget, pos)) != std::string::npos) {
        if ((pos > 0 && IsIdentifierChar(body[pos - 1])) ||
            (pos + widget.size() < body.size() &&
             IsIdentifierChar(body[pos + widget.size()]))) {
          pos += widget.size();
          continue;
        }
        const std::size_t open_paren =
            FindCallOpenParen(body, pos, widget.size());
        if (open_paren == std::string::npos) {
          pos += widget.size();
          continue;
        }
        std::string block;
        if (!ExtractBalancedBlock(body, open_paren, block)) {
          std::cerr << "failed to parse " << widget << ": " << path << "\n";
          return false;
        }
        if (!Contains(block, "readOnly = true") &&
            !Contains(block, "keyboardOptions =")) {
          std::cerr << widget << " missing keyboardOptions privacy hook: "
                    << path << "\n";
          ok = false;
        }
        pos = open_paren + block.size();
      }
    }
  }
  return ok;
}

bool CheckActivityHardening(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing Android activity source: " << path << "\n";
    return false;
  }
  const bool ok = Contains(body, "WindowManager.LayoutParams.FLAG_SECURE") &&
                  Contains(body, "IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS") &&
                  Contains(body, "IMPORTANT_FOR_CONTENT_CAPTURE_NO_EXCLUDE_DESCENDANTS");
  if (!ok) {
    std::cerr << "Android activity is missing privacy window hardening: " << path
              << "\n";
  }
  return ok;
}

bool CheckSecureClipboard(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing secure clipboard source: " << path << "\n";
    return false;
  }
  const bool ok = Contains(body, "zerox://clip/") &&
                  Contains(body, "60_000L") &&
                  Contains(body, "ConcurrentHashMap") &&
                  Contains(body, "SecureRandom");
  if (!ok) {
    std::cerr << "SecureClipboard lacks tokenized in-process clipboard design: "
              << path << "\n";
  }
  return ok;
}

bool CheckNoDirectClipboardSetText(const std::filesystem::path& root) {
  std::error_code ec;
  for (std::filesystem::recursive_directory_iterator it(root, ec), end; it != end;
       it.increment(ec)) {
    if (ec) {
      std::cerr << "failed to iterate Android source: " << ec.message() << "\n";
      return false;
    }
    if (!it->is_regular_file(ec) || it->path().extension() != ".kt") {
      continue;
    }
    if (IsGeneratedAndroidPath(it->path())) {
      continue;
    }
    const std::string path = it->path().generic_string();
    if (EndsWith(path, "/SecureClipboard.kt")) {
      continue;
    }
    std::string body;
    if (!ReadFile(it->path(), body)) {
      std::cerr << "failed to read Android source: " << path << "\n";
      return false;
    }
    if (Contains(body, ".setText(")) {
      std::cerr << "direct system clipboard write outside SecureClipboard: "
                << path << "\n";
      return false;
    }
  }
  return true;
}

bool CheckEndpointThreatDetector(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing Android endpoint threat detector source: " << path
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
    std::cerr << "Android endpoint detector lacks debugger/tracer/injection/root checks: "
              << path << "\n";
  }
  return ok;
}

bool CheckActivityThreatGuard(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing Android activity source: " << path << "\n";
    return false;
  }
  const bool ok = Contains(body, "EndpointThreatDetector.evaluate") &&
                  Contains(body, "finishAndRemoveTask()");
  if (!ok) {
    std::cerr << "Android activity is missing endpoint threat guard: " << path
              << "\n";
  }
  return ok;
}

bool CheckManifestPrivacyPolicy(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing Android manifest: " << path << "\n";
    return false;
  }
  const std::vector<std::string> required = {
      "android:allowBackup=\"false\"",
      "android:fullBackupContent=\"@xml/no_backup_rules\"",
      "android:dataExtractionRules=\"@xml/no_data_extraction\"",
      "android:usesCleartextTraffic=\"false\""};
  for (const auto& needle : required) {
    if (!Contains(body, needle)) {
      std::cerr << "Android manifest privacy policy missing " << needle
                << ": " << path << "\n";
      return false;
    }
  }
  return true;
}

bool CheckBackupRules(const std::filesystem::path& path) {
  std::string body;
  if (!ReadFile(path, body)) {
    std::cerr << "missing Android backup/data extraction rules: " << path
              << "\n";
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
      std::cerr << "Android backup/data extraction rules missing " << needle
                << ": " << path << "\n";
      return false;
    }
  }
  return true;
}

bool CheckNoTelemetryOrLogging(const std::filesystem::path& root) {
  const std::vector<std::string> forbidden = {
      "android.util.Log",
      "Log.",
      "println(",
      "printStackTrace(",
      "Timber",
      "Crashlytics",
      "Firebase",
      "analytics",
      "telemetry",
      "Sentry",
      "Bugsnag",
      "Datadog",
      "NewRelic"};
  std::error_code ec;
  for (std::filesystem::recursive_directory_iterator it(root, ec), end; it != end;
       it.increment(ec)) {
    if (ec) {
      std::cerr << "failed to iterate Android source: " << ec.message() << "\n";
      return false;
    }
    if (!it->is_regular_file(ec)) {
      continue;
    }
    if (IsGeneratedAndroidPath(it->path())) {
      continue;
    }
    const auto ext = it->path().extension().string();
    if (ext != ".kt" && ext != ".kts" && ext != ".xml") {
      continue;
    }
    std::string body;
    if (!ReadFile(it->path(), body)) {
      std::cerr << "failed to read Android source: "
                << it->path().generic_string() << "\n";
      return false;
    }
    for (const auto& needle : forbidden) {
      if (Contains(body, needle)) {
        std::cerr << "Android source contains logging/telemetry marker "
                  << needle << ": " << it->path().generic_string() << "\n";
        return false;
      }
    }
  }
  return true;
}

}  // namespace

int main() {
  const std::filesystem::path android_root = MI_E2EE_ANDROID_SOURCE_ROOT;
  if (!std::filesystem::is_directory(android_root)) {
    std::cerr << "Android source root missing: " << android_root << "\n";
    return 1;
  }
  if (!CheckSecureClipboard(
          android_root /
          "ui/src/main/java/mi/e2ee/android/ui/SecureClipboard.kt")) {
    return 1;
  }
  if (!CheckSecureClipboard(
          android_root /
          "rootapp/src/main/java/mi/e2ee/rootauth/SecureClipboard.kt")) {
    return 1;
  }
  if (!CheckManifestPrivacyPolicy(
          android_root / "app/src/main/AndroidManifest.xml")) {
    return 1;
  }
  if (!CheckManifestPrivacyPolicy(
          android_root / "rootapp/src/main/AndroidManifest.xml")) {
    return 1;
  }
  if (!CheckBackupRules(
          android_root / "app/src/main/res/xml/no_backup_rules.xml")) {
    return 1;
  }
  if (!CheckBackupRules(
          android_root / "app/src/main/res/xml/no_data_extraction.xml")) {
    return 1;
  }
  if (!CheckBackupRules(
          android_root / "rootapp/src/main/res/xml/no_backup_rules.xml")) {
    return 1;
  }
  if (!CheckBackupRules(
          android_root / "rootapp/src/main/res/xml/no_data_extraction.xml")) {
    return 1;
  }
  if (!CheckActivityHardening(
          android_root / "app/src/main/java/mi/e2ee/android/MainActivity.kt")) {
    return 1;
  }
  if (!CheckActivityThreatGuard(
          android_root / "app/src/main/java/mi/e2ee/android/MainActivity.kt")) {
    return 1;
  }
  if (!CheckActivityHardening(
          android_root /
          "rootapp/src/main/java/mi/e2ee/rootauth/RootAuthActivity.kt")) {
    return 1;
  }
  if (!CheckActivityThreatGuard(
          android_root /
          "rootapp/src/main/java/mi/e2ee/rootauth/RootAuthActivity.kt")) {
    return 1;
  }
  if (!CheckEndpointThreatDetector(
          android_root /
          "ui/src/main/java/mi/e2ee/android/ui/EndpointThreatDetector.kt")) {
    return 1;
  }
  if (!CheckEndpointThreatDetector(
          android_root /
          "rootapp/src/main/java/mi/e2ee/rootauth/EndpointThreatDetector.kt")) {
    return 1;
  }
  if (!CheckNoDirectClipboardSetText(android_root)) {
    return 1;
  }
  if (!CheckComposeTextInputPrivacy(android_root)) {
    return 1;
  }
  if (!CheckNoTelemetryOrLogging(android_root)) {
    return 1;
  }
  return 0;
}
