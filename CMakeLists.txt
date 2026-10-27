cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PICOLIBC "Use picolibc instead of nano.specs" OFF)

# ---------- picolibc 定位 ----------
set(PICOLIBC_ROOT "" CACHE PATH "picolibc install prefix (contains include/ and lib/)")
set(PICOLIBC_SPECS "" CACHE FILEPATH "Path to picolibc.specs")
set(PICOLIBC_PREFIX "" CACHE PATH "Prefix for --picolibc-prefix (usually not needed)")
set(PICOLIBC_PLATFORM "${PLATFORM}" CACHE STRING "picolibc target platform (defaults to PLATFORM; e.g. rv32imac, x86_64)")
option(PICOLIBC_FETCH "Download a prebuilt picolibc package when not found" OFF)
option(PICOLIBC_BUILD "Build picolibc from source when not found (requires meson/ninja)" OFF)
set(PICOLIBC_SOURCE_DIR "" CACHE PATH "picolibc source directory (empty: download source archive)")
set(PICOLIBC_SOURCE_URL "" CACHE STRING "picolibc source archive URL")
set(PICOLIBC_VERSION "1.8.11" CACHE STRING "Prebuilt package version")
set(PICOLIBC_URL "" CACHE STRING "Prebuilt package URL (local path allowed)")
set(PICOLIBC_SHA256 "" CACHE STRING "Prebuilt package SHA256")

# ---------- EK_CONF_PATH: 用户指定 ek_conf.h 路径 ----------
set(EK_CONF_PATH "" CACHE STRING "Path to user's ek_conf.h (e.g. Core/Inc/ek_conf.h)")

# ---------- 工具链（仅独立构建时） ----------
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    if(NOT PLATFORM)
        message(FATAL_ERROR "PLATFORM is required. Usage: cmake -DPLATFORM=cm4 -B build\n"
                            "Valid values: cm0, cm3, cm4, cm7, cm23, cm33, cm55,\n"
                            "              rv32imac, rv32imafc, rv64imac, rv64gc")
    endif()
    if(NOT CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)
    endif()
endif()

project(ek_platform C)

# ---------- 链接脚本 ----------
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ek_linker.cmake)

set(_ek_linker_enabled FALSE)
if(EK_LINKER_AUTO AND EK_CONF_PATH AND EK_BOARD_PATH AND CMAKE_CROSSCOMPILING)
    set(_ek_linker_enabled TRUE)
endif()

# ---------- picolibc 链接标志 ----------
if(PICOLIBC AND CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    string(REPLACE "--specs=nano.specs" ""
        CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endif()

if(PICOLIBC)
    if(CMAKE_EXE_LINKER_FLAGS MATCHES "nano\\.specs")
        message(WARNING "nano.specs and picolibc.specs are both present; remove nano.specs in the parent project")
    endif()
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/picolibc.cmake)
    ek_picolibc_resolve()
endif()

# ---------- 源文件 ----------
set(EK_CORES_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ek_assert.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ek_export.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ek_io.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ek_log.c
)

set(EK_MEM_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mem/ek_heap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mem/ek_static_alloc.c
)

set(EK_CONTAINER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/container/ek_str.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/container/ek_str_view.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/container/ek_ringbuf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/container/ek_snapshot.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/container/ek_stack.c
)

set(EK_SCHED_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sched/ek_evoke.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sched/ek_picothread.c
)

set(PORT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/port/clib/picolibc/picolibc_port.c
    ${CMAKE_CURRENT_SOURCE_DIR}/port/clib/newlib/newlib_port.c
)

set(TP_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tlsf/tlsf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lwprintf/lwprintf.c
)

set(EK_SRCS
    ${EK_CORES_SRCS}
    ${EK_MEM_SRCS}
    ${EK_CONTAINER_SRCS}
    ${EK_SCHED_SRCS}
)

set(EK_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ek
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ek/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ek/mem
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ek/container
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ek/sched
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tlsf
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lwprintf/inc
)

if(PICOLIBC)
    if(PICOLIBC_SPECS)
        set(_ek_picolibc_flags "--specs=${PICOLIBC_SPECS}")
        if(PICOLIBC_PREFIX)
            list(APPEND _ek_picolibc_flags "--picolibc-prefix=${PICOLIBC_PREFIX}")
        endif()
        if(PICOLIBC_PLATFORM STREQUAL "x86_64")
            list(APPEND _ek_picolibc_flags "-no-pie")
        endif()
    endif()
endif()

# ---------- 对象库（配置宏在用户项目中编译时生效） ----------
add_library(ek_platform OBJECT ${EK_SRCS} ${PORT_SRCS} ${TP_SRCS})

target_include_directories(ek_platform PUBLIC ${EK_INCLUDE_DIRS})
target_compile_options(ek_platform PRIVATE
    -Os
    -g
    -fdiagnostics-color=always
)

if(EK_CONF_PATH)
    target_compile_definitions(ek_platform PUBLIC EK_CONF_PATH="${EK_CONF_PATH}")
endif()

if(_ek_linker_enabled)
    _ek_linker_auto_apply()
endif()

if(PICOLIBC)
    if(PICOLIBC_SPECS)
        target_compile_options(ek_platform PUBLIC ${_ek_picolibc_flags})
        target_link_options(ek_platform INTERFACE ${_ek_picolibc_flags})
    else()
        if(NOT PICOLIBC_CRT0_A OR NOT PICOLIBC_LIBM_A OR NOT PICOLIBC_NOSYS_A)
            message(WARNING "picolibc installation is missing libcrt0.a/libm.a/libnosys.a; linking may fail")
        endif()
        target_include_directories(ek_platform PUBLIC ${PICOLIBC_INCLUDE_DIR})
        target_link_options(ek_platform INTERFACE -nostdlib)
        if(PICOLIBC_LD AND NOT _ek_linker_enabled)
            target_link_options(ek_platform INTERFACE "-T${PICOLIBC_LD}")
        endif()
        target_link_libraries(ek_platform INTERFACE
            -Wl,--start-group
            "${PICOLIBC_CRT0_A}"
            "${PICOLIBC_LIBC_A}"
            "${PICOLIBC_LIBM_A}"
            "${PICOLIBC_NOSYS_A}"
            gcc
            -Wl,--end-group)
    endif()
endif()
