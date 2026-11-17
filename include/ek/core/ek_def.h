/**
 * @file ek_def.h
 * @brief 基础类型定义和编译器相关宏
 * @author N1netyNine99
 *
 * 提供跨编译器的统一宏定义，包括：
 * - 弱符号、打包、对齐等属性宏
 * - 内联函数宏
 * - 汇编指令宏
 * - 平台相关的换行符定义
 *
 * 支持的编译器：GCC、ARM Compiler 6、ARM Compiler 5
 */

#ifndef EK_DEF_H
#define EK_DEF_H

/* ========== 软件版本号 ========== */
#define EK_VERSION_MAJOR 6
#define EK_VERSION_MINOR 2
#define EK_VERSION_PATCH 0

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>

/*
 * 部分 C 库（ARM Compiler 的 armlib/microlib、MinGW 等）的 errno.h 只提供
 * EDOM/ERANGE 等少数几个错误码。库内统一使用下列 POSIX 错误码，
 * 缺失时按 newlib 的取值补齐，保证在这些工具链上也能编译。
 */
#ifndef EINTR
#    define EINTR (4)
#endif
#ifndef ENOMEM
#    define ENOMEM (12)
#endif
#ifndef EBUSY
#    define EBUSY (16)
#endif
#ifndef EINVAL
#    define EINVAL (22)
#endif
#ifndef ENOSPC
#    define ENOSPC (28)
#endif
#ifndef ENODATA
#    define ENODATA (61)
#endif
#ifndef EBADMSG
#    define EBADMSG (77)
#endif
#ifndef ETIMEDOUT
#    define ETIMEDOUT (116)
#endif
#ifndef ECANCELED
#    define ECANCELED (140)
#endif

/* lwprintf 自带 va_list 支持，使用 lwprintf 时不需要 stdarg */
#include "ek/ek_conf_internal.h"
#if EKCFG_IO_LWPRTF != 1
#    include <stdarg.h>
#endif

/**
 * @brief 换行符定义
 * @note Linux 平台使用 \n，其他平台使用 \r\n；可在 ek_conf.h 中预先定义 CRLF 覆盖
 */
#ifndef CRLF
#    ifdef __linux__
#        define CRLF "\n"
#    else
#        define CRLF "\r\n"
#    endif /* __linux__ */
#endif /* CRLF */

/* ========== GCC 编译器宏定义 ========== */
#if defined(__GNUC__) && !defined(__ARMCC_VERSION)

#    define __EK_WEAK          __attribute__((weak))
#    define __EK_PACKED        __attribute__((packed))
#    define __EK_PACKED_STRUCT struct __attribute__((packed))
#    define __EK_ALIGNED(x)    __attribute__((aligned(x)))
#    define __EK_USED          __attribute__((used))
#    define __EK_SECTION(x)    __attribute__((section(x)))
#    define __EK_NO_RETURN     __attribute__((noreturn))
#    define __EK_UNUSED(x)     ((void)(x))
#    define __EK_INLINE        inline
#    define __EK_STATIC_INLINE static inline
#    define __EK_ALWAYS_INLINE __attribute__((always_inline)) static inline
#    define __EK_ASM           __asm
/* ========== ARM Compiler 6 宏定义 ========== */
#elif defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)

#    define __EK_WEAK          __attribute__((weak))
#    define __EK_PACKED        __attribute__((packed))
#    define __EK_PACKED_STRUCT struct __attribute__((packed))
#    define __EK_ALIGNED(x)    __attribute__((aligned(x)))
#    define __EK_USED          __attribute__((used))
#    define __EK_SECTION(x)    __attribute__((section(x)))
#    define __EK_NO_RETURN     __attribute__((noreturn))
#    define __EK_UNUSED(x)     ((void)(x))
#    define __EK_INLINE        inline
#    define __EK_STATIC_INLINE static inline
#    define __EK_ALWAYS_INLINE __attribute__((always_inline)) static inline
#    define __EK_ASM           __asm

/* ========== ARM Compiler 5 宏定义 ========== */
#elif defined(__CC_ARM)

#    define __EK_WEAK          __weak
#    define __EK_PACKED        __packed
#    define __EK_PACKED_STRUCT __packed struct
#    define __EK_ALIGNED(x)    __align(x)
#    define __EK_USED          __attribute__((used))
#    define __EK_SECTION(x)    __attribute__((section(x)))
#    define __EK_NO_RETURN     __declspec(noreturn)
#    define __EK_UNUSED(x)     ((void)(x))
#    define __EK_INLINE        __inline
#    define __EK_STATIC_INLINE static __inline
#    define __EK_ALWAYS_INLINE __forceinline
#    define __EK_ASM           __asm

/* ========== 不支持的编译器（空定义） ========== */
#else
#    define __EK_WEAK
#    define __EK_PACKED
#    define __EK_PACKED_STRUCT struct
#    define __EK_ALIGNED(x)
#    define __EK_USED
#    define __EK_SECTION(x)
#    define __EK_NO_RETURN
#    define __EK_UNUSED(x)     ((void)(x))
#    define __EK_INLINE        inline
#    define __EK_STATIC_INLINE static inline
#    define __EK_ALWAYS_INLINE static inline
#    define __EK_ASM           asm
#endif

/* ========== 功能宏 ========== */
#if defined(__cplusplus) && (__cplusplus >= 201103L)
#    define EK_STATIC_ASSERT(expr, msg) static_assert(expr, msg)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#    define EK_STATIC_ASSERT(expr, msg) _Static_assert(expr, msg)
#else
/* C99 及更早：用负长度数组触发编译错误 */
#    define _EK_SA_JOIN2(a, b)          a##b
#    define _EK_SA_JOIN(a, b)           _EK_SA_JOIN2(a, b)
#    define EK_STATIC_ASSERT(expr, msg) typedef char _EK_SA_JOIN(_ek_static_assert_, __LINE__)[(expr) ? 1 : -1]
#endif

/**
 * @brief 编译器屏障：禁止编译器跨越此处重排内存访问
 */
#if defined(__GNUC__) || defined(__clang__) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#    define EK_COMPILER_BARRIER() __asm volatile("" ::: "memory")
#elif defined(__CC_ARM)
#    define EK_COMPILER_BARRIER() __schedule_barrier()
#else
#    define EK_COMPILER_BARRIER() ((void)0)
#endif

/**
 * @brief 内存屏障：用于无锁结构在中断/多核之间发布数据
 * @note 单核 MCU 上 ISR 与主循环之间只需要编译器屏障，但 Cortex-M7 等
 *       带写缓冲的内核以及多核场景需要硬件屏障，这里统一使用完整屏障
 */
#if defined(__GNUC__) || defined(__clang__) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#    define EK_MEMORY_BARRIER() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#elif defined(__CC_ARM)
#    define EK_MEMORY_BARRIER()   \
        do                        \
        {                         \
            __schedule_barrier(); \
            __dmb(0xF);           \
            __schedule_barrier(); \
        } while (0)
#else
#    define EK_MEMORY_BARRIER() ((void)0)
#endif

/**
 * @brief 计算 32 位整数末尾 0 的个数
 * @param x 输入值，必须非 0
 */
__EK_STATIC_INLINE uint32_t ek_ctz32(uint32_t x)
{
#if defined(__GNUC__) || defined(__clang__) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
    return (uint32_t)__builtin_ctz(x);
#elif defined(__CC_ARM)
    return (uint32_t)__clz(__rbit(x));
#else
    uint32_t n = 0U;
    while ((x & 1U) == 0U)
    {
        x >>= 1;
        n++;
    }
    return n;
#endif
}

/**
 * @brief tick 比较（自动处理 32 位回绕）
 *
 * 只要两个 tick 的实际间隔小于 2^31，比较结果就正确。
 */
#define EK_TICK_AFTER(a, b)    ((int32_t)((uint32_t)(b) - (uint32_t)(a)) < 0) /**< a 晚于 b */
#define EK_TICK_AFTER_EQ(a, b) ((int32_t)((uint32_t)(a) - (uint32_t)(b)) >= 0) /**< a 晚于或等于 b */

#define EK_FREQ_K(x)                       ((x) * 1000UL)
#define EK_FREQ_M(x)                       ((x) * 1000UL * 1000UL)
#define EK_ARRAY_LEN(x)                    (sizeof(x) / (sizeof((x)[0])))
#define EK_CONTAINER_OF(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

#define EK_DIV_CEIL(x, div)             \
    ({                                  \
        __typeof__(x) _x = (x);         \
        __typeof__(div) _d = (div);     \
        _d ? ((_x) + _d - 1) / _d : _x; \
    })

#define EK_CLAMP(val, min, max)                           \
    ({                                                    \
        __typeof__(val) _val = (val);                     \
        __typeof__(min) _min = (min);                     \
        __typeof__(max) _max = (max);                     \
        _val < _min ? _min : (_val > _max ? _max : _val); \
    })

#define EK_MAX(v1, v2)             \
    ({                             \
        __typeof__(v1) _v1 = (v1); \
        __typeof__(v2) _v2 = (v2); \
        _v1 > _v2 ? _v1 : _v2;     \
    })

#define EK_MIN(v1, v2)             \
    ({                             \
        __typeof__(v1) _v1 = (v1); \
        __typeof__(v2) _v2 = (v2); \
        _v1 < _v2 ? _v1 : _v2;     \
    })

#define EK_GET_FILE_NAME(file_path)                            \
    (strrchr((file_path), '/') ? strrchr((file_path), '/') + 1 \
                               : (strrchr((file_path), '\\') ? strrchr((file_path), '\\') + 1 : (file_path)))

/* ========== 错误传播宏 ========== */
/** 表达式返回负错误码时直接返回该错误码 */
#define EK_ERR_RETURN(expr)          \
    do                               \
    {                                \
        int _err_ = (expr);          \
        if (_err_ < 0) return _err_; \
    } while (0)

/** 表达式返回负错误码时跳转到指定标签 */
#define EK_ERR_GOTO(expr, label)   \
    do                             \
    {                              \
        int _err_ = (expr);        \
        if (_err_ < 0) goto label; \
    } while (0)

#endif /* EK_DEF_H */
