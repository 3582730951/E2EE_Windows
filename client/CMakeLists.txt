cmake_minimum_required(VERSION 3.15)
project(mi_e2ee_client_demo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(MI_E2EE_BUILD_UI "Build Qt Quick UI demo" ON)
option(MI_E2EE_BUILD_DEMO_CLIENT "Build demo client CLI" ON)
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

add_library(mi_e2ee_client_core STATIC
    src/chat_history_store.cpp
    src/client_core.cpp
    src/client_config.cpp
    src/dpapi_util.cpp
    src/endpoint_hardening.cpp
    src/e2ee_engine.cpp
)
target_include_directories(mi_e2ee_client_core PUBLIC
    include
    ../server/include
    ../shard
)
target_link_libraries(mi_e2ee_client_core PUBLIC mi_e2ee_core)
target_link_libraries(mi_e2ee_client_core PUBLIC mi_e2ee_build_flags)
target_link_libraries(mi_e2ee_client_core PRIVATE monocypher miniz pqclean_mlkem768_clean pqclean_mldsa65_clean)
if(WIN32)
  target_link_libraries(mi_e2ee_client_core PUBLIC ws2_32 secur32 crypt32 bcrypt advapi32 ncrypt)
endif()

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
