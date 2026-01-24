add_executable(e2ee_engine_test
    e2ee_engine_test.cpp
)

target_link_libraries(e2ee_engine_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(e2ee_engine_test)
endif()

add_test(NAME e2ee_engine_test COMMAND e2ee_engine_test)

add_executable(media_crypto_test
    media_crypto_test.cpp
)

target_link_libraries(media_crypto_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(media_crypto_test)
endif()

add_test(NAME media_crypto_test COMMAND media_crypto_test)

add_executable(media_crypto_keyid_test
    media_crypto_keyid_test.cpp
)

target_link_libraries(media_crypto_keyid_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(media_crypto_keyid_test)
endif()

add_test(NAME media_crypto_keyid_test COMMAND media_crypto_keyid_test)

add_executable(group_call_key_test
    group_call_key_test.cpp
)

target_link_libraries(group_call_key_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(group_call_key_test)
endif()

add_test(NAME group_call_key_test COMMAND group_call_key_test)

add_executable(sdk_c_api_event_test
    sdk_c_api_event_test.cpp
)

target_link_libraries(sdk_c_api_event_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(sdk_c_api_event_test)
endif()

add_test(NAME sdk_c_api_event_test COMMAND sdk_c_api_event_test)

add_executable(sdk_c_api_friend_test
    sdk_c_api_friend_test.cpp
)

target_link_libraries(sdk_c_api_friend_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(sdk_c_api_friend_test)
endif()

add_test(NAME sdk_c_api_friend_test COMMAND sdk_c_api_friend_test)

add_executable(sdk_c_api_version_test
    sdk_c_api_version_test.cpp
)

target_link_libraries(sdk_c_api_version_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(sdk_c_api_version_test)
endif()

add_test(NAME sdk_c_api_version_test COMMAND sdk_c_api_version_test)

add_executable(sdk_c_api_status_test
    sdk_c_api_status_test.cpp
)

target_link_libraries(sdk_c_api_status_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(sdk_c_api_status_test)
endif()

add_test(NAME sdk_c_api_status_test COMMAND sdk_c_api_status_test)

add_executable(client_config_media_test
    client_config_media_test.cpp
)

target_link_libraries(client_config_media_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(client_config_media_test)
endif()

add_test(NAME client_config_media_test COMMAND client_config_media_test)

add_executable(client_config_tls_test
    client_config_tls_test.cpp
)

target_link_libraries(client_config_tls_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(client_config_tls_test)
endif()

add_test(NAME client_config_tls_test COMMAND client_config_tls_test)

add_executable(client_trust_store_test
    client_trust_store_test.cpp
)

target_link_libraries(client_trust_store_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(client_trust_store_test)
endif()

add_test(NAME client_trust_store_test COMMAND client_trust_store_test)

add_executable(device_sync_ratchet_test
    device_sync_ratchet_test.cpp
)

target_link_libraries(device_sync_ratchet_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(device_sync_ratchet_test)
endif()

add_test(NAME device_sync_ratchet_test COMMAND device_sync_ratchet_test)

add_executable(platform_fs_lock_test
    platform_fs_lock_test.cpp
)

target_link_libraries(platform_fs_lock_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(platform_fs_lock_test)
endif()

add_test(NAME platform_fs_lock_test COMMAND platform_fs_lock_test)

add_executable(sdk_c_api_e2e_test
    sdk_c_api_e2e_test.cpp
)

target_link_libraries(sdk_c_api_e2e_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(sdk_c_api_e2e_test)
endif()

add_test(NAME sdk_c_api_e2e_test COMMAND sdk_c_api_e2e_test)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  set(MI_E2EE_PY_SMOKE
      "${CMAKE_CURRENT_LIST_DIR}/../../bindings/python/test_smoke.py")
  add_test(NAME bindings_python_smoke
           COMMAND "${Python3_EXECUTABLE}" "${MI_E2EE_PY_SMOKE}")
endif()
