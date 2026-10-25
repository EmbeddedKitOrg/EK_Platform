/**
 * @file ek_static_alloc.c
 * @brief 静态对象自动注册与初始化实现
 * @author N1netyNine99
 */

#include "ek/mem/ek_static_alloc.h"

#if EKCFG_STATIC_ALLOC == 1
#    include "ek/core/ek_export.h"
#    if EKCFG_LOG == 1
#        include "ek/core/ek_log.h"
EK_LOG_MODULE("ek_static_alloc.c", EK_LOG_LEVEL_ERROR);
#    endif

extern const _ek_static_alloc_item_t _ek_static_alloc_start;
extern const _ek_static_alloc_item_t _ek_static_alloc_end;

static int _static_alloc_compare(const void *lhs, const void *rhs);

static int s_static_alloc_error;

void ek_static_alloc_init(void)
{
    const _ek_static_alloc_item_t *start = &_ek_static_alloc_start;
    const _ek_static_alloc_item_t *end = &_ek_static_alloc_end;
    const size_t count = (size_t)(end - start);

    s_static_alloc_error = 0;
    if (count == 0U) return;

    _ek_static_alloc_item_t items[count];
    for (size_t i = 0; i < count; i++) items[i] = start[i];

    if (count > 1U) qsort(items, count, sizeof(items[0]), _static_alloc_compare);

    for (size_t i = 0; i < count; i++)
    {
        if (items[i].fn == NULL) continue;
        const int err = items[i].fn();
        if (err < 0)
        {
            if (s_static_alloc_error == 0) s_static_alloc_error = err;
#    if EKCFG_LOG == 1
            EK_LOG_ERROR("static allocation init failed: %s: %s", items[i].name, strerror(-err));
#    endif
        }
    }
}

EK_EXPORT_COMPONENTS(ek_static_alloc_init, 1);

int ek_static_alloc_get_init_error(void)
{
    return s_static_alloc_error;
}

static int _static_alloc_compare(const void *lhs, const void *rhs)
{
    const _ek_static_alloc_item_t *lhs_item = (const _ek_static_alloc_item_t *)lhs;
    const _ek_static_alloc_item_t *rhs_item = (const _ek_static_alloc_item_t *)rhs;
    return (lhs_item->order > rhs_item->order) - (lhs_item->order < rhs_item->order);
}

#endif /* EKCFG_STATIC_ALLOC == 1 */
