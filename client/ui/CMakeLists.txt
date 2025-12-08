cmake_minimum_required(VERSION 3.15)
project(mi_e2ee_client_ui LANGUAGES CXX)

find_package(Qt6 QUIET COMPONENTS Quick REQUIRED)

qt_standard_project_setup()

qt_add_executable(mi_e2ee_client_ui
    main.cpp
    quick_client.cpp
    quick_client.h
)

qt_add_qml_module(mi_e2ee_client_ui
    URI mi.e2ee.ui
    VERSION 1.0
    QML_FILES
      qml/Main.qml
    RESOURCES
      icons/login.svg
      icons/logout.svg
      icons/group.svg
      icons/send.svg
      icons/file-upload.svg
      icons/offline.svg
)

target_link_libraries(mi_e2ee_client_ui
    PRIVATE
      Qt6::Quick
      mi_e2ee_client_core
)

target_include_directories(mi_e2ee_client_ui PRIVATE
    ../include
    ../../server/include
    ../../shard
)

if(MSVC)
  target_compile_options(mi_e2ee_client_ui PRIVATE /utf-8)
endif()
