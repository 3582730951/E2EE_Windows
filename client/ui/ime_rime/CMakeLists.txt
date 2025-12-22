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

option(MI_UI_RIME_DOWNLOAD "Download librime runtime when building UI" ON)
set(MI_UI_RIME_VERSION "1.15.0" CACHE STRING "librime runtime version")
set(MI_UI_RIME_ASSET "rime-75bc43a-Windows-msvc-x64.7z" CACHE STRING "librime runtime asset")
option(MI_UI_RIME_WITH_LUA "Use librime runtime with Lua support" ON)
set(MI_UI_RIME_WEASEL_VERSION "0.17.4" CACHE STRING "Weasel release version for Lua-enabled rime.dll")
set(MI_UI_RIME_WEASEL_ASSET "weasel-0.17.4.0-installer.exe" CACHE STRING "Weasel installer asset for Lua-enabled rime.dll")
if(MI_UI_RIME_DOWNLOAD AND WIN32)
    add_custom_command(TARGET mi_ime_rime POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DRIME_OUTPUT_DIR=$<TARGET_FILE_DIR:mi_ime_rime>
            -DRIME_VERSION=${MI_UI_RIME_VERSION}
            -DRIME_ASSET=${MI_UI_RIME_ASSET}
            -DRIME_WITH_LUA=${MI_UI_RIME_WITH_LUA}
            -DRIME_WEASEL_VERSION=${MI_UI_RIME_WEASEL_VERSION}
            -DRIME_WEASEL_ASSET=${MI_UI_RIME_WEASEL_ASSET}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/FetchRime.cmake
        COMMENT "Fetching librime runtime"
    )
endif()
