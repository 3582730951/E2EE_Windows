#ifndef MI_E2EE_SHARD_OPAQUE_PAKE_H
#define MI_E2EE_SHARD_OPAQUE_PAKE_H

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// All functions return 0 on success. Non-zero values indicate an error; the
// optional error buffer (out_err_ptr/out_err_len) returns a UTF-8 message
// allocated by the Rust library and must be freed with mi_opaque_free().

int mi_opaque_server_setup_generate(std::uint8_t** out_setup_ptr,
                                    std::size_t* out_setup_len,
                                    std::uint8_t** out_err_ptr,
                                    std::size_t* out_err_len);

int mi_opaque_server_setup_validate(const std::uint8_t* setup_ptr,
                                    std::size_t setup_len,
                                    std::uint8_t** out_err_ptr,
                                    std::size_t* out_err_len);

int mi_opaque_create_user_password_file(const std::uint8_t* setup_ptr,
                                        std::size_t setup_len,
                                        const std::uint8_t* user_id_ptr,
                                        std::size_t user_id_len,
                                        const std::uint8_t* password_ptr,
                                        std::size_t password_len,
                                        std::uint8_t** out_file_ptr,
                                        std::size_t* out_file_len,
                                        std::uint8_t** out_err_ptr,
                                        std::size_t* out_err_len);

int mi_opaque_client_register_start(const std::uint8_t* password_ptr,
                                    std::size_t password_len,
                                    std::uint8_t** out_req_ptr,
                                    std::size_t* out_req_len,
                                    std::uint8_t** out_state_ptr,
                                    std::size_t* out_state_len,
                                    std::uint8_t** out_err_ptr,
                                    std::size_t* out_err_len);

int mi_opaque_server_register_response(const std::uint8_t* setup_ptr,
                                       std::size_t setup_len,
                                       const std::uint8_t* user_id_ptr,
                                       std::size_t user_id_len,
                                       const std::uint8_t* req_ptr,
                                       std::size_t req_len,
                                       std::uint8_t** out_resp_ptr,
                                       std::size_t* out_resp_len,
                                       std::uint8_t** out_err_ptr,
                                       std::size_t* out_err_len);

int mi_opaque_client_register_finish(const std::uint8_t* user_id_ptr,
                                     std::size_t user_id_len,
                                     const std::uint8_t* password_ptr,
                                     std::size_t password_len,
                                     const std::uint8_t* state_ptr,
                                     std::size_t state_len,
                                     const std::uint8_t* resp_ptr,
                                     std::size_t resp_len,
                                     std::uint8_t** out_upload_ptr,
                                     std::size_t* out_upload_len,
                                     std::uint8_t** out_err_ptr,
                                     std::size_t* out_err_len);

int mi_opaque_server_register_finish(const std::uint8_t* upload_ptr,
                                     std::size_t upload_len,
                                     std::uint8_t** out_file_ptr,
                                     std::size_t* out_file_len,
                                     std::uint8_t** out_err_ptr,
                                     std::size_t* out_err_len);

int mi_opaque_client_login_start(const std::uint8_t* password_ptr,
                                 std::size_t password_len,
                                 std::uint8_t** out_req_ptr,
                                 std::size_t* out_req_len,
                                 std::uint8_t** out_state_ptr,
                                 std::size_t* out_state_len,
                                 std::uint8_t** out_err_ptr,
                                 std::size_t* out_err_len);

// has_password_file == 0 => dummy server response (password_file ignored).
int mi_opaque_server_login_start(const std::uint8_t* setup_ptr,
                                 std::size_t setup_len,
                                 const std::uint8_t* user_id_ptr,
                                 std::size_t user_id_len,
                                 int has_password_file,
                                 const std::uint8_t* password_file_ptr,
                                 std::size_t password_file_len,
                                 const std::uint8_t* req_ptr,
                                 std::size_t req_len,
                                 std::uint8_t** out_resp_ptr,
                                 std::size_t* out_resp_len,
                                 std::uint8_t** out_state_ptr,
                                 std::size_t* out_state_len,
                                 std::uint8_t** out_err_ptr,
                                 std::size_t* out_err_len);

int mi_opaque_client_login_finish(const std::uint8_t* user_id_ptr,
                                  std::size_t user_id_len,
                                  const std::uint8_t* password_ptr,
                                  std::size_t password_len,
                                  const std::uint8_t* state_ptr,
                                  std::size_t state_len,
                                  const std::uint8_t* resp_ptr,
                                  std::size_t resp_len,
                                  std::uint8_t** out_final_ptr,
                                  std::size_t* out_final_len,
                                  std::uint8_t** out_session_key_ptr,
                                  std::size_t* out_session_key_len,
                                  std::uint8_t** out_err_ptr,
                                  std::size_t* out_err_len);

int mi_opaque_server_login_finish(const std::uint8_t* user_id_ptr,
                                  std::size_t user_id_len,
                                  const std::uint8_t* state_ptr,
                                  std::size_t state_len,
                                  const std::uint8_t* final_ptr,
                                  std::size_t final_len,
                                  std::uint8_t** out_session_key_ptr,
                                  std::size_t* out_session_key_len,
                                  std::uint8_t** out_err_ptr,
                                  std::size_t* out_err_len);

void mi_opaque_free(std::uint8_t* ptr, std::size_t len);

#ifdef __cplusplus
}
#endif

#endif  // MI_E2EE_SHARD_OPAQUE_PAKE_H
