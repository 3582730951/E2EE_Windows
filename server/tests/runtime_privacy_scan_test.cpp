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

bool RunRuntimePrivacyScan(const std::filesystem::path& root) {
  const std::string cmd = "bash " + ShellQuote(MI_E2EE_RUNTIME_VERIFY_SH) +
                          " --root " + ShellQuote(root.string());
  return std::system(cmd.c_str()) == 0;
}

bool ExpectRejected(const std::filesystem::path& path,
                    const std::string& body,
                    const std::filesystem::path& root,
                    const char* label) {
  if (!WriteFile(path, body)) {
    return false;
  }
  if (RunRuntimePrivacyScan(root)) {
    std::cerr << "runtime privacy scan allowed " << label << "\n";
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
      std::filesystem::temp_directory_path(ec) / "mi_e2ee_runtime_privacy_scan";
  if (ec) {
    return 1;
  }
  std::filesystem::remove_all(root, ec);
  if (!mi::server::test::EnsureOwnerOnlyDirectory(root)) {
    return 1;
  }

  const auto client_root = root / "client_run";
  const auto server_root = root / "server_run";
  if (!WriteFile(client_root / "history" / "0001.so", "ciphertext") ||
      !WriteFile(server_root / "offline_store" / "queue.bin", "ciphertext")) {
    return 1;
  }
  if (!RunRuntimePrivacyScan(root)) {
    std::cerr << "clean runtime privacy fixture failed\n";
    return 1;
  }

  if (!ExpectRejected(client_root / "runtime.log.old.bak", "secret", root,
                      "log artifact")) {
    return 1;
  }
  if (!ExpectRejected(server_root / "diagnostics" / "health.bin", "ok", root,
                      "diagnostics directory")) {
    return 1;
  }
  if (!ExpectRejected(server_root / "metrics" / "counters.bin", "ok", root,
                      "metrics directory")) {
    return 1;
  }
  if (!ExpectRejected(server_root / "audit" / "events.bin", "ok", root,
                      "audit directory")) {
    return 1;
  }
  if (!ExpectRejected(server_root / "telemetry" / "events.bin", "ok", root,
                      "telemetry directory")) {
    return 1;
  }
  if (!ExpectRejected(client_root / "crash" / "dump.dmp", "ok", root,
                      "crash dump artifact")) {
    return 1;
  }
  if (!ExpectRejected(client_root / "state" / "payload.bin",
                      "payload_hex=00112233", root, "payload marker")) {
    return 1;
  }
  if (!ExpectRejected(client_root / "state" / "plaintext.bin",
                      "message_plaintext=hello", root,
                      "message plaintext marker")) {
    return 1;
  }
  if (!ExpectRejected(client_root / "state" / "plain_payload.bin",
                      "plaintext_payload=hello", root,
                      "plaintext payload marker")) {
    return 1;
  }
  if (!ExpectRejected(client_root / "state" / "file_key.bin",
                      "file_key=0011223344556677", root, "file key")) {
    return 1;
  }
  if (!ExpectRejected(server_root / "state" / "auth_cache",
                      "token=raw-session-token", root, "token marker")) {
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
  if (!ExpectRejected(client_root / "state" / "ui_state",
                      "local_path=/home/alice/private/photo.jpg", root,
                      "local path marker")) {
    return 1;
  }
  if (!ExpectRejected(client_root / "state" / "path_cache",
                      "last=/Users/alice/Private/photo.jpg", root,
                      "unix home path")) {
    return 1;
  }
  if (!ExpectRejected(client_root / "state" / "win_path_cache",
                      "last=C:\\Users\\Alice\\Desktop\\secret.txt", root,
                      "windows user path")) {
    return 1;
  }
  return 0;
}
