#ifndef MI_E2EE_HEX_UTILS_H
#define MI_E2EE_HEX_UTILS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mi::common {

std::string Sha256Hex(const std::uint8_t* data, std::size_t len);
bool HexToBytes(const std::string& hex, std::vector<std::uint8_t>& out);
bool HexToBytes(std::string_view hex, std::vector<std::uint8_t>& out);
std::string GroupHex4(const std::string& hex);

}  // namespace mi::common

#endif  // MI_E2EE_HEX_UTILS_H
