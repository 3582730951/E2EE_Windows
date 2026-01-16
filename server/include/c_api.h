#ifndef MI_E2EE_SERVER_C_API_H
#define MI_E2EE_SERVER_C_API_H

#include <cstddef>
#include <cstdint>

#define MI_E2EE_SERVER_SDK_ABI_VERSION 1
#define MI_E2EE_SERVER_SDK_VERSION_MAJOR 1
#define MI_E2EE_SERVER_SDK_VERSION_MINOR 0
#define MI_E2EE_SERVER_SDK_VERSION_PATCH 0

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mi_server_handle mi_server_handle;

#ifndef MI_E2EE_SDK_VERSION_DEFINED
#define MI_E2EE_SDK_VERSION_DEFINED
typedef struct mi_sdk_version {
  std::uint32_t major;
  std::uint32_t minor;
  std::uint32_t patch;
  std::uint32_t abi;
} mi_sdk_version;
#endif

typedef enum mi_server_capability {
  MI_SERVER_CAP_TLS = 1u << 0,
  MI_SERVER_CAP_KCP = 1u << 1,
  MI_SERVER_CAP_OPAQUE = 1u << 2,
  MI_SERVER_CAP_MYSQL = 1u << 3,
  MI_SERVER_CAP_OPS = 1u << 4
} mi_server_capability;

void mi_server_get_version(mi_sdk_version* out_version);
std::uint32_t mi_server_get_capabilities(void);

// config_path  "config.ini"
mi_server_handle* mi_server_create(const char* config_path);

// 
void mi_server_destroy(mi_server_handle* handle);

// 
// out_len  mi_server_free 
//  0  0 
int mi_server_process(mi_server_handle* handle,
                      const std::uint8_t* data,
                      std::size_t len,
                      std::uint8_t** out_buf,
                      std::size_t* out_len);

//  mi_server_process 
void mi_server_free(std::uint8_t* buf);

//  token  '\0' 
//  1 0 mi_server_free  token
int mi_server_login(mi_server_handle* handle,
                    const char* username,
                    const char* password,
                    char** out_token);

//  token  1/0 /
int mi_server_logout(mi_server_handle* handle, const char* token);

#ifdef __cplusplus
}
#endif

#endif  // MI_E2EE_SERVER_C_API_H
