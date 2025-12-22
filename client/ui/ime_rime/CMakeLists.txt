cmake_minimum_required(VERSION 3.15)
project(mi_ime_rime LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mi_ime_rime SHARED
    RimeImePlugin.cpp
)

target_include_directories(mi_ime_rime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../common
)

target_compile_definitions(mi_ime_rime PRIVATE MI_IME_PLUGIN_BUILD)
