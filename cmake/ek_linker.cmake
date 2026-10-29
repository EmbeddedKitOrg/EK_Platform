# ============================================================================
# EK 链接脚本
#
# ek_linker_script() 是纯生成函数，不读全局、不碰 target。
# ek_linker_attach() 是唯一注入点，按 target 类型选择 INTERFACE/PRIVATE。
# ============================================================================

set(EK_LINKER_ARCH "" CACHE STRING
    "Linker template architecture (cortexm, riscv, host)")
set(EK_LINKER_TEMPLATE "" CACHE FILEPATH
    "Explicit linker template override")
set(EK_LINKER_INCLUDE_DIRS "" CACHE STRING
    "Extra include directories for linker script preprocessing")

function(ek_linker_script out_var)
    cmake_parse_arguments(A "" "ARCH;CONF;BOARD;TEMPLATE;OUTPUT" "INCLUDE_DIRS" ${ARGN})

    if(NOT A_ARCH)
        message(FATAL_ERROR "ek_linker_script: ARCH is required (cortexm, riscv)")
    endif()
    if(NOT A_CONF OR NOT A_BOARD)
        message(FATAL_ERROR "ek_linker_script: CONF and BOARD are required")
    endif()

    get_filename_component(_conf "${A_CONF}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(_board "${A_BOARD}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${_conf}")
        message(FATAL_ERROR "ek_linker_script: CONF not found: ${_conf}")
    endif()
    if(NOT EXISTS "${_board}")
        message(FATAL_ERROR "ek_linker_script: BOARD not found: ${_board}")
    endif()

    set(_template "${A_TEMPLATE}")
    if(NOT _template)
        set(_template "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/linker/${A_ARCH}.ld.S")
    else()
        get_filename_component(_template "${_template}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    if(NOT EXISTS "${_template}")
        message(FATAL_ERROR "ek_linker_script: template not found: ${_template}")
    endif()

    set(_internal "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../include/ek/ek_conf_internal.h")
    if(NOT EXISTS "${_internal}")
        message(FATAL_ERROR "ek_linker_script: internal config not found: ${_internal}")
    endif()

    set(_out "${A_OUTPUT}")
    if(NOT _out)
        set(_out "${CMAKE_CURRENT_BINARY_DIR}/ek_platform.ld")
    endif()

    get_filename_component(_conf_dir "${_conf}" DIRECTORY)
    get_filename_component(_board_dir "${_board}" DIRECTORY)
    get_filename_component(_platform_include
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../include" ABSOLUTE)

    set(_incs "-I${_conf_dir}" "-I${_board_dir}" "-I${_platform_include}")
    set(_deps "")
    foreach(_dir IN LISTS A_INCLUDE_DIRS)
        list(APPEND _incs "-I${_dir}")
        file(GLOB_RECURSE _hdrs CONFIGURE_DEPENDS "${_dir}/*.h")
        list(APPEND _deps ${_hdrs})
    endforeach()

    execute_process(
        COMMAND "${CMAKE_C_COMPILER}" -E -P -x c
                -imacros "${_conf}"
                -imacros "${_internal}"
                -imacros "${_board}"
                ${_incs}
                "${_template}"
        OUTPUT_FILE "${_out}"
        RESULT_VARIABLE _rc
        ERROR_VARIABLE _err)

    if(NOT _rc EQUAL 0)
        file(REMOVE "${_out}")
        message(FATAL_ERROR
            "ek_linker_script: preprocessing failed for ${_template}:\n${_err}\nincludes: ${_incs}")
    endif()

    file(READ "${_out}" _ld_text)
    if(NOT _ld_text MATCHES "MEMORY")
        file(REMOVE "${_out}")
        message(FATAL_ERROR
            "ek_linker_script: generated script has no MEMORY section: ${_out}")
    endif()

    if(_deps)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${_conf}" "${_board}" "${_template}" "${_internal}" ${_deps})
    else()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${_conf}" "${_board}" "${_template}" "${_internal}")
    endif()

    set(${out_var} "${_out}" PARENT_SCOPE)
endfunction()

function(ek_linker_attach target)
    cmake_parse_arguments(A "" "ARCH;CONF;BOARD;TEMPLATE;OUTPUT" "" ${ARGN})

    get_target_property(_applied ${target} EK_LINKER_APPLIED)
    if(_applied)
        message(FATAL_ERROR "ek_linker_attach: ${target} already has an EK linker script")
    endif()

    set(_arch "${A_ARCH}")
    if(NOT _arch)
        set(_arch "${EK_LINKER_ARCH}")
    endif()
    set(_conf "${A_CONF}")
    if(NOT _conf)
        set(_conf "${EK_CONF_PATH}")
    endif()
    set(_board "${A_BOARD}")
    if(NOT _board)
        set(_board "${EK_BOARD_PATH}")
    endif()
    set(_template "${A_TEMPLATE}")
    if(NOT _template)
        set(_template "${EK_LINKER_TEMPLATE}")
    endif()

    if(EK_PLATFORM_ARCH MATCHES "^(host|x86_64)$")
        set(_ld "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/linker/host.ld")
        if(NOT EXISTS "${_ld}")
            message(FATAL_ERROR "ek_linker_attach: host linker script not found: ${_ld}")
        endif()
    else()
        if(NOT _conf OR NOT _board)
            message(FATAL_ERROR
                "ek_linker_attach: CONF and BOARD are required for cross builds")
        endif()
        ek_linker_script(_ld
            ARCH "${_arch}"
            CONF "${_conf}"
            BOARD "${_board}"
            TEMPLATE "${_template}"
            OUTPUT "${A_OUTPUT}"
            INCLUDE_DIRS ${EK_LINKER_INCLUDE_DIRS})
    endif()

    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "OBJECT_LIBRARY")
        target_link_options(${target} INTERFACE "-T${_ld}")
    else()
        target_link_options(${target} PRIVATE "-T${_ld}")
    endif()
    set_target_properties(${target} PROPERTIES EK_LINKER_APPLIED TRUE)
endfunction()
