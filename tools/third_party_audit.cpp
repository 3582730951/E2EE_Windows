#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "hex_utils.h"

namespace {

struct LockEntry {
  std::string name;
  std::string version;
  std::string license_id;
  std::string rel_path;
  std::string sha256;
};

std::string Trim(const std::string& in) {
  std::size_t start = 0;
  while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start])) != 0) {
    ++start;
  }
  std::size_t end = in.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(in[end - 1])) != 0) {
    --end;
  }
  return in.substr(start, end - start);
}

bool SplitPipe(const std::string& line, std::vector<std::string>& out) {
  out.clear();
  std::string current;
  for (char ch : line) {
    if (ch == '|') {
      out.push_back(current);
      current.clear();
    } else {
      current.push_back(ch);
    }
  }
  out.push_back(current);
  return out.size() >= 5;
}

bool LoadLockFile(const std::filesystem::path& path,
                  std::vector<LockEntry>& out,
                  std::string& error) {
  out.clear();
  error.clear();
  std::ifstream ifs(path);
  if (!ifs) {
    error = "lock file not found";
    return false;
  }
  std::string line;
  std::vector<std::string> parts;
  while (std::getline(ifs, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    if (!SplitPipe(line, parts) || parts.size() < 5) {
      error = "invalid lock line";
      return false;
    }
    LockEntry entry;
    entry.name = parts[0];
    entry.version = Trim(parts[1]);
    entry.license_id = Trim(parts[2]);
    entry.rel_path = Trim(parts[3]);
    entry.sha256 = Trim(parts[4]);
    entry.name = Trim(entry.name);
    if (entry.name.empty() || entry.rel_path.empty() || entry.sha256.empty()) {
      error = "invalid lock entry";
      return false;
    }
    out.push_back(std::move(entry));
  }
  return !out.empty();
}

bool HashFile(const std::filesystem::path& path, std::string& out) {
  out.clear();
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    return false;
  }
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec) {
    return false;
  }
  if (size > static_cast<std::uintmax_t>(
                 (std::numeric_limits<std::size_t>::max)())) {
    return false;
  }
  std::vector<std::uint8_t> bytes;
  bytes.resize(static_cast<std::size_t>(size));
  if (!bytes.empty()) {
    ifs.read(reinterpret_cast<char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
    if (!ifs || ifs.gcount() != static_cast<std::streamsize>(bytes.size())) {
      return false;
    }
  }
  out = mi::common::Sha256Hex(bytes.data(), bytes.size());
  return true;
}

bool HashDirectory(const std::filesystem::path& root, std::string& out) {
  out.clear();
  std::vector<std::pair<std::string, std::string>> entries;
  std::error_code ec;
  if (!std::filesystem::exists(root, ec) || ec) {
    return false;
  }
  for (const auto& it :
       std::filesystem::recursive_directory_iterator(root, ec)) {
    if (ec) {
      return false;
    }
    if (!it.is_regular_file()) {
      continue;
    }
    const auto rel = std::filesystem::relative(it.path(), root, ec);
    if (ec) {
      return false;
    }
    const std::string rel_str = rel.generic_string();
    std::string file_hash;
    if (!HashFile(it.path(), file_hash)) {
      return false;
    }
    entries.emplace_back(rel_str, std::move(file_hash));
  }
  std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  std::vector<std::uint8_t> buf;
  for (const auto& entry : entries) {
    buf.insert(buf.end(), entry.first.begin(), entry.first.end());
    buf.push_back(0);
    buf.insert(buf.end(), entry.second.begin(), entry.second.end());
    buf.push_back(0);
  }
  out = mi::common::Sha256Hex(buf.data(), buf.size());
  return true;
}

bool HashPath(const std::filesystem::path& path, std::string& out) {
  std::error_code ec;
  if (std::filesystem::is_regular_file(path, ec) && !ec) {
    return HashFile(path, out);
  }
  if (std::filesystem::is_directory(path, ec) && !ec) {
    return HashDirectory(path, out);
  }
  return false;
}

void WriteJsonEscaped(std::ostream& os, const std::string& value) {
  os << '"';
  for (char ch : value) {
    switch (ch) {
      case '\\':
        os << "\\\\";
        break;
      case '"':
        os << "\\\"";
        break;
      case '\b':
        os << "\\b";
        break;
      case '\f':
        os << "\\f";
        break;
      case '\n':
        os << "\\n";
        break;
      case '\r':
        os << "\\r";
        break;
      case '\t':
        os << "\\t";
        break;
      default:
        os << ch;
        break;
    }
  }
  os << '"';
}

bool WriteSbom(const std::filesystem::path& out_path,
               const std::filesystem::path& lock_path,
               const std::vector<LockEntry>& entries,
               const std::vector<std::string>& hashes) {
  std::ofstream ofs(out_path, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    return false;
  }

  const auto lock_dir = lock_path.parent_path();
  const auto core_dir = lock_dir.parent_path();

  ofs << "{\n";
  ofs << "  \"bomFormat\": \"CycloneDX\",\n";
  ofs << "  \"specVersion\": \"1.5\",\n";
  ofs << "  \"version\": 1,\n";
  ofs << "  \"metadata\": {\n";
  ofs << "    \"component\": {\n";
  ofs << "      \"type\": \"application\",\n";
  ofs << "      \"name\": \"mi_e2ee\",\n";
  ofs << "      \"version\": \"local\"\n";
  ofs << "    }\n";
  ofs << "  },\n";
  ofs << "  \"components\": [\n";
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    std::filesystem::path display_path =
        core_dir.filename() / lock_dir.filename() / entry.rel_path;
    ofs << "    {\n";
    ofs << "      \"type\": \"library\",\n";
    ofs << "      \"name\": ";
    WriteJsonEscaped(ofs, entry.name);
    ofs << ",\n";
    ofs << "      \"version\": ";
    WriteJsonEscaped(ofs, entry.version);
    ofs << ",\n";
    ofs << "      \"licenses\": [\n";
    ofs << "        { \"license\": { \"id\": ";
    WriteJsonEscaped(ofs, entry.license_id.empty() ? "UNKNOWN" : entry.license_id);
    ofs << " } }\n";
    ofs << "      ],\n";
    ofs << "      \"hashes\": [\n";
    ofs << "        { \"alg\": \"SHA-256\", \"content\": ";
    WriteJsonEscaped(ofs, hashes[i]);
    ofs << " }\n";
    ofs << "      ],\n";
    ofs << "      \"properties\": [\n";
    ofs << "        { \"name\": \"path\", \"value\": ";
    WriteJsonEscaped(ofs, display_path.generic_string());
    ofs << " }\n";
    ofs << "      ]\n";
    ofs << "    }";
    if (i + 1 < entries.size()) {
      ofs << ",";
    }
    ofs << "\n";
  }
  ofs << "  ]\n";
  ofs << "}\n";
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path lock_path;
  std::filesystem::path sbom_path;
  bool verify = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--lock" && i + 1 < argc) {
      lock_path = argv[++i];
    } else if (arg == "--sbom" && i + 1 < argc) {
      sbom_path = argv[++i];
    } else if (arg == "--verify") {
      verify = true;
    } else if (arg == "--help") {
      std::cout << "Usage: third_party_audit --lock <path> [--verify] [--sbom <out>]\n";
      return 0;
    }
  }

  if (lock_path.empty()) {
    std::cerr << "lock file required\n";
    return 2;
  }

  std::vector<LockEntry> entries;
  std::string error;
  if (!LoadLockFile(lock_path, entries, error)) {
    std::cerr << "load lock failed: " << error << "\n";
    return 2;
  }

  const auto base_dir = lock_path.parent_path();
  std::vector<std::string> hashes;
  hashes.reserve(entries.size());
  for (const auto& entry : entries) {
    const auto dir = base_dir / entry.rel_path;
    std::string hash;
    if (!HashPath(dir, hash)) {
      std::cerr << "hash failed: " << dir.string() << "\n";
      return 3;
    }
    hashes.push_back(hash);
    if (verify && hash != entry.sha256) {
      std::cerr << "hash mismatch: " << entry.name << "\n";
      return 4;
    }
  }

  if (!sbom_path.empty()) {
    if (!WriteSbom(sbom_path, lock_path, entries, hashes)) {
      std::cerr << "sbom write failed\n";
      return 5;
    }
  }

  return 0;
}
