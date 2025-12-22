cmake_minimum_required(VERSION 3.15)
project(mi_e2ee_client_ui LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Svg)
find_package(Qt6 QUIET COMPONENTS Multimedia MultimediaWidgets)

set(UI_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(UI_COMMON_SOURCES
    common/ChatInputEdit.cpp
    common/ChatInputEdit.h
    common/FramelessWindowBase.cpp
    common/IconButton.cpp
    common/ImePluginApi.h
    common/ImePluginLoader.cpp
    common/ImePluginLoader.h
    common/OverlayWidget.cpp
    common/SecureClipboard.cpp
    common/SecureClipboard.h
    common/Toast.cpp
    common/Theme.cpp
    common/UiIcons.cpp
    common/UiSettings.cpp
    common/UiStyle.cpp
    common/SettingsDialog.cpp
    common/ui_resources.qrc
)

add_library(mi_ui_common STATIC ${UI_COMMON_SOURCES})
target_include_directories(mi_ui_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_link_libraries(mi_ui_common PUBLIC Qt6::Widgets Qt6::Svg)
target_compile_definitions(mi_ui_common PUBLIC
    UI_REF_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets/ref"
)

option(MI_UI_ENABLE_RIME "Enable librime IME plugin" ON)
if(MI_UI_ENABLE_RIME)
    target_compile_definitions(mi_ui_common PUBLIC MI_UI_ENABLE_RIME)
    add_subdirectory(ime_rime)
endif()

function(copy_ref_images target_name)
    set(REF_DIR "${UI_ROOT}/../assets/ref")
    if(NOT EXISTS "${REF_DIR}")
        message(WARNING "ref assets not found at ${REF_DIR}, skip copying")
        return()
    endif()
    file(GLOB REF_FILES "${REF_DIR}/*.png")
    if(REF_FILES)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target_name}>/assets/ref"
        )
        foreach(ref ${REF_FILES})
            get_filename_component(ref_name "${ref}" NAME)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ref}"
                    "$<TARGET_FILE_DIR:${target_name}>/assets/ref/${ref_name}"
            )
        endforeach()
    endif()
endfunction()

function(add_ui_executable target_name)
    add_executable(${target_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE mi_ui_common Qt6::Widgets)
    copy_ref_images(${target_name})
    if(TARGET mi_ime_rime)
        add_dependencies(${target_name} mi_ime_rime)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:mi_ime_rime>"
                "$<TARGET_FILE_DIR:${target_name}>/$<TARGET_FILE_NAME:mi_ime_rime>"
        )
    endif()
    if (COMMAND mi_copy_msvc_runtime)
        mi_copy_msvc_runtime(${target_name})
    endif()
endfunction()

add_subdirectory(e2ee_login)
add_subdirectory(e2ee_main_list)
add_subdirectory(e2ee_group_chat)
add_subdirectory(e2ee_chat_empty)
