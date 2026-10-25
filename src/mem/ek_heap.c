/**
 * @file ek_heap.c
 * @brief 内存管理实现
 * @author N1netyNine99
 */

#include "ek_heap.h"

#if EKCFG_HEAP_TLSF == 1
#    include "ek_export.h"
/*
 * TLSF 索引配置由 ek_conf.h 中的下列宏控制：
 * - EKCFG_TLSF_FL_INDEX_MAX：最大连续内存块为 2 的该值次方字节。
 * - EKCFG_TLSF_SL_INDEX_COUNT_LOG2：二级链表数为 2 的该值次方；值越大，
 *   碎片率越低，但管理结构占用的 RAM 越多。
 *
 * 默认配置为 EKCFG_TLSF_FL_INDEX_MAX = 24、
 * EKCFG_TLSF_SL_INDEX_COUNT_LOG2 = 3，适合最大 16 MiB 的内存池。
 * 对于较小内存池，可降低 EKCFG_TLSF_FL_INDEX_MAX 以减少控制结构开销。
 */

#    ifdef EKCFG_HEAP_SECTION
static uint8_t s_default_heap[EKCFG_HEAP_SIZE] __EK_SECTION(EKCFG_HEAP_SECTION);
#    else
static uint8_t s_default_heap[EKCFG_HEAP_SIZE];
#    endif
static tlsf_t s_default_tlsf;

static size_t s_unused_bytes = 0;
static size_t s_used_bytes = 0;

/* 前向声明 */
static void _walker_unused(void *ptr, size_t size, int used, void *user);
static void _walker_used(void *ptr, size_t size, int used, void *user);

/* ========== 默认堆操作 ========== */

void ek_heap_init(void)
{
    s_default_tlsf = tlsf_create_with_pool(s_default_heap, EKCFG_HEAP_SIZE);
    while (s_default_tlsf == NULL);
}

EK_EXPORT_EARLIEST(ek_heap_init, 0);

void ek_heap_destory(void)
{
    tlsf_destroy(s_default_tlsf);
}

size_t ek_heap_total_size(void)
{
    return EKCFG_HEAP_SIZE - tlsf_size();
}

pool_t ek_heap_add_pool(void *ptr, size_t size)
{
    return tlsf_add_pool(s_default_tlsf, ptr, size);
}

void ek_heap_remove_pool(pool_t pool)
{
    tlsf_remove_pool(s_default_tlsf, pool);
}

size_t ek_heap_unused(void)
{
    pool_t pool = tlsf_get_pool(s_default_tlsf);
    s_unused_bytes = 0;
    tlsf_walk_pool(pool, _walker_unused, &s_unused_bytes);
    return s_unused_bytes;
}

size_t ek_heap_used(void)
{
    pool_t pool = tlsf_get_pool(s_default_tlsf);
    s_used_bytes = 0;
    tlsf_walk_pool(pool, _walker_used, &s_used_bytes);
    return s_used_bytes;
}

/* ========== 用户堆信息查询 ========== */

size_t ek_pool_total_size(pool_t pool)
{
    size_t total = 0;
    size_t unused = 0;
    size_t used = 0;
    tlsf_walk_pool(pool, _walker_unused, &unused);
    tlsf_walk_pool(pool, _walker_used, &used);
    total = unused + used;
    return total;
}

size_t ek_pool_unused(pool_t pool)
{
    size_t unused = 0;
    tlsf_walk_pool(pool, _walker_unused, &unused);
    return unused;
}

size_t ek_pool_used(pool_t pool)
{
    size_t used = 0;
    tlsf_walk_pool(pool, _walker_used, &used);
    return used;
}

/* ========== 弱函数实现 ========== */

__EK_WEAK void *ek_malloc(size_t size)
{
    return tlsf_malloc(s_default_tlsf, size);
}

__EK_WEAK void *ek_realloc(void *ptr, size_t size)
{
    return tlsf_realloc(s_default_tlsf, ptr, size);
}

__EK_WEAK void ek_free(void *ptr)
{
    tlsf_free(s_default_tlsf, ptr);
}

/* ========== 静态函数实现 ========== */

static void _walker_unused(void *ptr, size_t size, int used, void *user)
{
    __EK_UNUSED(ptr);

    if (!used)
    {
        size_t *count = (size_t *)user;
        *count += size;
    }
}

static void _walker_used(void *ptr, size_t size, int used, void *user)
{
    __EK_UNUSED(ptr);

    if (used)
    {
        size_t *count = (size_t *)user;
        *count += size;
    }
}

#endif /* EKCFG_HEAP_TLSF */
