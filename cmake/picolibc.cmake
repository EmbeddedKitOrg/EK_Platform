# picolibc 定位模块
#
# 检测顺序：
#   1. PICOLIBC_SPECS 显式指定
#   2. PICOLIBC_ROOT 下的 picolibc.specs
#   3. 工具链自带的 picolibc.specs（-print-file-name）
#   4. PICOLIBC_ROOT 下的 include/ + lib/（无 specs 的安装）
#   5. PICOLIBC_FETCH=ON 时下载预编译包
#
# 输出：
#   PICOLIBC_SPECS / PICOLIBC_ROOT
#   或 PICOLIBC_INCLUDE_DIR / PICOLIBC_LIBC_A / PICOLIBC_CRT0_A
#      PICOLIBC_LIBM_A / PICOLIBC_NOSYS_A

# 探测一个安装前缀。结果通过 PARENT_SCOPE 交给 ek_picolibc_resolve。
function(_ek_picolibc_probe_root root)
    foreach(_dir "${root}" "${root}/lib" "${root}/lib/picolibc/lib")
        if(EXISTS "${_dir}/picolibc.specs")
            set(PICOLIBC_SPECS "${_dir}/picolibc.specs" PARENT_SCOPE)
            set(PICOLIBC_ROOT "${root}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    find_path(_inc NAMES picolibc.h
        PATHS "${root}/include" "${root}/lib/picolibc/include"
        NO_DEFAULT_PATH)
    find_file(_libc NAMES libc.a
        PATHS "${root}/lib" "${root}/lib/picolibc/lib"
        NO_DEFAULT_PATH)
    if(NOT _inc OR NOT _libc)
        return()
    endif()

    get_filename_component(_libdir "${_libc}" DIRECTORY)
    find_file(_crt0 NAMES libcrt0.a PATHS "${_libdir}" NO_DEFAULT_PATH)
    find_file(_libm NAMES libm.a PATHS "${_libdir}" NO_DEFAULT_PATH)
    find_file(_nosys NAMES libnosys.a PATHS "${_libdir}" NO_DEFAULT_PATH)
    find_file(_ld NAMES picolibc.ld PATHS "${_libdir}" NO_DEFAULT_PATH)

    set(PICOLIBC_INCLUDE_DIR "${_inc}" PARENT_SCOPE)
    set(PICOLIBC_LIBC_A "${_libc}" PARENT_SCOPE)
    set(PICOLIBC_CRT0_A "${_crt0}" PARENT_SCOPE)
    set(PICOLIBC_LIBM_A "${_libm}" PARENT_SCOPE)
    set(PICOLIBC_NOSYS_A "${_nosys}" PARENT_SCOPE)
    set(PICOLIBC_LD "${_ld}" PARENT_SCOPE)
    set(PICOLIBC_ROOT "${root}" PARENT_SCOPE)
endfunction()

# 解析 picolibc。找不到时按 PICOLIBC_FETCH 决定报错还是下载。
function(ek_picolibc_resolve)
    set(_found FALSE)

    if(PICOLIBC_SPECS AND EXISTS "${PICOLIBC_SPECS}")
        set(_found TRUE)
        message(STATUS "picolibc: specs ${PICOLIBC_SPECS}")
    endif()

    if(NOT _found AND PICOLIBC_ROOT)
        _ek_picolibc_probe_root("${PICOLIBC_ROOT}")
        if(PICOLIBC_SPECS OR PICOLIBC_LIBC_A)
            set(_found TRUE)
            if(PICOLIBC_SPECS)
                message(STATUS "picolibc: specs ${PICOLIBC_SPECS}")
            else()
                message(STATUS "picolibc: ${PICOLIBC_LIBC_A}")
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
            set(PICOLIBC_SPECS "${_toolchain_specs}")
            set(_found TRUE)
            message(STATUS "picolibc: specs ${PICOLIBC_SPECS} (toolchain)")
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
                set(PICOLIBC_SPECS "${_sys_specs}")
                set(_found TRUE)
                message(STATUS "picolibc: specs ${PICOLIBC_SPECS} (system)")
                break()
            endif()
        endforeach()
    endif()

    if(NOT _found AND PICOLIBC_BUILD)
        if(NOT PLATFORM)
            message(FATAL_ERROR "PICOLIBC_BUILD=ON 需要 -DPLATFORM=cm0..cm55")
        endif()

        if(PLATFORM STREQUAL "cm0")
            set(_cflags "-mcpu=cortex-m0 -mfloat-abi=soft")
        elseif(PLATFORM STREQUAL "cm3")
            set(_cflags "-mcpu=cortex-m3 -mfloat-abi=soft")
        elseif(PLATFORM STREQUAL "cm4")
            set(_cflags "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
        elseif(PLATFORM STREQUAL "cm7")
            set(_cflags "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard")
        elseif(PLATFORM STREQUAL "cm23")
            set(_cflags "-mcpu=cortex-m23 -mfloat-abi=soft")
        elseif(PLATFORM STREQUAL "cm33")
            set(_cflags "-mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard")
        elseif(PLATFORM STREQUAL "cm55")
            set(_cflags "-mcpu=cortex-m55 -mfpu=fpv5-auto -mfloat-abi=hard")
        else()
            message(FATAL_ERROR "PICOLIBC_BUILD 不支持 PLATFORM=${PLATFORM}")
        endif()

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
                file(DOWNLOAD "${_src_url}" "${_src_archive}" TLS_VERIFY ON STATUS _dl)
                list(GET _dl 0 _dl_code)
                if(NOT _dl_code EQUAL 0)
                    message(FATAL_ERROR "picolibc 源码下载失败: ${_dl}")
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
            message(FATAL_ERROR "picolibc 源码目录无效: ${_src}")
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
                message(FATAL_ERROR "picolibc meson setup 失败:\n${_err}")
            endif()
            execute_process(
                COMMAND "${_ek_ninja}" -C "${_build}"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
            if(NOT _rc EQUAL 0)
                message(FATAL_ERROR "picolibc 构建失败:\n${_err}")
            endif()
            execute_process(
                COMMAND "${_ek_ninja}" -C "${_build}" install
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
            if(NOT _rc EQUAL 0)
                message(FATAL_ERROR "picolibc 安装失败:\n${_err}")
            endif()
        endif()

        _ek_picolibc_probe_root("${_prefix}")
        if(PICOLIBC_SPECS OR PICOLIBC_LIBC_A)
            set(_found TRUE)
            message(STATUS "picolibc: ${PICOLIBC_SPECS} (built)")
        endif()
    endif()

    if(NOT _found)
        if(NOT PICOLIBC_FETCH)
            message(FATAL_ERROR
                "picolibc not found.\n"
                "  1) -DPICOLIBC_ROOT=<prefix>     使用已安装的 picolibc\n"
                "  2) 工具链加 --specs=picolibc.specs\n"
                "  3) -DPICOLIBC_FETCH=ON          下载预编译包（SHA256 可选）\n"
                "  4) -DPICOLIBC_BUILD=ON          从源码构建（需要 meson/ninja）\n"
                "  Debian/Ubuntu 也可以先安装：sudo apt install picolibc-arm-none-eabi")
        endif()

        if(NOT PICOLIBC_PLATFORM)
            message(FATAL_ERROR "PICOLIBC_FETCH=ON 需要 -DPLATFORM 或 -DPICOLIBC_PLATFORM")
        endif()
        if(NOT PICOLIBC_SHA256 AND PICOLIBC_SHA256_${PICOLIBC_PLATFORM})
            set(PICOLIBC_SHA256 "${PICOLIBC_SHA256_${PICOLIBC_PLATFORM}}")
        endif()

        set(_url "${PICOLIBC_URL}")
        if(NOT _url)
            set(_url
                "https://github.com/EmbeddedKitOrg/EK_Platform/releases/download/"
                "picolibc-${PICOLIBC_VERSION}/picolibc-${PICOLIBC_VERSION}-${PICOLIBC_PLATFORM}.tar.gz")
        endif()

        set(_archive
            "${CMAKE_BINARY_DIR}/_deps/picolibc-fetch-${PICOLIBC_VERSION}-${PICOLIBC_PLATFORM}.tar.gz")
        set(_dest
            "${CMAKE_BINARY_DIR}/_deps/picolibc-fetch-${PICOLIBC_VERSION}-${PICOLIBC_PLATFORM}")

        if(NOT PICOLIBC_SHA256)
            set(_sha_text "")
            if(EXISTS "${_url}.sha256")
                file(READ "${_url}.sha256" _sha_text)
            else()
                file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps")
                file(DOWNLOAD "${_url}.sha256" "${_archive}.sha256"
                    TLS_VERIFY ON STATUS _dl_sha)
                list(GET _dl_sha 0 _dl_sha_code)
                if(_dl_sha_code EQUAL 0 AND EXISTS "${_archive}.sha256")
                    file(READ "${_archive}.sha256" _sha_text)
                endif()
            endif()
            if(_sha_text)
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
                file(DOWNLOAD "${_url}" "${_archive}" TLS_VERIFY ON STATUS _dl)
                list(GET _dl 0 _dl_code)
                if(NOT _dl_code EQUAL 0)
                    message(FATAL_ERROR "picolibc 下载失败: ${_dl}")
                endif()
            endif()

            file(SHA256 "${_archive}" _sha)
            if(PICOLIBC_SHA256)
                if(NOT _sha STREQUAL PICOLIBC_SHA256)
                    message(FATAL_ERROR
                        "picolibc SHA256 不匹配\n  期望: ${PICOLIBC_SHA256}\n  实际: ${_sha}")
                endif()
            else()
                message(STATUS "picolibc: sha256=${_sha}（未校验，可用 -DPICOLIBC_SHA256 固定）")
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

        _ek_picolibc_probe_root("${_dest}")
        if(NOT PICOLIBC_SPECS AND NOT PICOLIBC_LIBC_A)
            file(GLOB _subdirs "${_dest}/*")
            foreach(_sub IN LISTS _subdirs)
                if(IS_DIRECTORY "${_sub}")
                    _ek_picolibc_probe_root("${_sub}")
                    if(PICOLIBC_SPECS OR PICOLIBC_LIBC_A)
                        break()
                    endif()
                endif()
            endforeach()
        endif()

        if(NOT PICOLIBC_SPECS AND NOT PICOLIBC_LIBC_A)
            message(FATAL_ERROR "picolibc 包内未找到 specs/include/lib: ${_dest}")
        endif()

        if(PICOLIBC_SPECS)
            set(PICOLIBC_PREFIX "${PICOLIBC_ROOT}")
            message(STATUS "picolibc: specs ${PICOLIBC_SPECS} (downloaded)")
        else()
            message(STATUS "picolibc: ${PICOLIBC_LIBC_A} (downloaded)")
        endif()
    endif()

    foreach(_v
        PICOLIBC_SPECS
        PICOLIBC_ROOT
        PICOLIBC_INCLUDE_DIR
        PICOLIBC_LIBC_A
        PICOLIBC_CRT0_A
        PICOLIBC_LIBM_A
        PICOLIBC_NOSYS_A
        PICOLIBC_LD
        PICOLIBC_PREFIX)
        set(${_v} "${${_v}}" PARENT_SCOPE)
    endforeach()
endfunction()
