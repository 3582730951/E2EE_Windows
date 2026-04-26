#ifndef MI_E2EE_IOS_BRIDGE_H
#define MI_E2EE_IOS_BRIDGE_H

#include <stdint.h>

#define MI_IOS_CLIENT_SDK_ABI_VERSION 2u

#ifdef __cplusplus
#include "../c_api_client.h"

static_assert(MI_IOS_CLIENT_SDK_ABI_VERSION == MI_E2EE_SDK_ABI_VERSION,
              "iOS bridge ABI version must match the native SDK ABI");
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Swift and ObjC clients use this thin bridge as the stable module surface.
static inline uint32_t mi_ios_client_sdk_abi_version(void) {
  return MI_IOS_CLIENT_SDK_ABI_VERSION;
}

#ifdef __cplusplus
}
#endif

#endif  // MI_E2EE_IOS_BRIDGE_H
