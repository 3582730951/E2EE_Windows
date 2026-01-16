#include <cassert>
#include <cstdint>

#include "c_api_client.h"

int main() {
  mi_sdk_version version{};
  mi_client_get_version(&version);
  assert(version.abi == MI_E2EE_SDK_ABI_VERSION);
  assert(version.major == MI_E2EE_SDK_VERSION_MAJOR);

  const std::uint32_t caps = mi_client_get_capabilities();
  assert((caps & MI_CLIENT_CAP_CHAT) != 0);
  assert((caps & MI_CLIENT_CAP_GROUP) != 0);
  assert((caps & MI_CLIENT_CAP_MEDIA) != 0);
  assert((caps & MI_CLIENT_CAP_GROUP_CALL) != 0);
  assert((caps & MI_CLIENT_CAP_OFFLINE) != 0);
  assert((caps & MI_CLIENT_CAP_DEVICE_SYNC) != 0);
  assert((caps & MI_CLIENT_CAP_KCP) != 0);
  assert((caps & MI_CLIENT_CAP_OPAQUE) != 0);
  return 0;
}
