#ifndef MI_E2EE_SHARD_PATH_SECURITY_H
#define MI_E2EE_SHARD_PATH_SECURITY_H

#include <filesystem>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <aclapi.h>
#endif

namespace mi::shard::security {

inline bool CheckPathNotWorldWritable(const std::filesystem::path& path,
                                      std::string& error) {
  error.clear();
#ifdef _WIN32
  const std::wstring wpath = path.wstring();
  PSECURITY_DESCRIPTOR sd = nullptr;
  PACL dacl = nullptr;
  const DWORD rc = GetNamedSecurityInfoW(
      wpath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr,
      nullptr, &dacl, nullptr, &sd);
  if (rc != ERROR_SUCCESS) {
    error = "acl read failed";
    return false;
  }
  if (!dacl) {
    if (sd) {
      LocalFree(sd);
    }
    error = "acl missing";
    return false;
  }

  BYTE sid_buf[SECURITY_MAX_SID_SIZE];
  DWORD sid_size = static_cast<DWORD>(sizeof(sid_buf));
  PSID sid_everyone = sid_buf;
  if (!CreateWellKnownSid(WinWorldSid, nullptr, sid_everyone, &sid_size)) {
    if (sd) {
      LocalFree(sd);
    }
    error = "acl sid failed";
    return false;
  }

  BYTE sid_auth_buf[SECURITY_MAX_SID_SIZE];
  DWORD sid_auth_size = static_cast<DWORD>(sizeof(sid_auth_buf));
  PSID sid_auth = sid_auth_buf;
  if (!CreateWellKnownSid(WinAuthenticatedUserSid, nullptr, sid_auth,
                          &sid_auth_size)) {
    if (sd) {
      LocalFree(sd);
    }
    error = "acl sid failed";
    return false;
  }

  BYTE sid_users_buf[SECURITY_MAX_SID_SIZE];
  DWORD sid_users_size = static_cast<DWORD>(sizeof(sid_users_buf));
  PSID sid_users = sid_users_buf;
  if (!CreateWellKnownSid(WinBuiltinUsersSid, nullptr, sid_users,
                          &sid_users_size)) {
    if (sd) {
      LocalFree(sd);
    }
    error = "acl sid failed";
    return false;
  }

  BYTE sid_guests_buf[SECURITY_MAX_SID_SIZE];
  DWORD sid_guests_size = static_cast<DWORD>(sizeof(sid_guests_buf));
  PSID sid_guests = sid_guests_buf;
  if (!CreateWellKnownSid(WinBuiltinGuestsSid, nullptr, sid_guests,
                          &sid_guests_size)) {
    if (sd) {
      LocalFree(sd);
    }
    error = "acl sid failed";
    return false;
  }

  const PSID target_sids[] = {sid_everyone, sid_auth, sid_users, sid_guests};
  constexpr DWORD kWriteMask =
      FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA |
      FILE_WRITE_ATTRIBUTES | DELETE | WRITE_DAC | WRITE_OWNER |
      GENERIC_WRITE | GENERIC_ALL;

  for (DWORD i = 0; i < dacl->AceCount; ++i) {
    void* ace = nullptr;
    if (!GetAce(dacl, i, &ace) || !ace) {
      continue;
    }
    const auto* header = reinterpret_cast<const ACE_HEADER*>(ace);
    if (header->AceType == ACCESS_ALLOWED_ACE_TYPE) {
      const auto* allowed = reinterpret_cast<const ACCESS_ALLOWED_ACE*>(ace);
      const DWORD mask = allowed->Mask;
      if ((mask & kWriteMask) == 0) {
        continue;
      }
      const PSID ace_sid =
          const_cast<PSID>(reinterpret_cast<const void*>(
              &allowed->SidStart));
      for (PSID target : target_sids) {
        if (EqualSid(ace_sid, target)) {
          error = "insecure acl (world-writable)";
          LocalFree(sd);
          return false;
        }
      }
    } else if (header->AceType == ACCESS_ALLOWED_OBJECT_ACE_TYPE) {
      const auto* allowed =
          reinterpret_cast<const ACCESS_ALLOWED_OBJECT_ACE*>(ace);
      const DWORD mask = allowed->Mask;
      if ((mask & kWriteMask) == 0) {
        continue;
      }
      std::size_t offset = sizeof(ACCESS_ALLOWED_OBJECT_ACE);
      if (allowed->Flags & ACE_OBJECT_TYPE_PRESENT) {
        offset += sizeof(GUID);
      }
      if (allowed->Flags & ACE_INHERITED_OBJECT_TYPE_PRESENT) {
        offset += sizeof(GUID);
      }
      const PSID ace_sid = reinterpret_cast<PSID>(
          reinterpret_cast<const BYTE*>(ace) + offset);
      for (PSID target : target_sids) {
        if (EqualSid(ace_sid, target)) {
          error = "insecure acl (world-writable)";
          LocalFree(sd);
          return false;
        }
      }
    }
  }

  if (sd) {
    LocalFree(sd);
  }
  return true;
#else
  (void)path;
  (void)error;
  return true;
#endif
}

}  // namespace mi::shard::security

#endif  // MI_E2EE_SHARD_PATH_SECURITY_H
