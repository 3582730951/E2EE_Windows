#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool read_file(const std::filesystem::path& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  out.assign(std::istreambuf_iterator<char>(in),
             std::istreambuf_iterator<char>());
  return true;
}

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

}

int main() {
  const std::filesystem::path source = MI_E2EE_QUICK_CLIENT_SOURCE;
  const std::filesystem::path helpers_source =
      MI_E2EE_QUICK_CLIENT_HELPERS_SOURCE;
  std::string body;
  if (!read_file(source, body)) {
    std::cerr << "missing QuickClient source: " << source << "\n";
    return 1;
  }
  std::string helpers;
  if (!read_file(helpers_source, helpers)) {
    std::cerr << "missing QuickClient helpers source: " << helpers_source
              << "\n";
    return 1;
  }
  if (!contains(helpers, "sanitize_download_file_name(") ||
      !contains(helpers, "QFileInfo(fileName.trimmed()).fileName()") ||
      !contains(helpers, "ch == QLatin1Char('/')") ||
      !contains(helpers, "ch == QLatin1Char('\\\\')")) {
    std::cerr << "download filename sanitizer does not collapse path input\n";
    return 1;
  }
  if (!contains(body, "sanitize_download_file_name(\n        fileName") ||
      !contains(body, "sanitize_download_file_name(\n      fileName") ||
      !contains(body, "sanitize_download_file_name(\n          fileName")) {
    std::cerr << "download path flow does not use sanitized filenames\n";
    return 1;
  }
  if (contains(body, "QDir(base).filePath(fileName.trimmed())") ||
      contains(body, ": fileName.trimmed();")) {
    std::cerr << "download path flow still joins unsanitized remote filename\n";
    return 1;
  }
  return 0;
}
