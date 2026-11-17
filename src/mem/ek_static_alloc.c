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

static int s_static_alloc_error;

void ek_static_alloc_init(void)
{
    const _ek_static_alloc_item_t *items = &_ek_static_alloc_start;
    const size_t count = (size_t)(&_ek_static_alloc_end - &_ek_static_alloc_start);

    s_static_alloc_error = 0;

    /*
     * 按 (order, 段内位置) 升序执行，原地选择下一个条目：
     * 不在栈上开 VLA，也不依赖 qsort。
     */
    bool has_last = false;
    uint16_t last_order = 0U;
    size_t last_idx = 0U;

    for (size_t done = 0U; done < count; done++)
    {
        size_t best = count;
        uint16_t best_order = 0U;

        for (size_t i = 0U; i < count; i++)
        {
            const uint16_t order = items[i].order;
            if (has_last && (order < last_order || (order == last_order && i <= last_idx))) continue;
            if (best == count || order < best_order)
            {
                best = i;
                best_order = order;
            }
        }

        if (best == count) break;

        has_last = true;
        last_order = best_order;
        last_idx = best;

        if (items[best].fn == NULL) continue;
        const int err = items[best].fn();
        if (err < 0)
        {
            if (s_static_alloc_error == 0) s_static_alloc_error = err;
#    if EKCFG_LOG == 1
            EK_LOG_ERROR("static allocation init failed: %s: %d", items[best].name, err);
#    endif
        }
    }
}

EK_EXPORT_COMPONENTS(ek_static_alloc_init, 1);

int ek_static_alloc_get_init_error(void)
{
    return s_static_alloc_error;
}

#endif /* EKCFG_STATIC_ALLOC == 1 */
