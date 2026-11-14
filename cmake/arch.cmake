# ============================================================================
# EK 架构表 —— PLATFORM 的唯一真相
#
# 输出（PARENT_SCOPE）：
#   <prefix>_FAMILY            arm / riscv / host
#   <prefix>_FLAGS             编译器与链接器共用的 -mcpu/-march 标志
#   <prefix>_TOOLCHAIN         cmake/ 下的工具链文件名
#   <prefix>_LINKER_ARCH       cortexm / riscv / host
#   <prefix>_ENTRY             链接脚本入口符号
#   <prefix>_PICOLIBC_PLATFORM picolibc 预编译包平台名
# ============================================================================

include_guard(GLOBAL)

function(ek_arch_query arch prefix)
    if(arch MATCHES "^cm(0|3|4|7|23|33|55)$")
        set(_family arm)
        set(_toolchain gcc-arm-none-eabi.cmake)
        set(_linker_arch cortexm)
        set(_entry Reset_Handler)

        if(arch STREQUAL "cm0")
            set(_flags "-mcpu=cortex-m0 -mfloat-abi=soft")
        elseif(arch STREQUAL "cm3")
            set(_flags "-mcpu=cortex-m3 -mfloat-abi=soft")
        elseif(arch STREQUAL "cm4")
            set(_flags "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
        elseif(arch STREQUAL "cm7")
            set(_flags "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard")
        elseif(arch STREQUAL "cm23")
            set(_flags "-mcpu=cortex-m23 -mfloat-abi=soft")
        elseif(arch STREQUAL "cm33")
            set(_flags "-mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard")
        else()
            set(_flags "-mcpu=cortex-m55 -mfpu=auto -mfloat-abi=hard")
        endif()
    elseif(arch MATCHES "^rv(32imac|32imafc|64imac|64gc)$")
        set(_family riscv)
        set(_toolchain gcc-riscv-none-elf.cmake)
        set(_linker_arch riscv)
        set(_entry _start)

        if(arch STREQUAL "rv32imac")
            set(_flags "-march=rv32imac -mabi=ilp32")
        elseif(arch STREQUAL "rv32imafc")
            set(_flags "-march=rv32imafc -mabi=ilp32f")
        elseif(arch STREQUAL "rv64imac")
            set(_flags "-march=rv64imac -mabi=lp64")
        else()
            set(_flags "-march=rv64gc -mabi=lp64d")
        endif()
    elseif(arch STREQUAL "host" OR arch STREQUAL "x86_64")
        set(_family host)
        set(_flags "")
        set(_toolchain gnu-gcc.cmake)
        set(_linker_arch host)
        set(_entry "")
        set(_picolibc_platform x86_64)
    else()
        message(FATAL_ERROR "ek_arch_query: unknown arch '${arch}'")
    endif()

    if(NOT _picolibc_platform)
        set(_picolibc_platform "${arch}")
    endif()

    set(${prefix}_FAMILY "${_family}" PARENT_SCOPE)
    set(${prefix}_FLAGS "${_flags}" PARENT_SCOPE)
    set(${prefix}_TOOLCHAIN "${_toolchain}" PARENT_SCOPE)
    set(${prefix}_LINKER_ARCH "${_linker_arch}" PARENT_SCOPE)
    set(${prefix}_ENTRY "${_entry}" PARENT_SCOPE)
    set(${prefix}_PICOLIBC_PLATFORM "${_picolibc_platform}" PARENT_SCOPE)
endfunction()
