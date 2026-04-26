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

bool RunPrivacyScan(const std::filesystem::path& dist_root) {
  const std::string cmd = "bash " +
      ShellQuote(MI_E2EE_PACKAGE_VERIFY_POSIX_SH) + " --privacy-only --dist " +
      ShellQuote(dist_root.string()) + " >/dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
}

bool PowerShellAvailable() {
  return std::system("command -v pwsh >/dev/null 2>&1") == 0;
}

bool RunWindowsPrivacyScan(const std::filesystem::path& dist_root) {
  const std::string cmd = "pwsh -NoProfile -ExecutionPolicy Bypass -File " +
                          ShellQuote(MI_E2EE_PACKAGE_VERIFY_WINDOWS_PS1) +
                          " -PrivacyOnly -Dist " +
                          ShellQuote(dist_root.string()) + " >/dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
}

bool ExpectRejected(const std::filesystem::path& path, const std::string& body,
                    const std::filesystem::path& root, const char* label) {
  if (!WriteFile(path, body)) {
    return false;
  }
  if (RunPrivacyScan(root)) {
    std::cerr << "privacy scan allowed " << label << "\n";
    return false;
  }
  std::error_code ec;
  std::filesystem::remove(path, ec);
  return true;
}

}  // namespace

int main() {
  std::error_code ec;
  const auto root =
      std::filesystem::temp_directory_path(ec) / "mi_e2ee_package_privacy_scan";
  if (ec) {
    return 1;
  }
  std::filesystem::remove_all(root, ec);
  if (!mi::server::test::EnsureOwnerOnlyDirectory(root)) {
    return 1;
  }
  const auto client_root = root / "mi_e2ee_client";
  const auto server_root = root / "mi_e2ee_server";
  if (!WriteFile(client_root / "state" / "history.bin", "encrypted") ||
      !WriteFile(server_root / "state" / "queue.bin", "ciphertext")) {
    return 1;
  }
  if (!RunPrivacyScan(root)) {
    std::cerr << "clean privacy fixture failed\n";
    return 1;
  }
  const bool have_pwsh = PowerShellAvailable();
  if (have_pwsh && !RunWindowsPrivacyScan(root)) {
    std::cerr << "clean Windows privacy fixture failed\n";
    return 1;
  }

  if (!WriteFile(client_root / "cache" / "runtime.log", "secret")) {
    return 1;
  }
  if (RunPrivacyScan(root)) {
    std::cerr << "privacy scan allowed log artifact\n";
    return 1;
  }
  std::filesystem::remove(client_root / "cache" / "runtime.log", ec);

  if (!ExpectRejected(server_root / "diagnostics" / "state", "ok", root,
                      "diagnostics directory artifact")) {
    return 1;
  }
  if (!ExpectRejected(server_root / "metrics" / "counters", "ok", root,
                      "metrics directory artifact")) {
    return 1;
  }
  if (!ExpectRejected(server_root / "audit" / "events", "ok", root,
                      "audit directory artifact")) {
    return 1;
  }
  if (!ExpectRejected(server_root / "telemetry" / "events", "ok", root,
                      "telemetry directory artifact")) {
    return 1;
  }
  if (!ExpectRejected(server_root / "tools" / "mi_e2ee_ops_health_view",
                      "opaque", root, "ops health tool")) {
    return 1;
  }
  if (!ExpectRejected(server_root / "tools" / "mi_e2ee_third_party_audit",
                      "opaque", root, "audit tool")) {
    return 1;
  }
  if (have_pwsh) {
    if (!WriteFile(server_root / "diagnostics" / "state", "ok")) {
      return 1;
    }
    if (RunWindowsPrivacyScan(root)) {
      std::cerr << "Windows privacy scan allowed diagnostics directory artifact\n";
      return 1;
    }
    std::filesystem::remove(server_root / "diagnostics" / "state", ec);
  }

  if (!WriteFile(server_root / "state" / "payload", "payload_hex=00112233")) {
    return 1;
  }
  if (RunPrivacyScan(root)) {
    std::cerr << "privacy scan allowed plaintext payload marker\n";
    return 1;
  }
  std::filesystem::remove(server_root / "state" / "payload", ec);

  if (!ExpectRejected(client_root / "state" / "plaintext",
                      "message_plaintext=hello", root,
                      "message plaintext marker")) {
    return 1;
  }
  if (!ExpectRejected(client_root / "state" / "plain_payload",
                      "plaintext_payload=hello", root,
                      "plaintext payload marker")) {
    return 1;
  }
  if (!ExpectRejected(server_root / "config" / "debug.ini",
                      "[server]\ndebug_log=1\n", root,
                      "debug log config marker")) {
    return 1;
  }
  if (!ExpectRejected(server_root / "config" / "ops.ini",
                      "[server]\nops_enable=1\n", root,
                      "ops enable config marker")) {
    return 1;
  }

  if (!WriteFile(client_root / "state" / "ui_state",
                 "local_path=/home/alice/private/photo.jpg")) {
    return 1;
  }
  if (RunPrivacyScan(root)) {
    std::cerr << "privacy scan allowed local path marker\n";
    return 1;
  }
  std::filesystem::remove(client_root / "state" / "ui_state", ec);

  if (!ExpectRejected(client_root / "state" / "raw_unix_path",
                      "last=/Users/alice/Private/photo.jpg", root,
                      "raw unix user path")) {
    return 1;
  }
  if (!ExpectRejected(client_root / "state" / "raw_windows_path",
                      "last=C:\\Users\\Alice\\Desktop\\secret.txt", root,
                      "raw Windows user path")) {
    return 1;
  }
  if (have_pwsh) {
    if (!WriteFile(client_root / "state" / "raw_windows_path",
                   "last=C:\\Users\\Alice\\Desktop\\secret.txt")) {
      return 1;
    }
    if (RunWindowsPrivacyScan(root)) {
      std::cerr << "Windows privacy scan allowed raw Windows user path\n";
      return 1;
    }
    std::filesystem::remove(client_root / "state" / "raw_windows_path", ec);
  }

  if (!WriteFile(server_root / "state" / "auth_cache",
                 "token=raw-session-token")) {
    return 1;
  }
  if (RunPrivacyScan(root)) {
    std::cerr << "privacy scan allowed token marker\n";
    return 1;
  }
  return 0;
}
