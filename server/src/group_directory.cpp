#include "group_directory.h"

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

constexpr std::array<std::uint8_t, 8> kGroupDirMagic = {
    'M', 'I', 'G', 'D', 'I', 'R', '0', '1'};
constexpr std::uint8_t kGroupDirVersion = 1;
constexpr std::size_t kGroupDirHeaderBytes =
    kGroupDirMagic.size() + 1 + 3 + 4;

void WriteUint32Le(std::uint32_t v, std::vector<std::uint8_t>& out) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
}

std::uint32_t ReadUint32Le(const std::uint8_t* in) {
  return static_cast<std::uint32_t>(in[0]) |
         (static_cast<std::uint32_t>(in[1]) << 8) |
         (static_cast<std::uint32_t>(in[2]) << 16) |
         (static_cast<std::uint32_t>(in[3]) << 24);
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

GroupDirectory::GroupDirectory(std::filesystem::path persist_dir,
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
  persist_path_ = persist_dir / "group_directory.bin";
  persistence_enabled_ = true;
  if (!LoadFromDisk()) {
    if (!state_store_) {
      std::error_code ignore_ec;
      const auto bad_path = persist_path_.string() + ".bad";
      std::filesystem::rename(persist_path_, bad_path, ignore_ec);
    }
  }
}

std::string GroupDirectory::PickNewOwner(const GroupInfo& group) {
  std::string best;
  for (const auto& kv : group.members) {
    if (best.empty() || kv.first < best) {
      best = kv.first;
    }
  }
  return best;
}

bool GroupDirectory::LoadFromDisk() {
  if (state_store_) {
    return LoadFromStore();
  }
  return LoadFromFile();
}

bool GroupDirectory::LoadFromStore() {
  if (!state_store_) {
    return true;
  }
  BlobLoadResult blob;
  std::string load_err;
  if (!state_store_->LoadBlob("group_directory", blob, load_err)) {
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

bool GroupDirectory::LoadFromStoreLocked() {
  if (!state_store_) {
    return true;
  }
  BlobLoadResult blob;
  std::string load_err;
  if (!state_store_->LoadBlob("group_directory", blob, load_err)) {
    return false;
  }
  if (!blob.found || blob.data.empty()) {
    groups_.clear();
    return true;
  }
  return LoadFromBytes(blob.data);
}

bool GroupDirectory::LoadFromFile() {
  if (!persistence_enabled_ || persist_path_.empty()) {
    return true;
  }
  std::error_code ec;
  if (!std::filesystem::exists(persist_path_, ec) || ec) {
    return true;
  }

  const auto size = std::filesystem::file_size(persist_path_, ec);
  if (ec || size < kGroupDirHeaderBytes ||
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

bool GroupDirectory::LoadFromBytes(const std::vector<std::uint8_t>& bytes) {
  std::size_t off = 0;
  if (!std::equal(kGroupDirMagic.begin(), kGroupDirMagic.end(),
                  bytes.begin())) {
    return false;
  }
  off += kGroupDirMagic.size();
  const std::uint8_t version = bytes[off++];
  if (version != kGroupDirVersion) {
    return false;
  }
  off += 3;
  if (off + 4 > bytes.size()) {
    return false;
  }
  const std::uint32_t group_count = ReadUint32Le(bytes.data() + off);
  off += 4;

  std::unordered_map<std::string, GroupInfo> loaded;
  loaded.reserve(group_count);
  for (std::uint32_t i = 0; i < group_count; ++i) {
    if (off + 12 > bytes.size()) {
      return false;
    }
    const std::uint32_t group_len = ReadUint32Le(bytes.data() + off);
    off += 4;
    const std::uint32_t owner_len = ReadUint32Le(bytes.data() + off);
    off += 4;
    const std::uint32_t member_count = ReadUint32Le(bytes.data() + off);
    off += 4;
    if (group_len == 0 || off + group_len + owner_len > bytes.size()) {
      return false;
    }
    std::string group_id(
        reinterpret_cast<const char*>(bytes.data() + off),
        reinterpret_cast<const char*>(bytes.data() + off + group_len));
    off += group_len;
    std::string owner;
    if (owner_len != 0) {
      if (off + owner_len > bytes.size()) {
        return false;
      }
      owner.assign(reinterpret_cast<const char*>(bytes.data() + off),
                   reinterpret_cast<const char*>(bytes.data() + off + owner_len));
      off += owner_len;
    }

    GroupInfo info;
    info.owner = owner;
    for (std::uint32_t m = 0; m < member_count; ++m) {
      if (off + 4 > bytes.size()) {
        return false;
      }
      const std::uint32_t user_len = ReadUint32Le(bytes.data() + off);
      off += 4;
      if (user_len == 0 || off + user_len + 1 > bytes.size()) {
        return false;
      }
      std::string user(
          reinterpret_cast<const char*>(bytes.data() + off),
          reinterpret_cast<const char*>(bytes.data() + off + user_len));
      off += user_len;
      const std::uint8_t role_val = bytes[off++];
      if (role_val > static_cast<std::uint8_t>(GroupRole::kMember)) {
        return false;
      }
      info.members.emplace(std::move(user),
                           static_cast<GroupRole>(role_val));
    }

    if (!info.owner.empty()) {
      info.members[info.owner] = GroupRole::kOwner;
    } else {
      for (const auto& kv : info.members) {
        if (kv.second == GroupRole::kOwner) {
          info.owner = kv.first;
          break;
        }
      }
      if (info.owner.empty()) {
        const std::string new_owner = PickNewOwner(info);
        if (!new_owner.empty()) {
          info.owner = new_owner;
          info.members[new_owner] = GroupRole::kOwner;
        }
      }
    }
    if (info.members.empty()) {
      continue;
    }
    loaded.emplace(std::move(group_id), std::move(info));
  }
  if (off != bytes.size()) {
    return false;
  }

  groups_.swap(loaded);
  return true;
}

bool GroupDirectory::SaveLocked() {
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
  out.reserve(kGroupDirHeaderBytes + keys.size() * 32);
  out.insert(out.end(), kGroupDirMagic.begin(), kGroupDirMagic.end());
  out.push_back(kGroupDirVersion);
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
    const GroupInfo& info = it->second;
    if (group_id.size() >
            static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)()) ||
        info.owner.size() >
            static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)()) ||
        info.members.size() >
            static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)())) {
      return false;
    }
    WriteUint32Le(static_cast<std::uint32_t>(group_id.size()), out);
    WriteUint32Le(static_cast<std::uint32_t>(info.owner.size()), out);
    WriteUint32Le(static_cast<std::uint32_t>(info.members.size()), out);
    out.insert(out.end(), group_id.begin(), group_id.end());
    out.insert(out.end(), info.owner.begin(), info.owner.end());

    std::vector<std::string> members;
    members.reserve(info.members.size());
    for (const auto& kv : info.members) {
      members.push_back(kv.first);
    }
    std::sort(members.begin(), members.end());
    for (const auto& member : members) {
      const auto mit = info.members.find(member);
      if (mit == info.members.end()) {
        continue;
      }
      if (member.size() >
          static_cast<std::size_t>(
              (std::numeric_limits<std::uint32_t>::max)())) {
        return false;
      }
      WriteUint32Le(static_cast<std::uint32_t>(member.size()), out);
      out.insert(out.end(), member.begin(), member.end());
      out.push_back(static_cast<std::uint8_t>(mit->second));
    }
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

bool GroupDirectory::SaveToStoreLocked() {
  if (!state_store_) {
    return true;
  }
  std::string lock_err;
  StateStoreLock lock(state_store_, "group_directory",
                      std::chrono::milliseconds(5000), lock_err);
  if (!lock.locked()) {
    return false;
  }
  return SaveToStoreLockedUnlocked();
}

bool GroupDirectory::SaveToStoreLockedUnlocked() {
  if (!state_store_) {
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
  out.reserve(kGroupDirHeaderBytes + keys.size() * 32);
  out.insert(out.end(), kGroupDirMagic.begin(), kGroupDirMagic.end());
  out.push_back(kGroupDirVersion);
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
    const GroupInfo& info = it->second;
    if (group_id.size() >
            static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)()) ||
        info.owner.size() >
            static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)()) ||
        info.members.size() >
            static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)())) {
      return false;
    }
    WriteUint32Le(static_cast<std::uint32_t>(group_id.size()), out);
    WriteUint32Le(static_cast<std::uint32_t>(info.owner.size()), out);
    WriteUint32Le(static_cast<std::uint32_t>(info.members.size()), out);
    out.insert(out.end(), group_id.begin(), group_id.end());
    out.insert(out.end(), info.owner.begin(), info.owner.end());

    std::vector<std::string> members;
    members.reserve(info.members.size());
    for (const auto& kv : info.members) {
      members.push_back(kv.first);
    }
    std::sort(members.begin(), members.end());
    for (const auto& member : members) {
      const auto mit = info.members.find(member);
      if (mit == info.members.end()) {
        continue;
      }
      if (member.size() >
          static_cast<std::size_t>(
              (std::numeric_limits<std::uint32_t>::max)())) {
        return false;
      }
      WriteUint32Le(static_cast<std::uint32_t>(member.size()), out);
      out.insert(out.end(), member.begin(), member.end());
      out.push_back(static_cast<std::uint8_t>(mit->second));
    }
  }

  std::string store_err;
  if (!state_store_->SaveBlob("group_directory", out, store_err)) {
    return false;
  }
  return true;
}

bool GroupDirectory::AddGroup(const std::string& group_id,
                              const std::string& owner) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "group_directory",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && !store_lock.locked()) {
    return false;
  }
  if (state_store_ && store_lock.locked()) {
    if (!LoadFromStoreLocked()) {
      return false;
    }
  }
  if (group_id.empty() || owner.empty()) {
    return false;
  }
  if (groups_.count(group_id) != 0) {
    return false;
  }
  GroupInfo g;
  g.owner = owner;
  g.members.emplace(owner, GroupRole::kOwner);
  groups_[group_id] = std::move(g);
  if (state_store_ && store_lock.locked()) {
    if (!SaveToStoreLockedUnlocked()) {
      return false;
    }
  } else {
    SaveLocked();
  }
  return true;
}

bool GroupDirectory::AddMember(const std::string& group_id,
                               const std::string& user) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "group_directory",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && !store_lock.locked()) {
    return false;
  }
  if (state_store_ && store_lock.locked()) {
    if (!LoadFromStoreLocked()) {
      return false;
    }
  }
  if (group_id.empty() || user.empty()) {
    return false;
  }
  auto& g = groups_[group_id];
  if (g.members.empty()) {
    g.owner = user;
    g.members.emplace(user, GroupRole::kOwner);
    if (state_store_ && store_lock.locked()) {
      if (!SaveToStoreLockedUnlocked()) {
        return false;
      }
    } else {
      SaveLocked();
    }
    return true;
  }
  const auto [_, inserted] = g.members.emplace(user, GroupRole::kMember);
  if (inserted) {
    if (state_store_ && store_lock.locked()) {
      if (!SaveToStoreLockedUnlocked()) {
        return false;
      }
    } else {
      SaveLocked();
    }
  }
  return inserted;
}

bool GroupDirectory::RemoveMember(const std::string& group_id,
                                  const std::string& user) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "group_directory",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && !store_lock.locked()) {
    return false;
  }
  if (state_store_ && store_lock.locked()) {
    if (!LoadFromStoreLocked()) {
      return false;
    }
  }
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return false;
  }
  GroupInfo& g = it->second;
  const auto m_it = g.members.find(user);
  if (m_it == g.members.end()) {
    return false;
  }
  const bool was_owner = (g.owner == user);
  g.members.erase(m_it);

  if (g.members.empty()) {
    groups_.erase(it);
    if (state_store_ && store_lock.locked()) {
      if (!SaveToStoreLockedUnlocked()) {
        return false;
      }
    } else {
      SaveLocked();
    }
    return true;
  }

  if (was_owner) {
    g.owner.clear();
  }
  if (g.owner.empty()) {
    const std::string new_owner = PickNewOwner(g);
    if (!new_owner.empty()) {
      g.owner = new_owner;
      g.members[new_owner] = GroupRole::kOwner;
    }
  }

  if (state_store_ && store_lock.locked()) {
    if (!SaveToStoreLockedUnlocked()) {
      return false;
    }
  } else {
    SaveLocked();
  }
  return true;
}

bool GroupDirectory::HasMember(const std::string& group_id,
                               const std::string& user) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_store_) {
    (void)LoadFromStoreLocked();
  }
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return false;
  }
  return it->second.members.count(user) > 0;
}

std::vector<std::string> GroupDirectory::Members(
    const std::string& group_id) {
  std::vector<std::string> out;
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_store_) {
    (void)LoadFromStoreLocked();
  }
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return out;
  }
  out.reserve(it->second.members.size());
  for (const auto& kv : it->second.members) {
    out.push_back(kv.first);
  }
  return out;
}

std::vector<GroupMemberInfo> GroupDirectory::MembersWithRoles(
    const std::string& group_id) {
  std::vector<GroupMemberInfo> out;
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_store_) {
    (void)LoadFromStoreLocked();
  }
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return out;
  }
  out.reserve(it->second.members.size());
  for (const auto& kv : it->second.members) {
    GroupMemberInfo e;
    e.username = kv.first;
    e.role = kv.second;
    out.push_back(std::move(e));
  }
  return out;
}

std::optional<GroupRole> GroupDirectory::RoleOf(const std::string& group_id,
                                                const std::string& user) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_store_) {
    (void)LoadFromStoreLocked();
  }
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return std::nullopt;
  }
  const auto m_it = it->second.members.find(user);
  if (m_it == it->second.members.end()) {
    return std::nullopt;
  }
  return m_it->second;
}

bool GroupDirectory::SetRole(const std::string& group_id,
                             const std::string& user, GroupRole role) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string lock_err;
  StateStoreLock store_lock(state_store_, "group_directory",
                            std::chrono::milliseconds(5000), lock_err);
  if (state_store_ && !store_lock.locked()) {
    return false;
  }
  if (state_store_ && store_lock.locked()) {
    if (!LoadFromStoreLocked()) {
      return false;
    }
  }
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return false;
  }
  GroupInfo& g = it->second;
  if (user.empty() || g.owner == user) {
    return false;
  }
  if (role == GroupRole::kOwner) {
    return false;
  }
  const auto m_it = g.members.find(user);
  if (m_it == g.members.end()) {
    return false;
  }
  m_it->second = role;
  if (state_store_ && store_lock.locked()) {
    if (!SaveToStoreLockedUnlocked()) {
      return false;
    }
  } else {
    SaveLocked();
  }
  return true;
}

}  // namespace mi::server
