# ============================================================================
# EK 平台配置入口（两阶段）
#
# project() 之前：
#   ek_platform_prepare(ARCH cm4 PICOLIBC ON)
#
# add_subdirectory(ek_platform) 之前：
#   ek_platform_configure(CONF Core/Inc/ek_conf.h BOARD Core/Inc/ek_board.h)
# ============================================================================

include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/ek_arch.cmake")

if(NOT DEFINED EK_LINKER_MODE)
    set(EK_LINKER_MODE auto CACHE STRING "EK linker mode (auto, off)")
endif()

function(ek_platform_prepare)
    cmake_parse_arguments(A "" "ARCH;PICOLIBC;TOOLCHAIN" "" ${ARGN})

    if(CMAKE_C_COMPILER_LOADED)
        message(FATAL_ERROR "ek_platform_prepare() must be called before project()")
    endif()
    if(NOT A_ARCH)
        message(FATAL_ERROR "ek_platform_prepare(ARCH <arch>) is required")
    endif()

    ek_arch_query("${A_ARCH}" _ek_arch)

    if(NOT DEFINED A_PICOLIBC)
        set(A_PICOLIBC OFF)
    endif()

    if(A_TOOLCHAIN)
        set(_tc "${A_TOOLCHAIN}")
    else()
        set(_tc "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${_ek_arch_TOOLCHAIN}")
    endif()
    if(NOT EXISTS "${_tc}")
        message(FATAL_ERROR "ek_platform_prepare: toolchain file not found: ${_tc}")
    endif()

    set(PLATFORM "${A_ARCH}" CACHE STRING "EK target arch" FORCE)
    set(PICOLIBC "${A_PICOLIBC}" CACHE BOOL "Use picolibc" FORCE)
    set(PICOLIBC_PLATFORM "${_ek_arch_PICOLIBC_PLATFORM}" CACHE STRING
        "picolibc package platform" FORCE)
    set(CMAKE_TOOLCHAIN_FILE "${_tc}" CACHE FILEPATH "EK toolchain file" FORCE)
    set(EK_PLATFORM_ARCH "${A_ARCH}" CACHE INTERNAL "EK target arch")
    set(EK_LINKER_ARCH "${_ek_arch_LINKER_ARCH}" CACHE STRING "EK linker arch" FORCE)
endfunction()

function(ek_platform_configure)
    cmake_parse_arguments(A "" "CONF;BOARD;LINKER;TEMPLATE" "" ${ARGN})

    if(TARGET ek_platform)
        message(FATAL_ERROR "ek_platform_configure() must be called before add_subdirectory(ek_platform)")
    endif()
    if(NOT EK_PLATFORM_ARCH)
        message(FATAL_ERROR "ek_platform_configure: call ek_platform_prepare() before project()")
    endif()

    if(A_CONF)
        get_filename_component(_conf "${A_CONF}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        if(NOT EXISTS "${_conf}")
            message(FATAL_ERROR "ek_platform_configure: CONF not found: ${_conf}")
        endif()
        set(EK_CONF_PATH "${_conf}" CACHE STRING "Path to ek_conf.h" FORCE)
    endif()

    if(A_BOARD)
        get_filename_component(_board "${A_BOARD}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        if(NOT EXISTS "${_board}")
            message(FATAL_ERROR "ek_platform_configure: BOARD not found: ${_board}")
        endif()
        set(EK_BOARD_PATH "${_board}" CACHE FILEPATH "Path to ek_board.h" FORCE)
    endif()

    if(A_TEMPLATE)
        get_filename_component(_tpl "${A_TEMPLATE}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        if(NOT EXISTS "${_tpl}")
            message(FATAL_ERROR "ek_platform_configure: TEMPLATE not found: ${_tpl}")
        endif()
        set(EK_LINKER_TEMPLATE "${_tpl}" CACHE FILEPATH "EK linker template" FORCE)
    endif()

    set(_mode "${A_LINKER}")
    if(NOT _mode)
        set(_mode auto)
    endif()
    if(NOT _mode MATCHES "^(auto|off)$")
        message(FATAL_ERROR "ek_platform_configure: LINKER must be auto or off (got ${_mode})")
    endif()
    set(EK_LINKER_MODE "${_mode}" CACHE STRING "EK linker mode (auto, off)" FORCE)

    if(EK_LINKER_MODE STREQUAL "auto" AND
       NOT EK_PLATFORM_ARCH MATCHES "^(host|x86_64)$" AND
       (NOT EK_CONF_PATH OR NOT EK_BOARD_PATH))
        message(FATAL_ERROR
            "ek_platform_configure: CONF and BOARD are required for cross builds (or use LINKER off)")
    endif()
endfunction()
