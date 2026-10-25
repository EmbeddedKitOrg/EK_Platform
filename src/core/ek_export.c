/**
 * @file ek_export.c
 * @brief 函数导出机制实现
 * @author N1netyNine99
 */

#include "ek_export.h"

#if (EKCFG_EXPORT == 1)

#    include "ek_assert.h"

extern const _ek_export_item_t _ek_export_fn_start;
extern const _ek_export_item_t _ek_export_fn_end;

static int _ek_export_item_compare(const void *lhs, const void *rhs);

void ek_export_init(void)
{
    const _ek_export_item_t *items_start = &_ek_export_fn_start;
    const _ek_export_item_t *items_end = &_ek_export_fn_end;
    size_t count = (size_t)(items_end - items_start);
    if (count == 0U)
    {
        return;
    }

    _ek_export_item_t items[count];
    for (size_t i = 0; i < count; i++)
    {
        items[i] = items_start[i];
    }

    if (count > 1U)
    {
        qsort(items, count, sizeof(items[0]), _ek_export_item_compare);
    }

    for (size_t i = 0; i < count; i++)
    {
        ek_assert_param(items[i].fn != NULL);
        items[i].fn();
    }
}

static int _ek_export_item_compare(const void *lhs, const void *rhs)
{
    const _ek_export_item_t *lhs_item = (const _ek_export_item_t *)lhs;
    const _ek_export_item_t *rhs_item = (const _ek_export_item_t *)rhs;

    if (lhs_item->level < rhs_item->level) return -1;
    if (lhs_item->level > rhs_item->level) return 1;
    if (lhs_item->order < rhs_item->order) return -1;
    if (lhs_item->order > rhs_item->order) return 1;
    return 0;
}

#else

void ek_export_init(void)
{
}

#endif /* EKCFG_EXPORT */
