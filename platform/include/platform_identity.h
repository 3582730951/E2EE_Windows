#ifndef MI_E2EE_PLATFORM_IDENTITY_H
#define MI_E2EE_PLATFORM_IDENTITY_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mi::platform {

std::string MachineId();

bool TpmSupported();

bool TpmWrapKey(const std::array<std::uint8_t, 32>& key_bytes,
                std::vector<std::uint8_t>& out_wrapped,
                std::string& error);

bool TpmUnwrapKey(const std::vector<std::uint8_t>& wrapped,
                  std::array<std::uint8_t, 32>& out_key,
                  std::string& error);

}  // namespace mi::platform

#endif  // MI_E2EE_PLATFORM_IDENTITY_H
