add_executable(e2ee_engine_test
    e2ee_engine_test.cpp
)

target_link_libraries(e2ee_engine_test PRIVATE mi_e2ee_client_core)
if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(e2ee_engine_test)
endif()

add_test(NAME e2ee_engine_test COMMAND e2ee_engine_test)
