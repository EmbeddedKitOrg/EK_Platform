set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          riscv)
set(CMAKE_C_COMPILER_ID             GNU)
set(CMAKE_CXX_COMPILER_ID           GNU)

# 工具链前缀。默认依赖 PATH；xpack 等安装可用绝对路径，末尾保留 '-'。
# 例：-DRISCV_TOOLCHAIN_PREFIX=/opt/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-
set(RISCV_TOOLCHAIN_PREFIX "riscv-none-elf-" CACHE STRING
    "RISC-V toolchain prefix, including path and trailing '-'")

set(CMAKE_C_COMPILER                "${RISCV_TOOLCHAIN_PREFIX}gcc")
set(CMAKE_ASM_COMPILER              "${CMAKE_C_COMPILER}")
set(CMAKE_CXX_COMPILER              "${RISCV_TOOLCHAIN_PREFIX}g++")
set(CMAKE_LINKER                    "${RISCV_TOOLCHAIN_PREFIX}g++")
set(CMAKE_OBJCOPY                   "${RISCV_TOOLCHAIN_PREFIX}objcopy")
set(CMAKE_SIZE                      "${RISCV_TOOLCHAIN_PREFIX}size")

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX     ".elf")
set(CMAKE_TRY_COMPILE_TARGET_TYPE   STATIC_LIBRARY)

include("${CMAKE_CURRENT_LIST_DIR}/ek_arch.cmake")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES PLATFORM RISCV_TOOLCHAIN_PREFIX)

if(NOT PLATFORM)
    message(FATAL_ERROR "PLATFORM is required; call ek_platform_prepare(ARCH ...) before project()")
endif()

ek_arch_query("${PLATFORM}" _ek_arch)
if(NOT _ek_arch_FAMILY STREQUAL "riscv")
    message(FATAL_ERROR "gcc-riscv-none-elf.cmake: PLATFORM=${PLATFORM} is not a RISC-V core")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_ek_arch_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fdata-sections -ffunction-sections")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")

set(CMAKE_EXE_LINKER_FLAGS "${_ek_arch_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")
