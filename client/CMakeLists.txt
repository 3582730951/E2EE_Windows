cmake_minimum_required(VERSION 3.15)
project(mi_e2ee_client_demo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(MI_E2EE_BUILD_UI "Build Qt Quick UI demo" ON)
option(MI_E2EE_BUILD_DEMO_CLIENT "Build demo client CLI" ON)
option(MI_E2EE_BUILD_SDK_SHARED "Build shared C SDK library for bindings" ON)
if(MSVC)
  add_compile_options(/utf-8)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/mi_e2ee_build_options.cmake)

# Client builds link a subset of server components for protocol/crypto reuse and
# should not require MySQL by default. Enable explicitly when you intend to build
# the embedded server target with MySQL.
option(MI_E2EE_ENABLE_MYSQL "Enable MySQL auth provider" OFF)

# 引入后端静态库（不构建测试），供客户端直接链接。
add_subdirectory(../server ${CMAKE_CURRENT_BINARY_DIR}/server_build EXCLUDE_FROM_ALL)

if(NOT TARGET monocypher)
  add_library(monocypher STATIC
      ../third_party/monocypher/monocypher.c
  )
  target_include_directories(monocypher PUBLIC ../third_party/monocypher)
endif()

if(NOT TARGET miniz)
  add_library(miniz STATIC
      ../third_party/miniz/miniz.c
      ../third_party/miniz/miniz_tdef.c
      ../third_party/miniz/miniz_tinfl.c
      ../third_party/miniz/miniz_zip.c
  )
  target_include_directories(miniz PUBLIC ../third_party/miniz)
endif()

if(NOT TARGET kcp)
  add_library(kcp STATIC
      ../third_party/kcp/ikcp.c
  )
  target_include_directories(kcp PUBLIC ../third_party/kcp)
endif()

if(NOT TARGET pqclean_mlkem768_clean)
  add_library(pqclean_mlkem768_clean STATIC
      ../third_party/pqclean_mlkem768_clean/cbd.c
      ../third_party/pqclean_mlkem768_clean/fips202.c
      ../third_party/pqclean_mlkem768_clean/indcpa.c
      ../third_party/pqclean_mlkem768_clean/kem.c
      ../third_party/pqclean_mlkem768_clean/ntt.c
      ../third_party/pqclean_mlkem768_clean/poly.c
      ../third_party/pqclean_mlkem768_clean/polyvec.c
      ../third_party/pqclean_mlkem768_clean/randombytes.c
      ../third_party/pqclean_mlkem768_clean/reduce.c
      ../third_party/pqclean_mlkem768_clean/symmetric-shake.c
      ../third_party/pqclean_mlkem768_clean/verify.c
  )
  target_include_directories(pqclean_mlkem768_clean PRIVATE
      ../third_party/pqclean_mlkem768_clean
  )
endif()

if(NOT TARGET pqclean_mldsa65_clean)
  add_library(pqclean_mldsa65_clean STATIC
      ../third_party/pqclean_mldsa65_clean/ntt.c
      ../third_party/pqclean_mldsa65_clean/packing.c
      ../third_party/pqclean_mldsa65_clean/poly.c
      ../third_party/pqclean_mldsa65_clean/polyvec.c
      ../third_party/pqclean_mldsa65_clean/reduce.c
      ../third_party/pqclean_mldsa65_clean/rounding.c
      ../third_party/pqclean_mldsa65_clean/sign.c
      ../third_party/pqclean_mldsa65_clean/symmetric-shake.c
  )
  target_include_directories(pqclean_mldsa65_clean PRIVATE
      ../third_party/pqclean_mldsa65_clean
  )
  target_link_libraries(pqclean_mldsa65_clean PRIVATE pqclean_mlkem768_clean)
endif()

if(NOT WIN32)
  find_package(OpenSSL REQUIRED)
  if(APPLE)
    find_library(MI_E2EE_SECURITY_FRAMEWORK Security REQUIRED)
    find_library(MI_E2EE_COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MI_E2EE_LIBSECRET REQUIRED libsecret-1)
  endif()
endif()

set(MI_E2EE_CLIENT_CORE_SOURCES
    ../runtime/client/storage/chat_history_store.cpp
    ../runtime/client/core/client_core.cpp
    ../sdk/c_api_client.cpp
    ../sdk/cpp_client_adapter.cpp
    ../runtime/client/auth/client_auth.cpp
    ../runtime/client/auth/auth_service.cpp
    ../runtime/client/auth/e2ee_engine.cpp
    ../runtime/client/config/client_config.cpp
    ../runtime/client/config/config_service.cpp
    ../runtime/client/messaging/client_messaging.cpp
    ../runtime/client/messaging/messaging_service.cpp
    ../runtime/client/sync/client_sync.cpp
    ../runtime/client/sync/sync_service.cpp
    ../runtime/client/storage/client_storage.cpp
    ../runtime/client/storage/storage_service.cpp
    ../runtime/client/security/dpapi_util.cpp
    ../runtime/client/security/endpoint_hardening.cpp
    ../runtime/client/security/security_service.cpp
    ../runtime/client/security/trust_store.cpp
    ../runtime/client/security/client_trust.cpp
    ../runtime/client/transport/client_transport.cpp
    ../runtime/client/transport/transport_service.cpp
    ../runtime/client/media/media_jitter_buffer.cpp
    ../runtime/client/media/media_crypto.cpp
    ../runtime/client/media/media_pipeline.cpp
    ../runtime/client/media/media_service.cpp
    ../runtime/client/media/media_session.cpp
    ../runtime/client/media/group_call_media_adapter.cpp
    ../runtime/client/media/group_call_session.cpp
)
if(WIN32)
  list(APPEND MI_E2EE_CLIENT_CORE_SOURCES
      ../platform/win/platform_secure_store_win.cpp
      ../platform/win/platform_time_win.cpp
      ../platform/win/platform_random_win.cpp
      ../platform/win/platform_fs_win.cpp
      ../platform/win/platform_identity_win.cpp
      ../platform/win/platform_media_win.cpp
      ../platform/win/platform_net_win.cpp
      ../platform/win/platform_security_win.cpp
      ../platform/win/platform_sys_win.cpp
      ../platform/win/platform_tls_win.cpp
      ../platform/win/platform_log_win.cpp
  )
else()
  list(APPEND MI_E2EE_CLIENT_CORE_SOURCES
      ../platform/posix/platform_secure_store_posix.cpp
      ../platform/posix/platform_time_posix.cpp
      ../platform/posix/platform_random_posix.cpp
      ../platform/posix/platform_fs_posix.cpp
      ../platform/posix/platform_identity_posix.cpp
      ../platform/posix/platform_media_posix.cpp
      ../platform/posix/platform_net_posix.cpp
      ../platform/posix/platform_security_posix.cpp
      ../platform/posix/platform_sys_posix.cpp
      ../platform/posix/platform_tls_posix.cpp
      ../platform/posix/platform_log_posix.cpp
  )
endif()

add_library(mi_e2ee_client_core STATIC ${MI_E2EE_CLIENT_CORE_SOURCES})
if(MI_E2EE_BUILD_SDK_SHARED)
  add_library(mi_e2ee_client_sdk SHARED ${MI_E2EE_CLIENT_CORE_SOURCES})
  target_compile_definitions(mi_e2ee_client_sdk PRIVATE MI_E2EE_SDK_EXPORTS)
endif()

set(MI_E2EE_CLIENT_CORE_TARGETS mi_e2ee_client_core)
if(MI_E2EE_BUILD_SDK_SHARED)
  list(APPEND MI_E2EE_CLIENT_CORE_TARGETS mi_e2ee_client_sdk)
endif()

foreach(target IN LISTS MI_E2EE_CLIENT_CORE_TARGETS)
  target_include_directories(${target} PUBLIC
      include
      ../common
      ../sdk
      ../server/include
      ../shard
      ../platform/include
  )
  target_link_libraries(${target} PUBLIC mi_e2ee_core)
  target_link_libraries(${target} PUBLIC mi_e2ee_build_flags)
  target_link_libraries(${target} PRIVATE monocypher miniz pqclean_mlkem768_clean pqclean_mldsa65_clean kcp)
  if(WIN32)
    target_link_libraries(${target} PUBLIC ws2_32 secur32 crypt32 bcrypt advapi32 ncrypt
        mfplat mf mfreadwrite mfuuid ole32 psapi)
  else()
    target_link_libraries(${target} PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    if(APPLE)
      target_link_libraries(${target} PUBLIC ${MI_E2EE_SECURITY_FRAMEWORK} ${MI_E2EE_COREFOUNDATION_FRAMEWORK})
    else()
      target_include_directories(${target} PRIVATE ${MI_E2EE_LIBSECRET_INCLUDE_DIRS})
      target_compile_options(${target} PRIVATE ${MI_E2EE_LIBSECRET_CFLAGS_OTHER})
      target_link_libraries(${target} PUBLIC ${MI_E2EE_LIBSECRET_LIBRARIES})
    endif()
  endif()
endforeach()

if(MI_E2EE_BUILD_DEMO_CLIENT)
  add_executable(mi_e2ee_client
      demo_client.cpp
  )

  target_include_directories(mi_e2ee_client PRIVATE include)
  target_link_libraries(mi_e2ee_client PRIVATE mi_e2ee_client_core)
  if (COMMAND mi_copy_msvc_runtime)
    mi_copy_msvc_runtime(mi_e2ee_client)
  endif()
endif()

if(MI_E2EE_BUILD_UI)
  add_subdirectory(ui)
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

if(MI_E2EE_BUILD_FUZZERS)
  add_subdirectory(fuzz)
endif()
