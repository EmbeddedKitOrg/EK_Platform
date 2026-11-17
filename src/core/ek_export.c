/**
 * @file ek_export.c
 * @brief 函数导出机制实现
 * @author N1netyNine99
 */

#include "ek/core/ek_export.h"

#if (EKCFG_EXPORT == 1)

#    include "ek/core/ek_def.h"
#    include "ek/core/ek_assert.h"

extern const _ek_export_item_t _ek_export_fn_start;
extern const _ek_export_item_t _ek_export_fn_end;

static bool s_export_done;

__EK_STATIC_INLINE uint32_t _ek_export_key(const _ek_export_item_t *item)
{
    return ((uint32_t)item->level << 16) | (uint32_t)item->order;
}

#    if EKCFG_EXPORT_AUTO_INIT == 1
__attribute__((constructor))
#    endif
void ek_export_init(void)
{
    /* 只执行一次：自动执行后用户再手动调用不会重复初始化堆等模块 */
    if (s_export_done) return;
    s_export_done = true;

    const _ek_export_item_t *items = &_ek_export_fn_start;
    const size_t count = (size_t)(&_ek_export_fn_end - &_ek_export_fn_start);

    /*
     * 按 (level, order, 段内位置) 升序依次执行。
     * 每轮在剩余条目中找出比上一个已执行条目大的最小者，O(n^2) 但不需要
     * 额外内存，也不依赖 qsort；导出条目通常只有几十个，开销可以忽略。
     * 相同 (level, order) 的条目按段内顺序执行，保持稳定。
     */
    bool has_last = false;
    uint32_t last_key = 0U;
    size_t last_idx = 0U;

    for (size_t done = 0U; done < count; done++)
    {
        size_t best = count;
        uint32_t best_key = 0U;

        for (size_t i = 0U; i < count; i++)
        {
            const uint32_t key = _ek_export_key(&items[i]);
            if (has_last && (key < last_key || (key == last_key && i <= last_idx))) continue;
            if (best == count || key < best_key)
            {
                best = i;
                best_key = key;
            }
        }

        if (best == count) break;

        has_last = true;
        last_key = best_key;
        last_idx = best;

        ek_assert_param(items[best].fn != NULL);
        if (items[best].fn != NULL) items[best].fn();
    }
}

#else

void ek_export_init(void)
{
}

#endif /* EKCFG_EXPORT */
