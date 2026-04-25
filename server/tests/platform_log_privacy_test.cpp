#include "platform_log.h"

#include <cstddef>

namespace {

void CountCallback(mi::platform::log::Level,
                   const char*,
                   const char*,
                   const mi::platform::log::Field*,
                   std::size_t,
                   void* user_data) {
  auto* count = static_cast<int*>(user_data);
  ++(*count);
}

}  // namespace

int main() {
  int count = 0;
  mi::platform::log::SetLogCallback(&CountCallback, &count);
  mi::platform::log::Log(mi::platform::log::Level::kError, "privacy",
                         "token=secret password=secret",
                         {{"device_id", "device-raw-value"}});
#ifdef MI_E2EE_PRIVACY_STRICT
  if (!mi::platform::log::PrivacyStrictRuntime()) {
    return 1;
  }
  return count == 0 ? 0 : 1;
#else
  if (mi::platform::log::PrivacyStrictRuntime()) {
    return 1;
  }
  return count == 1 ? 0 : 1;
#endif
}
