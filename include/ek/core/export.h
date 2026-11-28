/**
 * @file export.h
 * @brief 函数导出机制
 * @author N1netyNine99
 *
 * 提供基于链接器段的函数自动导出和初始化机制。
 *
 * @note 使用前需在链接脚本 (.ld) 中添加以下段定义（依赖 SORT 在链接期完成排布）：
 * @code
 * .ek_export :
 * {
 *     . = ALIGN(4);
 *     _ek_export_fn_start = .;
 *     KEEP(*(SORT(.ek_export_fn*)))
 *     . = ALIGN(4);
 *     _ek_export_fn_end = .;
 * } > flash
 * @endcode
 */

#ifndef EK_EXPORT_H
#define EK_EXPORT_H

#include "ek/conf_internal.h"

#if (EKCFG_EXPORT == 1)

#    include "ek/core/def.h"

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
 * @brief 导出条目（阶段与顺序已编码进链接器输入段名，链接期由 SORT 完成排序）
 */
typedef struct
{
    _ek_export_init_fn_t fn;
} ek_export_item_t;

/* ========== 导出顺序常量 ========== */
/* 同一阶段内数值越小越先执行（支持 0..99）；相同 phase/order 不保证相对顺序。 */
#    define EK_EXPORT_ORDER_BOOT_ARCH              (1)
#    define EK_EXPORT_ORDER_BOOT_ARCH_IRQ          (2)

#    define EK_EXPORT_ORDER_COMPONENT_HEAP         (1)
#    define EK_EXPORT_ORDER_COMPONENT_VFS          (5)
#    define EK_EXPORT_ORDER_COMPONENT_SHELL        (6)
#    define EK_EXPORT_ORDER_COMPONENT_SCHED        (8)
#    define EK_EXPORT_ORDER_COMPONENT_STATIC_ALLOC (10)
#    define EK_EXPORT_ORDER_COMPONENT_IO           (11)

/* 生成唯一符号名（内部使用） */
#    define _EK_EXPORT_JOIN2(a, b) a##b
#    define _EK_EXPORT_JOIN(a, b)  _EK_EXPORT_JOIN2(a, b)

#    if defined(__COUNTER__)
#        define _EK_EXPORT_UNIQUE_ID __COUNTER__
#    else
#        define _EK_EXPORT_UNIQUE_ID __LINE__
#    endif

/* 导出函数宏（内部使用）：不对外暴露裸 phase，避免调用方绕过阶段分类 */
#    define _EK_EXPORT_LEVEL(fn, phase_str, order)          _EK_EXPORT_LEVEL_IMPL(_EK_EXPORT_UNIQUE_ID, fn, phase_str, order)
#    define _EK_EXPORT_LEVEL_IMPL(id, fn, phase_str, order) _EK_EXPORT_LEVEL_IMPL2(id, fn, phase_str, order)
#    define _EK_EXPORT_LEVEL_IMPL2(id, fn, phase_str, order)                         \
        static const ek_export_item_t _EK_EXPORT_JOIN(__ek_export_item_, id)         \
        __EK_USED __EK_SECTION(".ek_export_fn." phase_str "." EK_PP_PAD2_STR(order)) \
            __EK_ALIGNED(sizeof(void *)) = { (fn) }

/**
 * @brief 启动阶段导出
 * @param fn 要导出的函数指针
 * @param order 阶段内顺序（0..99），数值越小越先执行
 */
#    define EK_EXPORT_BOOT(fn, order) _EK_EXPORT_LEVEL(fn, "0", order)

/**
 * @brief 板级阶段导出
 * @param fn 要导出的函数指针
 * @param order 阶段内顺序（0..99），数值越小越先执行
 */
#    define EK_EXPORT_BOARD(fn, order) _EK_EXPORT_LEVEL(fn, "1", order)

/**
 * @brief 组件阶段导出
 * @param fn 要导出的函数指针
 * @param order 阶段内顺序（0..99），数值越小越先执行
 */
#    define EK_EXPORT_COMPONENT(fn, order) _EK_EXPORT_LEVEL(fn, "2", order)

/**
 * @brief 应用阶段导出
 * @param fn 要导出的函数指针
 * @param order 阶段内顺序（0..99），数值越小越先执行
 */
#    define EK_EXPORT_APP(fn, order) _EK_EXPORT_LEVEL(fn, "3", order)

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
 * @note 初始化顺序已由链接器按段名（阶段 + order）排布，运行时直接顺序遍历调用
 */
void ek_export_init(void);

#ifdef __cplusplus
}
#endif

#endif /* EK_EXPORT_H */
