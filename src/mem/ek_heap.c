/**
 * @file ek_heap.c
 * @brief 内存管理实现
 * @author N1netyNine99
 */

#include "ek/mem/ek_heap.h"

#if EKCFG_HEAP_TLSF == 1
#    include "ek/core/ek_export.h"
#    include "ek/core/ek_assert.h"
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

#    if EKCFG_RTOS == 1
/*
 * TLSF 本身不是线程安全的。RTOS 模式下所有堆操作都经过这把锁。
 * EK_LOCK_* 宏按 (obj)->lock 访问，因此用一个只含锁的结构体包装。
 */
static struct
{
    EK_LOCK_TYPE lock;
} s_heap_lock;
static bool s_heap_lock_ready;

#        define _HEAP_LOCK()                                                \
            do                                                              \
            {                                                               \
                if (s_heap_lock_ready) (void)EK_LOCK_ACQUIRE(&s_heap_lock); \
            } while (0)
#        define _HEAP_UNLOCK()                                        \
            do                                                        \
            {                                                         \
                if (s_heap_lock_ready) EK_LOCK_RELEASE(&s_heap_lock); \
            } while (0)
#    else
#        define _HEAP_LOCK()   ((void)0)
#        define _HEAP_UNLOCK() ((void)0)
#    endif /* EKCFG_RTOS */

/* 前向声明 */
static void _walker_unused(void *ptr, size_t size, int used, void *user);
static void _walker_used(void *ptr, size_t size, int used, void *user);

/* ========== 默认堆操作 ========== */

void ek_heap_init(void)
{
    if (s_default_tlsf != NULL) return; /* 重复初始化会清空已分配的块，直接忽略 */

    s_default_tlsf = tlsf_create_with_pool(s_default_heap, EKCFG_HEAP_SIZE);
    /*
     * 创建失败通常是 EKCFG_HEAP_SIZE 太小或超过 2^EKCFG_TLSF_FL_INDEX_MAX。
     * 断言开启时停在这里便于定位；断言关闭时 ek_malloc() 会返回 NULL，
     * 不再像旧版本那样在启动阶段无提示地死循环。
     */
    ek_assert_param(s_default_tlsf != NULL);

#    if EKCFG_RTOS == 1
    EK_LOCK_INIT(s_heap_lock.lock);
    s_heap_lock_ready = true;
#    endif
}

EK_EXPORT_EARLIEST(ek_heap_init, 0);

bool ek_heap_is_ready(void)
{
    return s_default_tlsf != NULL;
}

void ek_heap_destroy(void)
{
    if (s_default_tlsf == NULL) return;
#    if EKCFG_RTOS == 1
    if (s_heap_lock_ready)
    {
        s_heap_lock_ready = false;
        EK_LOCK_DEINIT(s_heap_lock.lock);
    }
#    endif
    tlsf_destroy(s_default_tlsf);
    s_default_tlsf = NULL;
}

void ek_heap_destory(void)
{
    ek_heap_destroy();
}

size_t ek_heap_total_size(void)
{
    return EKCFG_HEAP_SIZE - tlsf_size();
}

pool_t ek_heap_add_pool(void *ptr, size_t size)
{
    if (s_default_tlsf == NULL) return NULL;
    _HEAP_LOCK();
    pool_t pool = tlsf_add_pool(s_default_tlsf, ptr, size);
    _HEAP_UNLOCK();
    return pool;
}

void ek_heap_remove_pool(pool_t pool)
{
    if (s_default_tlsf == NULL) return;
    _HEAP_LOCK();
    tlsf_remove_pool(s_default_tlsf, pool);
    _HEAP_UNLOCK();
}

size_t ek_heap_unused(void)
{
    if (s_default_tlsf == NULL) return 0U;
    size_t unused = 0U;
    _HEAP_LOCK();
    tlsf_walk_pool(tlsf_get_pool(s_default_tlsf), _walker_unused, &unused);
    _HEAP_UNLOCK();
    return unused;
}

size_t ek_heap_used(void)
{
    if (s_default_tlsf == NULL) return 0U;
    size_t used = 0U;
    _HEAP_LOCK();
    tlsf_walk_pool(tlsf_get_pool(s_default_tlsf), _walker_used, &used);
    _HEAP_UNLOCK();
    return used;
}

/* ========== 用户堆信息查询 ========== */

size_t ek_pool_total_size(pool_t pool)
{
    size_t unused = 0;
    size_t used = 0;
    _HEAP_LOCK();
    tlsf_walk_pool(pool, _walker_unused, &unused);
    tlsf_walk_pool(pool, _walker_used, &used);
    _HEAP_UNLOCK();
    return unused + used;
}

size_t ek_pool_unused(pool_t pool)
{
    size_t unused = 0;
    _HEAP_LOCK();
    tlsf_walk_pool(pool, _walker_unused, &unused);
    _HEAP_UNLOCK();
    return unused;
}

size_t ek_pool_used(pool_t pool)
{
    size_t used = 0;
    _HEAP_LOCK();
    tlsf_walk_pool(pool, _walker_used, &used);
    _HEAP_UNLOCK();
    return used;
}

/* ========== 弱函数实现 ========== */

__EK_WEAK void *ek_malloc(size_t size)
{
    if (s_default_tlsf == NULL) return NULL;
    _HEAP_LOCK();
    void *ptr = tlsf_malloc(s_default_tlsf, size);
    _HEAP_UNLOCK();
    return ptr;
}

__EK_WEAK void *ek_realloc(void *ptr, size_t size)
{
    if (s_default_tlsf == NULL) return NULL;
    _HEAP_LOCK();
    void *new_ptr = tlsf_realloc(s_default_tlsf, ptr, size);
    _HEAP_UNLOCK();
    return new_ptr;
}

__EK_WEAK void ek_free(void *ptr)
{
    if (s_default_tlsf == NULL || ptr == NULL) return;
    _HEAP_LOCK();
    tlsf_free(s_default_tlsf, ptr);
    _HEAP_UNLOCK();
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
