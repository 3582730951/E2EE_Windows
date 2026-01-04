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
