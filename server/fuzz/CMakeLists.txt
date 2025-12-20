add_executable(mi_e2ee_fuzz_frame_decode
    fuzz_frame_decode.cpp
)
target_link_libraries(mi_e2ee_fuzz_frame_decode PRIVATE mi_e2ee_core)
target_include_directories(mi_e2ee_fuzz_frame_decode PRIVATE ../include ../shard)
mi_copy_msvc_runtime(mi_e2ee_fuzz_frame_decode)

if(MI_E2EE_FUZZ_USE_LIBFUZZER)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(mi_e2ee_fuzz_frame_decode PRIVATE -fsanitize=fuzzer)
    target_link_options(mi_e2ee_fuzz_frame_decode PRIVATE -fsanitize=fuzzer)
  else()
    message(FATAL_ERROR "MI_E2EE_FUZZ_USE_LIBFUZZER requires Clang")
  endif()
else()
  target_compile_definitions(mi_e2ee_fuzz_frame_decode PRIVATE MI_E2EE_FUZZ_STANDALONE=1)
endif()

if(MI_E2EE_FUZZ_SMOKE AND NOT MI_E2EE_FUZZ_USE_LIBFUZZER AND
   CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  add_test(NAME fuzz_frame_decode_smoke
           COMMAND mi_e2ee_fuzz_frame_decode ${CMAKE_CURRENT_LIST_DIR}/seed_frame.bin)
endif()
