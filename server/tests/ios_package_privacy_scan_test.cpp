#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "test_permissions.h"

namespace {

std::string ShellQuote(const std::string& value) {
  std::string out = "'";
  for (char ch : value) {
    if (ch == '\'') {
      out += "'\\''";
    } else {
      out += ch;
    }
  }
  out += "'";
  return out;
}

bool WriteFile(const std::filesystem::path& path, const std::string& value) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out << value;
  out.close();
  return out.good();
}

bool RunIosPackagePrivacyScan(const std::filesystem::path& root) {
  const std::string cmd = "bash " +
      ShellQuote(MI_E2EE_IOS_PACKAGE_VERIFY_SH) + " --root " +
      ShellQuote(root.string());
  return std::system(cmd.c_str()) == 0;
}

bool WriteCleanExpandedApp(const std::filesystem::path& root) {
  const std::string info_plist =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
      "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
      "<plist version=\"1.0\">\n"
      "<dict>\n"
      "  <key>CFBundleIdentifier</key>\n"
      "  <string>mi.e2ee.RootAuthApp</string>\n"
      "  <key>NSFileProtectionKey</key>\n"
      "  <string>NSFileProtectionComplete</string>\n"
      "  <key>NSAppTransportSecurity</key>\n"
      "  <dict>\n"
      "    <key>NSAllowsArbitraryLoads</key>\n"
      "    <false/>\n"
      "  </dict>\n"
      "</dict>\n"
      "</plist>\n";
  return WriteFile(root / "Payload" / "RootAuthApp.app" / "Info.plist",
                   info_plist) &&
         WriteFile(root / "Payload" / "RootAuthApp.app" / "RootAuthApp",
                   "\xcf\xfa\xed\xfe opaque mach-o");
}

bool ExpectRejected(const std::filesystem::path& root, const char* label) {
  if (RunIosPackagePrivacyScan(root)) {
    std::cerr << "iOS package privacy scan allowed " << label << "\n";
    return false;
  }
  return true;
}

}  // namespace

int main() {
  std::error_code ec;
  const auto root = std::filesystem::temp_directory_path(ec) /
                    "mi_e2ee_ios_package_privacy_scan";
  if (ec) {
    return 1;
  }
  std::filesystem::remove_all(root, ec);
  if (!mi::server::test::EnsureOwnerOnlyDirectory(root)) {
    return 1;
  }

  const auto clean = root / "clean";
  if (!WriteCleanExpandedApp(clean)) {
    return 1;
  }
  if (!RunIosPackagePrivacyScan(clean)) {
    std::cerr << "clean expanded iOS package privacy fixture failed\n";
    return 1;
  }

  const auto log_artifact = root / "log_artifact";
  if (!WriteCleanExpandedApp(log_artifact) ||
      !WriteFile(log_artifact / "Payload" / "RootAuthApp.app" / "runtime.log",
                 "secret")) {
    return 1;
  }
  if (!ExpectRejected(log_artifact, "log artifact")) {
    return 1;
  }

  const auto telemetry = root / "telemetry";
  if (!WriteCleanExpandedApp(telemetry) ||
      !WriteFile(telemetry / "Payload" / "RootAuthApp.app" / "RootAuthApp",
                 "io.sentry.Sentry Crashlytics FirebaseAnalytics")) {
    return 1;
  }
  if (!ExpectRejected(telemetry, "telemetry SDK marker")) {
    return 1;
  }

  const auto plaintext = root / "plaintext";
  if (!WriteCleanExpandedApp(plaintext) ||
      !WriteFile(plaintext / "Payload" / "RootAuthApp.app" / "state.bin",
                 "file_key=001122 token=raw payload_hex=aabb")) {
    return 1;
  }
  if (!ExpectRejected(plaintext, "plaintext marker")) {
    return 1;
  }

  const auto debug_config = root / "debug_config";
  if (!WriteCleanExpandedApp(debug_config) ||
      !WriteFile(debug_config / "Payload" / "RootAuthApp.app" / "config.ini",
                 "[server]\ndebug_log=1\nops_enable=1\n")) {
    return 1;
  }
  if (!ExpectRejected(debug_config, "debug/ops config marker")) {
    return 1;
  }

  const auto diagnostics = root / "diagnostics";
  if (!WriteCleanExpandedApp(diagnostics) ||
      !WriteFile(diagnostics / "Payload" / "RootAuthApp.app" /
                     "diagnostics" / "state.bin",
                 "opaque")) {
    return 1;
  }
  if (!ExpectRejected(diagnostics, "diagnostics artifact")) {
    return 1;
  }

  const auto metrics = root / "metrics";
  if (!WriteCleanExpandedApp(metrics) ||
      !WriteFile(metrics / "Payload" / "RootAuthApp.app" /
                     "metrics" / "counters.bin",
                 "opaque")) {
    return 1;
  }
  if (!ExpectRejected(metrics, "metrics artifact")) {
    return 1;
  }

  const auto audit = root / "audit";
  if (!WriteCleanExpandedApp(audit) ||
      !WriteFile(audit / "Payload" / "RootAuthApp.app" /
                     "audit" / "events.bin",
                 "opaque")) {
    return 1;
  }
  if (!ExpectRejected(audit, "audit artifact")) {
    return 1;
  }

  const auto ops_health = root / "ops_health";
  if (!WriteCleanExpandedApp(ops_health) ||
      !WriteFile(ops_health / "Payload" / "RootAuthApp.app" /
                     "mi_e2ee_ops_health_view",
                 "opaque")) {
    return 1;
  }
  if (!ExpectRejected(ops_health, "ops health tool")) {
    return 1;
  }

  const auto local_path = root / "local_path";
  if (!WriteCleanExpandedApp(local_path) ||
      !WriteFile(local_path / "Payload" / "RootAuthApp.app" / "cache.bin",
                 "last=/Users/alice/private")) {
    return 1;
  }
  if (!ExpectRejected(local_path, "local user path")) {
    return 1;
  }

  const auto no_protection = root / "no_protection";
  if (!WriteCleanExpandedApp(no_protection) ||
      !WriteFile(no_protection / "Payload" / "RootAuthApp.app" / "Info.plist",
                 "<plist><dict><key>CFBundleIdentifier</key>"
                 "<string>mi.e2ee.RootAuthApp</string></dict></plist>\n")) {
    return 1;
  }
  if (!ExpectRejected(no_protection, "missing file protection")) {
    return 1;
  }

  const auto arbitrary_loads = root / "arbitrary_loads";
  if (!WriteCleanExpandedApp(arbitrary_loads) ||
      !WriteFile(arbitrary_loads / "Payload" / "RootAuthApp.app" / "Info.plist",
                 "<plist><dict><key>NSFileProtectionKey</key>"
                 "<string>NSFileProtectionComplete</string>"
                 "<key>NSAllowsArbitraryLoads</key><true/></dict></plist>\n")) {
    return 1;
  }
  if (!ExpectRejected(arbitrary_loads, "ATS arbitrary loads")) {
    return 1;
  }

  const auto file_sharing = root / "file_sharing";
  if (!WriteCleanExpandedApp(file_sharing) ||
      !WriteFile(file_sharing / "Payload" / "RootAuthApp.app" / "Info.plist",
                 "<plist><dict><key>NSFileProtectionKey</key>"
                 "<string>NSFileProtectionComplete</string>"
                 "<key>UIFileSharingEnabled</key><true/></dict></plist>\n")) {
    return 1;
  }
  if (!ExpectRejected(file_sharing, "file sharing enabled")) {
    return 1;
  }

  return 0;
}
