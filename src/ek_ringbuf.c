/**
 * @file ek_ringbuf.c
 * @brief 环形缓冲区实现
 * @author N1netyNine99
 */

#include "ek_ringbuf.h"

#if (EKCFG_RINGBUF == 1) || (EKCFG_RINGBUF_SPSC == 1)

#    include "ek_heap.h"

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

#        if EKCFG_STATIC_ALLOC == 1
ek_err_t ek_ringbuf_init_static(ek_ringbuf_t *rb, size_t item_size, uint32_t item_amount)
{
    if (rb == NULL || item_size == 0U || item_amount == 0U) return EK_ERR_INVAL;

    rb->cap = item_amount;
    rb->item_size = item_size;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
    rb->item_amount = 0U;
#            if EKCFG_RTOS == 1
    EK_LOCK_INIT(rb->lock);
#            endif /* EKCFG_RTOS */
    return EK_OK;
}

void ek_ringbuf_deinit_static(ek_ringbuf_t *rb)
{
    if (rb == NULL) return;
#            if EKCFG_RTOS == 1
    EK_LOCK_DEINIT(rb->lock);
#            endif
    rb->cap = 0U;
    rb->item_size = 0U;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
    rb->item_amount = 0U;
}
#        endif /* EKCFG_STATIC_ALLOC */

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

ek_err_t ek_ringbuf_write(ek_ringbuf_t *rb, const void *item)
{
    if (rb == NULL || item == NULL) return EK_ERR_INVAL;

    if (!EK_LOCK_TRY(rb)) return EK_ERR_BUSY;

    if (ek_ringbuf_full(rb) == true)
    {
        EK_LOCK_RELEASE(rb);
        return EK_ERR_FULL;
    }

    uint8_t *target = rb->data + (rb->write_idx * rb->item_size);
    memcpy(target, item, rb->item_size);

    rb->write_idx = (rb->write_idx + 1) % rb->cap;
    rb->item_amount++;

    EK_LOCK_RELEASE(rb);

    return EK_OK;
}

ek_err_t ek_ringbuf_read(ek_ringbuf_t *rb, void *item)
{
    if (rb == NULL) return EK_ERR_INVAL;

    if (!EK_LOCK_TRY(rb)) return EK_ERR_BUSY;

    if (ek_ringbuf_empty(rb) == true)
    {
        EK_LOCK_RELEASE(rb);
        return EK_ERR_EMPTY;
    }

    if (item != NULL)
    {
        const uint8_t *source = rb->data + (rb->read_idx * rb->item_size);
        memcpy(item, source, rb->item_size);
    }

    rb->read_idx = (rb->read_idx + 1) % rb->cap;
    rb->item_amount--;

    EK_LOCK_RELEASE(rb);

    return EK_OK;
}

ek_err_t ek_ringbuf_peek(ek_ringbuf_t *rb, void *item)
{
    if (rb == NULL || item == NULL) return EK_ERR_INVAL;

    if (!EK_LOCK_TRY(rb)) return EK_ERR_BUSY;

    if (ek_ringbuf_empty(rb) == true)
    {
        EK_LOCK_RELEASE(rb);
        return EK_ERR_EMPTY;
    }

    const uint8_t *source = rb->data + (rb->read_idx * rb->item_size);
    memcpy(item, source, rb->item_size);

    EK_LOCK_RELEASE(rb);

    return EK_OK;
}
#    endif /* EKCFG_RINGBUF */

#    if EKCFG_RINGBUF_SPSC == 1
__EK_STATIC_INLINE uint32_t _ek_ringbuf_spsc_next_idx(const ek_ringbuf_spsc_t *rb, uint32_t idx)
{
    return (idx + 1U) % rb->cap;
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
#        if EKCFG_STATIC_ALLOC == 1
ek_err_t ek_ringbuf_init_spsc_static(ek_ringbuf_spsc_t *rb, size_t item_size, uint32_t amount)
{
    if (rb == NULL || item_size == 0U || amount <= 1U) return EK_ERR_INVAL;

    rb->cap = amount;
    rb->item_size = item_size;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
    return EK_OK;
}

void ek_ringbuf_deinit_spsc_static(ek_ringbuf_spsc_t *rb)
{
    if (rb == NULL) return;
    rb->cap = 0U;
    rb->item_size = 0U;
    rb->read_idx = 0U;
    rb->write_idx = 0U;
}
#        endif /* EKCFG_STATIC_ALLOC */

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

ek_err_t ek_ringbuf_write_spsc(ek_ringbuf_spsc_t *rb, const void *item)
{
    if (rb == NULL || item == NULL) return EK_ERR_INVAL;

    uint32_t next_idx = _ek_ringbuf_spsc_next_idx(rb, rb->write_idx);
    if (next_idx == rb->read_idx)
    {
        return EK_ERR_FULL;
    }

    uint8_t *target = rb->data + (rb->write_idx * rb->item_size);
    memcpy(target, item, rb->item_size);
    rb->write_idx = next_idx;

    return EK_OK;
}

ek_err_t ek_ringbuf_read_spsc(ek_ringbuf_spsc_t *rb, void *item)
{
    if (rb == NULL) return EK_ERR_INVAL;

    if (rb->read_idx == rb->write_idx)
    {
        return EK_ERR_EMPTY;
    }

    if (item != NULL)
    {
        const uint8_t *source = rb->data + (rb->read_idx * rb->item_size);
        memcpy(item, source, rb->item_size);
    }

    rb->read_idx = _ek_ringbuf_spsc_next_idx(rb, rb->read_idx);

    return EK_OK;
}

ek_err_t ek_ringbuf_peek_spsc(ek_ringbuf_spsc_t *rb, void *item)
{
    if (rb == NULL || item == NULL) return EK_ERR_INVAL;

    if (rb->read_idx == rb->write_idx)
    {
        return EK_ERR_EMPTY;
    }

    const uint8_t *source = rb->data + (rb->read_idx * rb->item_size);
    memcpy(item, source, rb->item_size);

    return EK_OK;
}
#    endif /* EKCFG_RINGBUF_SPSC */

#endif /* EKCFG_RINGBUF || EKCFG_RINGBUF_SPSC */
