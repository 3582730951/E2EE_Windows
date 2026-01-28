#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "client_config.h"
#include "key_transparency.h"
#include "platform_fs.h"
#include "platform_sys.h"

namespace mi::client::core_helpers {

namespace pfs = mi::platform::fs;

constexpr std::size_t kKtRootPubkeyBytes = mi::server::kKtSthSigPublicKeyBytes;

inline std::string Trim(const std::string& input) {
  const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  auto begin = std::find_if_not(input.begin(), input.end(), is_space);
  auto end = std::find_if_not(input.rbegin(), input.rend(), is_space).base();
  if (begin >= end) {
    return {};
  }
  return std::string(begin, end);
}

inline std::string ToLower(std::string s) {
  for (auto& ch : s) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return s;
}

inline bool IsLoopbackHost(const std::string& host) {
  const std::string h = ToLower(Trim(host));
  return h == "127.0.0.1" || h == "localhost" || h == "::1";
}

inline bool ReadFileBytes(const std::filesystem::path& path,
                          std::vector<std::uint8_t>& out,
                          std::string& error) {
  error.clear();
  out.clear();
  if (path.empty()) {
    error = "kt root pubkey path empty";
    return false;
  }
  std::error_code ec;
  if (!pfs::Exists(path, ec)) {
    error = ec ? "kt root pubkey path error" : "kt root pubkey not found";
    return false;
  }
  const auto size = pfs::FileSize(path, ec);
  if (ec) {
    error = "kt root pubkey size stat failed";
    return false;
  }
  if (size != kKtRootPubkeyBytes) {
    error = "kt root pubkey size invalid";
    return false;
  }
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    error = "kt root pubkey not found";
    return false;
  }
  out.resize(static_cast<std::size_t>(size));
  ifs.read(reinterpret_cast<char*>(out.data()),
           static_cast<std::streamsize>(out.size()));
  if (!ifs) {
    out.clear();
    error = "kt root pubkey read failed";
    return false;
  }
  return true;
}

inline bool TryLoadKtRootPubkeyFromLoopback(
    const std::filesystem::path& base_dir,
    const std::string& host,
    std::vector<std::uint8_t>& out,
    std::string& error) {
  out.clear();
  error.clear();
  if (!IsLoopbackHost(host)) {
    return false;
  }
  std::vector<std::filesystem::path> candidates;
  const std::filesystem::path base = base_dir.empty()
                                         ? std::filesystem::path{"."}
                                         : base_dir;
  candidates.emplace_back(base / "kt_root_pub.bin");
  candidates.emplace_back(base / "offline_store" / "kt_root_pub.bin");
  const auto parent = base.parent_path();
  if (!parent.empty()) {
    candidates.emplace_back(parent / "s" / "kt_root_pub.bin");
    candidates.emplace_back(parent / "s" / "offline_store" / "kt_root_pub.bin");
    candidates.emplace_back(parent / "server" / "kt_root_pub.bin");
    candidates.emplace_back(parent / "server" / "offline_store" /
                            "kt_root_pub.bin");
  }
  std::string last_err;
  for (const auto& path : candidates) {
    std::string read_err;
    if (ReadFileBytes(path, out, read_err)) {
      return true;
    }
    if (!read_err.empty()) {
      last_err = read_err;
    }
  }
  error = last_err.empty() ? "kt root pubkey missing" : last_err;
  return false;
}

inline bool IsLowEndDevice() {
  const unsigned int hc = std::thread::hardware_concurrency();
  if (hc != 0 && hc <= 4) {
    return true;
  }
  const std::uint64_t total = mi::platform::SystemMemoryTotalBytes();
  constexpr std::uint64_t kLowEndMem = 4ull * 1024ull * 1024ull * 1024ull;
  if (total != 0 && total <= kLowEndMem) {
    return true;
  }
  return false;
}

inline bool ResolveCoverTrafficEnabled(const TrafficConfig& cfg) {
  switch (cfg.cover_traffic_mode) {
    case CoverTrafficMode::kOn:
      return true;
    case CoverTrafficMode::kOff:
      return false;
    case CoverTrafficMode::kAuto:
    default:
      return !IsLowEndDevice();
  }
}

}  // namespace mi::client::core_helpers
