include_guard(GLOBAL)

option(MI_E2EE_ENABLE_LTO "Enable LTO (Release only)" ON)
option(MI_E2EE_PGO_INSTRUMENT "Enable PGO instrumentation build (Release only)" ON)
option(MI_E2EE_PGO_USE "Enable PGO profile-use build (Release only)" OFF)
set(MI_E2EE_PGO_PROFILE_DIR "" CACHE PATH "Optional PGO profile directory (gcc/clang)")
option(MI_E2EE_ENABLE_ASAN "Enable AddressSanitizer" ON)
option(MI_E2EE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (clang/gcc)" OFF)
option(MI_E2EE_BUILD_FUZZERS "Build fuzz harness targets" OFF)
option(MI_E2EE_FUZZ_USE_LIBFUZZER "Use libFuzzer engine (clang only)" OFF)
option(MI_E2EE_FUZZ_SMOKE "Run fuzz smoke tests via CTest" OFF)

if(MI_E2EE_PGO_INSTRUMENT AND MI_E2EE_PGO_USE)
  message(FATAL_ERROR "MI_E2EE_PGO_INSTRUMENT and MI_E2EE_PGO_USE are mutually exclusive")
endif()

if(MSVC AND MI_E2EE_ENABLE_ASAN)
  if(MI_E2EE_ENABLE_LTO OR MI_E2EE_PGO_INSTRUMENT OR MI_E2EE_PGO_USE)
    message(WARNING "MSVC ASAN is incompatible with LTCG/PGO; disabling LTO/PGO for this build")
    set(MI_E2EE_ENABLE_LTO OFF CACHE BOOL "Enable LTO (Release only)" FORCE)
    set(MI_E2EE_PGO_INSTRUMENT OFF CACHE BOOL "Enable PGO instrumentation build (Release only)" FORCE)
    set(MI_E2EE_PGO_USE OFF CACHE BOOL "Enable PGO profile-use build (Release only)" FORCE)
  endif()
endif()

if(MI_E2EE_ENABLE_LTO)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

if(NOT TARGET mi_e2ee_build_flags)
  add_library(mi_e2ee_build_flags INTERFACE)
endif()

if(MI_E2EE_ENABLE_ASAN)
  if(MSVC)
    target_compile_options(mi_e2ee_build_flags INTERFACE /fsanitize=address)
    target_link_options(mi_e2ee_build_flags INTERFACE /fsanitize=address)
  else()
    target_compile_options(mi_e2ee_build_flags INTERFACE
      -fsanitize=address
      -fno-omit-frame-pointer
    )
    target_link_options(mi_e2ee_build_flags INTERFACE -fsanitize=address)
  endif()
endif()

if(MI_E2EE_ENABLE_UBSAN)
  if(MSVC)
    message(WARNING "UBSan is not supported on MSVC; ignoring MI_E2EE_ENABLE_UBSAN")
  else()
    target_compile_options(mi_e2ee_build_flags INTERFACE
      -fsanitize=undefined
      -fno-omit-frame-pointer
    )
    target_link_options(mi_e2ee_build_flags INTERFACE -fsanitize=undefined)
  endif()
endif()

if(MI_E2EE_PGO_INSTRUMENT)
  if(MSVC)
    target_compile_options(mi_e2ee_build_flags INTERFACE $<$<CONFIG:Release>:/GL>)
    target_link_options(mi_e2ee_build_flags INTERFACE $<$<CONFIG:Release>:/LTCG:PGINSTRUMENT>)
  else()
    set(_pgo_gen -fprofile-generate)
    if(MI_E2EE_PGO_PROFILE_DIR)
      set(_pgo_gen "-fprofile-generate=${MI_E2EE_PGO_PROFILE_DIR}")
    endif()
    target_compile_options(mi_e2ee_build_flags INTERFACE $<$<CONFIG:Release>:${_pgo_gen}>)
    target_link_options(mi_e2ee_build_flags INTERFACE $<$<CONFIG:Release>:${_pgo_gen}>)
  endif()
elseif(MI_E2EE_PGO_USE)
  if(MSVC)
    target_compile_options(mi_e2ee_build_flags INTERFACE $<$<CONFIG:Release>:/GL>)
    target_link_options(mi_e2ee_build_flags INTERFACE
      $<$<CONFIG:Release>:/LTCG:PGOPTIMIZE>
      $<$<CONFIG:Release>:/USEPROFILE>
    )
  else()
    set(_pgo_use -fprofile-use)
    if(MI_E2EE_PGO_PROFILE_DIR)
      set(_pgo_use "-fprofile-use=${MI_E2EE_PGO_PROFILE_DIR}")
    endif()
    target_compile_options(mi_e2ee_build_flags INTERFACE $<$<CONFIG:Release>:${_pgo_use}>)
    target_link_options(mi_e2ee_build_flags INTERFACE $<$<CONFIG:Release>:${_pgo_use}>)
  endif()
endif()
