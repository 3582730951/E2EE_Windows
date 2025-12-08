cmake_minimum_required(VERSION 3.15)
project(mi_e2ee_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MI_E2EE_ENABLE_MYSQL "Enable MySQL auth provider" OFF)
option(MI_E2EE_ENABLE_TCP_SERVER "Enable simple TCP server listener" OFF)

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

add_library(mi_crypto STATIC
    src/crypto.cpp
)
target_include_directories(mi_crypto PUBLIC include)

add_library(mi_e2ee_core STATIC
    src/config.cpp
    src/auth_provider.cpp
    src/pake.cpp
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
target_link_libraries(mi_e2ee_core PUBLIC shard_secure mi_crypto)

if(MSVC)
  add_compile_options(/utf-8)
  target_compile_options(shard_secure PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
  target_compile_options(mi_e2ee_core PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()

add_executable(mi_e2ee_server
    src/main.cpp
)

target_include_directories(mi_e2ee_server PRIVATE include ../shard)
target_link_libraries(mi_e2ee_server PRIVATE mi_e2ee_core)
mi_copy_msvc_runtime(mi_e2ee_server)

if(MI_E2EE_ENABLE_MYSQL)
  target_compile_definitions(mi_e2ee_server PRIVATE MI_E2EE_ENABLE_MYSQL=1)
  target_compile_definitions(mi_e2ee_core PRIVATE MI_E2EE_ENABLE_MYSQL=1)
  message(STATUS "MySQL auth enabled (ensure include/lib paths are provided)")
endif()

if(MI_E2EE_ENABLE_TCP_SERVER)
  target_compile_definitions(mi_e2ee_core PRIVATE MI_E2EE_ENABLE_TCP_SERVER=1)
  target_compile_definitions(mi_e2ee_server PRIVATE MI_E2EE_ENABLE_TCP_SERVER=1)
  if(WIN32)
    target_link_libraries(mi_e2ee_core PUBLIC ws2_32)
  else()
    target_link_libraries(mi_e2ee_core PUBLIC pthread)
  endif()
  message(STATUS "TCP server enabled (simple blocking listener)")
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

# 可根据需要在此处添加第三方依赖（优先使用预编译库）
