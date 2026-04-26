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

bool RunPolicy(const std::filesystem::path& file_list,
               const std::string& branch) {
  const std::string cmd = "bash " + ShellQuote(MI_E2EE_GIT_POLICY_SH) +
                          " --changed-file-list " +
                          ShellQuote(file_list.string()) + " --branch " +
                          ShellQuote(branch) + " >/dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
}

}  // namespace

int main() {
  std::error_code ec;
  const auto root =
      std::filesystem::temp_directory_path(ec) / "mi_e2ee_repository_policy";
  if (ec) {
    return 1;
  }
  std::filesystem::remove_all(root, ec);
  if (!mi::server::test::EnsureOwnerOnlyDirectory(root)) {
    return 1;
  }
  const auto list_path = root / "changed.list";

  if (!WriteFile(list_path,
                 "core/client/include/client_core.h\n"
                 "core/client/tests/CMakeLists.txt\n"
                 "core/server/src/config.cpp\n"
                 "core/README.md\n")) {
    return 1;
  }
  if (!RunPolicy(list_path, "e2ee_dev")) {
    std::cerr << "allowed repository policy fixture failed\n";
    return 1;
  }
  if (RunPolicy(list_path, "main")) {
    std::cerr << "repository policy allowed non-e2ee_dev branch\n";
    return 1;
  }

  if (!WriteFile(list_path, "core/docs/security.md\n")) {
    return 1;
  }
  if (RunPolicy(list_path, "e2ee_dev")) {
    std::cerr << "repository policy allowed non-README markdown\n";
    return 1;
  }

  if (!WriteFile(list_path, "core/server/debug_fixture.txt\n")) {
    return 1;
  }
  if (RunPolicy(list_path, "e2ee_dev")) {
    std::cerr << "repository policy allowed ordinary txt file\n";
    return 1;
  }

  if (!WriteFile(list_path, "core/client/assets/ref/ref_login.png\n")) {
    return 1;
  }
  if (RunPolicy(list_path, "e2ee_dev")) {
    std::cerr << "repository policy allowed forbidden asset directory\n";
    return 1;
  }

  if (!WriteFile(list_path, "docs/outside_core.cpp\n")) {
    return 1;
  }
  if (RunPolicy(list_path, "e2ee_dev")) {
    std::cerr << "repository policy allowed writes outside core\n";
    return 1;
  }
  return 0;
}
