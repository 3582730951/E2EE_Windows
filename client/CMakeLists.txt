cmake_minimum_required(VERSION 3.15)
project(mi_e2ee_client_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(MI_E2EE_BUILD_UI "Build Qt Quick UI demo" ON)
if(MSVC)
  add_compile_options(/utf-8)
endif()

# 引入后端静态库（不构建测试），供客户端直接链接。
add_subdirectory(../server ${CMAKE_CURRENT_BINARY_DIR}/server_build EXCLUDE_FROM_ALL)

add_library(mi_e2ee_client_core STATIC
    src/client_core.cpp
    src/client_config.cpp
)
target_include_directories(mi_e2ee_client_core PUBLIC
    include
    ../server/include
    ../shard
)
target_link_libraries(mi_e2ee_client_core PUBLIC mi_e2ee_core)
if(WIN32)
  target_link_libraries(mi_e2ee_client_core PUBLIC ws2_32)
endif()

add_executable(mi_e2ee_client
    demo_client.cpp
)

target_include_directories(mi_e2ee_client PRIVATE include)
target_link_libraries(mi_e2ee_client PRIVATE mi_e2ee_client_core)

if(MI_E2EE_BUILD_UI)
  add_subdirectory(ui)
endif()
