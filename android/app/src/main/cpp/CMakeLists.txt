cmake_minimum_required(VERSION 3.22)
project(mi_e2ee_sdk LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

get_filename_component(MI_E2EE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
include("${MI_E2EE_ROOT}/cmake/mi_e2ee_build_options.cmake")

option(MI_E2EE_ANDROID_USE_RUST_OPAQUE "Build Rust OPAQUE PAKE library" ON)
option(MI_E2EE_ANDROID_USE_OPENSSL "Use OpenSSL TLS implementation" OFF)

find_program(MI_E2EE_CARGO_EXECUTABLE cargo)
if(MI_E2EE_ANDROID_USE_RUST_OPAQUE AND NOT MI_E2EE_CARGO_EXECUTABLE)
  message(WARNING "cargo not found; disabling Rust OPAQUE support")
  set(MI_E2EE_ANDROID_USE_RUST_OPAQUE OFF)
endif()

if(MI_E2EE_ANDROID_USE_OPENSSL)
  find_package(OpenSSL)
  if(NOT OpenSSL_FOUND)
    message(WARNING "OpenSSL not found; using TLS stub")
    set(MI_E2EE_ANDROID_USE_OPENSSL OFF)
  endif()
endif()

add_library(monocypher STATIC
  "${MI_E2EE_ROOT}/third_party/monocypher/monocypher.c"
)
target_include_directories(monocypher PUBLIC
  "${MI_E2EE_ROOT}/third_party/monocypher"
)

add_library(miniz STATIC
  "${MI_E2EE_ROOT}/third_party/miniz/miniz.c"
  "${MI_E2EE_ROOT}/third_party/miniz/miniz_tdef.c"
  "${MI_E2EE_ROOT}/third_party/miniz/miniz_tinfl.c"
  "${MI_E2EE_ROOT}/third_party/miniz/miniz_zip.c"
)
target_include_directories(miniz PUBLIC
  "${MI_E2EE_ROOT}/third_party/miniz"
)

add_library(kcp STATIC
  "${MI_E2EE_ROOT}/third_party/kcp/ikcp.c"
)
target_include_directories(kcp PUBLIC
  "${MI_E2EE_ROOT}/third_party/kcp"
)

add_library(pqclean_mlkem768_clean STATIC
  "${MI_E2EE_ROOT}/third_party/pqclean_mlkem768_clean/cbd.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mlkem768_clean/fips202.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mlkem768_clean/indcpa.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mlkem768_clean/kem.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mlkem768_clean/ntt.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mlkem768_clean/poly.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mlkem768_clean/polyvec.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mlkem768_clean/randombytes.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mlkem768_clean/reduce.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mlkem768_clean/symmetric-shake.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mlkem768_clean/verify.c"
)
target_include_directories(pqclean_mlkem768_clean PRIVATE
  "${MI_E2EE_ROOT}/third_party/pqclean_mlkem768_clean"
)

add_library(pqclean_mldsa65_clean STATIC
  "${MI_E2EE_ROOT}/third_party/pqclean_mldsa65_clean/ntt.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mldsa65_clean/packing.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mldsa65_clean/poly.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mldsa65_clean/polyvec.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mldsa65_clean/reduce.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mldsa65_clean/rounding.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mldsa65_clean/sign.c"
  "${MI_E2EE_ROOT}/third_party/pqclean_mldsa65_clean/symmetric-shake.c"
)
target_include_directories(pqclean_mldsa65_clean PRIVATE
  "${MI_E2EE_ROOT}/third_party/pqclean_mldsa65_clean"
)

set(MI_E2EE_PLATFORM_SOURCES
  "${MI_E2EE_ROOT}/platform/posix/platform_time_posix.cpp"
  "${MI_E2EE_ROOT}/platform/posix/platform_random_posix.cpp"
  "${MI_E2EE_ROOT}/platform/posix/platform_fs_posix.cpp"
  "${MI_E2EE_ROOT}/platform/posix/platform_identity_posix.cpp"
  "${MI_E2EE_ROOT}/platform/posix/platform_media_posix.cpp"
  "${MI_E2EE_ROOT}/platform/posix/platform_net_posix.cpp"
  "${MI_E2EE_ROOT}/platform/posix/platform_security_posix.cpp"
  "${MI_E2EE_ROOT}/platform/posix/platform_sys_posix.cpp"
  "${MI_E2EE_ROOT}/platform/posix/platform_log_posix.cpp"
  "${MI_E2EE_ROOT}/platform/android/platform_secure_store_android.cpp"
)
if(MI_E2EE_ANDROID_USE_OPENSSL)
  list(APPEND MI_E2EE_PLATFORM_SOURCES
    "${MI_E2EE_ROOT}/platform/posix/platform_tls_posix.cpp"
  )
else()
  list(APPEND MI_E2EE_PLATFORM_SOURCES
    "${MI_E2EE_ROOT}/platform/android/platform_tls_android.cpp"
  )
endif()

set(MI_E2EE_SDK_SOURCES
  "${MI_E2EE_ROOT}/sdk/c_api_client.cpp"
  "${MI_E2EE_ROOT}/sdk/cpp_client_adapter.cpp"
)

set(MI_E2EE_RUNTIME_SOURCES
  "${MI_E2EE_ROOT}/runtime/client/core/client_core.cpp"
  "${MI_E2EE_ROOT}/runtime/client/auth/client_auth.cpp"
  "${MI_E2EE_ROOT}/runtime/client/auth/auth_service.cpp"
  "${MI_E2EE_ROOT}/runtime/client/auth/e2ee_engine.cpp"
  "${MI_E2EE_ROOT}/runtime/client/config/client_config.cpp"
  "${MI_E2EE_ROOT}/runtime/client/config/config_service.cpp"
  "${MI_E2EE_ROOT}/runtime/client/messaging/client_messaging.cpp"
  "${MI_E2EE_ROOT}/runtime/client/messaging/messaging_service.cpp"
  "${MI_E2EE_ROOT}/runtime/client/sync/client_sync.cpp"
  "${MI_E2EE_ROOT}/runtime/client/sync/sync_service.cpp"
  "${MI_E2EE_ROOT}/runtime/client/storage/client_storage.cpp"
  "${MI_E2EE_ROOT}/runtime/client/storage/storage_service.cpp"
  "${MI_E2EE_ROOT}/runtime/client/storage/chat_history_store.cpp"
  "${MI_E2EE_ROOT}/runtime/client/security/dpapi_util.cpp"
  "${MI_E2EE_ROOT}/runtime/client/security/endpoint_hardening.cpp"
  "${MI_E2EE_ROOT}/runtime/client/security/security_service.cpp"
  "${MI_E2EE_ROOT}/runtime/client/security/trust_store.cpp"
  "${MI_E2EE_ROOT}/runtime/client/security/client_trust.cpp"
  "${MI_E2EE_ROOT}/runtime/client/transport/client_transport.cpp"
  "${MI_E2EE_ROOT}/runtime/client/transport/transport_service.cpp"
  "${MI_E2EE_ROOT}/runtime/client/media/media_jitter_buffer.cpp"
  "${MI_E2EE_ROOT}/runtime/client/media/media_crypto.cpp"
  "${MI_E2EE_ROOT}/runtime/client/media/media_pipeline.cpp"
  "${MI_E2EE_ROOT}/runtime/client/media/media_service.cpp"
  "${MI_E2EE_ROOT}/runtime/client/media/media_session.cpp"
  "${MI_E2EE_ROOT}/runtime/client/media/group_call_media_adapter.cpp"
  "${MI_E2EE_ROOT}/runtime/client/media/group_call_session.cpp"
)

set(MI_E2EE_SERVER_SOURCES
  "${MI_E2EE_ROOT}/server/src/crypto.cpp"
  "${MI_E2EE_ROOT}/server/src/crypto_random.cpp"
  "${MI_E2EE_ROOT}/server/src/config.cpp"
  "${MI_E2EE_ROOT}/server/src/auth_provider.cpp"
  "${MI_E2EE_ROOT}/server/src/pake.cpp"
  "${MI_E2EE_ROOT}/server/src/key_transparency.cpp"
  "${MI_E2EE_ROOT}/server/src/session_manager.cpp"
  "${MI_E2EE_ROOT}/server/src/group_manager.cpp"
  "${MI_E2EE_ROOT}/server/src/group_call_manager.cpp"
  "${MI_E2EE_ROOT}/server/src/group_directory.cpp"
  "${MI_E2EE_ROOT}/server/src/frame.cpp"
  "${MI_E2EE_ROOT}/server/src/offline_storage.cpp"
  "${MI_E2EE_ROOT}/server/src/media_relay.cpp"
  "${MI_E2EE_ROOT}/server/src/api_service.cpp"
  "${MI_E2EE_ROOT}/server/src/protocol.cpp"
  "${MI_E2EE_ROOT}/server/src/frame_router.cpp"
  "${MI_E2EE_ROOT}/server/src/connection_handler.cpp"
  "${MI_E2EE_ROOT}/server/src/secure_channel.cpp"
  "${MI_E2EE_ROOT}/server/src/server_app.cpp"
  "${MI_E2EE_ROOT}/server/src/listener.cpp"
  "${MI_E2EE_ROOT}/server/src/kcp_server.cpp"
  "${MI_E2EE_ROOT}/server/src/network_server.cpp"
  "${MI_E2EE_ROOT}/server/src/c_api.cpp"
)

set(MI_E2EE_SHARD_SOURCES
  "${MI_E2EE_ROOT}/shard/secure_types.cpp"
  "${MI_E2EE_ROOT}/shard/media_frame.cpp"
)

set(MI_E2EE_COMMON_SOURCES
  "${MI_E2EE_ROOT}/common/hex_utils.cpp"
)

set(MI_E2EE_JNI_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/native_sdk_jni.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/media_engine_jni.cpp"
)

if(MI_E2EE_ANDROID_USE_RUST_OPAQUE)
  # Expect a prebuilt Rust static library for the current ABI.
  # Users can set MI_E2EE_OPAQUE_LIB to override the path.
  set(MI_E2EE_OPAQUE_LIB "" CACHE FILEPATH "Path to mi_opaque_pake static lib")
  if(NOT MI_E2EE_OPAQUE_LIB)
    message(WARNING "MI_E2EE_OPAQUE_LIB not set; build will fail unless OPAQUE is provided")
  endif()
else()
  list(APPEND MI_E2EE_SHARD_SOURCES "${MI_E2EE_ROOT}/shard/opaque_pake_stub.cpp")
  add_compile_definitions(MI_E2EE_OPAQUE_DISABLED)
  add_compile_definitions(MI_E2EE_ALLOW_PLAINTEXT_LEGACY)
endif()

add_library(mi_e2ee_sdk SHARED
  ${MI_E2EE_PLATFORM_SOURCES}
  ${MI_E2EE_COMMON_SOURCES}
  ${MI_E2EE_SHARD_SOURCES}
  ${MI_E2EE_SERVER_SOURCES}
  ${MI_E2EE_RUNTIME_SOURCES}
  ${MI_E2EE_SDK_SOURCES}
  ${MI_E2EE_JNI_SOURCES}
)

target_include_directories(mi_e2ee_sdk PRIVATE
  "${MI_E2EE_ROOT}/client/include"
  "${MI_E2EE_ROOT}/client/ui"
  "${MI_E2EE_ROOT}/common"
  "${MI_E2EE_ROOT}/sdk"
  "${MI_E2EE_ROOT}/server/include"
  "${MI_E2EE_ROOT}/runtime"
  "${MI_E2EE_ROOT}/shard"
  "${MI_E2EE_ROOT}/platform/include"
)

target_link_libraries(mi_e2ee_sdk PRIVATE
  monocypher
  miniz
  kcp
  pqclean_mlkem768_clean
  pqclean_mldsa65_clean
  mi_e2ee_build_flags
  log
)

if(MI_E2EE_ANDROID_USE_OPENSSL)
  target_link_libraries(mi_e2ee_sdk PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

if(MI_E2EE_ANDROID_USE_RUST_OPAQUE)
  if(MI_E2EE_OPAQUE_LIB)
    target_link_libraries(mi_e2ee_sdk PRIVATE "${MI_E2EE_OPAQUE_LIB}")
  endif()
endif()
