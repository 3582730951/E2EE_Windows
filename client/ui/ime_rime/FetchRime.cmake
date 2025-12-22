if(NOT DEFINED RIME_OUTPUT_DIR)
    message(FATAL_ERROR "RIME_OUTPUT_DIR is required")
endif()

set(RIME_VERSION "1.15.0" CACHE STRING "librime runtime version")
set(RIME_ASSET "rime-75bc43a-Windows-msvc-x64.7z" CACHE STRING "librime runtime asset")

set(_rime_url "https://github.com/rime/librime/releases/download/${RIME_VERSION}/${RIME_ASSET}")
set(_cache_dir "${CMAKE_BINARY_DIR}/rime_download")
set(_archive "${_cache_dir}/${RIME_ASSET}")
set(_extract_dir "${_cache_dir}/extract")

file(MAKE_DIRECTORY "${_cache_dir}")
file(MAKE_DIRECTORY "${_extract_dir}")
file(MAKE_DIRECTORY "${RIME_OUTPUT_DIR}")

if(EXISTS "${RIME_OUTPUT_DIR}/rime.dll")
    message(STATUS "rime.dll already present in ${RIME_OUTPUT_DIR}")
    return()
endif()

if(NOT EXISTS "${_archive}")
    message(STATUS "Downloading ${_rime_url}")
    file(DOWNLOAD "${_rime_url}" "${_archive}" STATUS _dl_status SHOW_PROGRESS)
    list(GET _dl_status 0 _dl_code)
    if(NOT _dl_code EQUAL 0)
        list(GET _dl_status 1 _dl_msg)
        message(WARNING "Failed to download librime runtime: ${_dl_msg}")
        return()
    endif()
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xf "${_archive}"
    WORKING_DIRECTORY "${_extract_dir}"
    RESULT_VARIABLE _extract_result
)
if(NOT _extract_result EQUAL 0)
    message(WARNING "Failed to extract librime runtime from ${_archive}")
    return()
endif()

file(GLOB_RECURSE _rime_dll "${_extract_dir}/**/rime.dll")
if(NOT _rime_dll)
    message(WARNING "rime.dll not found after extracting ${_archive}")
    return()
endif()

list(GET _rime_dll 0 _rime_src)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_rime_src}" "${RIME_OUTPUT_DIR}/rime.dll"
    RESULT_VARIABLE _copy_result
)
if(NOT _copy_result EQUAL 0)
    message(WARNING "Failed to copy rime.dll to ${RIME_OUTPUT_DIR}")
    return()
endif()

message(STATUS "rime.dll copied to ${RIME_OUTPUT_DIR}")
