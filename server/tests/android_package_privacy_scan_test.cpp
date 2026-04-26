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

bool RunAndroidPackagePrivacyScan(const std::filesystem::path& root) {
  const std::string cmd = "bash " +
      ShellQuote(MI_E2EE_ANDROID_PACKAGE_VERIFY_SH) + " --root " +
      ShellQuote(root.string()) + " >/dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
}

bool WriteCleanExpandedApk(const std::filesystem::path& root) {
  const std::string manifest =
      "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\">\n"
      "  <application android:allowBackup=\"false\"\n"
      "      android:fullBackupContent=\"@xml/no_backup_rules\"\n"
      "      android:dataExtractionRules=\"@xml/no_data_extraction\"\n"
      "      android:usesCleartextTraffic=\"false\" />\n"
      "</manifest>\n";
  const std::string no_backup =
      "<full-backup-content>\n"
      "  <exclude domain=\"file\" path=\".\" />\n"
      "  <exclude domain=\"database\" path=\".\" />\n"
      "  <exclude domain=\"sharedpref\" path=\".\" />\n"
      "  <exclude domain=\"external\" path=\".\" />\n"
      "</full-backup-content>\n";
  const std::string no_extract =
      "<data-extraction-rules>\n"
      "  <cloud-backup disableIfNoEncryptionCapabilities=\"true\">\n"
      "    <exclude domain=\"file\" path=\".\" />\n"
      "    <exclude domain=\"database\" path=\".\" />\n"
      "    <exclude domain=\"sharedpref\" path=\".\" />\n"
      "    <exclude domain=\"external\" path=\".\" />\n"
      "  </cloud-backup>\n"
      "  <device-transfer>\n"
      "    <exclude domain=\"file\" path=\".\" />\n"
      "    <exclude domain=\"database\" path=\".\" />\n"
      "    <exclude domain=\"sharedpref\" path=\".\" />\n"
      "    <exclude domain=\"external\" path=\".\" />\n"
      "  </device-transfer>\n"
      "</data-extraction-rules>\n";
  return WriteFile(root / "AndroidManifest.xml", manifest) &&
         WriteFile(root / "res" / "xml" / "no_backup_rules.xml", no_backup) &&
         WriteFile(root / "res" / "xml" / "no_data_extraction.xml", no_extract) &&
         WriteFile(root / "classes.dex", "opaque bytecode");
}

bool ExpectRejected(const std::filesystem::path& root, const char* label) {
  if (RunAndroidPackagePrivacyScan(root)) {
    std::cerr << "Android package privacy scan allowed " << label << "\n";
    return false;
  }
  return true;
}

}  // namespace

int main() {
  std::error_code ec;
  const auto root = std::filesystem::temp_directory_path(ec) /
                    "mi_e2ee_android_package_privacy_scan";
  if (ec) {
    return 1;
  }
  std::filesystem::remove_all(root, ec);
  if (!mi::server::test::EnsureOwnerOnlyDirectory(root)) {
    return 1;
  }

  const auto clean = root / "clean";
  if (!WriteCleanExpandedApk(clean)) {
    return 1;
  }
  if (!RunAndroidPackagePrivacyScan(clean)) {
    std::cerr << "clean expanded APK privacy fixture failed\n";
    return 1;
  }

  const auto log_artifact = root / "log_artifact";
  if (!WriteCleanExpandedApk(log_artifact) ||
      !WriteFile(log_artifact / "files" / "runtime.log", "secret")) {
    return 1;
  }
  if (!ExpectRejected(log_artifact, "log artifact")) {
    return 1;
  }

  const auto telemetry = root / "telemetry";
  if (!WriteCleanExpandedApk(telemetry) ||
      !WriteFile(telemetry / "classes.dex",
                 "com.google.firebase.crashlytics.FirebaseCrashlytics")) {
    return 1;
  }
  if (!ExpectRejected(telemetry, "Crashlytics marker")) {
    return 1;
  }

  const auto plaintext = root / "plaintext";
  if (!WriteCleanExpandedApk(plaintext) ||
      !WriteFile(plaintext / "assets" / "state", "payload_hex=001122 token=raw")) {
    return 1;
  }
  if (!ExpectRejected(plaintext, "plaintext marker")) {
    return 1;
  }

  const auto debug_config = root / "debug_config";
  if (!WriteCleanExpandedApk(debug_config) ||
      !WriteFile(debug_config / "assets" / "config.ini",
                 "[server]\ndebug_log=1\nops_enable=1\n")) {
    return 1;
  }
  if (!ExpectRejected(debug_config, "debug/ops config marker")) {
    return 1;
  }

  const auto diagnostics = root / "diagnostics";
  if (!WriteCleanExpandedApk(diagnostics) ||
      !WriteFile(diagnostics / "assets" / "diagnostics" / "state.bin",
                 "opaque")) {
    return 1;
  }
  if (!ExpectRejected(diagnostics, "diagnostics artifact")) {
    return 1;
  }

  const auto metrics = root / "metrics";
  if (!WriteCleanExpandedApk(metrics) ||
      !WriteFile(metrics / "assets" / "metrics" / "counters.bin", "opaque")) {
    return 1;
  }
  if (!ExpectRejected(metrics, "metrics artifact")) {
    return 1;
  }

  const auto audit = root / "audit";
  if (!WriteCleanExpandedApk(audit) ||
      !WriteFile(audit / "assets" / "audit" / "events.bin", "opaque")) {
    return 1;
  }
  if (!ExpectRejected(audit, "audit artifact")) {
    return 1;
  }

  const auto ops_health = root / "ops_health";
  if (!WriteCleanExpandedApk(ops_health) ||
      !WriteFile(ops_health / "assets" / "mi_e2ee_ops_health_view",
                 "opaque")) {
    return 1;
  }
  if (!ExpectRejected(ops_health, "ops health tool")) {
    return 1;
  }

  const auto local_path = root / "local_path";
  if (!WriteCleanExpandedApk(local_path) ||
      !WriteFile(local_path / "assets" / "path", "last=C:\\Users\\Alice\\secret")) {
    return 1;
  }
  if (!ExpectRejected(local_path, "local user path")) {
    return 1;
  }

  const auto backup_enabled = root / "backup_enabled";
  if (!WriteCleanExpandedApk(backup_enabled) ||
      !WriteFile(backup_enabled / "AndroidManifest.xml",
                 "<manifest><application android:allowBackup=\"true\" /></manifest>\n")) {
    return 1;
  }
  if (!ExpectRejected(backup_enabled, "backup-enabled manifest")) {
    return 1;
  }

  const auto cleartext = root / "cleartext";
  if (!WriteCleanExpandedApk(cleartext) ||
      !WriteFile(cleartext / "AndroidManifest.xml",
                 "<manifest><application android:allowBackup=\"false\" "
                 "android:usesCleartextTraffic=\"true\" /></manifest>\n")) {
    return 1;
  }
  if (!ExpectRejected(cleartext, "cleartext-enabled manifest")) {
    return 1;
  }

  return 0;
}
