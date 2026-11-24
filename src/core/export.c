/**
 * @file export.c
 * @brief 函数导出机制实现
 * @author N1netyNine99
 */

#include "ek/core/export.h"

#if (EKCFG_EXPORT == 1)

#    include "ek/core/assert.h"
#    include "ek/core/def.h"

extern const ek_export_item_t _ek_export_fn_start;
extern const ek_export_item_t _ek_export_fn_end;

__attribute__((constructor)) void ek_export_init(void)
{
    const ek_export_item_t *items_start = &_ek_export_fn_start;
    const ek_export_item_t *items_end = &_ek_export_fn_end;
    size_t count = (size_t)(items_end - items_start);

    for (size_t i = 0; i < count; i++)
    {
        ek_assert(items_start[i].fn != NULL);
        items_start[i].fn();
    }
}

#else

void ek_export_init(void)
{
}

#endif /* EKCFG_EXPORT */
