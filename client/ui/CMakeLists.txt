cmake_minimum_required(VERSION 3.15)
project(mi_e2ee_client_ui LANGUAGES CXX)

find_package(Qt6 QUIET COMPONENTS Widgets Gui REQUIRED)

qt_standard_project_setup()

set(UI_COMMON_INCLUDES
    ../include
    ../../server/include
    ../../shard
)

qt_add_executable(mi_e2ee_client_desktop
    widget_main.cpp
    widgets/theme.h
    widgets/login_dialog.cpp
    widgets/login_dialog.h
    widgets/chat_window.cpp
    widgets/chat_window.h
    widgets/main_window.cpp
    widgets/main_window.h
    widgets/list_window.cpp
    widgets/list_window.h
)

target_link_libraries(mi_e2ee_client_desktop
    PRIVATE
      Qt6::Widgets
      Qt6::Gui
      mi_e2ee_client_core
)

target_include_directories(mi_e2ee_client_desktop PRIVATE ${UI_COMMON_INCLUDES})

qt_add_executable(mi_e2ee_client_ui
    main.cpp
    widgets/theme.h
    widgets/login_dialog.cpp
    widgets/login_dialog.h
    widgets/chat_window.cpp
    widgets/chat_window.h
    widgets/main_window.cpp
    widgets/main_window.h
    widgets/list_window.cpp
    widgets/list_window.h
)

target_link_libraries(mi_e2ee_client_ui
    PRIVATE
      Qt6::Widgets
      Qt6::Gui
      mi_e2ee_client_core
)

target_include_directories(mi_e2ee_client_ui PRIVATE ${UI_COMMON_INCLUDES})

add_custom_command(TARGET mi_e2ee_client_ui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:mi_e2ee_client_ui>/qml
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/qml $<TARGET_FILE_DIR:mi_e2ee_client_ui>/qml
)

if(MSVC)
  target_compile_options(mi_e2ee_client_ui PRIVATE /utf-8)
  target_compile_options(mi_e2ee_client_desktop PRIVATE /utf-8)
endif()
