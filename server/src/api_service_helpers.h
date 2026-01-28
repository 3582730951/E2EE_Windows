#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "api_service.h"
#include "media_frame.h"
#include "protocol.h"
#include "protected_store.h"

namespace mi::server::api_helpers {

constexpr std::uint8_t kGroupNoticeJoin = 1;
constexpr std::uint8_t kGroupNoticeLeave = 2;
constexpr std::uint8_t kGroupNoticeKick = 3;
constexpr std::uint8_t kGroupNoticeRoleSet = 4;

inline std::vector<std::uint8_t> BuildGroupNoticePayload(
    std::uint8_t kind, const std::string& target_username,
    std::optional<GroupRole> role = std::nullopt) {
  std::vector<std::uint8_t> out;
  out.reserve(1 + 2 + target_username.size() + (role.has_value() ? 1u : 0u));
  out.push_back(kind);
  mi::server::proto::WriteString(target_username, out);
  if (kind == kGroupNoticeRoleSet && role.has_value()) {
    out.push_back(static_cast<std::uint8_t>(role.value()));
  }
  return out;
}

inline bool DecodeGroupCallSubscriptions(
    const std::vector<std::uint8_t>& ext,
    std::vector<GroupCallSubscription>& out,
    std::string& error) {
  out.clear();
  error.clear();
  if (ext.empty()) {
    return true;
  }
  std::size_t off = 0;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(ext, off, count)) {
    error = "subscription payload invalid";
    return false;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::string sender;
    if (!mi::server::proto::ReadString(ext, off, sender)) {
      error = "subscription payload invalid";
      return false;
    }
    if (off >= ext.size()) {
      error = "subscription payload invalid";
      return false;
    }
    const std::uint8_t flags = ext[off++];
    GroupCallSubscription sub;
    sub.sender = std::move(sender);
    sub.media_flags = flags;
    out.push_back(std::move(sub));
  }
  if (off != ext.size()) {
    error = "subscription payload invalid";
    return false;
  }
  return true;
}

inline bool PeekMediaPacketKindFlag(const std::vector<std::uint8_t>& payload,
                                    std::uint8_t& out_flag) {
  out_flag = 0;
  if (payload.size() < 2) {
    return false;
  }
  const std::uint8_t version = payload[0];
  const std::uint8_t kind = payload[1];
  const std::size_t min_size_v2 = 1 + 1 + 4 + 16;
  const std::size_t min_size_v3 = 1 + 1 + 4 + 4 + 16;
  if (version == 2) {
    if (payload.size() < min_size_v2) {
      return false;
    }
  } else if (version == 3) {
    if (payload.size() < min_size_v3) {
      return false;
    }
  } else {
    return false;
  }

  if (kind == static_cast<std::uint8_t>(mi::media::StreamKind::kAudio)) {
    out_flag = mi::server::kGroupCallMediaAudio;
    return true;
  }
  if (kind == static_cast<std::uint8_t>(mi::media::StreamKind::kVideo)) {
    out_flag = mi::server::kGroupCallMediaVideo;
    return true;
  }
  return false;
}

inline bool ReadFileBytes(const std::filesystem::path& path,
                          std::vector<std::uint8_t>& out,
                          std::string& error) {
  error.clear();
  out.clear();
  if (path.empty()) {
    error = "kt signing key path empty";
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    error = ec ? "kt signing key path error" : "kt signing key not found";
    return false;
  }
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    error = "kt signing key not found";
    return false;
  }
  std::error_code size_ec;
  const std::uint64_t size = std::filesystem::file_size(path, size_ec);
  if (size_ec ||
      size > static_cast<std::uint64_t>(
                 (std::numeric_limits<std::size_t>::max)())) {
    error = "kt signing key read failed";
    return false;
  }
  std::vector<std::uint8_t> file_bytes;
  file_bytes.resize(static_cast<std::size_t>(size));
  if (!file_bytes.empty()) {
    ifs.read(reinterpret_cast<char*>(file_bytes.data()),
             static_cast<std::streamsize>(file_bytes.size()));
    if (!ifs || ifs.gcount() != static_cast<std::streamsize>(file_bytes.size())) {
      error = "kt signing key read failed";
      return false;
    }
  }
  if (!DecodeProtectedFileBytes(file_bytes, out, error)) {
    return false;
  }
  if (out.size() != kKtSthSigSecretKeyBytes) {
    error = "kt signing key size invalid";
    out.clear();
    return false;
  }
  return true;
}

inline bool LooksLikeHexId(const std::string& s, std::size_t expect_len) {
  if (expect_len != 0 && s.size() != expect_len) {
    return false;
  }
  if (s.empty()) {
    return false;
  }
  for (const char c : s) {
    const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                    (c >= 'A' && c <= 'F');
    if (!ok) {
      return false;
    }
  }
  return true;
}

inline std::string MakeDeviceQueueKey(const std::string& username,
                                      const std::string& device_id) {
  return username + "|" + device_id;
}

inline std::string MakePairingRequestQueueKey(
    const std::string& username,
    const std::string& pairing_id_hex) {
  return "pair_req|" + username + "|" + pairing_id_hex;
}

inline std::string MakePairingResponseQueueKey(
    const std::string& username,
    const std::string& pairing_id_hex,
    const std::string& device_id) {
  return "pair_resp|" + username + "|" + pairing_id_hex + "|" + device_id;
}

}  // namespace mi::server::api_helpers
