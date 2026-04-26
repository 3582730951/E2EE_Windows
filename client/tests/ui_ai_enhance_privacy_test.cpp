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
  if (!Contains(body, "QStandardPaths::TempLocation") ||
      !Contains(body, "mi_e2ee_ai_upscale") ||
      !Contains(body, "applicationPid")) {
    std::cerr << "AI enhance output is not scoped to an ephemeral process temp dir\n";
    return 1;
  }
  if (Contains(body, "QDir(dataDir).filePath(QStringLiteral(\"ai_upscale\"))")) {
    std::cerr << "AI enhance output still uses persistent UI data dir\n";
    return 1;
  }
  if (Contains(body, "QProcess::execute(")) {
    std::cerr << "AI enhance still forwards model subprocess output\n";
    return 1;
  }
  if (!Contains(body, "RunRealEsrganQuietly(") ||
      !Contains(body, "setProcessChannelMode(QProcess::SeparateChannels)") ||
      !Contains(body, "readAllStandardOutput") ||
      !Contains(body, "readAllStandardError")) {
    std::cerr << "AI enhance subprocess output is not explicitly discarded\n";
    return 1;
  }
  return 0;
}
