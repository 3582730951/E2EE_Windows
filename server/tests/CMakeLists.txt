add_executable(config_test
    config_test.cpp
)
target_link_libraries(config_test PRIVATE mi_e2ee_core)
target_include_directories(config_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(config_test)
if(MSVC)
  target_compile_options(config_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME config_test COMMAND config_test)

add_executable(crypto_test
    crypto_test.cpp
)
target_link_libraries(crypto_test PRIVATE mi_e2ee_core)
target_include_directories(crypto_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(crypto_test)
if(MSVC)
  target_compile_options(crypto_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME crypto_test COMMAND crypto_test)

add_executable(friend_test
    friend_test.cpp
)
target_link_libraries(friend_test PRIVATE mi_e2ee_core)
target_include_directories(friend_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(friend_test)
if(MSVC)
  target_compile_options(friend_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME friend_test COMMAND friend_test)

add_executable(friend_request_test
    friend_request_test.cpp
)
target_link_libraries(friend_request_test PRIVATE mi_e2ee_core)
target_include_directories(friend_request_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(friend_request_test)
if(MSVC)
  target_compile_options(friend_request_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME friend_request_test COMMAND friend_request_test)

add_executable(group_manager_test
    group_manager_test.cpp
)
target_link_libraries(group_manager_test PRIVATE mi_e2ee_core)
target_include_directories(group_manager_test PRIVATE ../include)
mi_copy_msvc_runtime(group_manager_test)
if(MSVC)
  target_compile_options(group_manager_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME group_manager_test COMMAND group_manager_test)

add_executable(group_directory_test
    group_directory_test.cpp
)
target_link_libraries(group_directory_test PRIVATE mi_e2ee_core)
target_include_directories(group_directory_test PRIVATE ../include)
mi_copy_msvc_runtime(group_directory_test)
if(MSVC)
  target_compile_options(group_directory_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME group_directory_test COMMAND group_directory_test)

add_executable(api_service_test
    api_service_test.cpp
)
target_link_libraries(api_service_test PRIVATE mi_e2ee_core)
target_include_directories(api_service_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(api_service_test)
if(MSVC)
  target_compile_options(api_service_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME api_service_test COMMAND api_service_test)

add_executable(session_manager_test
    session_manager_test.cpp
)
target_link_libraries(session_manager_test PRIVATE mi_e2ee_core)
target_include_directories(session_manager_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(session_manager_test)
if(MSVC)
  target_compile_options(session_manager_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME session_manager_test COMMAND session_manager_test)

add_executable(opaque_pake_test
    opaque_pake_test.cpp
)
target_link_libraries(opaque_pake_test PRIVATE mi_e2ee_core)
target_include_directories(opaque_pake_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(opaque_pake_test)
if(MSVC)
  target_compile_options(opaque_pake_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME opaque_pake_test COMMAND opaque_pake_test)

add_executable(frame_test
    frame_test.cpp
)
target_link_libraries(frame_test PRIVATE mi_e2ee_core)
target_include_directories(frame_test PRIVATE ../include)
mi_copy_msvc_runtime(frame_test)
if(MSVC)
  target_compile_options(frame_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME frame_test COMMAND frame_test)

add_executable(key_transparency_test
    key_transparency_test.cpp
)
target_link_libraries(key_transparency_test PRIVATE mi_e2ee_core)
target_include_directories(key_transparency_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(key_transparency_test)
if(MSVC)
  target_compile_options(key_transparency_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME key_transparency_test COMMAND key_transparency_test)

add_executable(router_test
    router_test.cpp
)
target_link_libraries(router_test PRIVATE mi_e2ee_core)
target_include_directories(router_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(router_test)
if(MSVC)
  target_compile_options(router_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME router_test COMMAND router_test)

add_executable(server_app_test
    server_app_test.cpp
)
target_link_libraries(server_app_test PRIVATE mi_e2ee_core)
target_include_directories(server_app_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(server_app_test)
if(MSVC)
  target_compile_options(server_app_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME server_app_test COMMAND server_app_test)

add_executable(ops_health_test
    ops_health_test.cpp
)
target_link_libraries(ops_health_test PRIVATE mi_e2ee_core)
target_include_directories(ops_health_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(ops_health_test)
if(MSVC)
  target_compile_options(ops_health_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME ops_health_test COMMAND ops_health_test)

add_executable(connection_handler_test
    connection_handler_test.cpp
)
target_link_libraries(connection_handler_test PRIVATE mi_e2ee_core)
target_include_directories(connection_handler_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(connection_handler_test)
if(MSVC)
  target_compile_options(connection_handler_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME connection_handler_test COMMAND connection_handler_test)

add_executable(secure_channel_test
    secure_channel_test.cpp
)
target_link_libraries(secure_channel_test PRIVATE mi_e2ee_core)
target_include_directories(secure_channel_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(secure_channel_test)
if(MSVC)
  target_compile_options(secure_channel_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME secure_channel_test COMMAND secure_channel_test)

add_executable(pake_test
    pake_test.cpp
)
target_link_libraries(pake_test PRIVATE mi_e2ee_core)
target_include_directories(pake_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(pake_test)
if(MSVC)
  target_compile_options(pake_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME pake_test COMMAND pake_test)

add_executable(c_api_test
    c_api_test.cpp
)
target_link_libraries(c_api_test PRIVATE mi_e2ee_core)
target_include_directories(c_api_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(c_api_test)
if(MSVC)
  target_compile_options(c_api_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME c_api_test COMMAND c_api_test)

add_executable(c_api_login_test
    c_api_login_test.cpp
)
target_link_libraries(c_api_login_test PRIVATE mi_e2ee_core)
target_include_directories(c_api_login_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(c_api_login_test)
if(MSVC)
  target_compile_options(c_api_login_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME c_api_login_test COMMAND c_api_login_test)

add_executable(offline_storage_test
    offline_storage_test.cpp
)
target_link_libraries(offline_storage_test PRIVATE mi_e2ee_core)
target_include_directories(offline_storage_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(offline_storage_test)
if(MSVC)
  target_compile_options(offline_storage_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME offline_storage_test COMMAND offline_storage_test)

add_executable(private_chat_test
    private_chat_test.cpp
)
target_link_libraries(private_chat_test PRIVATE mi_e2ee_core)
target_include_directories(private_chat_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(private_chat_test)
if(MSVC)
  target_compile_options(private_chat_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME private_chat_test COMMAND private_chat_test)

add_executable(group_cipher_test
    group_cipher_test.cpp
)
target_link_libraries(group_cipher_test PRIVATE mi_e2ee_core)
target_include_directories(group_cipher_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(group_cipher_test)
if(MSVC)
  target_compile_options(group_cipher_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME group_cipher_test COMMAND group_cipher_test)

add_executable(group_notice_test
    group_notice_test.cpp
)
target_link_libraries(group_notice_test PRIVATE mi_e2ee_core)
target_include_directories(group_notice_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(group_notice_test)
if(MSVC)
  target_compile_options(group_notice_test PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
add_test(NAME group_notice_test COMMAND group_notice_test)

if(TARGET mi_e2ee_third_party_audit)
  set(THIRD_PARTY_LOCK "${CMAKE_CURRENT_LIST_DIR}/../../third_party/third_party.lock")
  add_test(NAME third_party_lock_test
           COMMAND mi_e2ee_third_party_audit --verify --lock "${THIRD_PARTY_LOCK}")
endif()
