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

if(NOT CMAKE_SOURCE_DIR MATCHES "CMakeScratch")
    # 平台必填，不设默认值
    if(NOT PLATFORM)
        message(FATAL_ERROR "PLATFORM is required. Valid values: rv32imac, rv32imafc, rv64imac, rv64gc")
    endif()

    # ISA / ABI 编译标志映射
    if(PLATFORM STREQUAL "rv32imac")
        set(TARGET_FLAGS "-march=rv32imac -mabi=ilp32")
    elseif(PLATFORM STREQUAL "rv32imafc")
        set(TARGET_FLAGS "-march=rv32imafc -mabi=ilp32f")
    elseif(PLATFORM STREQUAL "rv64imac")
        set(TARGET_FLAGS "-march=rv64imac -mabi=lp64")
    elseif(PLATFORM STREQUAL "rv64gc")
        set(TARGET_FLAGS "-march=rv64gc -mabi=lp64d")
    else()
        message(FATAL_ERROR "Unknown PLATFORM: ${PLATFORM}. Valid values: rv32imac, rv32imafc, rv64imac, rv64gc")
    endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fdata-sections -ffunction-sections")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")

set(CMAKE_EXE_LINKER_FLAGS "${TARGET_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")

set(TOOLCHAIN_LINK_LIBRARIES "m")
