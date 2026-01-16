#ifndef MI_E2EE_CONSTANT_TIME_H
#define MI_E2EE_CONSTANT_TIME_H

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace mi::common {

inline bool ConstantTimeEqual(std::string_view a, std::string_view b) {
  const std::size_t max_len = a.size() > b.size() ? a.size() : b.size();
  std::size_t diff = a.size() ^ b.size();
  for (std::size_t i = 0; i < max_len; ++i) {
    const std::uint8_t ac =
        i < a.size() ? static_cast<std::uint8_t>(a[i]) : 0;
    const std::uint8_t bc =
        i < b.size() ? static_cast<std::uint8_t>(b[i]) : 0;
    diff |= static_cast<std::size_t>(ac ^ bc);
  }
  return diff == 0;
}

}  // namespace mi::common

#endif  // MI_E2EE_CONSTANT_TIME_H
