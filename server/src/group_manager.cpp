#include "group_manager.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>
#include <vector>

#include "path_security.h"

namespace mi::server {

namespace {

constexpr std::array<std::uint8_t, 8> kGroupMgrMagic = {
    'M', 'I', 'G', 'M', 'G', 'R', '0', '1'};
constexpr std::uint8_t kGroupMgrVersion = 1;
constexpr std::size_t kGroupMgrHeaderBytes =
    kGroupMgrMagic.size() + 1 + 3 + 4;

void WriteUint32Le(std::uint32_t v, std::vector<std::uint8_t>& out) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
}

void WriteUint64Le(std::uint64_t v, std::vector<std::uint8_t>& out) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 32) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 40) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 48) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 56) & 0xFFu));
}

std::uint32_t ReadUint32Le(const std::uint8_t* in) {
  return static_cast<std::uint32_t>(in[0]) |
         (static_cast<std::uint32_t>(in[1]) << 8) |
         (static_cast<std::uint32_t>(in[2]) << 16) |
         (static_cast<std::uint32_t>(in[3]) << 24);
}

std::uint64_t ReadUint64Le(const std::uint8_t* in) {
  return static_cast<std::uint64_t>(in[0]) |
         (static_cast<std::uint64_t>(in[1]) << 8) |
         (static_cast<std::uint64_t>(in[2]) << 16) |
         (static_cast<std::uint64_t>(in[3]) << 24) |
         (static_cast<std::uint64_t>(in[4]) << 32) |
         (static_cast<std::uint64_t>(in[5]) << 40) |
         (static_cast<std::uint64_t>(in[6]) << 48) |
         (static_cast<std::uint64_t>(in[7]) << 56);
}

void SetOwnerOnlyPermissions(const std::filesystem::path& path) {
#ifdef _WIN32
  std::string acl_err;
  (void)mi::shard::security::HardenPathAcl(path, acl_err);
#else
  std::error_code ec;
  std::filesystem::permissions(
      path, std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace, ec);
#endif
}

}  // namespace

GroupManager::GroupManager(std::filesystem::path persist_dir) {
  if (persist_dir.empty()) {
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(persist_dir, ec);
  if (ec) {
    return;
  }
  persist_path_ = persist_dir / "group_manager.bin";
  persistence_enabled_ = true;
  if (!LoadFromDisk()) {
    const auto bad_path = persist_path_.string() + ".bad";
    std::filesystem::rename(persist_path_, bad_path, ec);
  }
}

GroupKey GroupManager::MakeKey(std::uint32_t next_version,
                               RotationReason reason) {
  GroupKey key;
  key.version = next_version;
  key.reason = reason;
  return key;
}

bool GroupManager::LoadFromDisk() {
  if (!persistence_enabled_ || persist_path_.empty()) {
    return true;
  }
  std::error_code ec;
  if (!std::filesystem::exists(persist_path_, ec) || ec) {
    return true;
  }

  const auto size = std::filesystem::file_size(persist_path_, ec);
  if (ec || size < kGroupMgrHeaderBytes ||
      size > static_cast<std::uint64_t>(
                 (std::numeric_limits<std::size_t>::max)())) {
    return false;
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  std::ifstream ifs(persist_path_, std::ios::binary);
  if (!ifs) {
    return false;
  }
  ifs.read(reinterpret_cast<char*>(bytes.data()),
           static_cast<std::streamsize>(bytes.size()));
  if (!ifs ||
      ifs.gcount() != static_cast<std::streamsize>(bytes.size())) {
    return false;
  }

  std::size_t off = 0;
  if (!std::equal(kGroupMgrMagic.begin(), kGroupMgrMagic.end(),
                  bytes.begin())) {
    return false;
  }
  off += kGroupMgrMagic.size();
  const std::uint8_t version = bytes[off++];
  if (version != kGroupMgrVersion) {
    return false;
  }
  off += 3;
  if (off + 4 > bytes.size()) {
    return false;
  }
  const std::uint32_t group_count = ReadUint32Le(bytes.data() + off);
  off += 4;

  std::unordered_map<std::string, GroupState> loaded;
  loaded.reserve(group_count);
  for (std::uint32_t i = 0; i < group_count; ++i) {
    if (off + 4 > bytes.size()) {
      return false;
    }
    const std::uint32_t group_len = ReadUint32Le(bytes.data() + off);
    off += 4;
    if (group_len == 0 || off + group_len > bytes.size()) {
      return false;
    }
    std::string group_id(
        reinterpret_cast<const char*>(bytes.data() + off),
        reinterpret_cast<const char*>(bytes.data() + off + group_len));
    off += group_len;
    if (off + 4 + 1 + 8 > bytes.size()) {
      return false;
    }
    const std::uint32_t version_num = ReadUint32Le(bytes.data() + off);
    off += 4;
    const std::uint8_t reason_val = bytes[off++];
    const std::uint64_t message_count = ReadUint64Le(bytes.data() + off);
    off += 8;
    if (reason_val >
        static_cast<std::uint8_t>(RotationReason::kMessageThreshold)) {
      return false;
    }

    GroupState state;
    state.group_id = std::move(group_id);
    state.key.version = version_num;
    state.key.reason = static_cast<RotationReason>(reason_val);
    state.message_count = message_count;
    loaded.emplace(state.group_id, std::move(state));
  }
  if (off != bytes.size()) {
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    groups_.swap(loaded);
  }
  return true;
}

bool GroupManager::SaveLocked() {
  if (!persistence_enabled_ || persist_path_.empty()) {
    return true;
  }
  if (groups_.size() >
      static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())) {
    return false;
  }

  std::vector<std::string> keys;
  keys.reserve(groups_.size());
  for (const auto& kv : groups_) {
    keys.push_back(kv.first);
  }
  std::sort(keys.begin(), keys.end());

  std::vector<std::uint8_t> out;
  out.reserve(kGroupMgrHeaderBytes + keys.size() * 24);
  out.insert(out.end(), kGroupMgrMagic.begin(), kGroupMgrMagic.end());
  out.push_back(kGroupMgrVersion);
  out.push_back(0);
  out.push_back(0);
  out.push_back(0);
  WriteUint32Le(static_cast<std::uint32_t>(keys.size()), out);

  for (const auto& group_id : keys) {
    const auto it = groups_.find(group_id);
    if (it == groups_.end()) {
      continue;
    }
    if (group_id.empty()) {
      return false;
    }
    if (group_id.size() >
        static_cast<std::size_t>(
            (std::numeric_limits<std::uint32_t>::max)())) {
      return false;
    }
    WriteUint32Le(static_cast<std::uint32_t>(group_id.size()), out);
    out.insert(out.end(), group_id.begin(), group_id.end());
    WriteUint32Le(it->second.key.version, out);
    out.push_back(static_cast<std::uint8_t>(it->second.key.reason));
    WriteUint64Le(it->second.message_count, out);
  }

  const std::filesystem::path tmp = persist_path_.string() + ".tmp";
  std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    return false;
  }
  ofs.write(reinterpret_cast<const char*>(out.data()),
            static_cast<std::streamsize>(out.size()));
  ofs.close();
  if (!ofs.good()) {
    std::error_code rm_ec;
    std::filesystem::remove(tmp, rm_ec);
    return false;
  }
  std::error_code ec;
  std::filesystem::remove(persist_path_, ec);
  std::filesystem::rename(tmp, persist_path_, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    return false;
  }
  SetOwnerOnlyPermissions(persist_path_);
  return true;
}

GroupKey GroupManager::Rotate(const std::string& group_id,
                              RotationReason reason) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& state = groups_[group_id];
  state.group_id = group_id;
  const std::uint32_t next_version = state.key.version + 1;
  state.key = MakeKey(next_version, reason);
  state.message_count = 0;
  SaveLocked();
  return state.key;
}

std::optional<GroupKey> GroupManager::GetKey(const std::string& group_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return std::nullopt;
  }
  return it->second.key;
}

std::optional<GroupKey> GroupManager::OnMessage(const std::string& group_id,
                                                std::uint64_t threshold) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    GroupState state;
    state.group_id = group_id;
    state.key = MakeKey(1, RotationReason::kJoin);
    state.message_count = 1;
    groups_.emplace(group_id, std::move(state));
    SaveLocked();
    return std::nullopt;
  }
  it->second.message_count += 1;
  if (threshold > 0 && it->second.message_count >= threshold) {
    const std::uint32_t next_version = it->second.key.version + 1;
    it->second.key = MakeKey(next_version, RotationReason::kMessageThreshold);
    it->second.message_count = 0;
    SaveLocked();
    return it->second.key;
  }
  return std::nullopt;
}

}  // namespace mi::server
