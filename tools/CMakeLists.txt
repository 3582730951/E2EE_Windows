add_executable(mi_e2ee_third_party_audit
    third_party_audit.cpp
)

target_include_directories(mi_e2ee_third_party_audit PRIVATE ../server/include)
target_link_libraries(mi_e2ee_third_party_audit PRIVATE mi_crypto mi_e2ee_build_flags)

if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(mi_e2ee_third_party_audit)
endif()

if(MSVC)
  target_compile_options(mi_e2ee_third_party_audit PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()

add_executable(mi_e2ee_kt_keygen
    kt_keygen.cpp
)

target_include_directories(mi_e2ee_kt_keygen PRIVATE ../server/include)
target_link_libraries(mi_e2ee_kt_keygen PRIVATE
    mi_e2ee_build_flags
    pqclean_mldsa65_clean
    pqclean_mlkem768_clean
)

if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(mi_e2ee_kt_keygen)
endif()

if(MSVC)
  target_compile_options(mi_e2ee_kt_keygen PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()

add_executable(mi_e2ee_kt_pubinfo
    kt_pubinfo.cpp
    ../third_party/qrcodegen/qrcodegen.cpp
)

target_include_directories(mi_e2ee_kt_pubinfo PRIVATE
    ../server/include
    ../third_party/qrcodegen
)
target_link_libraries(mi_e2ee_kt_pubinfo PRIVATE mi_crypto mi_e2ee_build_flags)

if (COMMAND mi_copy_msvc_runtime)
  mi_copy_msvc_runtime(mi_e2ee_kt_pubinfo)
endif()

if(MSVC)
  target_compile_options(mi_e2ee_kt_pubinfo PRIVATE $<$<CONFIG:Debug>:/RTC1>)
endif()
