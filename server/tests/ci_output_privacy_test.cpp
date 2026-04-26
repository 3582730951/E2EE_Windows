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

bool RunCiOutputScan(const std::filesystem::path& path) {
  const std::string cmd = "bash " + ShellQuote(MI_E2EE_CI_OUTPUT_VERIFY_SH) +
                          " --input " + ShellQuote(path.string()) +
                          " >/dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
}

bool ExpectRejected(const std::filesystem::path& path, const std::string& body,
                    const char* label) {
  if (!WriteFile(path, body)) {
    return false;
  }
  if (RunCiOutputScan(path)) {
    std::cerr << "CI output scan allowed " << label << "\n";
    return false;
  }
  return true;
}

}  // namespace

int main() {
  std::error_code ec;
  const auto root =
      std::filesystem::temp_directory_path(ec) / "mi_e2ee_ci_output_scan";
  if (ec) {
    return 1;
  }
  std::filesystem::remove_all(root, ec);
  if (!mi::server::test::EnsureOwnerOnlyDirectory(root)) {
    return 1;
  }
  const auto path = root / "build.out";

  if (!WriteFile(path,
                 "[100%] Built target mi_e2ee_core\n"
                 "Test project /workspace/core/build/server\n"
                 "100% tests passed, 0 tests failed out of 5\n")) {
    return 1;
  }
  if (!RunCiOutputScan(path)) {
    std::cerr << "clean CI output fixture failed\n";
    return 1;
  }

  if (!ExpectRejected(path, "file_key=00112233445566778899\n", "file key")) {
    return 1;
  }
  if (!ExpectRejected(path, "payload_hex=00112233\n", "payload marker")) {
    return 1;
  }
  if (!ExpectRejected(path, "token=raw-session-token\n", "session token")) {
    return 1;
  }
  if (!ExpectRejected(path, "local_path=/home/alice/private/photo.jpg\n",
                      "local path")) {
    return 1;
  }
  if (!ExpectRejected(path, "wrote /home/alice/private/photo.jpg\n",
                      "unix home path")) {
    return 1;
  }
  if (!ExpectRejected(path, "wrote C:\\Users\\Alice\\Desktop\\secret.txt\n",
                      "windows user path")) {
    return 1;
  }
  if (!ExpectRejected(path, "github_pat_0123456789abcdefghijABCDE\n",
                      "GitHub token")) {
    return 1;
  }
  if (!ExpectRejected(path, "Authorization: Bearer abcdefghijklmnop12345678\n",
                      "bearer token")) {
    return 1;
  }
  return 0;
}
