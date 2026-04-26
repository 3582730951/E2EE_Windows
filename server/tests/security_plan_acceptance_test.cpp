#include <cstdlib>
#include <iostream>
#include <string>

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

}  // namespace

int main() {
  const std::string cmd =
      "python3 " + ShellQuote(MI_E2EE_SECURITY_PLAN_VERIFY_PY) + " --root " +
      ShellQuote(MI_E2EE_REPOSITORY_ROOT);
  const int rc = std::system(cmd.c_str());
  if (rc != 0) {
    std::cerr << "security plan acceptance verifier failed\n";
    return 1;
  }
  return 0;
}
