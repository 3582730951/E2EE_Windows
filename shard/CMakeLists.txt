cmake_minimum_required(VERSION 3.16)

project(mi_e2ee_shard LANGUAGES CXX)

option(MI_E2EE_SHARD_BUILD_TESTS "Build shard tests" ON)

add_library(shard_secure STATIC
    secure_types.cpp
    media_frame.cpp
)

target_include_directories(shard_secure PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(shard_secure PUBLIC cxx_std_17)

if(MSVC)
  target_compile_options(shard_secure PRIVATE /W4)
else()
  target_compile_options(shard_secure PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MI_E2EE_SHARD_BUILD_TESTS)
  enable_testing()

  add_executable(shard_secure_test
      tests/shard_secure_test.cpp
  )
  target_link_libraries(shard_secure_test PRIVATE shard_secure)

  add_test(NAME shard_secure_test COMMAND shard_secure_test)
endif()
