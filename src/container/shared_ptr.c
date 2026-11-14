/**
 * @file shared_ptr.c
 * @brief 共享指针实现：隐藏头部与原子引用计数
 * @author N1netyNine99
 *
 * 内存布局：
 *   [ struct ptr_header ][ data ]
 *                         ^
 *                         ek_shared_ptr_alloc() 返回这里
 */

#include "ek/container/shared_ptr.h"

#if EKCFG_SHARED_PTR == 1

#    include <stdatomic.h>
#    include "ek/mem/heap.h"

#    define BUSY_ASCII 0x42555359UL

struct ptr_header
{
    _Atomic uint32_t refcnt;
    size_t size;
    uint32_t magic;
};

#    define HEADER_ALIGN _Alignof(max_align_t)
#    define HEADER_SIZE  ((sizeof(struct ptr_header) + HEADER_ALIGN - 1) & ~(HEADER_ALIGN - 1))

static inline struct ptr_header *_get_header(void *ptr)
{
    return (struct ptr_header *)((uint8_t *)ptr - HEADER_SIZE);
}

static inline const struct ptr_header *_get_header_const(const void *ptr)
{
    return (const struct ptr_header *)((const uint8_t *)ptr - HEADER_SIZE);
}

void *ek_shared_ptr_alloc(size_t size)
{
    if (size == 0) return NULL;
    if (size > SIZE_MAX - HEADER_SIZE) return NULL;

    size_t total = HEADER_SIZE + size;
    struct ptr_header *hdr = ek_malloc(total);
    if (!hdr) return NULL;

    memset(hdr, 0, total);

    atomic_init(&hdr->refcnt, 1);
    hdr->size = size;
    hdr->magic = BUSY_ASCII;

    return (uint8_t *)hdr + HEADER_SIZE;
}

static inline bool _ref_inc(_Atomic uint32_t *refcnt)
{
    uint32_t old = atomic_load_explicit(refcnt, memory_order_relaxed);

    while (old != UINT32_MAX)
    {
        if (atomic_compare_exchange_weak_explicit(refcnt, &old, old + 1, memory_order_relaxed, memory_order_relaxed))
        {
            return true;
        }
    }

    return false;
}

void *ek_shared_ptr_ref(void *ptr)
{
    if (ptr == NULL) return NULL;

    struct ptr_header *hdr = _get_header(ptr);
    if (hdr->magic != BUSY_ASCII) return NULL;
    if (!_ref_inc(&hdr->refcnt)) return NULL;

    return ptr;
}

void ek_shared_ptr_unref(void *ptr)
{
    if (ptr == NULL) return;

    struct ptr_header *hdr = _get_header(ptr);
    if (hdr->magic != BUSY_ASCII) return;

    if (atomic_fetch_sub_explicit(&hdr->refcnt, 1, memory_order_release) == 1)
    {
        atomic_thread_fence(memory_order_acquire);
        hdr->magic = 0;
        ek_free(hdr);
    }
}

size_t ek_shared_ptr_size(const void *ptr)
{
    if (ptr == NULL) return 0;

    const struct ptr_header *hdr = _get_header_const(ptr);
    if (hdr->magic != BUSY_ASCII) return 0;

    return hdr->size;
}

#endif
