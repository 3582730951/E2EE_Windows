set(UI_LOGIN_SOURCES
    main.cpp
    LoginWindow.cpp
    LoginWindow.h
)

add_ui_executable(qq_login ${UI_LOGIN_SOURCES})
target_link_libraries(qq_login PRIVATE mi_e2ee_client_core)
target_include_directories(qq_login PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
