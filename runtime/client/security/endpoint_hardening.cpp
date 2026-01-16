#include "endpoint_hardening.h"
#include "platform_security.h"

namespace mi::client::security {

void StartEndpointHardening() noexcept {
  mi::platform::StartEndpointHardening();
}

}  // namespace mi::client::security
