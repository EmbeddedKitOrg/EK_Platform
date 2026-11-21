/**
 * @file heap.c
 * @brief 内存管理实现
 * @author N1netyNine99
 */

#include "ek/mem/heap.h"

#if EKCFG_HEAP_TLSF == 1
#    include "ek/core/export.h"
#    if EKCFG_RTOS == 1
#        include "ek/os/kernel.h"
#        include "ek/core/arch.h"
#    endif
/*
 * TLSF 索引配置由 conf_internal.h 自动推导，亦可在 ek_conf.h 中显式覆盖：
 * - EKCFG_TLSF_FL_INDEX_MAX：默认按 EKCFG_HEAP_MAX_POOL_SIZE（默认等于 EKCFG_HEAP_SIZE）
 *   自动取 ceil(log2)；多池场景需要追加更大的池时，定义 EKCFG_HEAP_MAX_POOL_SIZE 即可。
 * - EKCFG_TLSF_SL_INDEX_COUNT_LOG2：默认按 EKCFG_HEAP_SIZE 分档自动选择（2~5），
 *   在碎片率与控制块内存开销间折中；需要微调时可显式覆盖。
 */

#    ifdef EKCFG_HEAP_SECTION
static uint8_t s_default_heap[EKCFG_HEAP_SIZE] __EK_SECTION(EKCFG_HEAP_SECTION);
#    else
static uint8_t s_default_heap[EKCFG_HEAP_SIZE];
#    endif
static tlsf_t s_default_tlsf;

/* ========== 静态函数实现 ========== */

static inline bool _heap_in_isr(void)
{
#    if EKCFG_RTOS == 1
    return ek_arch_in_isr();
#    else
    return false;
#    endif
}

static inline bool _heap_lock(void)
{
#    if EKCFG_RTOS == 1
    // 内核未启动时返回 -ESRCH，自动退化为无锁；启动后加调度器锁保护堆操作
    return ek_os_kernel_sched_lock() > 0;
#    else
    return false;
#    endif
}

static inline void _heap_unlock(bool locked)
{
#    if EKCFG_RTOS == 1
    if (locked) ek_os_kernel_sched_unlock();
#    else
    __EK_UNUSED(locked);
#    endif
}

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

/* ========== 默认堆操作 ========== */

void ek_heap_init(void)
{
    s_default_tlsf = tlsf_create_with_pool(s_default_heap, EKCFG_HEAP_SIZE);
    while (s_default_tlsf == NULL);
}

EK_EXPORT_COMPONENT(ek_heap_init, EK_EXPORT_ORDER_COMPONENT_HEAP);

void ek_heap_destory(void)
{
    if (_heap_in_isr()) return;

    bool locked = _heap_lock();
    tlsf_destroy(s_default_tlsf);
    _heap_unlock(locked);
}

size_t ek_heap_total_size(void)
{
    return EKCFG_HEAP_SIZE - tlsf_size();
}

pool_t ek_heap_add_pool(void *ptr, size_t size)
{
    if (_heap_in_isr()) return NULL;

    bool locked = _heap_lock();
    pool_t pool = tlsf_add_pool(s_default_tlsf, ptr, size);
    _heap_unlock(locked);
    return pool;
}

void ek_heap_remove_pool(pool_t pool)
{
    if (_heap_in_isr()) return;

    bool locked = _heap_lock();
    tlsf_remove_pool(s_default_tlsf, pool);
    _heap_unlock(locked);
}

size_t ek_heap_unused(void)
{
    if (_heap_in_isr()) return 0;

    size_t unused = 0;
    bool locked = _heap_lock();
    pool_t pool = tlsf_get_pool(s_default_tlsf);
    tlsf_walk_pool(pool, _walker_unused, &unused);
    _heap_unlock(locked);
    return unused;
}

size_t ek_heap_used(void)
{
    if (_heap_in_isr()) return 0;

    size_t used = 0;
    bool locked = _heap_lock();
    pool_t pool = tlsf_get_pool(s_default_tlsf);
    tlsf_walk_pool(pool, _walker_used, &used);
    _heap_unlock(locked);
    return used;
}

/* ========== 用户堆信息查询 ========== */

size_t ek_pool_total_size(pool_t pool)
{
    if (_heap_in_isr()) return 0;

    size_t unused = 0;
    size_t used = 0;
    bool locked = _heap_lock();
    tlsf_walk_pool(pool, _walker_unused, &unused);
    tlsf_walk_pool(pool, _walker_used, &used);
    _heap_unlock(locked);
    return unused + used;
}

size_t ek_pool_unused(pool_t pool)
{
    if (_heap_in_isr()) return 0;

    size_t unused = 0;
    bool locked = _heap_lock();
    tlsf_walk_pool(pool, _walker_unused, &unused);
    _heap_unlock(locked);
    return unused;
}

size_t ek_pool_used(pool_t pool)
{
    if (_heap_in_isr()) return 0;

    size_t used = 0;
    bool locked = _heap_lock();
    tlsf_walk_pool(pool, _walker_used, &used);
    _heap_unlock(locked);
    return used;
}

/* ========== 弱函数实现 ========== */

__EK_WEAK void *ek_malloc(size_t size)
{
    if (_heap_in_isr()) return NULL;

    bool locked = _heap_lock();
    void *ptr = tlsf_malloc(s_default_tlsf, size);
    _heap_unlock(locked);
    return ptr;
}

__EK_WEAK void *ek_realloc(void *ptr, size_t size)
{
    if (_heap_in_isr()) return NULL;

    bool locked = _heap_lock();
    void *new_ptr = tlsf_realloc(s_default_tlsf, ptr, size);
    _heap_unlock(locked);
    return new_ptr;
}

__EK_WEAK void ek_free(void *ptr)
{
    if (!ptr) return;
    if (_heap_in_isr()) return;

    bool locked = _heap_lock();
    tlsf_free(s_default_tlsf, ptr);
    _heap_unlock(locked);
}

#endif /* EKCFG_HEAP_TLSF */
