#ifndef MI_E2EE_SERVER_C_API_H
#define MI_E2EE_SERVER_C_API_H

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mi_server_handle mi_server_handle;

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
