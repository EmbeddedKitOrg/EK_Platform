# ============================================================================
# EK 链接脚本生成
#
# 输入：
#   EK_CONF_PATH   ek_conf.h 路径（组件开关，必需）
#   EK_BOARD_PATH  ek_board.h 路径（板级内存布局，必需）
#   EK_LINKER_TEMPLATE  链接脚本模板，默认 cmake/linker/cortexm.ld.S
#
# 输出：
#   ${CMAKE_CURRENT_BINARY_DIR}/ek_platform.ld（自动模式）
#   ${CMAKE_CURRENT_BINARY_DIR}/<target>.ld（ek_linker_attach）
# ============================================================================

include(${CMAKE_CURRENT_LIST_DIR}/ek_config.cmake)

set(EK_BOARD_PATH "" CACHE FILEPATH
    "Path to user's ek_board.h (copy of ek_board_template.h)")
option(EK_LINKER_AUTO
    "Generate and apply the EK linker script automatically" ON)
set(EK_LINKER_ARCH "" CACHE STRING
    "Linker template architecture (cortexm, riscv); empty = derive from PLATFORM")
set(EK_LINKER_TEMPLATE "" CACHE FILEPATH
    "Explicit linker template override; empty = auto by EK_LINKER_ARCH")
set(EK_LINKER_INCLUDE_DIRS "" CACHE STRING
    "Extra include directories for linker script preprocessing")

function(_ek_linker_resolve_template out_var)
    if(EK_LINKER_TEMPLATE)
        get_filename_component(_template "${EK_LINKER_TEMPLATE}" ABSOLUTE
            BASE_DIR "${CMAKE_SOURCE_DIR}")
        set(${out_var} "${_template}" PARENT_SCOPE)
        return()
    endif()

    set(_arch "${EK_LINKER_ARCH}")
    if(NOT _arch)
        if(PLATFORM MATCHES "^rv")
            set(_arch riscv)
        else()
            set(_arch cortexm)
        endif()
    endif()

    set(_template "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/linker/${_arch}.ld.S")
    if(NOT EXISTS "${_template}")
        message(FATAL_ERROR
            "ek_linker: no template for architecture '${_arch}': ${_template}")
    endif()
    set(${out_var} "${_template}" PARENT_SCOPE)
endfunction()

function(_ek_linker_generate out_var)
    cmake_parse_arguments(ARG "" "CONF_PATH;BOARD_PATH;TEMPLATE;OUTPUT" "" ${ARGN})

    if(NOT ARG_CONF_PATH)
        message(FATAL_ERROR "ek_linker: CONF_PATH is required (set EK_CONF_PATH)")
    endif()
    if(NOT ARG_BOARD_PATH)
        message(FATAL_ERROR "ek_linker: BOARD_PATH is required (set EK_BOARD_PATH)")
    endif()
    if(NOT ARG_TEMPLATE)
        _ek_linker_resolve_template(ARG_TEMPLATE)
    endif()

    get_filename_component(_conf_abs "${ARG_CONF_PATH}" ABSOLUTE
        BASE_DIR "${CMAKE_SOURCE_DIR}")
    get_filename_component(_board_abs "${ARG_BOARD_PATH}" ABSOLUTE
        BASE_DIR "${CMAKE_SOURCE_DIR}")
    get_filename_component(_template_abs "${ARG_TEMPLATE}" ABSOLUTE
        BASE_DIR "${CMAKE_SOURCE_DIR}")

    if(NOT EXISTS "${_conf_abs}")
        message(FATAL_ERROR "ek_linker: EK_CONF_PATH not found: ${_conf_abs}")
    endif()
    if(NOT EXISTS "${_board_abs}")
        message(FATAL_ERROR "ek_linker: EK_BOARD_PATH not found: ${_board_abs}")
    endif()
    if(NOT EXISTS "${_template_abs}")
        message(FATAL_ERROR "ek_linker: template not found: ${_template_abs}")
    endif()

    get_filename_component(_conf_name "${_conf_abs}" NAME)
    if(NOT _conf_name STREQUAL "ek_conf.h")
        message(FATAL_ERROR
            "ek_linker: EK_CONF_PATH must point to ek_conf.h (got ${_conf_name})")
    endif()

    get_filename_component(_conf_dir "${_conf_abs}" DIRECTORY)
    get_filename_component(_board_dir "${_board_abs}" DIRECTORY)

    set(_platform_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
    get_filename_component(_platform_dir "${_platform_dir}" ABSOLUTE)
    set(_internal "${_platform_dir}/include/ek/ek_conf_internal.h")
    set(_platform_include "${_platform_dir}/include")

    if(NOT EXISTS "${_internal}")
        message(FATAL_ERROR "ek_linker: internal config not found: ${_internal}")
    endif()

    if(NOT ARG_OUTPUT)
        set(_out "${CMAKE_CURRENT_BINARY_DIR}/ek_platform.ld")
    else()
        get_filename_component(_out "${ARG_OUTPUT}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    endif()

    set(_incs "-I${_conf_dir}" "-I${_board_dir}" "-I${_platform_include}")
    foreach(_dir IN LISTS EK_LINKER_INCLUDE_DIRS)
        list(APPEND _incs "-I${_dir}")
    endforeach()

    execute_process(
        COMMAND "${CMAKE_C_COMPILER}" -E -P -x c
                -imacros "${_conf_abs}"
                -imacros "${_internal}"
                -imacros "${_board_abs}"
                ${_incs}
                "${_template_abs}"
        OUTPUT_FILE "${_out}"
        RESULT_VARIABLE _rc
        ERROR_VARIABLE _err)

    if(NOT _rc EQUAL 0)
        file(REMOVE "${_out}")
        message(FATAL_ERROR
            "ek_linker: preprocessing failed for ${_template_abs}:\n${_err}")
    endif()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${_conf_abs}" "${_board_abs}" "${_template_abs}" "${_internal}")

    set(${out_var} "${_out}" PARENT_SCOPE)
endfunction()

# 自动模式：生成到 ek_platform 的 build 目录，并通过 INTERFACE 传播给消费者。
function(_ek_linker_auto_apply)
    if(NOT TARGET ek_platform)
        message(FATAL_ERROR "ek_linker: target ek_platform is not defined")
    endif()

    _ek_linker_generate(_ld
        CONF_PATH "${EK_CONF_PATH}"
        BOARD_PATH "${EK_BOARD_PATH}"
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ek_platform.ld")

    target_link_options(ek_platform INTERFACE "-T${_ld}")
    set_property(TARGET ek_platform APPEND PROPERTY
        INTERFACE_LINK_DEPENDS "${_ld}")
endfunction()

# 手动模式：给指定 target 生成独立链接脚本，适用于多 target / 多板子。
function(ek_linker_attach target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "ek_linker_attach: target not found: ${target}")
    endif()

    cmake_parse_arguments(ARG "" "BOARD_PATH;TEMPLATE;OUTPUT" "" ${ARGN})

    set(_board "${ARG_BOARD_PATH}")
    if(NOT _board)
        set(_board "${EK_BOARD_PATH}")
    endif()

    set(_template "${ARG_TEMPLATE}")

    set(_out "${ARG_OUTPUT}")
    if(NOT _out)
        set(_out "${CMAKE_CURRENT_BINARY_DIR}/${target}.ld")
    endif()

    _ek_linker_generate(_ld
        CONF_PATH "${EK_CONF_PATH}"
        BOARD_PATH "${_board}"
        TEMPLATE "${_template}"
        OUTPUT "${_out}")

    target_link_options(${target} PRIVATE "-T${_ld}")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${_ld}")
endfunction()
