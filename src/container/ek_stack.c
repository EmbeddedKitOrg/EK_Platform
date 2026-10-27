/**
 * @file ek_stack.c
 * @brief 栈数据结构实现
 * @author N1netyNine99
 */

#include "ek/container/ek_stack.h"

#if EKCFG_STACK == 1

#    include "ek/mem/ek_heap.h"

bool ek_stack_full(ek_stack_t *sk)
{
    if (sk == NULL) return false;
    return sk->sp >= sk->cap;
}

bool ek_stack_empty(ek_stack_t *sk)
{
    if (sk == NULL) return false;
    return sk->sp == 0;
}

int ek_stack_init_static(ek_stack_t *sk, size_t item_size, uint32_t item_amount)
{
    if (sk == NULL || item_size == 0U || item_amount == 0U) return -EINVAL;

    sk->sp = 0U;
    sk->cap = item_amount;
    sk->item_size = item_size;
#    if EKCFG_RTOS == 1
    EK_LOCK_INIT(sk->lock);
#    endif /* EKCFG_RTOS */
    return 0;
}

void ek_stack_deinit_static(ek_stack_t *sk)
{
    if (sk == NULL) return;
#    if EKCFG_RTOS == 1
    EK_LOCK_DEINIT(sk->lock);
#    endif
    sk->sp = 0U;
    sk->cap = 0U;
    sk->item_size = 0U;
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

    sk->sp = 0U;
    sk->cap = item_amount;
    sk->item_size = item_size;
#    if EKCFG_RTOS == 1
    EK_LOCK_INIT(sk->lock);
#    endif /* EKCFG_RTOS */
    return sk;
}

void ek_stack_destroy(ek_stack_t *sk)
{
    if (sk == NULL) return;

#    if EKCFG_RTOS == 1
    EK_LOCK_DEINIT(sk->lock);
#    endif
    ek_free(sk);
}

int ek_stack_push(ek_stack_t *sk, const void *item)
{
    if (sk == NULL || item == NULL) return -EINVAL;

    if (!EK_LOCK_TRY(sk)) return -EBUSY;

    if (ek_stack_full(sk) == true)
    {
        EK_LOCK_RELEASE(sk);
        return -ENOSPC;
    }

    uint8_t *target = sk->data + sk->sp * sk->item_size;
    memcpy(target, item, sk->item_size);
    sk->sp++;

    EK_LOCK_RELEASE(sk);

    return 0;
}

int ek_stack_pop(ek_stack_t *sk, void *item)
{
    if (sk == NULL || item == NULL) return -EINVAL;

    if (!EK_LOCK_TRY(sk)) return -EBUSY;

    if (ek_stack_empty(sk) == true)
    {
        EK_LOCK_RELEASE(sk);
        return -ENODATA;
    }

    sk->sp--;
    uint8_t *source = sk->data + sk->sp * sk->item_size;
    memcpy(item, source, sk->item_size);

    EK_LOCK_RELEASE(sk);

    return 0;
}

#endif /* EKCFG_STACK */
