#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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

}  // namespace

int main() {
  const std::filesystem::path source = MI_E2EE_QUICK_CLIENT_SOURCE;
  std::string body;
  if (!ReadFile(source, body)) {
    std::cerr << "missing QuickClient source: " << source << "\n";
    return 1;
  }
  if (!Contains(body, "#include \"protected_text_vm.h\"") ||
      !Contains(body, "WithProtectedUiText(") ||
      !Contains(body, "UiProtectedText::Protect(") ||
      !Contains(body, "kUiPlaintextLease")) {
    std::cerr << "Quick UI text path is not routed through protected VM leases\n";
    return 1;
  }
  if (Contains(body, "msg.insert(QStringLiteral(\"text\"), message);")) {
    std::cerr << "sendText still inserts a persistent plaintext QString directly\n";
    return 1;
  }
  if (Contains(body, "msg.insert(QStringLiteral(\"text\"), text);")) {
    std::cerr << "poll paths still insert decrypted text without a lease boundary\n";
    return 1;
  }
  if (Contains(body, "entry.text ? QString::fromUtf8(entry.text) : QString()")) {
    std::cerr << "history replay still inserts decoded text without a lease boundary\n";
    return 1;
  }
  return 0;
}
