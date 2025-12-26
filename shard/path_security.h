#ifndef MI_E2EE_SHARD_PATH_SECURITY_H
#define MI_E2EE_SHARD_PATH_SECURITY_H

#include <filesystem>
#include <string>
#include <vector>

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
      const void* sid_ptr = &allowed->SidStart;
      PSID ace_sid = const_cast<void*>(sid_ptr);
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
      const auto* ace_bytes = reinterpret_cast<const BYTE*>(ace);
      PSID ace_sid = const_cast<BYTE*>(ace_bytes + offset);
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

inline bool HardenPathAcl(const std::filesystem::path& path,
                          std::string& error) {
  error.clear();
#ifdef _WIN32
  const std::wstring wpath = path.wstring();
  PSECURITY_DESCRIPTOR sd = nullptr;
  PACL dacl = nullptr;
  DWORD rc = GetNamedSecurityInfoW(
      wpath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr,
      nullptr, &dacl, nullptr, &sd);
  if (rc != ERROR_SUCCESS) {
    error = "acl read failed";
    return false;
  }

  BYTE sid_everyone_buf[SECURITY_MAX_SID_SIZE];
  DWORD sid_everyone_size = static_cast<DWORD>(sizeof(sid_everyone_buf));
  PSID sid_everyone = sid_everyone_buf;
  if (!CreateWellKnownSid(WinWorldSid, nullptr, sid_everyone,
                          &sid_everyone_size)) {
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

  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    if (sd) {
      LocalFree(sd);
    }
    error = "acl token failed";
    return false;
  }
  DWORD token_len = 0;
  GetTokenInformation(token, TokenUser, nullptr, 0, &token_len);
  std::vector<BYTE> token_buf(token_len);
  if (!GetTokenInformation(token, TokenUser, token_buf.data(), token_len,
                           &token_len)) {
    CloseHandle(token);
    if (sd) {
      LocalFree(sd);
    }
    error = "acl token failed";
    return false;
  }
  const auto* token_user =
      reinterpret_cast<const TOKEN_USER*>(token_buf.data());
  PSID sid_user = token_user->User.Sid;
  CloseHandle(token);

  BYTE sid_admin_buf[SECURITY_MAX_SID_SIZE];
  DWORD sid_admin_size = static_cast<DWORD>(sizeof(sid_admin_buf));
  PSID sid_admin = sid_admin_buf;
  if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, sid_admin,
                          &sid_admin_size)) {
    if (sd) {
      LocalFree(sd);
    }
    error = "acl sid failed";
    return false;
  }

  BYTE sid_system_buf[SECURITY_MAX_SID_SIZE];
  DWORD sid_system_size = static_cast<DWORD>(sizeof(sid_system_buf));
  PSID sid_system = sid_system_buf;
  if (!CreateWellKnownSid(WinLocalSystemSid, nullptr, sid_system,
                          &sid_system_size)) {
    if (sd) {
      LocalFree(sd);
    }
    error = "acl sid failed";
    return false;
  }

  auto ace_size_for_sid = [](PSID sid) -> DWORD {
    return static_cast<DWORD>(sizeof(ACCESS_ALLOWED_ACE) +
                              GetLengthSid(sid) - sizeof(DWORD));
  };

  DWORD new_acl_size = sizeof(ACL);
  if (dacl) {
    for (DWORD i = 0; i < dacl->AceCount; ++i) {
      void* ace = nullptr;
      if (!GetAce(dacl, i, &ace) || !ace) {
        continue;
      }
      const auto* header = reinterpret_cast<const ACE_HEADER*>(ace);
      bool keep = true;
      if (header->AceType == ACCESS_ALLOWED_ACE_TYPE) {
        const auto* allowed = reinterpret_cast<const ACCESS_ALLOWED_ACE*>(ace);
        if ((allowed->Mask & kWriteMask) != 0) {
          const void* sid_ptr = &allowed->SidStart;
          PSID ace_sid = const_cast<void*>(sid_ptr);
          for (PSID target : target_sids) {
            if (EqualSid(ace_sid, target)) {
              keep = false;
              break;
            }
          }
        }
      } else if (header->AceType == ACCESS_ALLOWED_OBJECT_ACE_TYPE) {
        const auto* allowed =
            reinterpret_cast<const ACCESS_ALLOWED_OBJECT_ACE*>(ace);
        if ((allowed->Mask & kWriteMask) != 0) {
          std::size_t offset = sizeof(ACCESS_ALLOWED_OBJECT_ACE);
          if (allowed->Flags & ACE_OBJECT_TYPE_PRESENT) {
            offset += sizeof(GUID);
          }
          if (allowed->Flags & ACE_INHERITED_OBJECT_TYPE_PRESENT) {
            offset += sizeof(GUID);
          }
          const auto* ace_bytes = reinterpret_cast<const BYTE*>(ace);
          PSID ace_sid = const_cast<BYTE*>(ace_bytes + offset);
          for (PSID target : target_sids) {
            if (EqualSid(ace_sid, target)) {
              keep = false;
              break;
            }
          }
        }
      }
      if (keep) {
        new_acl_size += header->AceSize;
      }
    }
  }

  new_acl_size += ace_size_for_sid(sid_user);
  new_acl_size += ace_size_for_sid(sid_admin);
  new_acl_size += ace_size_for_sid(sid_system);

  PACL new_dacl =
      reinterpret_cast<PACL>(LocalAlloc(LPTR, new_acl_size));
  if (!new_dacl || !InitializeAcl(new_dacl, new_acl_size, ACL_REVISION)) {
    if (new_dacl) {
      LocalFree(new_dacl);
    }
    if (sd) {
      LocalFree(sd);
    }
    error = "acl update failed";
    return false;
  }

  if (dacl) {
    for (DWORD i = 0; i < dacl->AceCount; ++i) {
      void* ace = nullptr;
      if (!GetAce(dacl, i, &ace) || !ace) {
        continue;
      }
      const auto* header = reinterpret_cast<const ACE_HEADER*>(ace);
      bool keep = true;
      if (header->AceType == ACCESS_ALLOWED_ACE_TYPE) {
        const auto* allowed = reinterpret_cast<const ACCESS_ALLOWED_ACE*>(ace);
        if ((allowed->Mask & kWriteMask) != 0) {
          const void* sid_ptr = &allowed->SidStart;
          PSID ace_sid = const_cast<void*>(sid_ptr);
          for (PSID target : target_sids) {
            if (EqualSid(ace_sid, target)) {
              keep = false;
              break;
            }
          }
        }
      } else if (header->AceType == ACCESS_ALLOWED_OBJECT_ACE_TYPE) {
        const auto* allowed =
            reinterpret_cast<const ACCESS_ALLOWED_OBJECT_ACE*>(ace);
        if ((allowed->Mask & kWriteMask) != 0) {
          std::size_t offset = sizeof(ACCESS_ALLOWED_OBJECT_ACE);
          if (allowed->Flags & ACE_OBJECT_TYPE_PRESENT) {
            offset += sizeof(GUID);
          }
          if (allowed->Flags & ACE_INHERITED_OBJECT_TYPE_PRESENT) {
            offset += sizeof(GUID);
          }
          const auto* ace_bytes = reinterpret_cast<const BYTE*>(ace);
          PSID ace_sid = const_cast<BYTE*>(ace_bytes + offset);
          for (PSID target : target_sids) {
            if (EqualSid(ace_sid, target)) {
              keep = false;
              break;
            }
          }
        }
      }
      if (keep) {
        if (!AddAce(new_dacl, ACL_REVISION, MAXDWORD, ace, header->AceSize)) {
          LocalFree(new_dacl);
          if (sd) {
            LocalFree(sd);
          }
          error = "acl update failed";
          return false;
        }
      }
    }
  }

  constexpr DWORD kFullControl = GENERIC_ALL;
  constexpr DWORD kInheritFlags = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
  if (!AddAccessAllowedAceEx(new_dacl, ACL_REVISION, kInheritFlags,
                             kFullControl, sid_user) ||
      !AddAccessAllowedAceEx(new_dacl, ACL_REVISION, kInheritFlags,
                             kFullControl, sid_admin) ||
      !AddAccessAllowedAceEx(new_dacl, ACL_REVISION, kInheritFlags,
                             kFullControl, sid_system)) {
    LocalFree(new_dacl);
    if (sd) {
      LocalFree(sd);
    }
    error = "acl update failed";
    return false;
  }

  rc = SetNamedSecurityInfoW(
      const_cast<wchar_t*>(wpath.c_str()), SE_FILE_OBJECT,
      DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
      nullptr, nullptr, new_dacl, nullptr);
  LocalFree(new_dacl);
  if (sd) {
    LocalFree(sd);
  }
  if (rc != ERROR_SUCCESS) {
    error = "acl set failed";
    return false;
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
