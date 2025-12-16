add_executable(mi_e2ee_fuzz_file_blob_v2
    fuzz_file_blob_v2.cpp
)
target_link_libraries(mi_e2ee_fuzz_file_blob_v2 PRIVATE mi_e2ee_client_core monocypher)
target_include_directories(mi_e2ee_fuzz_file_blob_v2 PRIVATE ../include)
mi_copy_msvc_runtime(mi_e2ee_fuzz_file_blob_v2)

if(MI_E2EE_FUZZ_USE_LIBFUZZER)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(mi_e2ee_fuzz_file_blob_v2 PRIVATE -fsanitize=fuzzer)
    target_link_options(mi_e2ee_fuzz_file_blob_v2 PRIVATE -fsanitize=fuzzer)
  else()
    message(FATAL_ERROR "MI_E2EE_FUZZ_USE_LIBFUZZER requires Clang")
  endif()
else()
  target_compile_definitions(mi_e2ee_fuzz_file_blob_v2 PRIVATE MI_E2EE_FUZZ_STANDALONE=1)
endif()

