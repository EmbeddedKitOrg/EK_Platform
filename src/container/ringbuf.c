/**
 * @file ringbuf.c
 * @brief 环形缓冲区实现
 * @author N1netyNine99
 */

#include "ek/container/ringbuf.h"

#if (EKCFG_RINGBUF == 1) || (EKCFG_RINGBUF_SPSC == 1)

#    include "ek/mem/heap.h"

#    if EKCFG_RINGBUF == 1

#        define RINGBUF_MAGIC            0x52425546UL
#        define RINGBUF_HDR              ((size_t)EK_ALIGN_UP(sizeof(uint32_t), alignof(ek_ringbuf_t)))
#        define RINGBUF_FROM_BUF(buf)    ((ek_ringbuf_t *)(uintptr_t)((uintptr_t)(buf) + RINGBUF_HDR))
#        define RINGBUF_GET_MAGIC(rb)    (*(const uint32_t *)(uintptr_t)((uintptr_t)(rb) - RINGBUF_HDR))
#        define RINGBUF_SET_MAGIC(rb, v) (*(uint32_t *)(uintptr_t)((uintptr_t)(rb) - RINGBUF_HDR) = (uint32_t)(v))

static inline bool _ringbuf_valid(const ek_ringbuf_t *rb)
{
    if (!rb) return false;
    if (!rb->mem_owned) return RINGBUF_GET_MAGIC(rb) == RINGBUF_MAGIC;
    return true;
}

bool ek_ringbuf_full(const ek_ringbuf_t *rb)
{
    if (!_ringbuf_valid(rb)) return false;
    return rb->item_amount == rb->cap;
}

bool ek_ringbuf_empty(const ek_ringbuf_t *rb)
{
    if (!_ringbuf_valid(rb)) return false;
    return rb->item_amount == 0U;
}

ek_ringbuf_t *ek_ringbuf_create_static(void *buf, size_t size, size_t item_size, uint32_t item_amount)
{
    if (!buf || item_size == 0U || item_amount == 0U) return NULL;
    if (((uintptr_t)buf % alignof(ek_ringbuf_t)) != 0U) return NULL;
    // 防止总大小溢出
    if (item_amount > (SIZE_MAX - RINGBUF_HDR - sizeof(ek_ringbuf_t)) / item_size)
    {
        return NULL;
    }
    if (size < EK_RINGBUF_STATIC_BUF_SIZE(item_size, item_amount)) return NULL;

    ek_ringbuf_t *rb = RINGBUF_FROM_BUF(buf);
    rb->mem_owned = false;
    rb->cap = item_amount;
    rb->item_size = item_size;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
    rb->item_amount = 0U;
    RINGBUF_SET_MAGIC(rb, RINGBUF_MAGIC);
    return rb;
}

ek_ringbuf_t *ek_ringbuf_create(size_t item_size, uint32_t item_amount)
{
    if (item_amount == 0U || item_size == 0U) return NULL;
    // 防止总大小溢出
    if (item_amount > (SIZE_MAX - sizeof(ek_ringbuf_t)) / item_size)
    {
        return NULL;
    }

    ek_ringbuf_t *rb = (ek_ringbuf_t *)ek_malloc(sizeof(ek_ringbuf_t) + (size_t)item_amount * item_size);
    if (rb == NULL)
    {
        return NULL;
    }

    rb->mem_owned = true;
    rb->cap = item_amount;
    rb->item_size = item_size;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
    rb->item_amount = 0U;
    return rb;
}

void ek_ringbuf_destroy(ek_ringbuf_t *rb)
{
    if (!_ringbuf_valid(rb)) return;

    if (rb->mem_owned)
    {
        ek_free(rb);
        return;
    }

    RINGBUF_SET_MAGIC(rb, 0U);
    rb->cap = 0U;
    rb->item_size = 0U;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
    rb->item_amount = 0U;
}

int ek_ringbuf_write(ek_ringbuf_t *rb, const void *item)
{
    if (!_ringbuf_valid(rb)) return -ENOENT;
    if (item == NULL) return -EINVAL;

    if (rb->item_amount == rb->cap)
    {
        return -ENOSPC;
    }

    uint8_t *target = rb->data + (rb->write_idx * rb->item_size);
    memcpy(target, item, rb->item_size);

    rb->write_idx = EK_INC_MOD(rb->write_idx, rb->cap);
    rb->item_amount++;

    return 0;
}

int ek_ringbuf_read(ek_ringbuf_t *rb, void *item)
{
    if (!_ringbuf_valid(rb)) return -ENOENT;

    if (rb->item_amount == 0U)
    {
        return -ENODATA;
    }

    if (item != NULL)
    {
        const uint8_t *source = rb->data + (rb->read_idx * rb->item_size);
        memcpy(item, source, rb->item_size);
    }

    rb->read_idx = EK_INC_MOD(rb->read_idx, rb->cap);
    rb->item_amount--;

    return 0;
}

int ek_ringbuf_peek(ek_ringbuf_t *rb, void *item)
{
    if (!_ringbuf_valid(rb)) return -ENOENT;
    if (item == NULL) return -EINVAL;

    if (rb->item_amount == 0U)
    {
        return -ENODATA;
    }

    const uint8_t *source = rb->data + (rb->read_idx * rb->item_size);
    memcpy(item, source, rb->item_size);

    return 0;
}
#    endif /* EKCFG_RINGBUF */

#    if EKCFG_RINGBUF_SPSC == 1
#        define RINGBUF_SPSC_MAGIC         0x53505343UL
#        define RINGBUF_SPSC_HDR           ((size_t)EK_ALIGN_UP(sizeof(uint32_t), alignof(ek_ringbuf_spsc_t)))
#        define RINGBUF_SPSC_FROM_BUF(buf) ((ek_ringbuf_spsc_t *)(uintptr_t)((uintptr_t)(buf) + RINGBUF_SPSC_HDR))
#        define RINGBUF_SPSC_GET_MAGIC(rb) (*(const uint32_t *)(uintptr_t)((uintptr_t)(rb) - RINGBUF_SPSC_HDR))
#        define RINGBUF_SPSC_SET_MAGIC(rb, v) \
            (*(uint32_t *)(uintptr_t)((uintptr_t)(rb) - RINGBUF_SPSC_HDR) = (uint32_t)(v))

static inline bool _ringbuf_spsc_valid(const ek_ringbuf_spsc_t *rb)
{
    if (!rb) return false;
    if (!rb->mem_owned) return RINGBUF_SPSC_GET_MAGIC(rb) == RINGBUF_SPSC_MAGIC;
    return true;
}

__EK_STATIC_INLINE uint32_t _ek_ringbuf_spsc_next_idx(const ek_ringbuf_spsc_t *rb, uint32_t idx)
{
    return EK_INC_MOD(idx, rb->cap);
}

bool ek_ringbuf_full_spsc(const ek_ringbuf_spsc_t *rb)
{
    if (!_ringbuf_spsc_valid(rb)) return false;
    return _ek_ringbuf_spsc_next_idx(rb, rb->write_idx) == rb->read_idx;
}

bool ek_ringbuf_empty_spsc(const ek_ringbuf_spsc_t *rb)
{
    if (!_ringbuf_spsc_valid(rb)) return false;
    return rb->read_idx == rb->write_idx;
}

ek_ringbuf_spsc_t *ek_ringbuf_create_spsc_static(void *buf, size_t size, size_t item_size, uint32_t amount)
{
    if (!buf || item_size == 0U || amount <= 1U) return NULL;
    if (((uintptr_t)buf % alignof(ek_ringbuf_spsc_t)) != 0U) return NULL;
    // 防止总大小溢出
    if (amount > (SIZE_MAX - RINGBUF_SPSC_HDR - sizeof(ek_ringbuf_spsc_t)) / item_size)
    {
        return NULL;
    }
    if (size < EK_RINGBUF_SPSC_STATIC_BUF_SIZE(item_size, amount)) return NULL;

    ek_ringbuf_spsc_t *rb = RINGBUF_SPSC_FROM_BUF(buf);
    rb->mem_owned = false;
    rb->cap = amount;
    rb->item_size = item_size;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
    RINGBUF_SPSC_SET_MAGIC(rb, RINGBUF_SPSC_MAGIC);
    return rb;
}

ek_ringbuf_spsc_t *ek_ringbuf_create_spsc(size_t item_size, uint32_t item_amount)
{
    if (item_amount <= 1U || item_size == 0U) return NULL;
    // 防止总大小溢出
    if (item_amount > (SIZE_MAX - sizeof(ek_ringbuf_spsc_t)) / item_size)
    {
        return NULL;
    }

    ek_ringbuf_spsc_t *rb = (ek_ringbuf_spsc_t *)ek_malloc(sizeof(ek_ringbuf_spsc_t) + (size_t)item_amount * item_size);
    if (rb == NULL)
    {
        return NULL;
    }

    rb->mem_owned = true;
    rb->cap = item_amount;
    rb->item_size = item_size;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
    return rb;
}

void ek_ringbuf_destroy_spsc(ek_ringbuf_spsc_t *rb)
{
    if (!_ringbuf_spsc_valid(rb)) return;

    if (rb->mem_owned)
    {
        ek_free(rb);
        return;
    }

    RINGBUF_SPSC_SET_MAGIC(rb, 0U);
    rb->cap = 0U;
    rb->item_size = 0U;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
}

int ek_ringbuf_write_spsc(ek_ringbuf_spsc_t *rb, const void *item)
{
    if (!_ringbuf_spsc_valid(rb)) return -ENOENT;
    if (item == NULL) return -EINVAL;

    uint32_t next_idx = _ek_ringbuf_spsc_next_idx(rb, rb->write_idx);
    if (next_idx == rb->read_idx)
    {
        return -ENOSPC;
    }

    uint8_t *target = rb->data + (rb->write_idx * rb->item_size);
    memcpy(target, item, rb->item_size);
    rb->write_idx = next_idx;

    return 0;
}

int ek_ringbuf_read_spsc(ek_ringbuf_spsc_t *rb, void *item)
{
    if (!_ringbuf_spsc_valid(rb)) return -ENOENT;

    if (rb->read_idx == rb->write_idx)
    {
        return -ENODATA;
    }

    if (item != NULL)
    {
        const uint8_t *source = rb->data + (rb->read_idx * rb->item_size);
        memcpy(item, source, rb->item_size);
    }

    rb->read_idx = _ek_ringbuf_spsc_next_idx(rb, rb->read_idx);

    return 0;
}

int ek_ringbuf_peek_spsc(ek_ringbuf_spsc_t *rb, void *item)
{
    if (!_ringbuf_spsc_valid(rb)) return -ENOENT;
    if (item == NULL) return -EINVAL;

    if (rb->read_idx == rb->write_idx)
    {
        return -ENODATA;
    }

    const uint8_t *source = rb->data + (rb->read_idx * rb->item_size);
    memcpy(item, source, rb->item_size);

    return 0;
}
#    endif /* EKCFG_RINGBUF_SPSC */

#endif /* EKCFG_RINGBUF || EKCFG_RINGBUF_SPSC */
