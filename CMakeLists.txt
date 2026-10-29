cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/ek_platform.cmake")

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    if(NOT PLATFORM)
        message(FATAL_ERROR "PLATFORM is required (cm0..cm55, rv32imac, rv32imafc, rv64imac, rv64gc, host)")
    endif()
    ek_platform_prepare(ARCH "${PLATFORM}" PICOLIBC "${PICOLIBC}")
endif()

project(ek_platform C)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/ek_linker.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/picolibc.cmake")

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

set(EK_VFS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vfs/ek_vfs_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vfs/vfs_priv.c
)

set(PORT_SRCS)
if(PICOLIBC)
    list(APPEND PORT_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/port/clib/picolibc/picolibc_port.c)
else()
    # newlib/nanolibc 端口；是否真正参与编译由 EKCFG_NEWLIB 决定
    list(APPEND PORT_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/port/clib/newlib/newlib_port.c)
endif()

set(TP_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tlsf/tlsf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lwprintf/lwprintf.c
)

set(EK_SRCS
    ${EK_CORES_SRCS}
    ${EK_MEM_SRCS}
    ${EK_CONTAINER_SRCS}
    ${EK_SCHED_SRCS}
    ${EK_VFS_SRCS}
)

set(EK_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ek
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ek/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ek/mem
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ek/container
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ek/sched
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ek/vfs
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tlsf
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lwprintf/inc
)

file(GLOB_RECURSE _ek_all_src CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
foreach(_src IN LISTS _ek_all_src)
    list(FIND EK_SRCS "${_src}" _idx)
    if(_idx EQUAL -1)
        message(FATAL_ERROR "ek_platform: source not listed in EK_*_SRCS: ${_src}")
    endif()
endforeach()

# ---------- 对象库（配置宏在用户项目中编译时生效） ----------
add_library(ek_platform OBJECT ${EK_SRCS} ${PORT_SRCS} ${TP_SRCS})

target_include_directories(ek_platform PUBLIC ${EK_INCLUDE_DIRS})
target_compile_options(ek_platform PRIVATE
    -fdiagnostics-color=always
    -Os
)

if(EK_CONF_PATH)
    target_compile_definitions(ek_platform PUBLIC EK_CONF_PATH="${EK_CONF_PATH}")
endif()

if(PICOLIBC)
    target_compile_definitions(ek_platform PUBLIC EK_PLATFORM_PICOLIBC=1)
else()
    target_compile_definitions(ek_platform PUBLIC EK_PLATFORM_PICOLIBC=0)
endif()

if(NOT EK_LINKER_MODE STREQUAL "off")
    ek_linker_attach(ek_platform)
endif()

if(PICOLIBC)
    ek_picolibc_apply(ek_platform)
endif()
