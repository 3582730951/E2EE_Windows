#include "trust_store.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

#include "secure_store_util.h"
#include "path_security.h"
#include "platform_fs.h"

namespace mi::client::security {

namespace pfs = mi::platform::fs;

namespace {

constexpr char kTrustStoreMagic[] = "MI_TRUST1";
constexpr char kTrustStoreEntropy[] = "mi_e2ee_trust_store_v1";

bool StoreTrustStoreText(const std::string& path, const std::string& text,
                         std::string& error);

std::string Trim(const std::string& input) {
  const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  auto begin = std::find_if_not(input.begin(), input.end(), is_space);
  auto end = std::find_if_not(input.rbegin(), input.rend(), is_space).base();
  if (begin >= end) {
    return {};
  }
  return std::string(begin, end);
}

std::string StripInlineComment(const std::string& input) {
  for (std::size_t i = 0; i < input.size(); ++i) {
    const char ch = input[i];
    if ((ch == '#' || ch == ';') &&
        (i == 0 ||
         std::isspace(static_cast<unsigned char>(input[i - 1])) != 0)) {
      return Trim(input.substr(0, i));
    }
  }
  return input;
}

std::string ToLower(std::string s) {
  for (auto& ch : s) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return s;
}

bool ParseTrustValue(const std::string& value, TrustEntry& out) {
  out = TrustEntry{};
  if (value.empty()) {
    return false;
  }
  std::string v = value;
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= v.size()) {
    const auto pos = v.find(',', start);
    if (pos == std::string::npos) {
      parts.push_back(Trim(v.substr(start)));
      break;
    }
    parts.push_back(Trim(v.substr(start, pos - start)));
    start = pos + 1;
  }
  if (parts.empty() || parts[0].empty()) {
    return false;
  }
  std::string fp = ToLower(Trim(parts[0]));
  if (!IsHex64(fp)) {
    return false;
  }
  out.fingerprint = fp;
  for (std::size_t i = 1; i < parts.size(); ++i) {
    const std::string token = ToLower(parts[i]);
    if (token == "tls=1" || token == "tls=true" || token == "tls=on" ||
        token == "tls_required=1" || token == "tls_required=true") {
      out.tls_required = true;
    }
  }
  return true;
}

std::string BuildTrustValue(const TrustEntry& entry) {
  if (entry.fingerprint.empty()) {
    return {};
  }
  std::string out = entry.fingerprint;
  if (entry.tls_required) {
    out += ",tls=1";
  }
  return out;
}

bool LoadTrustStoreText(const std::string& path, std::string& out_text,
                        std::string& error) {
  out_text.clear();
  error.clear();
  if (path.empty()) {
    error = "trust store path empty";
    return false;
  }
  {
    std::error_code ec;
    if (pfs::Exists(path, ec) && !ec) {
      std::string perm_err;
      if (!mi::shard::security::CheckPathNotWorldWritable(path, perm_err)) {
        error = perm_err.empty() ? "trust store permissions insecure" : perm_err;
        return false;
      }
    }
  }
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) {
    return false;
  }
  std::error_code ec;
  const std::uint64_t size = pfs::FileSize(path, ec);
  if (ec) {
    error = "trust store read failed";
    return false;
  }
  if (size == 0) {
    return false;
  }
  if (size > static_cast<std::uint64_t>(
                 (std::numeric_limits<std::size_t>::max)())) {
    error = "trust store too large";
    return false;
  }
  std::vector<std::uint8_t> bytes;
  bytes.resize(static_cast<std::size_t>(size));
  f.read(reinterpret_cast<char*>(bytes.data()),
         static_cast<std::streamsize>(bytes.size()));
  if (!f || f.gcount() != static_cast<std::streamsize>(bytes.size())) {
    error = "trust store read failed";
    return false;
  }
  if (bytes.empty()) {
    return false;
  }
  std::vector<std::uint8_t> plain;
  bool was_dpapi = false;
  if (!MaybeUnprotectSecureStore(bytes, kTrustStoreMagic, kTrustStoreEntropy, plain,
                           was_dpapi, error)) {
    return false;
  }
  const auto& view = was_dpapi ? plain : bytes;
  out_text.assign(view.begin(), view.end());
  if (!was_dpapi) {
    std::string wrap_err;
    if (!StoreTrustStoreText(path, out_text, wrap_err)) {
      error = wrap_err.empty() ? "trust store rewrap failed" : wrap_err;
      return false;
    }
  }
  return true;
}

bool StoreTrustStoreText(const std::string& path, const std::string& text,
                         std::string& error) {
  error.clear();
  if (path.empty()) {
    error = "trust store path empty";
    return false;
  }
  std::string perm_err;
  if (!mi::shard::security::CheckPathNotWorldWritable(path, perm_err)) {
    error = perm_err.empty() ? "trust store permissions insecure" : perm_err;
    return false;
  }

  std::vector<std::uint8_t> out_bytes;
  std::vector<std::uint8_t> plain(text.begin(), text.end());
  if (!ProtectSecureStore(plain, kTrustStoreMagic, kTrustStoreEntropy, out_bytes,
                    error)) {
    return false;
  }
  std::error_code write_ec;
  if (!pfs::AtomicWrite(path, out_bytes.data(), out_bytes.size(), write_ec)) {
    error = "open trust store failed";
    return false;
  }
#ifndef _WIN32
  {
    std::error_code ec;
    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace, ec);
  }
#else
  {
    std::string acl_err;
    if (!mi::shard::security::HardenPathAcl(path, acl_err)) {
      error = acl_err.empty() ? "trust store acl harden failed" : acl_err;
      return false;
    }
  }
#endif
  return true;
}

}  // namespace

std::string EndpointKey(const std::string& host, std::uint16_t port) {
  return host + ":" + std::to_string(port);
}

std::string NormalizeFingerprint(std::string v) {
  return ToLower(Trim(v));
}

std::string NormalizeCode(const std::string& input) {
  std::string out;
  out.reserve(input.size());
  for (char c : input) {
    const unsigned char uc = static_cast<unsigned char>(c);
    if (std::isspace(uc) != 0 || c == '-') {
      continue;
    }
    if (c >= 'A' && c <= 'Z') {
      out.push_back(static_cast<char>(c - 'A' + 'a'));
      continue;
    }
    out.push_back(c);
  }
  return out;
}

bool IsHex64(const std::string& v) {
  if (v.size() != 64) {
    return false;
  }
  for (const char c : v) {
    if (!std::isxdigit(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  return true;
}

bool LoadTrustEntry(const std::string& path, const std::string& endpoint,
                    TrustEntry& out_entry) {
  out_entry = TrustEntry{};
  if (path.empty() || endpoint.empty()) {
    return false;
  }
  std::string content;
  std::string load_err;
  if (!LoadTrustStoreText(path, content, load_err)) {
    return false;
  }
  std::istringstream iss(content);
  std::string line;
  while (std::getline(iss, line)) {
    const std::string t = StripInlineComment(Trim(line));
    if (t.empty()) {
      continue;
    }
    const auto pos = t.find('=');
    if (pos == std::string::npos) {
      continue;
    }
    const std::string key = Trim(t.substr(0, pos));
    const std::string val = Trim(t.substr(pos + 1));
    if (key == endpoint && !val.empty()) {
      TrustEntry entry;
      if (ParseTrustValue(val, entry)) {
        out_entry = entry;
        return true;
      }
      return false;
    }
  }
  return false;
}

bool StoreTrustEntry(const std::string& path, const std::string& endpoint,
                     const TrustEntry& entry, std::string& error) {
  error.clear();
  if (path.empty() || endpoint.empty() || entry.fingerprint.empty()) {
    error = "invalid trust store input";
    return false;
  }

  std::vector<std::pair<std::string, std::string>> entries;
  std::string content;
  std::string load_err;
  if (LoadTrustStoreText(path, content, load_err)) {
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
      const std::string t = StripInlineComment(Trim(line));
      if (t.empty()) {
        continue;
      }
      const auto pos = t.find('=');
      if (pos == std::string::npos) {
        continue;
      }
      const std::string key = Trim(t.substr(0, pos));
      const std::string val = Trim(t.substr(pos + 1));
      if (key.empty() || val.empty() || key == endpoint) {
        continue;
      }
      entries.emplace_back(key, val);
    }
  }
  entries.emplace_back(endpoint, BuildTrustValue(entry));
  std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  std::error_code ec;
  const std::filesystem::path p(path);
  const auto dir =
      p.has_parent_path() ? p.parent_path() : std::filesystem::path{};
  if (!dir.empty()) {
    pfs::CreateDirectories(dir, ec);
  }
  std::ostringstream oss;
  oss << "# mi_e2ee client trust store\n";
  oss << "# format: host:port=sha256(cert_der)_hex[,tls=1]\n";
  for (const auto& kv : entries) {
    oss << kv.first << "=" << kv.second << "\n";
  }
  if (!StoreTrustStoreText(path, oss.str(), error)) {
    return false;
  }
  return true;
}

}  // namespace mi::client::security
