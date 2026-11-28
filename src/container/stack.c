/**
 * @file stack.c
 * @brief 栈数据结构实现
 * @author N1netyNine99
 */

#include "ek/container/stack.h"

#if EKCFG_STACK == 1

#    include "ek/mem/heap.h"

#    define STACK_MAGIC            0x5354414BUL
#    define STACK_HDR              ((size_t)EK_ALIGN_UP(sizeof(uint32_t), alignof(ek_stack_t)))
#    define STACK_FROM_BUF(buf)    ((ek_stack_t *)(uintptr_t)((uintptr_t)(buf) + STACK_HDR))
#    define STACK_GET_MAGIC(sk)    (*(const uint32_t *)(uintptr_t)((uintptr_t)(sk) - STACK_HDR))
#    define STACK_SET_MAGIC(sk, v) (*(uint32_t *)(uintptr_t)((uintptr_t)(sk) - STACK_HDR) = (uint32_t)(v))

static inline bool _stack_valid(const ek_stack_t *sk)
{
    if (!sk) return false;
    if (!sk->mem_owned) return STACK_GET_MAGIC(sk) == STACK_MAGIC;
    return true;
}

bool ek_stack_full(ek_stack_t *sk)
{
    if (!_stack_valid(sk)) return false;
    return sk->sp >= sk->cap;
}

bool ek_stack_empty(ek_stack_t *sk)
{
    if (!_stack_valid(sk)) return false;
    return sk->sp == 0U;
}

ek_stack_t *ek_stack_create_static(void *buf, size_t size, size_t item_size, uint32_t item_amount)
{
    if (!buf || item_size == 0U || item_amount == 0U) return NULL;
    if (((uintptr_t)buf % alignof(ek_stack_t)) != 0U) return NULL;
    // 防止总大小溢出
    if (item_amount > (SIZE_MAX - STACK_HDR - sizeof(ek_stack_t)) / item_size)
    {
        return NULL;
    }
    if (size < EK_STACK_STATIC_BUF_SIZE(item_size, item_amount)) return NULL;

    ek_stack_t *sk = STACK_FROM_BUF(buf);
    sk->mem_owned = false;
    sk->sp = 0U;
    sk->cap = item_amount;
    sk->item_size = item_size;
    STACK_SET_MAGIC(sk, STACK_MAGIC);
    return sk;
}

ek_stack_t *ek_stack_create(size_t item_size, uint32_t item_amount)
{
    if (item_amount == 0U || item_size == 0U) return NULL;
    // 防止总大小溢出
    if (item_amount > (SIZE_MAX - sizeof(ek_stack_t)) / item_size)
    {
        return NULL;
    }

    ek_stack_t *sk = (ek_stack_t *)ek_malloc(sizeof(ek_stack_t) + (size_t)item_amount * item_size);
    if (sk == NULL)
    {
        return NULL;
    }

    sk->mem_owned = true;
    sk->sp = 0U;
    sk->cap = item_amount;
    sk->item_size = item_size;
    return sk;
}

void ek_stack_destroy(ek_stack_t *sk)
{
    if (!_stack_valid(sk)) return;

    if (sk->mem_owned)
    {
        ek_free(sk);
        return;
    }

    STACK_SET_MAGIC(sk, 0U);
    sk->sp = 0U;
    sk->cap = 0U;
    sk->item_size = 0U;
}

int ek_stack_push(ek_stack_t *sk, const void *item)
{
    if (!_stack_valid(sk)) return -ENOENT;
    if (item == NULL) return -EINVAL;

    if (sk->sp >= sk->cap)
    {
        return -ENOSPC;
    }

    uint8_t *target = sk->data + sk->sp * sk->item_size;
    memcpy(target, item, sk->item_size);
    sk->sp++;

    return 0;
}

int ek_stack_pop(ek_stack_t *sk, void *item)
{
    if (!_stack_valid(sk)) return -ENOENT;
    if (item == NULL) return -EINVAL;

    if (sk->sp == 0U)
    {
        return -ENODATA;
    }

    sk->sp--;
    uint8_t *source = sk->data + sk->sp * sk->item_size;
    memcpy(item, source, sk->item_size);

    return 0;
}

#endif /* EKCFG_STACK */
