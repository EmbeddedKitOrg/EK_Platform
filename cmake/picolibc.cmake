# ============================================================================
# picolibc 定位与应用
#
# 对外唯一入口：ek_picolibc_apply(<target>)
# 内部按顺序探测：显式 specs → ROOT → 工具链 specs → 系统 specs
#                   → 源码构建（PICOLIBC_BUILD）→ 下载（PICOLIBC_FETCH）
# ============================================================================

include("${CMAKE_CURRENT_LIST_DIR}/ek_arch.cmake")

set(PICOLIBC_ROOT "" CACHE PATH "picolibc install prefix (contains include/ and lib/)")
set(PICOLIBC_SPECS "" CACHE FILEPATH "Path to picolibc.specs")
set(PICOLIBC_PREFIX "" CACHE PATH "Prefix for --picolibc-prefix (usually not needed)")
set(PICOLIBC_PLATFORM "${PLATFORM}" CACHE STRING
    "picolibc target platform (defaults to PLATFORM; e.g. rv32imac, x86_64)")
option(PICOLIBC_FETCH "Download a prebuilt picolibc package when not found" OFF)
option(PICOLIBC_BUILD "Build picolibc from source when not found (requires meson/ninja)" OFF)
set(PICOLIBC_SOURCE_DIR "" CACHE PATH "picolibc source directory (empty: download source archive)")
set(PICOLIBC_SOURCE_URL "" CACHE STRING "picolibc source archive URL")
set(PICOLIBC_VERSION "1.8.11" CACHE STRING "Prebuilt package version")
set(PICOLIBC_URL "" CACHE STRING "Prebuilt package URL (local path allowed)")
set(PICOLIBC_SHA256 "" CACHE STRING "Prebuilt package SHA256")

# 探测一个安装前缀，结果写入 <prefix>_*。
function(_ek_picolibc_probe_root root prefix)
    foreach(_dir "${root}" "${root}/lib" "${root}/lib/picolibc/lib")
        if(EXISTS "${_dir}/picolibc.specs")
            set(${prefix}_SPECS "${_dir}/picolibc.specs" PARENT_SCOPE)
            set(${prefix}_ROOT "${root}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    unset(_ek_inc CACHE)
    unset(_ek_libc CACHE)
    unset(_ek_crt0 CACHE)
    unset(_ek_libm CACHE)
    unset(_ek_nosys CACHE)
    unset(_ek_ld CACHE)

    find_path(_ek_inc NAMES picolibc.h
        PATHS "${root}/include" "${root}/lib/picolibc/include"
        NO_DEFAULT_PATH)
    find_file(_ek_libc NAMES libc.a
        PATHS "${root}/lib" "${root}/lib/picolibc/lib"
        NO_DEFAULT_PATH)
    if(NOT _ek_inc OR NOT _ek_libc)
        return()
    endif()

    get_filename_component(_libdir "${_ek_libc}" DIRECTORY)
    find_file(_ek_crt0 NAMES libcrt0.a PATHS "${_libdir}" NO_DEFAULT_PATH)
    find_file(_ek_libm NAMES libm.a PATHS "${_libdir}" NO_DEFAULT_PATH)
    find_file(_ek_nosys NAMES libnosys.a PATHS "${_libdir}" NO_DEFAULT_PATH)
    find_file(_ek_ld NAMES picolibc.ld PATHS "${_libdir}" NO_DEFAULT_PATH)

    set(${prefix}_INCLUDE_DIR "${_ek_inc}" PARENT_SCOPE)
    set(${prefix}_LIBC_A "${_ek_libc}" PARENT_SCOPE)
    set(${prefix}_CRT0_A "${_ek_crt0}" PARENT_SCOPE)
    set(${prefix}_LIBM_A "${_ek_libm}" PARENT_SCOPE)
    set(${prefix}_NOSYS_A "${_ek_nosys}" PARENT_SCOPE)
    set(${prefix}_LD "${_ek_ld}" PARENT_SCOPE)
    set(${prefix}_ROOT "${root}" PARENT_SCOPE)
endfunction()

# 从源码构建 picolibc，输出安装前缀路径。
function(_ek_picolibc_build out_var)
    if(NOT PLATFORM MATCHES "^cm")
        message(FATAL_ERROR "PICOLIBC_BUILD only supports ARM PLATFORM (cm0..cm55)")
    endif()

    ek_arch_query("${PLATFORM}" _ek_arch)
    set(_cflags "${_ek_arch_FLAGS}")

    set(_src "${PICOLIBC_SOURCE_DIR}")
    if(NOT _src)
        set(_src "${CMAKE_BINARY_DIR}/_deps/picolibc-src-${PICOLIBC_VERSION}")
        if(NOT EXISTS "${_src}/meson.build")
            set(_src_url "${PICOLIBC_SOURCE_URL}")
            if(NOT _src_url)
                set(_src_url
                    "https://github.com/picolibc/picolibc/archive/refs/tags/${PICOLIBC_VERSION}.tar.gz")
            endif()
            set(_src_archive
                "${CMAKE_BINARY_DIR}/_deps/picolibc-src-${PICOLIBC_VERSION}.tar.gz")
            message(STATUS "picolibc: downloading source ${_src_url}")
            file(DOWNLOAD "${_src_url}" "${_src_archive}" TLS_VERIFY ON TIMEOUT 30 STATUS _dl)
            list(GET _dl 0 _dl_code)
            if(NOT _dl_code EQUAL 0)
                message(FATAL_ERROR "Failed to download picolibc source: ${_dl}")
            endif()
            file(ARCHIVE_EXTRACT INPUT "${_src_archive}" DESTINATION "${_src}")
            file(GLOB _meson_files "${_src}/*/meson.build")
            if(_meson_files)
                list(GET _meson_files 0 _meson_file)
                get_filename_component(_src "${_meson_file}" DIRECTORY)
            endif()
        endif()
    endif()

    if(NOT EXISTS "${_src}/meson.build")
        message(FATAL_ERROR "Invalid picolibc source directory: ${_src}")
    endif()

    find_program(_ek_meson meson REQUIRED)
    find_program(_ek_ninja ninja REQUIRED)

    set(_build "${CMAKE_BINARY_DIR}/_deps/picolibc-build-${PICOLIBC_VERSION}-${PLATFORM}")
    set(_prefix "${CMAKE_BINARY_DIR}/_deps/picolibc-${PICOLIBC_VERSION}-${PLATFORM}")
    set(_cross "${CMAKE_BINARY_DIR}/_deps/picolibc-cross-${PLATFORM}.txt")
    file(WRITE "${_cross}"
        "[binaries]\n"
        "c = ['${CMAKE_C_COMPILER}', '-nostdlib']\n"
        "ar = '${CMAKE_AR}'\n"
        "[host_machine]\n"
        "system = 'none'\n"
        "cpu_family = 'arm'\n"
        "cpu = 'arm'\n"
        "endian = 'little'\n"
        "[properties]\n"
        "skip_sanity_check = true\n")

    if(NOT EXISTS "${_prefix}/lib/picolibc.specs")
        message(STATUS "picolibc: building from source (${PLATFORM})")
        execute_process(
            COMMAND "${_ek_meson}" setup "${_build}" "${_src}"
                --cross-file "${_cross}" --prefix "${_prefix}"
                -Dmultilib=false -Dtests=false -Dspecsdir=lib
                "-Dc_args=${_cflags}"
            RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "picolibc meson setup failed:\n${_err}")
        endif()
        execute_process(
            COMMAND "${_ek_ninja}" -C "${_build}"
            RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "picolibc build failed:\n${_err}")
        endif()
        execute_process(
            COMMAND "${_ek_ninja}" -C "${_build}" install
            RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "picolibc install failed:\n${_err}")
        endif()
    endif()

    set(${out_var} "${_prefix}" PARENT_SCOPE)
endfunction()

# 下载并解压预编译包，输出解压目录路径。
function(_ek_picolibc_fetch out_var)
    if(NOT PICOLIBC_PLATFORM)
        message(FATAL_ERROR "PICOLIBC_FETCH=ON requires PLATFORM or PICOLIBC_PLATFORM")
    endif()

    set(_url "${PICOLIBC_URL}")
    if(NOT _url)
        set(_url "https://github.com/EmbeddedKitOrg/EK_Platform/releases/download/picolibc-${PICOLIBC_VERSION}/picolibc-${PICOLIBC_VERSION}-${PICOLIBC_PLATFORM}.tar.gz")
    endif()

    set(_archive
        "${CMAKE_BINARY_DIR}/_deps/picolibc-fetch-${PICOLIBC_VERSION}-${PICOLIBC_PLATFORM}.tar.gz")
    set(_dest
        "${CMAKE_BINARY_DIR}/_deps/picolibc-fetch-${PICOLIBC_VERSION}-${PICOLIBC_PLATFORM}")

    if(NOT PICOLIBC_SHA256)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps")
        file(DOWNLOAD "${_url}.sha256" "${_archive}.sha256"
            TLS_VERIFY ON TIMEOUT 30 STATUS _dl_sha)
        list(GET _dl_sha 0 _dl_sha_code)
        if(_dl_sha_code EQUAL 0 AND EXISTS "${_archive}.sha256")
            file(READ "${_archive}.sha256" _sha_text)
            string(REGEX MATCH "[0-9a-fA-F]+" _remote_sha "${_sha_text}")
            if(_remote_sha)
                set(PICOLIBC_SHA256 "${_remote_sha}")
                message(STATUS "picolibc: sha256 from ${_url}.sha256")
            endif()
        endif()
    endif()

    set(_archive_ok FALSE)
    if(EXISTS "${_archive}")
        if(PICOLIBC_SHA256)
            file(SHA256 "${_archive}" _cached_sha)
            if(_cached_sha STREQUAL PICOLIBC_SHA256)
                set(_archive_ok TRUE)
            endif()
        else()
            file(SIZE "${_archive}" _cached_size)
            if(_cached_size GREATER 0)
                set(_archive_ok TRUE)
            endif()
        endif()
    endif()

    if(_archive_ok)
        message(STATUS "picolibc: using cached ${_archive}")
    else()
        if(EXISTS "${_url}" AND NOT IS_DIRECTORY "${_url}")
            set(_archive "${_url}")
            set(_archive_ok TRUE)
        else()
            message(STATUS "picolibc: downloading ${_url}")
            file(DOWNLOAD "${_url}" "${_archive}" TLS_VERIFY ON TIMEOUT 30 STATUS _dl)
            list(GET _dl 0 _dl_code)
            if(NOT _dl_code EQUAL 0)
                message(FATAL_ERROR "Failed to download picolibc: ${_dl}")
            endif()
        endif()

        file(SHA256 "${_archive}" _sha)
        if(PICOLIBC_SHA256)
            if(NOT _sha STREQUAL PICOLIBC_SHA256)
                message(FATAL_ERROR
                    "picolibc SHA256 mismatch\n  expected: ${PICOLIBC_SHA256}\n  actual: ${_sha}")
            endif()
        else()
            message(STATUS "picolibc: sha256=${_sha} (not verified; pin it with -DPICOLIBC_SHA256)")
        endif()
    endif()

    set(_dest_ok FALSE)
    if(EXISTS "${_dest}")
        file(GLOB_RECURSE _cached_items
            "${_dest}/*picolibc.specs"
            "${_dest}/*picolibc.h"
            "${_dest}/*libc.a")
        if(_cached_items)
            set(_dest_ok TRUE)
        endif()
    endif()

    if(NOT _dest_ok OR NOT _archive_ok)
        file(REMOVE_RECURSE "${_dest}")
        file(ARCHIVE_EXTRACT INPUT "${_archive}" DESTINATION "${_dest}")
    else()
        message(STATUS "picolibc: using extracted ${_dest}")
    endif()

    set(${out_var} "${_dest}" PARENT_SCOPE)
endfunction()

function(ek_picolibc_apply target)
    set(_found FALSE)
    set(_ek_p_SPECS "")
    set(_ek_p_ROOT "")
    set(_ek_p_INCLUDE_DIR "")
    set(_ek_p_LIBC_A "")
    set(_ek_p_CRT0_A "")
    set(_ek_p_LIBM_A "")
    set(_ek_p_NOSYS_A "")
    set(_ek_p_LD "")

    if(PICOLIBC_SPECS AND EXISTS "${PICOLIBC_SPECS}")
        set(_ek_p_SPECS "${PICOLIBC_SPECS}")
        set(_ek_p_ROOT "${PICOLIBC_ROOT}")
        set(_found TRUE)
        message(STATUS "picolibc: specs ${_ek_p_SPECS}")
    endif()

    if(NOT _found AND PICOLIBC_ROOT)
        _ek_picolibc_probe_root("${PICOLIBC_ROOT}" _ek_p)
        if(_ek_p_SPECS OR _ek_p_LIBC_A)
            set(_found TRUE)
            if(_ek_p_SPECS)
                message(STATUS "picolibc: specs ${_ek_p_SPECS}")
            else()
                message(STATUS "picolibc: ${_ek_p_LIBC_A}")
            endif()
        endif()
    endif()

    if(NOT _found)
        execute_process(
            COMMAND ${CMAKE_C_COMPILER} -print-file-name=picolibc.specs
            OUTPUT_VARIABLE _toolchain_specs
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(IS_ABSOLUTE "${_toolchain_specs}" AND EXISTS "${_toolchain_specs}")
            set(_ek_p_SPECS "${_toolchain_specs}")
            set(_found TRUE)
            message(STATUS "picolibc: specs ${_ek_p_SPECS} (toolchain)")
        endif()
    endif()

    if(NOT _found)
        execute_process(
            COMMAND ${CMAKE_C_COMPILER} -dumpmachine
            OUTPUT_VARIABLE _triple
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        foreach(_sys_specs
            "/usr/lib/picolibc/${_triple}/picolibc.specs"
            "/usr/local/lib/picolibc/${_triple}/picolibc.specs")
            if(EXISTS "${_sys_specs}")
                set(_ek_p_SPECS "${_sys_specs}")
                set(_found TRUE)
                message(STATUS "picolibc: specs ${_ek_p_SPECS} (system)")
                break()
            endif()
        endforeach()
    endif()

    if(NOT _found AND PICOLIBC_BUILD)
        _ek_picolibc_build(_built_prefix)
        _ek_picolibc_probe_root("${_built_prefix}" _ek_p)
        if(_ek_p_SPECS OR _ek_p_LIBC_A)
            set(_found TRUE)
            message(STATUS "picolibc: ${_ek_p_SPECS} (built)")
        endif()
    endif()

    if(NOT _found AND PICOLIBC_FETCH)
        _ek_picolibc_fetch(_fetched_dir)
        _ek_picolibc_probe_root("${_fetched_dir}" _ek_p)
        if(NOT _ek_p_SPECS AND NOT _ek_p_LIBC_A)
            file(GLOB _subdirs "${_fetched_dir}/*")
            foreach(_sub IN LISTS _subdirs)
                if(IS_DIRECTORY "${_sub}")
                    _ek_picolibc_probe_root("${_sub}" _ek_p)
                    if(_ek_p_SPECS OR _ek_p_LIBC_A)
                        break()
                    endif()
                endif()
            endforeach()
        endif()
        if(NOT _ek_p_SPECS AND NOT _ek_p_LIBC_A)
            message(FATAL_ERROR "picolibc package has no specs/include/lib: ${_fetched_dir}")
        endif()
        set(_found TRUE)
        if(_ek_p_SPECS)
            message(STATUS "picolibc: specs ${_ek_p_SPECS} (downloaded)")
        else()
            message(STATUS "picolibc: ${_ek_p_LIBC_A} (downloaded)")
        endif()
    endif()

    if(NOT _found)
        message(FATAL_ERROR
            "picolibc not found.\n"
            "  1) -DPICOLIBC_ROOT=<prefix>     use an installed picolibc\n"
            "  2) add --specs=picolibc.specs to the toolchain\n"
            "  3) -DPICOLIBC_FETCH=ON          download a prebuilt package (SHA256 optional)\n"
            "  4) -DPICOLIBC_BUILD=ON          build from source (requires meson/ninja)\n"
            "  On Debian/Ubuntu: sudo apt install picolibc-arm-none-eabi")
    endif()

    if(_ek_p_SPECS)
        set(_prefix "${PICOLIBC_PREFIX}")
        if(NOT _prefix)
            set(_prefix "${_ek_p_ROOT}")
        endif()
        target_compile_options(${target} PUBLIC "--specs=${_ek_p_SPECS}")
        target_link_options(${target} INTERFACE "--specs=${_ek_p_SPECS}")
        if(_prefix)
            target_compile_options(${target} PUBLIC "--picolibc-prefix=${_prefix}")
            target_link_options(${target} INTERFACE "--picolibc-prefix=${_prefix}")
        endif()
        if(PICOLIBC_PLATFORM STREQUAL "x86_64")
            target_link_options(${target} INTERFACE -no-pie)
        endif()
    else()
        if(NOT _ek_p_CRT0_A OR NOT _ek_p_LIBM_A OR NOT _ek_p_NOSYS_A)
            message(WARNING
                "picolibc installation is missing libcrt0.a/libm.a/libnosys.a; linking may fail")
        endif()
        target_include_directories(${target} PUBLIC "${_ek_p_INCLUDE_DIR}")
        target_link_options(${target} INTERFACE -nostdlib)
        if(_ek_p_LD AND EK_LINKER_MODE STREQUAL "off")
            target_link_options(${target} INTERFACE "-T${_ek_p_LD}")
        endif()
        target_link_libraries(${target} INTERFACE
            -Wl,--start-group
            "${_ek_p_CRT0_A}"
            "${_ek_p_LIBC_A}"
            "${_ek_p_LIBM_A}"
            "${_ek_p_NOSYS_A}"
            gcc
            -Wl,--end-group)
    endif()
endfunction()
