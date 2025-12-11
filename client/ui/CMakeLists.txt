cmake_minimum_required(VERSION 3.15)
project(mi_e2ee_client_ui LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(UI_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(UI_COMMON_SOURCES
    common/FramelessWindowBase.cpp
    common/IconButton.cpp
    common/OverlayWidget.cpp
    common/Theme.cpp
)

add_library(mi_ui_common STATIC ${UI_COMMON_SOURCES})
target_include_directories(mi_ui_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_link_libraries(mi_ui_common PUBLIC Qt6::Widgets)
target_compile_definitions(mi_ui_common PUBLIC
    UI_REF_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets/ref"
)

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
endfunction()

add_subdirectory(qq_login)
add_subdirectory(qq_main_list)
add_subdirectory(qq_group_chat)
add_subdirectory(qq_chat_empty)
