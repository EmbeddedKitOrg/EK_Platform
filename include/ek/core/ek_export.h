/**
 * @file ek_export.h
 * @brief 函数导出机制
 * @author N1netyNine99
 *
 * 提供基于链接器段的函数自动导出和初始化机制。
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

#include "ek/ek_conf_internal.h"

#if (EKCFG_EXPORT == 1)

#    include <stdint.h>

/**
 * @brief 导出初始化函数类型
 */
typedef void (*_ek_export_init_fn_t)(void);

/**
 * @brief 导出阶段
 *
 * 不同阶段可以依赖前面的阶段；同一阶段内的顺序由 order 决定。
 */
typedef enum
{
    EK_EXPORT_PHASE_BOOT = 0, /**< CPU、时钟、VTOR、运行时底座 */
    EK_EXPORT_PHASE_BOARD, /**< BSP：GPIO、UART、Timer、总线 */
    EK_EXPORT_PHASE_COMPONENT, /**< 库组件：heap、io、vfs、scheduler */
    EK_EXPORT_PHASE_APP, /**< 应用服务 */

    EK_EXPORT_PHASE_MAX /**< 阶段数量哨兵 */
} ek_export_phase_e;

/**
 * @brief 导出条目
 */
typedef struct
{
    uint16_t phase;
    uint16_t order;
    _ek_export_init_fn_t fn;
} _ek_export_item_t;

/**
 * @brief 导出顺序常量
 *
 * 同一阶段内数值越小越先执行；相同 phase/order 不保证相对顺序。
 */
#    define EK_EXPORT_ORDER_BOOT_SYSTEM          (0)
#    define EK_EXPORT_ORDER_BOOT_ARCH            (1)

#    define EK_EXPORT_ORDER_BOARD_TIMER          (0)
#    define EK_EXPORT_ORDER_BOARD_UART           (1)
#    define EK_EXPORT_ORDER_BOARD_LED            (2)

#    define EK_EXPORT_ORDER_COMPONENT_HEAP       (1)
#    define EK_EXPORT_ORDER_COMPONENT_VFS        (5)
#    define EK_EXPORT_ORDER_COMPONENT_IO         (10)
#    define EK_EXPORT_ORDER_COMPONENT_SCHED      (10)
#    define EK_EXPORT_ORDER_COMPONENT_STATIC_ALLOC (11)

/**
 * @brief 生成唯一符号名（内部使用）
 */
#    define _EK_EXPORT_JOIN2(a, b) a##b
#    define _EK_EXPORT_JOIN(a, b)  _EK_EXPORT_JOIN2(a, b)

#    if defined(__COUNTER__)
#        define _EK_EXPORT_UNIQUE_ID __COUNTER__
#    else
#        define _EK_EXPORT_UNIQUE_ID __LINE__
#    endif

/**
 * @brief 导出函数宏（内部使用）
 *
 * 不对外暴露裸 phase，避免调用方绕过阶段分类。
 */
#    define _EK_EXPORT_LEVEL(fn, phase, order) \
        _EK_EXPORT_LEVEL_IMPL(_EK_EXPORT_UNIQUE_ID, fn, phase, order)
#    define _EK_EXPORT_LEVEL_IMPL(id, fn, phase, order) \
        _EK_EXPORT_LEVEL_IMPL2(id, fn, phase, order)
#    define _EK_EXPORT_LEVEL_IMPL2(id, fn, phase, order)                         \
        static const _ek_export_item_t _EK_EXPORT_JOIN(__ek_export_item_, id)    \
            __attribute__((used, section(".ek_export_fn"))) = {                  \
                (uint16_t)(phase), (uint16_t)(order), (fn)                       \
            }

/**
 * @brief 启动阶段导出
 * @param fn 要导出的函数指针
 * @param order 阶段内顺序，数值越小越先执行
 */
#    define EK_EXPORT_BOOT(fn, order) \
        _EK_EXPORT_LEVEL(fn, EK_EXPORT_PHASE_BOOT, order)

/**
 * @brief 板级阶段导出
 */
#    define EK_EXPORT_BOARD(fn, order) \
        _EK_EXPORT_LEVEL(fn, EK_EXPORT_PHASE_BOARD, order)

/**
 * @brief 组件阶段导出
 */
#    define EK_EXPORT_COMPONENT(fn, order) \
        _EK_EXPORT_LEVEL(fn, EK_EXPORT_PHASE_COMPONENT, order)

/**
 * @brief 应用阶段导出
 */
#    define EK_EXPORT_APP(fn, order) \
        _EK_EXPORT_LEVEL(fn, EK_EXPORT_PHASE_APP, order)

#else

#    define EK_EXPORT_BOOT(fn, order)
#    define EK_EXPORT_BOARD(fn, order)
#    define EK_EXPORT_COMPONENT(fn, order)
#    define EK_EXPORT_APP(fn, order)

#endif /* EKCFG_EXPORT */

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 执行所有导出的初始化函数
 * @note 运行时先按阶段排序，再按阶段内 order 排序，然后逐个调用
 */
void ek_export_init(void);

#ifdef __cplusplus
}
#endif

#endif /* EK_EXPORT_H */
