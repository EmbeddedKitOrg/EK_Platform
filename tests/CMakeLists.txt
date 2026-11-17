# 主机单元测试（独立工程，不需要交叉工具链）
#
#   cmake -S tests -B build/host-tests -G Ninja
#   cmake --build build/host-tests
#   ctest --test-dir build/host-tests --output-on-failure

cmake_minimum_required(VERSION 3.20)
project(ek_platform_host_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

get_filename_component(EK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(EK_TEST_CONF "${CMAKE_CURRENT_SOURCE_DIR}/ek_conf.h")

option(EK_TEST_SANITIZE "Enable AddressSanitizer/UBSan (GCC/Clang on Linux/macOS)" OFF)

set(EK_LIB_SRCS
    ${EK_ROOT}/src/core/ek_assert.c
    ${EK_ROOT}/src/core/ek_critical.c
    ${EK_ROOT}/src/core/ek_export.c
    ${EK_ROOT}/src/core/ek_io.c
    ${EK_ROOT}/src/core/ek_log.c
    ${EK_ROOT}/src/mem/ek_heap.c
    ${EK_ROOT}/src/mem/ek_static_alloc.c
    ${EK_ROOT}/src/container/ek_str.c
    ${EK_ROOT}/src/container/ek_str_view.c
    ${EK_ROOT}/src/container/ek_ringbuf.c
    ${EK_ROOT}/src/container/ek_kfifo.c
    ${EK_ROOT}/src/container/ek_snapshot.c
    ${EK_ROOT}/src/container/ek_stack.c
    ${EK_ROOT}/src/sched/ek_evoke.c
    ${EK_ROOT}/src/sched/ek_picothread.c
    ${EK_ROOT}/third_party/tlsf/tlsf.c
    ${EK_ROOT}/third_party/lwprintf/lwprintf.c
)

set(EK_WARN_FLAGS -Wall -Wextra)

set(EK_TEST_INCLUDE_DIRS
    ${EK_ROOT}/include
    ${EK_ROOT}/include/ek
    ${EK_ROOT}/include/ek/core
    ${EK_ROOT}/include/ek/mem
    ${EK_ROOT}/include/ek/container
    ${EK_ROOT}/include/ek/sched
    ${EK_ROOT}/third_party/tlsf
    ${EK_ROOT}/third_party/lwprintf/inc
)

# 与主工程一致使用 OBJECT 库：库里的默认实现都是弱符号，
# 放进静态库时 PE/COFF 链接器（MinGW）不会为弱定义拉取归档成员。
add_library(ek_host OBJECT ${EK_LIB_SRCS})
target_include_directories(ek_host PUBLIC ${EK_TEST_INCLUDE_DIRS})
target_compile_definitions(ek_host PUBLIC EK_CONF_PATH="${EK_TEST_CONF}")
target_compile_options(ek_host PRIVATE ${EK_WARN_FLAGS})

if(EK_TEST_SANITIZE)
    target_compile_options(ek_host PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(ek_host PUBLIC -fsanitize=address,undefined)
endif()

enable_testing()

function(ek_add_test name)
    add_executable(${name} ${name}.c test_support.c)
    target_link_libraries(${name} PRIVATE ek_host)
    target_compile_options(${name} PRIVATE ${EK_WARN_FLAGS})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

ek_add_test(test_list)
ek_add_test(test_ringbuf)
ek_add_test(test_kfifo)
ek_add_test(test_log)
ek_add_test(test_picothread)
ek_add_test(test_evoke)

# export / static_alloc 需要链接脚本提供段边界符号，测试里直接 include 实现文件并替换边界符号，
# 因此不链接 ek_host（避免与库内 EKCFG_EXPORT=0 版本的 ek_export_init 重复定义）
add_executable(test_export test_export.c test_support.c
    ${EK_ROOT}/src/core/ek_assert.c
    ${EK_ROOT}/src/core/ek_io.c
    ${EK_ROOT}/src/core/ek_log.c
)
target_include_directories(test_export PRIVATE ${EK_TEST_INCLUDE_DIRS})
target_compile_definitions(test_export PRIVATE EK_CONF_PATH="${EK_TEST_CONF}")
target_compile_options(test_export PRIVATE ${EK_WARN_FLAGS})
add_test(NAME test_export COMMAND test_export)
