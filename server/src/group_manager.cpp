#include "group_manager.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>
#include <vector>

#include "path_security.h"
#include "protected_store.h"

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

GroupManager::GroupManager(std::filesystem::path persist_dir,
                           KeyProtectionMode state_protection,
                           StateStore* state_store)
    : state_protection_(state_protection),
      state_store_(state_store) {
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
    if (!state_store_) {
      const auto bad_path = persist_path_.string() + ".bad";
      std::filesystem::rename(persist_path_, bad_path, ec);
    }
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
  if (state_store_) {
    return LoadFromStore();
  }
  return LoadFromFile();
}

bool GroupManager::LoadFromFile() {
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

  bool need_rewrap = false;
  {
    std::vector<std::uint8_t> plain;
    bool was_protected = false;
    std::string protect_err;
    if (!DecodeProtectedFileBytes(bytes, state_protection_, plain, was_protected,
                                  protect_err)) {
      return false;
    }
    need_rewrap =
        !was_protected && state_protection_ != KeyProtectionMode::kNone;
    bytes.swap(plain);
  }
  if (!LoadFromBytes(bytes)) {
    return false;
  }
  if (need_rewrap && !state_store_) {
    SaveLocked();
  }
  return true;
}

bool GroupManager::LoadFromStore() {
  if (!state_store_) {
    return true;
  }
  BlobLoadResult blob;
  std::string load_err;
  if (!state_store_->LoadBlob("group_manager", blob, load_err)) {
    return false;
  }
  if (!blob.found || blob.data.empty()) {
    if (!persist_path_.empty()) {
      std::error_code ec;
      if (std::filesystem::exists(persist_path_, ec) && !ec) {
        if (!LoadFromFile()) {
          return false;
        }
        return SaveToStoreLocked();
      }
    }
    return true;
  }
  return LoadFromBytes(blob.data);
}

bool GroupManager::LoadFromStoreLocked() {
  if (!state_store_) {
    return true;
  }
  BlobLoadResult blob;
  std::string load_err;
  if (!state_store_->LoadBlob("group_manager", blob, load_err)) {
    return false;
  }
  if (!blob.found || blob.data.empty()) {
    groups_.clear();
    return true;
  }
  return LoadFromBytes(blob.data);
}

bool GroupManager::LoadFromBytes(const std::vector<std::uint8_t>& bytes) {
  std::size_t off = 0;
  if (bytes.size() < kGroupMgrHeaderBytes ||
      !std::equal(kGroupMgrMagic.begin(), kGroupMgrMagic.end(),
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

  groups_.swap(loaded);
  return true;
}

bool GroupManager::SaveLocked() {
  if (state_store_) {
    return SaveToStoreLocked();
  }
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

  std::vector<std::uint8_t> protected_bytes;
  std::string protect_err;
  if (!EncodeProtectedFileBytes(out, state_protection_, protected_bytes,
                                protect_err)) {
    return false;
  }

  const std::filesystem::path tmp = persist_path_.string() + ".tmp";
  std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    return false;
  }
  ofs.write(reinterpret_cast<const char*>(protected_bytes.data()),
            static_cast<std::streamsize>(protected_bytes.size()));
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

bool GroupManager::SaveToStoreLocked() {
  if (!state_store_) {
    return true;
  }
  std::string lock_err;
  StateStoreLock lock(state_store_, "group_manager",
                      std::chrono::milliseconds(5000), lock_err);
  if (!lock.locked()) {
    return false;
  }
  return SaveToStoreLockedUnlocked();
}

bool GroupManager::SaveToStoreLockedUnlocked() {
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

  std::string store_err;
  if (!state_store_->SaveBlob("group_manager", out, store_err)) {
    return false;
  }
  return true;
}

GroupKey GroupManager::Rotate(const std::string& group_id,
                              RotationReason reason) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "group_manager",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && store_lock.locked()) {
    (void)LoadFromStoreLocked();
  }
  auto& state = groups_[group_id];
  state.group_id = group_id;
  const std::uint32_t next_version = state.key.version + 1;
  state.key = MakeKey(next_version, reason);
  state.message_count = 0;
  if (state_store_ && store_lock.locked()) {
    (void)SaveToStoreLockedUnlocked();
  } else {
    SaveLocked();
  }
  return state.key;
}

std::optional<GroupKey> GroupManager::GetKey(const std::string& group_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_store_) {
    (void)LoadFromStoreLocked();
  }
  const auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return std::nullopt;
  }
  return it->second.key;
}

std::optional<GroupKey> GroupManager::OnMessage(const std::string& group_id,
                                                std::uint64_t threshold) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "group_manager",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && store_lock.locked()) {
    (void)LoadFromStoreLocked();
  }
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    GroupState state;
    state.group_id = group_id;
    state.key = MakeKey(1, RotationReason::kJoin);
    state.message_count = 1;
    groups_.emplace(group_id, std::move(state));
    if (state_store_ && store_lock.locked()) {
      (void)SaveToStoreLockedUnlocked();
    } else {
      SaveLocked();
    }
    return std::nullopt;
  }
  it->second.message_count += 1;
  if (threshold > 0 && it->second.message_count >= threshold) {
    const std::uint32_t next_version = it->second.key.version + 1;
    it->second.key = MakeKey(next_version, RotationReason::kMessageThreshold);
    it->second.message_count = 0;
    if (state_store_ && store_lock.locked()) {
      (void)SaveToStoreLockedUnlocked();
    } else {
      SaveLocked();
    }
    return it->second.key;
  }
  return std::nullopt;
}

}  // namespace mi::server
