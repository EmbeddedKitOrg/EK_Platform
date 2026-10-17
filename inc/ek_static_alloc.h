/**
 * @file ek_static_alloc.h
 * @brief 静态对象自动注册与初始化
 * @author N1netyNine99
 *
 * 将 EK_DEFINE_* 展开出的对象登记到 .ek_static_alloc 链接器段，
 * 由 ek_static_alloc_init() 按 order 升序批量初始化。
 *
 * @note 使用前需在链接脚本 (.ld) 中添加以下段定义：
 * @code
 * .ek_static_alloc :
 * {
 *     . = ALIGN(4);
 *     _ek_static_alloc_start = .;
 *     KEEP(*(SORT(.ek_static_alloc*)))
 *     . = ALIGN(4);
 *     _ek_static_alloc_end = .;
 * } > FLASH
 * @endcode
 *
 * @note EKCFG_EXPORT == 1 时，ek_static_alloc_init() 会通过
 *       EK_EXPORT_COMPONENTS 在 ek_export_init() 中自动调用。
 *       未启用 ek_export 时，必须在堆初始化之后手动调用
 *       ek_static_alloc_init()。
 */

#ifndef EK_STATIC_ALLOC_H
#define EK_STATIC_ALLOC_H

#include "ek_conf_internal.h"

#if EKCFG_STATIC_ALLOC == 1

#    include "ek_def.h"
#    include "ek_err.h"

#    ifdef __cplusplus
extern "C"
{
#    endif

/**
 * @brief 静态对象初始化回调
 * @return EK_OK 成功，非零表示该对象初始化失败
 */
typedef ek_err_t (*_ek_static_alloc_init_fn_t)(void);

/**
 * @brief 静态对象注册项
 */
typedef struct
{
    uint16_t order; /**< 初始化顺序，数值越小越先执行 */
    const char *name; /**< 实例名，失败日志使用 */
    _ek_static_alloc_init_fn_t fn; /**< 初始化回调 */
} _ek_static_alloc_item_t;

#    define _EK_STATIC_ALLOC_JOIN2(a, b) a##b
#    define _EK_STATIC_ALLOC_JOIN(a, b)  _EK_STATIC_ALLOC_JOIN2(a, b)
#    define _EK_STATIC_ALLOC_STRING2(x)  #x
#    define _EK_STATIC_ALLOC_STRING(x)   _EK_STATIC_ALLOC_STRING2(x)

/**
 * @brief 将静态对象初始化回调登记到 .ek_static_alloc 段
 * @param instance 对象实例名，同一翻译单元内必须唯一
 * @param order    初始化顺序，数值越小越先执行
 * @param fn       返回 ek_err_t 的初始化函数
 */
#    define EK_STATIC_ALLOC_REGISTER(instance, order, fn)                                             \
        static const _ek_static_alloc_item_t _EK_STATIC_ALLOC_JOIN(__ek_static_alloc_item_, instance) \
        __EK_USED __EK_SECTION(".ek_static_alloc") = { (uint16_t)(order), _EK_STATIC_ALLOC_STRING(instance), (fn) }

/**
 * @brief 扫描注册段并按 order 初始化全部静态对象
 *
 * @note EKCFG_EXPORT == 1 时由 ek_export_init() 自动调用。
 *       未启用 ek_export 时，必须在堆初始化之后手动调用本函数。
 */
void ek_static_alloc_init(void);

/**
 * @brief 查询最近一次静态初始化的首个错误
 * @return EK_OK 全部成功，否则为第一个失败项的错误码
 */
ek_err_t ek_static_alloc_get_init_error(void);

#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_STATIC_ALLOC == 1 */

#endif /* EK_STATIC_ALLOC_H */
