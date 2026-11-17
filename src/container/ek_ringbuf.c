/**
 * @file ek_ringbuf.c
 * @brief 环形缓冲区实现
 * @author N1netyNine99
 */

#include "ek/container/ek_ringbuf.h"

#if (EKCFG_RINGBUF == 1) || (EKCFG_RINGBUF_SPSC == 1)

#    include "ek/mem/ek_heap.h"

/* 索引加一并回绕；用比较代替取模，Cortex-M0 等没有硬件除法的内核上更快 */
__EK_STATIC_INLINE uint32_t _ek_rb_next(uint32_t idx, size_t cap)
{
    idx++;
    return (idx >= cap) ? 0U : idx;
}

#    if EKCFG_RINGBUF == 1

bool ek_ringbuf_full(const ek_ringbuf_t *rb)
{
    if (rb == NULL) return false;
    return rb->item_amount == rb->cap;
}

bool ek_ringbuf_empty(const ek_ringbuf_t *rb)
{
    if (rb == NULL) return false;
    return rb->item_amount == 0;
}

int ek_ringbuf_init_static(ek_ringbuf_t *rb, size_t item_size, uint32_t item_amount)
{
    if (rb == NULL || item_size == 0U || item_amount == 0U) return -EINVAL;

    rb->cap = item_amount;
    rb->item_size = item_size;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
    rb->item_amount = 0U;
#        if EKCFG_RTOS == 1
    EK_LOCK_INIT(rb->lock);
#        endif /* EKCFG_RTOS */
    return 0;
}

void ek_ringbuf_deinit_static(ek_ringbuf_t *rb)
{
    if (rb == NULL) return;
#        if EKCFG_RTOS == 1
    EK_LOCK_DEINIT(rb->lock);
#        endif
    rb->cap = 0U;
    rb->item_size = 0U;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
    rb->item_amount = 0U;
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

    rb->cap = item_amount;
    rb->item_size = item_size;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
    rb->item_amount = 0U;
#        if EKCFG_RTOS == 1
    EK_LOCK_INIT(rb->lock);
#        endif /* EKCFG_RTOS */
    return rb;
}

void ek_ringbuf_destroy(ek_ringbuf_t *rb)
{
    if (rb == NULL) return;
#        if EKCFG_RTOS == 1
    EK_LOCK_DEINIT(rb->lock);
#        endif

    ek_free(rb);
}

int ek_ringbuf_write(ek_ringbuf_t *rb, const void *item)
{
    if (rb == NULL || item == NULL) return -EINVAL;

    (void)EK_LOCK_ACQUIRE(rb);

    if (ek_ringbuf_full(rb) == true)
    {
        EK_LOCK_RELEASE(rb);
        return -ENOSPC;
    }

    uint8_t *target = rb->data + (rb->write_idx * rb->item_size);
    memcpy(target, item, rb->item_size);

    rb->write_idx = _ek_rb_next(rb->write_idx, rb->cap);
    rb->item_amount++;

    EK_LOCK_RELEASE(rb);

    return 0;
}

int ek_ringbuf_read(ek_ringbuf_t *rb, void *item)
{
    if (rb == NULL) return -EINVAL;

    (void)EK_LOCK_ACQUIRE(rb);

    if (ek_ringbuf_empty(rb) == true)
    {
        EK_LOCK_RELEASE(rb);
        return -ENODATA;
    }

    if (item != NULL)
    {
        const uint8_t *source = rb->data + (rb->read_idx * rb->item_size);
        memcpy(item, source, rb->item_size);
    }

    rb->read_idx = _ek_rb_next(rb->read_idx, rb->cap);
    rb->item_amount--;

    EK_LOCK_RELEASE(rb);

    return 0;
}

int ek_ringbuf_peek(ek_ringbuf_t *rb, void *item)
{
    if (rb == NULL || item == NULL) return -EINVAL;

    (void)EK_LOCK_ACQUIRE(rb);

    if (ek_ringbuf_empty(rb) == true)
    {
        EK_LOCK_RELEASE(rb);
        return -ENODATA;
    }

    const uint8_t *source = rb->data + (rb->read_idx * rb->item_size);
    memcpy(item, source, rb->item_size);

    EK_LOCK_RELEASE(rb);

    return 0;
}
#    endif /* EKCFG_RINGBUF */

#    if EKCFG_RINGBUF_SPSC == 1
__EK_STATIC_INLINE uint32_t _ek_ringbuf_spsc_next_idx(const ek_ringbuf_spsc_t *rb, uint32_t idx)
{
    return _ek_rb_next(idx, rb->cap);
}

bool ek_ringbuf_full_spsc(const ek_ringbuf_spsc_t *rb)
{
    if (rb == NULL) return false;
    return _ek_ringbuf_spsc_next_idx(rb, rb->write_idx) == rb->read_idx;
}

bool ek_ringbuf_empty_spsc(const ek_ringbuf_spsc_t *rb)
{
    if (rb == NULL) return false;
    return rb->read_idx == rb->write_idx;
}

int ek_ringbuf_init_spsc_static(ek_ringbuf_spsc_t *rb, size_t item_size, uint32_t amount)
{
    if (rb == NULL || item_size == 0U || amount <= 1U) return -EINVAL;

    rb->cap = amount;
    rb->item_size = item_size;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
    return 0;
}

void ek_ringbuf_deinit_spsc_static(ek_ringbuf_spsc_t *rb)
{
    if (rb == NULL) return;
    rb->cap = 0U;
    rb->item_size = 0U;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
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

    rb->cap = item_amount;
    rb->item_size = item_size;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
    return rb;
}

void ek_ringbuf_destroy_spsc(ek_ringbuf_spsc_t *rb)
{
    if (rb == NULL) return;

    ek_free(rb);
}

int ek_ringbuf_write_spsc(ek_ringbuf_spsc_t *rb, const void *item)
{
    if (rb == NULL || item == NULL) return -EINVAL;

    const uint32_t write_idx = rb->write_idx;
    const uint32_t next_idx = _ek_ringbuf_spsc_next_idx(rb, write_idx);
    if (next_idx == rb->read_idx)
    {
        return -ENOSPC;
    }

    uint8_t *target = rb->data + (write_idx * rb->item_size);
    memcpy(target, item, rb->item_size);
    /* 数据写完后再发布写索引，防止消费者读到未写完的元素 */
    EK_MEMORY_BARRIER();
    rb->write_idx = next_idx;

    return 0;
}

int ek_ringbuf_read_spsc(ek_ringbuf_spsc_t *rb, void *item)
{
    if (rb == NULL) return -EINVAL;

    const uint32_t read_idx = rb->read_idx;
    if (read_idx == rb->write_idx)
    {
        return -ENODATA;
    }
    /* 先观察到写索引，再读数据 */
    EK_MEMORY_BARRIER();

    if (item != NULL)
    {
        const uint8_t *source = rb->data + (read_idx * rb->item_size);
        memcpy(item, source, rb->item_size);
    }

    /* 数据拷出后再释放槽位 */
    EK_MEMORY_BARRIER();
    rb->read_idx = _ek_ringbuf_spsc_next_idx(rb, read_idx);

    return 0;
}

int ek_ringbuf_peek_spsc(ek_ringbuf_spsc_t *rb, void *item)
{
    if (rb == NULL || item == NULL) return -EINVAL;

    const uint32_t read_idx = rb->read_idx;
    if (read_idx == rb->write_idx)
    {
        return -ENODATA;
    }
    EK_MEMORY_BARRIER();

    const uint8_t *source = rb->data + (read_idx * rb->item_size);
    memcpy(item, source, rb->item_size);

    return 0;
}
#    endif /* EKCFG_RINGBUF_SPSC */

#endif /* EKCFG_RINGBUF || EKCFG_RINGBUF_SPSC */
