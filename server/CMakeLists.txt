cmake_minimum_required(VERSION 3.15)
project(mi_e2ee_server LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MI_E2EE_ENABLE_MYSQL "Enable MySQL auth provider" OFF)
option(MI_E2EE_ENABLE_TCP_SERVER "Enable simple TCP server listener" OFF)

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/mi_e2ee_build_options.cmake)

if(NOT TARGET monocypher)
  add_library(monocypher STATIC
      ../third_party/monocypher/monocypher.c
  )
  target_include_directories(monocypher PUBLIC ../third_party/monocypher)
endif()

set(MI_OPAQUE_PAKE_MANIFEST ${CMAKE_CURRENT_LIST_DIR}/../shard/opaque_pake/Cargo.toml)
if(EXISTS ${MI_OPAQUE_PAKE_MANIFEST} AND NOT TARGET mi_opaque_pake_rust)
  set(MI_OPAQUE_PAKE_CARGO_TARGET_ARGS "")
  set(MI_OPAQUE_PAKE_LIB_SUBDIR "")
  if(WIN32 AND NOT MSVC)
    # Building with MinGW requires the GNU Rust target to avoid MSVC ABI/runtime
    # mismatches at link time.
    set(MI_OPAQUE_PAKE_CARGO_TARGET_ARGS --target x86_64-pc-windows-gnu)
    set(MI_OPAQUE_PAKE_LIB_SUBDIR x86_64-pc-windows-gnu)
    set(MI_OPAQUE_PAKE_LIB_NAME libmi_opaque_pake.a)
  elseif(WIN32)
    set(MI_OPAQUE_PAKE_LIB_NAME mi_opaque_pake.lib)
  else()
    set(MI_OPAQUE_PAKE_LIB_NAME libmi_opaque_pake.a)
  endif()
  set(MI_OPAQUE_PAKE_TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/cargo_mi_opaque_pake)
  if(MI_OPAQUE_PAKE_LIB_SUBDIR)
    set(MI_OPAQUE_PAKE_DEBUG_LIB ${MI_OPAQUE_PAKE_TARGET_DIR}/${MI_OPAQUE_PAKE_LIB_SUBDIR}/debug/${MI_OPAQUE_PAKE_LIB_NAME})
    set(MI_OPAQUE_PAKE_RELEASE_LIB ${MI_OPAQUE_PAKE_TARGET_DIR}/${MI_OPAQUE_PAKE_LIB_SUBDIR}/release/${MI_OPAQUE_PAKE_LIB_NAME})
  else()
    set(MI_OPAQUE_PAKE_DEBUG_LIB ${MI_OPAQUE_PAKE_TARGET_DIR}/debug/${MI_OPAQUE_PAKE_LIB_NAME})
    set(MI_OPAQUE_PAKE_RELEASE_LIB ${MI_OPAQUE_PAKE_TARGET_DIR}/release/${MI_OPAQUE_PAKE_LIB_NAME})
  endif()

  file(GLOB_RECURSE MI_OPAQUE_PAKE_RUST_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/../shard/opaque_pake/src/*.rs
    ${CMAKE_CURRENT_LIST_DIR}/../shard/opaque_pake/Cargo.toml
  )

  add_custom_command(
    OUTPUT ${MI_OPAQUE_PAKE_DEBUG_LIB}
    COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${MI_OPAQUE_PAKE_TARGET_DIR}
            cargo build --manifest-path ${MI_OPAQUE_PAKE_MANIFEST} ${MI_OPAQUE_PAKE_CARGO_TARGET_ARGS}
    DEPENDS ${MI_OPAQUE_PAKE_RUST_SOURCES}
    COMMENT "Building Rust OPAQUE PAKE (debug)"
  )
  add_custom_command(
    OUTPUT ${MI_OPAQUE_PAKE_RELEASE_LIB}
    COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${MI_OPAQUE_PAKE_TARGET_DIR}
            cargo build --manifest-path ${MI_OPAQUE_PAKE_MANIFEST} --release ${MI_OPAQUE_PAKE_CARGO_TARGET_ARGS}
    DEPENDS ${MI_OPAQUE_PAKE_RUST_SOURCES}
    COMMENT "Building Rust OPAQUE PAKE (release)"
  )

  add_custom_target(mi_opaque_pake_rust_build
    DEPENDS
      $<$<CONFIG:Release>:${MI_OPAQUE_PAKE_RELEASE_LIB}>
      $<$<NOT:$<CONFIG:Release>>:${MI_OPAQUE_PAKE_DEBUG_LIB}>
  )

  add_library(mi_opaque_pake_rust STATIC IMPORTED GLOBAL)
  set_target_properties(mi_opaque_pake_rust PROPERTIES
    IMPORTED_LOCATION ${MI_OPAQUE_PAKE_DEBUG_LIB}
    IMPORTED_LOCATION_DEBUG ${MI_OPAQUE_PAKE_DEBUG_LIB}
    IMPORTED_LOCATION_RELEASE ${MI_OPAQUE_PAKE_RELEASE_LIB}
    IMPORTED_LOCATION_RELWITHDEBINFO ${MI_OPAQUE_PAKE_RELEASE_LIB}
    IMPORTED_LOCATION_MINSIZEREL ${MI_OPAQUE_PAKE_RELEASE_LIB}
  )
  add_dependencies(mi_opaque_pake_rust mi_opaque_pake_rust_build)
  if(WIN32)
    # Rust staticlibs don't automatically propagate their Win32 system library
    # dependencies to CMake/MSVC consumers.
    target_link_libraries(mi_opaque_pake_rust INTERFACE
      ws2_32
      bcrypt
      advapi32
      userenv
      secur32
      crypt32
      ntdll
    )
  endif()
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
  if(WIN32)
    target_link_libraries(pqclean_mlkem768_clean PUBLIC advapi32)
  endif()
endif()

if(MSVC)
  # Derive the latest available MSVC redist next to the compiler to avoid
  # depending on a potentially old system-wide runtime (std::mutex crashed with
  # stale msvcp140.dll).
  get_filename_component(_cl_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
  get_filename_component(_host_dir "${_cl_dir}" DIRECTORY)
  get_filename_component(_bin_dir "${_host_dir}" DIRECTORY)
  get_filename_component(_msvc_ver_dir "${_bin_dir}" DIRECTORY)
  get_filename_component(_msvc_root "${_msvc_ver_dir}" DIRECTORY)     # .../MSVC
  get_filename_component(_tools_dir "${_msvc_root}" DIRECTORY)        # .../Tools
  get_filename_component(_vc_root "${_tools_dir}" DIRECTORY)          # .../VC
  file(GLOB _redist_dirs LIST_DIRECTORIES true
       "${_vc_root}/Redist/MSVC/*/x64/Microsoft.VC*.CRT")
  list(SORT _redist_dirs)
  list(REVERSE _redist_dirs)
  if(_redist_dirs)
    list(GET _redist_dirs 0 MI_MSVC_REDIST_DIR)
    file(GLOB MI_MSVC_REDIST_DLLS "${MI_MSVC_REDIST_DIR}/*.dll")
    message(STATUS "Using MSVC redist from ${MI_MSVC_REDIST_DIR}")
  else()
    message(WARNING "MSVC redist not found; runtime DLLs will not be copied")
  endif()
endif()

function(mi_copy_msvc_runtime target_name)
  if(MSVC AND MI_MSVC_REDIST_DLLS)
    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${MI_MSVC_REDIST_DLLS}
              $<TARGET_FILE_DIR:${target_name}>)
  endif()
endfunction()

add_library(shard_secure STATIC
    ../shard/secure_types.cpp
)
target_include_directories(shard_secure PUBLIC ../shard)
target_link_libraries(shard_secure PUBLIC mi_e2ee_build_flags)

add_library(mi_crypto STATIC
    src/crypto.cpp
)
target_include_directories(mi_crypto PUBLIC include)
target_link_libraries(mi_crypto PUBLIC mi_e2ee_build_flags)
if(WIN32)
  target_link_libraries(mi_crypto PUBLIC bcrypt)
endif()

add_library(mi_e2ee_core STATIC
    src/config.cpp
    src/auth_provider.cpp
    src/pake.cpp
    src/key_transparency.cpp
    src/session_manager.cpp
    src/group_manager.cpp
    src/group_directory.cpp
    src/frame.cpp
    src/offline_storage.cpp
    src/api_service.cpp
    src/protocol.cpp
    src/frame_router.cpp
    src/connection_handler.cpp
    src/secure_channel.cpp
    src/server_app.cpp
    src/listener.cpp
    src/network_server.cpp
    src/c_api.cpp
)

target_include_directories(mi_e2ee_core PUBLIC include ../shard)
target_link_libraries(mi_e2ee_core PUBLIC mi_e2ee_build_flags shard_secure mi_crypto monocypher pqclean_mlkem768_clean)
if(TARGET mi_opaque_pake_rust)
  target_link_libraries(mi_e2ee_core PUBLIC mi_opaque_pake_rust)
endif()

if(MSVC)
  add_compile_options(/utf-8)
  target_compile_options(shard_secure PRIVATE $<$<CONFIG:Debug>:/RTC1>)
  target_compile_options(mi_e2ee_core PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()

add_executable(mi_e2ee_server
    src/main.cpp
)

target_include_directories(mi_e2ee_server PRIVATE include ../shard)
target_link_libraries(mi_e2ee_server PRIVATE mi_e2ee_core)
mi_copy_msvc_runtime(mi_e2ee_server)

# Generate demo auth user sample into build and output directories.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/test_user.txt" [=[# Demo auth user table (mode=1).
# Format: username:password per line.
# Lines starting with # are comments; inline comments after whitespace are allowed.
u1:p1  # example user 1
alice:alice123
bob:bob123
]=])
add_custom_command(TARGET mi_e2ee_server POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_BINARY_DIR}/test_user.txt"
          $<TARGET_FILE_DIR:mi_e2ee_server>)

if(MI_E2EE_ENABLE_MYSQL)
  find_path(MYSQL_INCLUDE_DIR mysql.h
    PATHS /usr/include /usr/local/include
    PATH_SUFFIXES mysql)
  find_library(MYSQL_CLIENT_LIB mysqlclient
    PATHS /usr/lib /usr/lib64 /usr/local/lib /usr/local/lib64
    PATH_SUFFIXES mysql)
  if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_CLIENT_LIB)
    message(FATAL_ERROR "MySQL client headers or library not found; install libmysqlclient-dev or set MYSQL_INCLUDE_DIR/MYSQL_CLIENT_LIB")
  endif()
  target_compile_definitions(mi_e2ee_server PRIVATE MI_E2EE_ENABLE_MYSQL=1)
  target_compile_definitions(mi_e2ee_core PRIVATE MI_E2EE_ENABLE_MYSQL=1)
  target_include_directories(mi_e2ee_core PRIVATE ${MYSQL_INCLUDE_DIR})
  target_link_libraries(mi_e2ee_core PRIVATE ${MYSQL_CLIENT_LIB})
  target_link_libraries(mi_e2ee_server PRIVATE ${MYSQL_CLIENT_LIB})
  message(STATUS "MySQL auth enabled (ensure include/lib paths are provided)")
endif()

if(MI_E2EE_ENABLE_TCP_SERVER)
  target_compile_definitions(mi_e2ee_core PRIVATE MI_E2EE_ENABLE_TCP_SERVER=1)
  target_compile_definitions(mi_e2ee_server PRIVATE MI_E2EE_ENABLE_TCP_SERVER=1)
  if(WIN32)
    target_link_libraries(mi_e2ee_core PUBLIC ws2_32 secur32 crypt32 advapi32)
  else()
    target_link_libraries(mi_e2ee_core PUBLIC pthread)
  endif()
  message(STATUS "TCP server enabled (simple blocking listener)")
endif()

include(CTest)
if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  add_subdirectory(tests)
endif()

if(MI_E2EE_BUILD_FUZZERS)
  add_subdirectory(fuzz)
endif()

# 可根据需要在此处添加第三方依赖（优先使用预编译库）
