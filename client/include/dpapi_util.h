#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mi::client {

bool MaybeUnprotectDpapi(const std::vector<std::uint8_t>& in,
                         const char* magic,
                         const char* entropy,
                         std::vector<std::uint8_t>& out_plain,
                         bool& out_was_dpapi,
                         std::string& error);

bool ProtectDpapi(const std::vector<std::uint8_t>& plain,
                  const char* magic,
                  const char* entropy,
                  std::vector<std::uint8_t>& out_wrapped,
                  std::string& error);

}  // namespace mi::client

