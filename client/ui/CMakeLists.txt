cmake_minimum_required(VERSION 3.15)
project(mi_e2ee_client_ui LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Svg Quick Qml QuickControls2)
find_package(Qt6 QUIET COMPONENTS Multimedia MultimediaWidgets)

set(UI_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(UI_COMMON_SOURCES
    common/ChatInputEdit.cpp
    common/ChatInputEdit.h
    common/EmojiPackManager.cpp
    common/EmojiPackManager.h
    common/FramelessWindowBase.cpp
    common/IconButton.cpp
    common/ImePluginApi.h
    common/ImeLanguagePackManager.cpp
    common/ImeLanguagePackManager.h
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
    common/UiRuntimePaths.cpp
    common/UiRuntimePaths.h
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

function(copy_icon_assets target_name)
    set(ICON_DIR "${UI_ROOT}/icons")
    if(NOT EXISTS "${ICON_DIR}")
        message(WARNING "icons not found at ${ICON_DIR}, skip copying")
        return()
    endif()
    file(GLOB ICON_FILES "${ICON_DIR}/*.svg")
    if(ICON_FILES)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target_name}>/icon"
        )
        foreach(icon ${ICON_FILES})
            get_filename_component(icon_name "${icon}" NAME)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${icon}"
                    "$<TARGET_FILE_DIR:${target_name}>/icon/${icon_name}"
            )
        endforeach()
    endif()
endfunction()

function(add_ui_executable target_name)
    add_executable(${target_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE mi_ui_common Qt6::Widgets)
    copy_ref_images(${target_name})
    copy_icon_assets(${target_name})
    if(TARGET mi_ime_rime)
        add_dependencies(${target_name} mi_ime_rime)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target_name}>/runtime"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:mi_ime_rime>"
                "$<TARGET_FILE_DIR:${target_name}>/runtime/$<TARGET_FILE_NAME:mi_ime_rime>"
        )
        if(MI_UI_RIME_DOWNLOAD AND WIN32)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                    -DRIME_OUTPUT_DIR=$<TARGET_FILE_DIR:${target_name}>/runtime
                    -DRIME_VERSION=${MI_UI_RIME_VERSION}
                    -DRIME_ASSET=${MI_UI_RIME_ASSET}
                    -DRIME_WITH_LUA=${MI_UI_RIME_WITH_LUA}
                    -DRIME_WEASEL_VERSION=${MI_UI_RIME_WEASEL_VERSION}
                    -DRIME_WEASEL_ASSET=${MI_UI_RIME_WEASEL_ASSET}
                    -P ${UI_ROOT}/ime_rime/FetchRime.cmake
                COMMENT "Fetching librime runtime for ${target_name}"
            )
        endif()
    endif()
    if (COMMAND mi_copy_msvc_runtime)
        mi_copy_msvc_runtime(${target_name})
    endif()
endfunction()

add_subdirectory(e2ee_login)
add_subdirectory(e2ee_main_list)
add_subdirectory(e2ee_group_chat)
add_subdirectory(e2ee_chat_empty)

add_executable(mi_e2ee_client_ui
    qml_main.cpp
    quick_client.cpp
)
target_link_libraries(mi_e2ee_client_ui PRIVATE mi_ui_common mi_e2ee_client_core
    Qt6::Quick Qt6::Qml Qt6::QuickControls2)
copy_ref_images(mi_e2ee_client_ui)
copy_icon_assets(mi_e2ee_client_ui)
if(TARGET mi_ime_rime)
    add_dependencies(mi_e2ee_client_ui mi_ime_rime)
    add_custom_command(TARGET mi_e2ee_client_ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:mi_e2ee_client_ui>/runtime"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:mi_ime_rime>"
            "$<TARGET_FILE_DIR:mi_e2ee_client_ui>/runtime/$<TARGET_FILE_NAME:mi_ime_rime>"
    )
    if(MI_UI_RIME_DOWNLOAD AND WIN32)
        add_custom_command(TARGET mi_e2ee_client_ui POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DRIME_OUTPUT_DIR=$<TARGET_FILE_DIR:mi_e2ee_client_ui>/runtime
                -DRIME_VERSION=${MI_UI_RIME_VERSION}
                -DRIME_ASSET=${MI_UI_RIME_ASSET}
                -DRIME_WITH_LUA=${MI_UI_RIME_WITH_LUA}
                -DRIME_WEASEL_VERSION=${MI_UI_RIME_WEASEL_VERSION}
                -DRIME_WEASEL_ASSET=${MI_UI_RIME_WEASEL_ASSET}
                -P ${UI_ROOT}/ime_rime/FetchRime.cmake
            COMMENT "Fetching librime runtime for mi_e2ee_client_ui"
        )
    endif()
endif()
if (COMMAND mi_copy_msvc_runtime)
    mi_copy_msvc_runtime(mi_e2ee_client_ui)
endif()

if(WIN32)
    add_executable(mi_e2ee_launcher WIN32 launcher/UiLauncher.cpp)
    set_target_properties(mi_e2ee_launcher PROPERTIES OUTPUT_NAME "mi_e2ee")
    if(MSVC)
        set_property(TARGET mi_e2ee_launcher PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()
