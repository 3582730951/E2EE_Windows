set(UI_GROUP_CHAT_SOURCES
    main.cpp
    GroupChatWindow.cpp
    GroupChatWindow.h
)

add_ui_executable(e2ee_group_chat ${UI_GROUP_CHAT_SOURCES})
target_link_libraries(e2ee_group_chat PRIVATE mi_e2ee_client_core)
target_include_directories(e2ee_group_chat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
