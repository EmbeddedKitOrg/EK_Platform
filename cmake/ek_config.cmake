# ============================================================================
# EK 项目配置入口
#
# 在 add_subdirectory(ek_platform) 之前 include 本文件，然后调用：
#   ek_set_conf_file("Core/Inc/ek_conf.h")
#   ek_set_linker_file("Core/Inc/ek_board.h")
#   ek_set_linker_arch(riscv)          # 可选，默认从 PLATFORM 推导
#   ek_set_linker_template("custom.ld.S")  # 可选，显式覆盖模板
#
# 这些函数写入 CMake cache；库在 add_subdirectory 时读取。
# ============================================================================

include_guard(GLOBAL)

# ek_conf.h：组件功能开关
function(ek_set_conf_file path)
    get_filename_component(_path "${path}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(EK_CONF_PATH "${_path}" CACHE STRING "Path to ek_conf.h" FORCE)
endfunction()

# ek_board.h：板级内存布局，链接脚本的输入
function(ek_set_board_file path)
    get_filename_component(_path "${path}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(EK_BOARD_PATH "${_path}" CACHE FILEPATH "Path to ek_board.h" FORCE)
endfunction()

# ek_set_linker_file 是 ek_set_board_file 的别名：
# 这里的 "linker file" 指驱动链接脚本的板级内存布局文件。
function(ek_set_linker_file path)
    ek_set_board_file("${path}")
endfunction()

# 目标平台：cm0..cm55 / rv32imac..rv64gc。
# 使用外部 toolchain file 时必须在 project() 之前调用。
function(ek_set_arch arch)
    set(PLATFORM "${arch}" CACHE STRING "Target platform" FORCE)
endfunction()

# 是否使用 picolibc（ON/OFF）。
function(ek_set_picolibc enabled)
    set(PICOLIBC "${enabled}" CACHE BOOL "Use picolibc" FORCE)
endfunction()

# 链接模板架构：cortexm / riscv。
# 不调用时由 PLATFORM 推导（cm* -> cortexm，rv* -> riscv）。
function(ek_set_linker_arch arch)
    set(EK_LINKER_ARCH "${arch}" CACHE STRING
        "Linker template architecture (cortexm, riscv)" FORCE)
endfunction()

# 显式覆盖链接模板；一般不设置，由 EK_LINKER_ARCH 自动选择。
function(ek_set_linker_template path)
    get_filename_component(_path "${path}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(EK_LINKER_TEMPLATE "${_path}" CACHE FILEPATH
        "Explicit linker template override" FORCE)
endfunction()
