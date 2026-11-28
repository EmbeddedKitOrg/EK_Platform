/**
 * @file def.h
 * @brief 基础类型定义、版本号与通用工具宏
 * @author N1netyNine99
 *
 * 汇总标准头文件与编译器抽象宏（compiler.h），并提供：
 * - 软件版本号与构建模式查询接口
 * - 平台相关的换行符定义（CRLF）
 * - 常用位运算、对齐、容器偏移与数学辅助宏
 * - 错误码传播宏（EK_ERR_RETURN / EK_ERR_GOTO）
 * - 跨平台基础类型兜底定义（ssize_t / off_t / ek_off_t）
 */

#ifndef EK_DEF_H
#define EK_DEF_H

/* ========== 软件版本号 ========== */
#define EK_VERSION_MAJOR 7
#define EK_VERSION_MINOR 1
#define EK_VERSION_PATCH 0

#include "ek/core/compiler.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include <limits.h>

/* lwprintf 自带 va_list 支持，使用 lwprintf 时不需要 stdarg */
#include "ek/conf_internal.h"
#if EKCFG_IO_LWPRTF != 1
#    include <stdarg.h>
#endif

/* ========== 构建模式接口 ========== */
#define EK_IS_DEBUG   (EKCFG_DEBUG == 1)
#define EK_IS_RELEASE (EKCFG_RELEASE == 1)

#if EK_IS_DEBUG
#    define EK_DEBUG_ONLY(stmt) \
        do                      \
        {                       \
            stmt;               \
        } while (0)
#else
#    define EK_DEBUG_ONLY(stmt) ((void)0)
#endif

/**
 * @brief 查询当前是否以 Debug 模式构建
 * @return true 为 Debug 模式，false 为 Release（非 Debug）模式
 */
static inline bool ek_is_debug(void)
{
    return EK_IS_DEBUG;
}

/**
 * @brief 获取当前构建模式名称字符串
 * @return "Debug" 或 "Release"
 */
static inline const char *ek_build_mode_str(void)
{
    return EK_IS_DEBUG ? "Debug" : "Release";
}

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
#define EK_INC_MOD(x, max)                 (((x) + 1 >= (max)) ? 0 : ((x) + 1))
#define EK_MOD(x, max)                     ((x) >= (max) ? (x) - (max) : (x))

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

/* ========== 编译期整数格式化辅助宏 ========== */
/* 剥除可选外层圆括号，再把 0..99 字面量映射为两位零填充字符串供链接器 SORT 段名使用 */
#define _EK_PP_CAT2(a, b) a##b
#define _EK_PP_CAT(a, b)  _EK_PP_CAT2(a, b)
#define _EK_PP_ISH(...)   _EK_PP_ISH __VA_ARGS__
#define _EK_PP_VAN_EK_PP_ISH
#define _EK_PP_ESC_(...)    _EK_PP_VAN##__VA_ARGS__
#define _EK_PP_ESC(...)     _EK_PP_ESC_(__VA_ARGS__)
#define _EK_PP_UNPAREN(x)   _EK_PP_ESC(_EK_PP_ISH x)
#define _EK_PP_PAD2_IMPL(n) _EK_PP_CAT(_EK_PP_PAD2_, n)
#define EK_PP_PAD2_STR(x)   _EK_PP_PAD2_IMPL(_EK_PP_UNPAREN(x))

#define _EK_PP_PAD2_0       "00"
#define _EK_PP_PAD2_1       "01"
#define _EK_PP_PAD2_2       "02"
#define _EK_PP_PAD2_3       "03"
#define _EK_PP_PAD2_4       "04"
#define _EK_PP_PAD2_5       "05"
#define _EK_PP_PAD2_6       "06"
#define _EK_PP_PAD2_7       "07"
#define _EK_PP_PAD2_8       "08"
#define _EK_PP_PAD2_9       "09"
#define _EK_PP_PAD2_10      "10"
#define _EK_PP_PAD2_11      "11"
#define _EK_PP_PAD2_12      "12"
#define _EK_PP_PAD2_13      "13"
#define _EK_PP_PAD2_14      "14"
#define _EK_PP_PAD2_15      "15"
#define _EK_PP_PAD2_16      "16"
#define _EK_PP_PAD2_17      "17"
#define _EK_PP_PAD2_18      "18"
#define _EK_PP_PAD2_19      "19"
#define _EK_PP_PAD2_20      "20"
#define _EK_PP_PAD2_21      "21"
#define _EK_PP_PAD2_22      "22"
#define _EK_PP_PAD2_23      "23"
#define _EK_PP_PAD2_24      "24"
#define _EK_PP_PAD2_25      "25"
#define _EK_PP_PAD2_26      "26"
#define _EK_PP_PAD2_27      "27"
#define _EK_PP_PAD2_28      "28"
#define _EK_PP_PAD2_29      "29"
#define _EK_PP_PAD2_30      "30"
#define _EK_PP_PAD2_31      "31"
#define _EK_PP_PAD2_32      "32"
#define _EK_PP_PAD2_33      "33"
#define _EK_PP_PAD2_34      "34"
#define _EK_PP_PAD2_35      "35"
#define _EK_PP_PAD2_36      "36"
#define _EK_PP_PAD2_37      "37"
#define _EK_PP_PAD2_38      "38"
#define _EK_PP_PAD2_39      "39"
#define _EK_PP_PAD2_40      "40"
#define _EK_PP_PAD2_41      "41"
#define _EK_PP_PAD2_42      "42"
#define _EK_PP_PAD2_43      "43"
#define _EK_PP_PAD2_44      "44"
#define _EK_PP_PAD2_45      "45"
#define _EK_PP_PAD2_46      "46"
#define _EK_PP_PAD2_47      "47"
#define _EK_PP_PAD2_48      "48"
#define _EK_PP_PAD2_49      "49"
#define _EK_PP_PAD2_50      "50"
#define _EK_PP_PAD2_51      "51"
#define _EK_PP_PAD2_52      "52"
#define _EK_PP_PAD2_53      "53"
#define _EK_PP_PAD2_54      "54"
#define _EK_PP_PAD2_55      "55"
#define _EK_PP_PAD2_56      "56"
#define _EK_PP_PAD2_57      "57"
#define _EK_PP_PAD2_58      "58"
#define _EK_PP_PAD2_59      "59"
#define _EK_PP_PAD2_60      "60"
#define _EK_PP_PAD2_61      "61"
#define _EK_PP_PAD2_62      "62"
#define _EK_PP_PAD2_63      "63"
#define _EK_PP_PAD2_64      "64"
#define _EK_PP_PAD2_65      "65"
#define _EK_PP_PAD2_66      "66"
#define _EK_PP_PAD2_67      "67"
#define _EK_PP_PAD2_68      "68"
#define _EK_PP_PAD2_69      "69"
#define _EK_PP_PAD2_70      "70"
#define _EK_PP_PAD2_71      "71"
#define _EK_PP_PAD2_72      "72"
#define _EK_PP_PAD2_73      "73"
#define _EK_PP_PAD2_74      "74"
#define _EK_PP_PAD2_75      "75"
#define _EK_PP_PAD2_76      "76"
#define _EK_PP_PAD2_77      "77"
#define _EK_PP_PAD2_78      "78"
#define _EK_PP_PAD2_79      "79"
#define _EK_PP_PAD2_80      "80"
#define _EK_PP_PAD2_81      "81"
#define _EK_PP_PAD2_82      "82"
#define _EK_PP_PAD2_83      "83"
#define _EK_PP_PAD2_84      "84"
#define _EK_PP_PAD2_85      "85"
#define _EK_PP_PAD2_86      "86"
#define _EK_PP_PAD2_87      "87"
#define _EK_PP_PAD2_88      "88"
#define _EK_PP_PAD2_89      "89"
#define _EK_PP_PAD2_90      "90"
#define _EK_PP_PAD2_91      "91"
#define _EK_PP_PAD2_92      "92"
#define _EK_PP_PAD2_93      "93"
#define _EK_PP_PAD2_94      "94"
#define _EK_PP_PAD2_95      "95"
#define _EK_PP_PAD2_96      "96"
#define _EK_PP_PAD2_97      "97"
#define _EK_PP_PAD2_98      "98"
#define _EK_PP_PAD2_99      "99"

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
