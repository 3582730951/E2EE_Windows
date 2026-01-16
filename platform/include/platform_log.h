#ifndef MI_E2EE_PLATFORM_LOG_H
#define MI_E2EE_PLATFORM_LOG_H

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

namespace mi::platform::log {

enum class Level : std::uint8_t {
  kDebug = 0,
  kInfo = 1,
  kWarn = 2,
  kError = 3
};

struct Field {
  std::string_view key;
  std::string_view value;
};

using LogCallback = void (*)(Level level,
                             const char* tag,
                             const char* message,
                             const Field* fields,
                             std::size_t field_count,
                             void* user_data);

void SetLogCallback(LogCallback cb, void* user_data);

void Log(Level level, std::string_view tag, std::string_view message);
void Log(Level level,
         std::string_view tag,
         std::string_view message,
         std::initializer_list<Field> fields);

bool IsSensitiveKey(std::string_view key);
std::string RedactValue(std::string_view key, std::string_view value);
std::string RedactMessage(std::string_view message);

}  // namespace mi::platform::log

#endif  // MI_E2EE_PLATFORM_LOG_H
