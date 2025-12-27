set(UI_MAIN_LIST_SOURCES
    main.cpp
    MainListWindow.cpp
    MainListWindow.h
    ChatWindow.cpp
    ChatWindow.h
    MessageModel.cpp
    MessageModel.h
    MessageDelegate.cpp
    MessageDelegate.h
    EmojiPickerDialog.cpp
    EmojiPickerDialog.h
    LoginDialog.cpp
    LoginDialog.h
    AuthFlowWidget.cpp
    AuthFlowWidget.h
    BackendAdapter.cpp
    BackendAdapter.h
    TrustPromptDialog.cpp
    TrustPromptDialog.h
    NotificationCenterDialog.cpp
    NotificationCenterDialog.h
    ConversationDetailsDialog.cpp
    ConversationDetailsDialog.h
)

add_ui_executable(e2ee_main_list ${UI_MAIN_LIST_SOURCES})
target_link_libraries(e2ee_main_list PRIVATE mi_e2ee_client_core Qt6::Network)
target_include_directories(e2ee_main_list PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

if (TARGET Qt6::Multimedia AND TARGET Qt6::MultimediaWidgets)
    target_link_libraries(e2ee_main_list PRIVATE Qt6::Multimedia Qt6::MultimediaWidgets)
    target_compile_definitions(e2ee_main_list PRIVATE MI_UI_HAS_QT_MULTIMEDIA=1)
endif()
