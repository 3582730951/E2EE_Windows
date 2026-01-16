#ifndef MI_E2EE_SDK_CPP_CLIENT_ADAPTER_H
#define MI_E2EE_SDK_CPP_CLIENT_ADAPTER_H

#include <cstdint>
#include <string>
#include <vector>

#include "c_api_client.h"
#include "sdk_client_types.h"

namespace mi::sdk {

bool PollEvents(mi_client_handle* handle,
                std::uint32_t max_events,
                std::uint32_t wait_ms,
                PollResult& out,
                std::string& error);

}  // namespace mi::sdk

#endif  // MI_E2EE_SDK_CPP_CLIENT_ADAPTER_H
