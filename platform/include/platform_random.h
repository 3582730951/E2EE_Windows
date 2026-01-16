#ifndef MI_E2EE_PLATFORM_RANDOM_H
#define MI_E2EE_PLATFORM_RANDOM_H

#include <cstddef>
#include <cstdint>

namespace mi::platform {

bool RandomBytes(std::uint8_t* out, std::size_t len);
bool RandomUint32(std::uint32_t& out);

}  // namespace mi::platform

#endif  // MI_E2EE_PLATFORM_RANDOM_H
