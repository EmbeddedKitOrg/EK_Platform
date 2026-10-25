cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PICOLIBC "使用 picolibc 替代 nano.specs" OFF)

# ---------- EK_CONF_PATH: 用户指定 ek_conf.h 路径 ----------
set(EK_CONF_PATH "" CACHE STRING "Path to user's ek_conf.h (e.g. Core/Inc/ek_conf.h)")

# ---------- 工具链（仅独立构建时） ----------
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    if(NOT PLATFORM)
        message(FATAL_ERROR "PLATFORM is required. Usage: cmake -DPLATFORM=cm4 -B build\n"
                            "Valid values: cm0, cm3, cm4, cm7, cm23, cm33, cm55")
    endif()
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)
endif()

project(ek_platform C)

# ---------- picolibc 链接标志 ----------
if(PICOLIBC AND CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    string(REPLACE "--specs=nano.specs" ""
        CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
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
    list(APPEND EK_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/picolibc/include
    )
    set(PICOLIBC_LIB_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/picolibc/libs/cortexm/${PLATFORM}
    )
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

if(PICOLIBC)
    target_link_directories(ek_platform PUBLIC ${PICOLIBC_LIB_DIR})
    target_link_libraries(ek_platform PUBLIC c nosys m)
endif()
