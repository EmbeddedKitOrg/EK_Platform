/**
 * @file static_alloc.c
 * @brief 静态对象自动注册与初始化实现
 * @author N1netyNine99
 */

#include "ek/mem/static_alloc.h"

#if EKCFG_STATIC_ALLOC == 1
#    include "ek/core/export.h"
#    if EKCFG_LOG == 1
#        include "ek/core/log.h"
EK_LOG_MODULE(__FILE_NAME__, EK_LOG_LEVEL_ERROR);
#    endif

extern const ek_static_alloc_item_t _ek_static_alloc_start;
extern const ek_static_alloc_item_t _ek_static_alloc_end;

static int s_static_alloc_error;

void ek_static_alloc_init(void)
{
    const ek_static_alloc_item_t *start = &_ek_static_alloc_start;
    const ek_static_alloc_item_t *end = &_ek_static_alloc_end;
    const size_t count = (size_t)(end - start);

    s_static_alloc_error = 0;
    for (size_t i = 0; i < count; i++)
    {
        if (start[i].fn == NULL) continue;
        const int err = start[i].fn();
        if (err < 0)
        {
            if (s_static_alloc_error == 0) s_static_alloc_error = err;
#    if EKCFG_LOG == 1
            EK_LOG_ERROR("static allocation init failed: %s: %s", start[i].name, strerror(-err));
#    endif
        }
    }
}

EK_EXPORT_COMPONENT(ek_static_alloc_init, EK_EXPORT_ORDER_COMPONENT_STATIC_ALLOC);

int ek_static_alloc_get_init_error(void)
{
    return s_static_alloc_error;
}

#endif /* EKCFG_STATIC_ALLOC == 1 */
