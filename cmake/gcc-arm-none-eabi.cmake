set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)
set(CMAKE_C_COMPILER_ID             GNU)
set(CMAKE_CXX_COMPILER_ID           GNU)

set(TOOLCHAIN_PREFIX                arm-none-eabi-)

set(CMAKE_C_COMPILER                ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER              ${CMAKE_C_COMPILER})
set(CMAKE_CXX_COMPILER              ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_LINKER                    ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_OBJCOPY                   ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE                      ${TOOLCHAIN_PREFIX}size)

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX     ".elf")
set(CMAKE_TRY_COMPILE_TARGET_TYPE   STATIC_LIBRARY)

if(NOT CMAKE_SOURCE_DIR MATCHES "CMakeScratch")
    # 平台必填，不设默认值
    if(NOT PLATFORM)
        message(FATAL_ERROR "PLATFORM is required. Valid values: cm0, cm3, cm4, cm7, cm23, cm33, cm55")
    endif()

    # 内核编译标志映射
    if(PLATFORM STREQUAL cm0)
        set(TARGET_FLAGS "-mcpu=cortex-m0 -mfloat-abi=soft")
    elseif(PLATFORM STREQUAL cm3)
        set(TARGET_FLAGS "-mcpu=cortex-m3 -mfloat-abi=soft")
    elseif(PLATFORM STREQUAL cm4)
        set(TARGET_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
    elseif(PLATFORM STREQUAL cm7)
        set(TARGET_FLAGS "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard")
    elseif(PLATFORM STREQUAL cm23)
        set(TARGET_FLAGS "-mcpu=cortex-m23 -mfloat-abi=soft")
    elseif(PLATFORM STREQUAL cm33)
        set(TARGET_FLAGS "-mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard")
    elseif(PLATFORM STREQUAL cm55)
        set(TARGET_FLAGS "-mcpu=cortex-m55 -mfpu=auto -mfloat-abi=hard")
    else()
        message(FATAL_ERROR "Unknown PLATFORM: ${PLATFORM}. Valid values: cm0, cm3, cm4, cm7, cm23, cm33, cm55")
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
# picolibc 时由 cmake/picolibc.cmake 注入 --specs，工具链不再叠加 nano.specs
if(NOT PICOLIBC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs")
endif()
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")

set(TOOLCHAIN_LINK_LIBRARIES "m")
