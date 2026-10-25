/**
 * @file ek_export.h
 * @brief 函数导出机制
 * @author N1netyNine99
 *
 * 提供基于链接器段的函数自动导出和初始化机制
 *
 * @note 使用前需在链接脚本 (.ld) 中添加以下段定义：
 * @code
 * .ek_export :
 * {
 *     . = ALIGN(4);
 *     _ek_export_fn_start = .;
 *     KEEP(*(.ek_export_fn*))
 *     . = ALIGN(4);
 *     _ek_export_fn_end = .;
 * } > flash
 * @endcode
 */

#ifndef EK_EXPORT_H
#define EK_EXPORT_H

#include "ek_conf_internal.h"

#if (EKCFG_EXPORT == 1)

#    include <stdint.h>

/**
 * @brief 导出初始化函数类型
 */
typedef void (*_ek_export_init_fn_t)(void);

/**
 * @brief 导出条目
 */
typedef struct
{
    uint16_t level;
    uint16_t order;
    _ek_export_init_fn_t fn;
} _ek_export_item_t;

/**
 * @brief 生成唯一符号名（内部使用）
 */
#    define _EK_EXPORT_JOIN2(a, b) a##b
#    define _EK_EXPORT_JOIN(a, b)  _EK_EXPORT_JOIN2(a, b)

/**
 * @brief 导出函数宏
 * @param fn 要导出的函数指针
 * @param level 层级（数字越小越先执行）
 * @param order 层内优先级（数字越小越先执行）
 *
 * @note 所有条目先按 level 排序，再按 order 排序。
 */
#    define EK_EXPORT_LEVEL(fn, level, order)                                       \
        static const _ek_export_item_t _EK_EXPORT_JOIN(__ek_export_item_, __LINE__) \
            __attribute__((used, section(".ek_export_fn"))) = { (uint16_t)(level), (uint16_t)(order), (fn) }

/**
 * @brief 分层导出宏
 */
#    define EK_EXPORT_EARLIEST(fn, order)   EK_EXPORT_LEVEL(fn, 0, order)
#    define EK_EXPORT_HARDWARE(fn, order)   EK_EXPORT_LEVEL(fn, 1, order)
#    define EK_EXPORT_COMPONENTS(fn, order) EK_EXPORT_LEVEL(fn, 2, order)
#    define EK_EXPORT_APP(fn, order)        EK_EXPORT_LEVEL(fn, 3, order)
#    define EK_EXPORT_USER(fn, order)       EK_EXPORT_LEVEL(fn, 4, order)

#else

#    define EK_EXPORT_LEVEL(fn, level, order)
#    define EK_EXPORT_EARLIEST(fn, order)
#    define EK_EXPORT_HARDWARE(fn, order)
#    define EK_EXPORT_COMPONENTS(fn, order)
#    define EK_EXPORT_APP(fn, order)
#    define EK_EXPORT_USER(fn, order)

#endif /* EKCFG_EXPORT */

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 执行所有导出的初始化函数
 * @note 运行时先按层级和层内优先级排序，再逐个调用
 */
void ek_export_init(void);

#ifdef __cplusplus
}
#endif

#endif /* EK_EXPORT_H */
