#ifndef MI_E2EE_SERVER_PROTECTED_STORE_H
#define MI_E2EE_SERVER_PROTECTED_STORE_H

#include <string>
#include <vector>

#include "config.h"

namespace mi::server {

bool EncodeProtectedFileBytes(const std::vector<std::uint8_t>& plain,
                              KeyProtectionMode mode,
                              std::vector<std::uint8_t>& out,
                              std::string& error);

bool DecodeProtectedFileBytes(const std::vector<std::uint8_t>& file_bytes,
                              KeyProtectionMode mode,
                              std::vector<std::uint8_t>& out_plain,
                              bool& was_protected,
                              std::string& error);

bool DecodeProtectedFileBytes(const std::vector<std::uint8_t>& file_bytes,
                              std::vector<std::uint8_t>& out_plain,
                              std::string& error);

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_PROTECTED_STORE_H
