if(NOT DEFINED RIME_OUTPUT_DIR)
    message(FATAL_ERROR "RIME_OUTPUT_DIR is required")
endif()

set(RIME_VERSION "1.15.0" CACHE STRING "librime runtime version")
set(RIME_ASSET "rime-75bc43a-Windows-msvc-x64.7z" CACHE STRING "librime runtime asset")
set(RIME_WITH_LUA "OFF" CACHE BOOL "Use Lua-enabled rime runtime")
set(RIME_WEASEL_VERSION "0.17.4" CACHE STRING "Weasel release version for Lua-enabled rime.dll")
set(RIME_WEASEL_ASSET "weasel-0.17.4.0-installer.exe" CACHE STRING "Weasel installer asset for Lua-enabled rime.dll")
set(RIME_DOWNLOAD_TIMEOUT "600" CACHE STRING "Download timeout in seconds")
set(RIME_DOWNLOAD_RETRIES "2" CACHE STRING "Retry count for downloads")
set(RIME_CACHE_DIR "" CACHE PATH "Optional cache directory for downloads")
set(RIME_EXTRACT_DIR "" CACHE PATH "Optional extract directory for archives")

set(_cache_dir "${CMAKE_BINARY_DIR}/rime_download")
if(DEFINED RIME_CACHE_DIR AND NOT RIME_CACHE_DIR STREQUAL "")
    set(_cache_dir "${RIME_CACHE_DIR}")
endif()
set(_extract_dir "${_cache_dir}/extract")
if(DEFINED RIME_EXTRACT_DIR AND NOT RIME_EXTRACT_DIR STREQUAL "")
    set(_extract_dir "${RIME_EXTRACT_DIR}")
endif()
set(_lua_marker "${RIME_OUTPUT_DIR}/rime.with_lua")
set(_plain_marker "${RIME_OUTPUT_DIR}/rime.plain")

function(download_with_retry url out_path label result_var)
    set(_ok FALSE)
    set(_attempt 0)
    math(EXPR _max_attempts "${RIME_DOWNLOAD_RETRIES} + 1")
    while(_attempt LESS _max_attempts)
        math(EXPR _attempt "${_attempt} + 1")
        message(STATUS "Downloading ${label} (attempt ${_attempt}/${_max_attempts})")
        file(DOWNLOAD "${url}" "${out_path}"
            STATUS _dl_status
            SHOW_PROGRESS
            TIMEOUT "${RIME_DOWNLOAD_TIMEOUT}")
        list(GET _dl_status 0 _dl_code)
        if(_dl_code EQUAL 0)
            set(_ok TRUE)
            break()
        endif()
        list(GET _dl_status 1 _dl_msg)
        message(WARNING "Failed to download ${label}: ${_dl_msg}")
        file(REMOVE "${out_path}")
    endwhile()
    set(${result_var} ${_ok} PARENT_SCOPE)
endfunction()

function(copy_opencc_data root_dir)
    if(NOT EXISTS "${root_dir}")
        return()
    endif()
    set(_opencc_dirs
        "${root_dir}/opencc"
        "${root_dir}/data/opencc"
        "${root_dir}/share/opencc"
        "${root_dir}/dist/share/opencc"
    )
    foreach(_dir IN LISTS _opencc_dirs)
        if(NOT EXISTS "${_dir}")
            continue()
        endif()
        file(GLOB _opencc_files
            "${_dir}/*.json"
            "${_dir}/*.ocd2"
            "${_dir}/*.txt"
        )
        if(NOT _opencc_files)
            continue()
        endif()
        set(_opencc_dst "${RIME_OUTPUT_DIR}/opencc")
        file(MAKE_DIRECTORY "${_opencc_dst}")
        foreach(_file IN LISTS _opencc_files)
            get_filename_component(_name "${_file}" NAME)
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_file}" "${_opencc_dst}/${_name}"
            )
        endforeach()
    endforeach()
endfunction()

file(MAKE_DIRECTORY "${_cache_dir}")
file(MAKE_DIRECTORY "${_extract_dir}")
file(MAKE_DIRECTORY "${RIME_OUTPUT_DIR}")

if(RIME_WITH_LUA)
    if(EXISTS "${RIME_OUTPUT_DIR}/rime.dll" AND EXISTS "${_lua_marker}")
        message(STATUS "Lua-enabled rime.dll already present in ${RIME_OUTPUT_DIR}")
        return()
    endif()
else()
    if(EXISTS "${RIME_OUTPUT_DIR}/rime.dll" AND EXISTS "${_plain_marker}")
        message(STATUS "rime.dll already present in ${RIME_OUTPUT_DIR}")
        return()
    endif()
endif()

if(RIME_WITH_LUA)
    set(_weasel_url "https://github.com/rime/weasel/releases/download/${RIME_WEASEL_VERSION}/${RIME_WEASEL_ASSET}")
    set(_weasel_archive "${_cache_dir}/${RIME_WEASEL_ASSET}")
    if(NOT EXISTS "${_weasel_archive}")
        download_with_retry("${_weasel_url}" "${_weasel_archive}" "Weasel runtime" _weasel_ok)
        if(NOT _weasel_ok)
            return()
        endif()
    endif()

    set(_seven_zip_msi "${_cache_dir}/7zip.msi")
    set(_seven_zip_root "${_cache_dir}/7zip")
    set(_seven_zip_exe "${_seven_zip_root}/Files/7-Zip/7z.exe")
    if(NOT EXISTS "${_seven_zip_exe}")
        file(MAKE_DIRECTORY "${_seven_zip_root}")
        if(NOT EXISTS "${_seven_zip_msi}")
            download_with_retry("https://www.7-zip.org/a/7z2401-x64.msi" "${_seven_zip_msi}" "7-Zip MSI" _zip_ok)
            if(NOT _zip_ok)
                return()
            endif()
        endif()
        execute_process(
            COMMAND msiexec /a "${_seven_zip_msi}" /qn TARGETDIR="${_seven_zip_root}"
            RESULT_VARIABLE _msi_result
        )
        if(NOT _msi_result EQUAL 0 OR NOT EXISTS "${_seven_zip_exe}")
            message(WARNING "Failed to extract 7-Zip CLI from ${_seven_zip_msi}")
            return()
        endif()
    endif()

    set(_weasel_extract_dir "${_cache_dir}/weasel_extract")
    file(MAKE_DIRECTORY "${_weasel_extract_dir}")
    execute_process(
        COMMAND "${_seven_zip_exe}" x -y -aos "${_weasel_archive}" -o"${_weasel_extract_dir}"
        RESULT_VARIABLE _extract_result
    )
    if(NOT _extract_result EQUAL 0)
        message(WARNING "Failed to extract Weasel runtime from ${_weasel_archive}")
        return()
    endif()

    file(GLOB_RECURSE _rime_candidates "${_weasel_extract_dir}/rime.dll")
    if(NOT _rime_candidates)
        message(WARNING "rime.dll not found after extracting ${_weasel_archive}")
        return()
    endif()
    set(_rime_src "")
    set(_max_size 0)
    foreach(_cand IN LISTS _rime_candidates)
        file(SIZE "${_cand}" _cand_size)
        if(_cand_size GREATER _max_size)
            set(_max_size "${_cand_size}")
            set(_rime_src "${_cand}")
        endif()
    endforeach()
    if(_rime_src STREQUAL "")
        message(WARNING "rime.dll candidate selection failed")
        return()
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_rime_src}" "${RIME_OUTPUT_DIR}/rime.dll"
        RESULT_VARIABLE _copy_result
    )
    if(NOT _copy_result EQUAL 0)
        message(WARNING "Failed to copy rime.dll to ${RIME_OUTPUT_DIR}")
        return()
    endif()
    copy_opencc_data("${_weasel_extract_dir}")
    file(WRITE "${_lua_marker}" "weasel")
    file(REMOVE "${_plain_marker}")
    message(STATUS "Lua-enabled rime.dll copied to ${RIME_OUTPUT_DIR}")
else()
    set(_rime_url "https://github.com/rime/librime/releases/download/${RIME_VERSION}/${RIME_ASSET}")
    set(_archive "${_cache_dir}/${RIME_ASSET}")
    if(NOT EXISTS "${_archive}")
        download_with_retry("${_rime_url}" "${_archive}" "librime runtime" _rime_ok)
        if(NOT _rime_ok)
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
    copy_opencc_data("${_extract_dir}")
    file(WRITE "${_plain_marker}" "librime")
    file(REMOVE "${_lua_marker}")
    message(STATUS "rime.dll copied to ${RIME_OUTPUT_DIR}")
endif()

set(_opencc_dir "${RIME_OUTPUT_DIR}/opencc")
file(MAKE_DIRECTORY "${_opencc_dir}")
foreach(_opencc_file IN ITEMS "emoji.txt" "others.txt")
    set(_opencc_dst "${_opencc_dir}/${_opencc_file}")
    if(EXISTS "${_opencc_dst}")
        continue()
    endif()
    set(_opencc_url "https://raw.githubusercontent.com/iDvel/rime-ice/main/opencc/${_opencc_file}")
    download_with_retry("${_opencc_url}" "${_opencc_dst}" "${_opencc_file}" _opencc_ok)
    if(NOT _opencc_ok)
        message(WARNING "Failed to download ${_opencc_file}")
    endif()
endforeach()
