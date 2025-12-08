add_executable(config_test
    config_test.cpp
)
target_link_libraries(config_test PRIVATE mi_e2ee_core)
target_include_directories(config_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(config_test)
if(MSVC)
  target_compile_options(config_test PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()
add_test(NAME config_test COMMAND config_test)

add_executable(group_manager_test
    group_manager_test.cpp
)
target_link_libraries(group_manager_test PRIVATE mi_e2ee_core)
target_include_directories(group_manager_test PRIVATE ../include)
mi_copy_msvc_runtime(group_manager_test)
if(MSVC)
  target_compile_options(group_manager_test PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()
add_test(NAME group_manager_test COMMAND group_manager_test)

add_executable(group_directory_test
    group_directory_test.cpp
)
target_link_libraries(group_directory_test PRIVATE mi_e2ee_core)
target_include_directories(group_directory_test PRIVATE ../include)
mi_copy_msvc_runtime(group_directory_test)
if(MSVC)
  target_compile_options(group_directory_test PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()
add_test(NAME group_directory_test COMMAND group_directory_test)

add_executable(api_service_test
    api_service_test.cpp
)
target_link_libraries(api_service_test PRIVATE mi_e2ee_core)
target_include_directories(api_service_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(api_service_test)
if(MSVC)
  target_compile_options(api_service_test PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()
add_test(NAME api_service_test COMMAND api_service_test)

add_executable(session_manager_test
    session_manager_test.cpp
)
target_link_libraries(session_manager_test PRIVATE mi_e2ee_core)
target_include_directories(session_manager_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(session_manager_test)
if(MSVC)
  target_compile_options(session_manager_test PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()
add_test(NAME session_manager_test COMMAND session_manager_test)

add_executable(frame_test
    frame_test.cpp
)
target_link_libraries(frame_test PRIVATE mi_e2ee_core)
target_include_directories(frame_test PRIVATE ../include)
mi_copy_msvc_runtime(frame_test)
if(MSVC)
  target_compile_options(frame_test PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()
add_test(NAME frame_test COMMAND frame_test)

add_executable(router_test
    router_test.cpp
)
target_link_libraries(router_test PRIVATE mi_e2ee_core)
target_include_directories(router_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(router_test)
if(MSVC)
  target_compile_options(router_test PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()
add_test(NAME router_test COMMAND router_test)

add_executable(server_app_test
    server_app_test.cpp
)
target_link_libraries(server_app_test PRIVATE mi_e2ee_core)
target_include_directories(server_app_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(server_app_test)
if(MSVC)
  target_compile_options(server_app_test PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()
add_test(NAME server_app_test COMMAND server_app_test)

add_executable(connection_handler_test
    connection_handler_test.cpp
)
target_link_libraries(connection_handler_test PRIVATE mi_e2ee_core)
target_include_directories(connection_handler_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(connection_handler_test)
if(MSVC)
  target_compile_options(connection_handler_test PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()
add_test(NAME connection_handler_test COMMAND connection_handler_test)

add_executable(secure_channel_test
    secure_channel_test.cpp
)
target_link_libraries(secure_channel_test PRIVATE mi_e2ee_core)
target_include_directories(secure_channel_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(secure_channel_test)
if(MSVC)
  target_compile_options(secure_channel_test PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()
add_test(NAME secure_channel_test COMMAND secure_channel_test)

add_executable(pake_test
    pake_test.cpp
)
target_link_libraries(pake_test PRIVATE mi_e2ee_core)
target_include_directories(pake_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(pake_test)
if(MSVC)
  target_compile_options(pake_test PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()
add_test(NAME pake_test COMMAND pake_test)

add_executable(c_api_test
    c_api_test.cpp
)
target_link_libraries(c_api_test PRIVATE mi_e2ee_core)
target_include_directories(c_api_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(c_api_test)
if(MSVC)
  target_compile_options(c_api_test PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()
add_test(NAME c_api_test COMMAND c_api_test)

add_executable(c_api_login_test
    c_api_login_test.cpp
)
target_link_libraries(c_api_login_test PRIVATE mi_e2ee_core)
target_include_directories(c_api_login_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(c_api_login_test)
if(MSVC)
  target_compile_options(c_api_login_test PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()
add_test(NAME c_api_login_test COMMAND c_api_login_test)

add_executable(offline_storage_test
    offline_storage_test.cpp
)
target_link_libraries(offline_storage_test PRIVATE mi_e2ee_core)
target_include_directories(offline_storage_test PRIVATE ../include ../shard)
mi_copy_msvc_runtime(offline_storage_test)
if(MSVC)
  target_compile_options(offline_storage_test PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endif()
add_test(NAME offline_storage_test COMMAND offline_storage_test)
