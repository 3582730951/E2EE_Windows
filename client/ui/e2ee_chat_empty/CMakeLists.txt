set(UI_CHAT_EMPTY_SOURCES
    main.cpp
    ChatEmptyWindow.cpp
    ChatEmptyWindow.h
)

add_ui_executable(e2ee_chat_empty ${UI_CHAT_EMPTY_SOURCES})
target_link_libraries(e2ee_chat_empty PRIVATE mi_e2ee_client_core)
target_include_directories(e2ee_chat_empty PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
