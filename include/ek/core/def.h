/**
 * @file def.h
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
#define EK_VERSION_MAJOR 7
#define EK_VERSION_MINOR 0
#define EK_VERSION_PATCH 0

#include "ek/core/compiler.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include <limits.h>

/* lwprintf 自带 va_list 支持，使用 lwprintf 时不需要 stdarg */
#include "ek/conf_internal.h"
#if EKCFG_IO_LWPRTF != 1
#    include <stdarg.h>
#endif

/**
 * @brief 换行符定义
 * @note Linux 平台使用 \n，其他平台使用 \r\n
 */
#ifdef __linux__
#    define CRLF "\n"
#else
#    define CRLF "\r\n"
#endif /* __linux__ */

/* ========== 功能宏 ========== */
#define EK_STATIC_ASSERT(expr, msg)        static_assert(expr, msg)
#define EK_FREQ_K(x)                       ((x) * 1000UL)
#define EK_FREQ_M(x)                       ((x) * 1000UL * 1000UL)
#define EK_ARRAY_LEN(x)                    (sizeof(x) / (sizeof((x)[0])))
#define EK_CONTAINER_OF(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#define EK_ALIGN_UP(n, align)              (((uintptr_t)(n) + ((uintptr_t)(align) - 1U)) & ~((uintptr_t)(align) - 1U))
#define EK_ALIGN_DOWN(n, align)            ((uintptr_t)(n) & ~((uintptr_t)(align) - 1U))

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

#if !defined(_SSIZE_T_DECLARED) && !defined(__ssize_t_defined)
typedef long ssize_t;
#    define _SSIZE_T_DECLARED
#    define __ssize_t_defined
#endif

#if !defined(_OFF_T_DECLARED) && !defined(__off_t_defined) && !defined(_OFF_T)
typedef long off_t;
#    define _OFF_T_DECLARED
#    define __off_t_defined
#    define _OFF_T
#endif

typedef int64_t ek_off_t;

#endif /* EK_DEF_H */
