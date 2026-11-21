/**
 * @file msgq.c
 * @brief RTOS 定长消息队列实现
 * @author N1netyNine99
 */

#include "ek/os/msgq.h"

#if EKCFG_RTOS == 1

#    include "os_priv.h"
#    include "ek/os/kernel.h"
#    include "ek/mem/heap.h"
#    include "ek/core/arch.h"

#    define MAGIC                 0x4D534751UL
#    define MSGQ_HDR              ((size_t)EK_OS_MSGQ_HDR_SIZE)
#    define MSGQ_FROM_BUF(buf)    ((ek_os_msgq_t *)(uintptr_t)((uintptr_t)(buf) + MSGQ_HDR))
#    define MSGQ_TO_BUF(mq)       ((void *)(uintptr_t)((uintptr_t)(mq) - MSGQ_HDR))
#    define MSGQ_GET_MAGIC(mq)    (*(const uint32_t *)(uintptr_t)((uintptr_t)(mq) - MSGQ_HDR))
#    define MSGQ_SET_MAGIC(mq, v) (*(uint32_t *)(uintptr_t)((uintptr_t)(mq) - MSGQ_HDR) = (uint32_t)(v))

static bool _msgq_setup(ek_os_msgq_t *mq, size_t item_size, size_t depth, bool owned)
{
    mq->mem_owned = owned;
    mq->item_size = item_size;
    mq->cap = depth;
    mq->count = 0;
    mq->head = 0;
    mq->tail = 0;
    mq->buf = (uint8_t *)EK_ALIGN_UP((uintptr_t)(mq + 1), 8U);
    ek_list_init(&mq->recv_list);
    ek_list_init(&mq->send_list);

    return true;
}

ek_os_msgq_t *ek_os_msgq_create(const size_t item_size, const size_t depth)
{
    if (ek_arch_in_isr()) return NULL;
    if (!item_size || !depth) return NULL;
    if (depth > SIZE_MAX / item_size) return NULL;

    size_t slots = item_size * depth;
    size_t overhead = sizeof(ek_os_msgq_t) + 7U;
    if (slots > SIZE_MAX - overhead) return NULL;

    ek_os_msgq_t *mq = ek_malloc(overhead + slots);
    if (!mq) return NULL;

    _msgq_setup(mq, item_size, depth, true);

    return mq;
}

ek_os_msgq_t *ek_os_msgq_create_static(const size_t item_size, const size_t depth, void *buf, size_t size)
{
    if (ek_arch_in_isr()) return NULL;
    if (!item_size || !depth || !buf) return NULL;
    if (((uintptr_t)buf % _Alignof(ek_os_msgq_t)) != 0U) return NULL;
    if (depth > SIZE_MAX / item_size) return NULL;
    if (size < EK_OS_MSGQ_STATIC_BUF_SIZE(item_size, depth)) return NULL;

    ek_os_msgq_t *mq = MSGQ_FROM_BUF(buf);
    MSGQ_SET_MAGIC(mq, MAGIC);
    _msgq_setup(mq, item_size, depth, false);

    return mq;
}

static inline bool _msgq_valid(const ek_os_msgq_t *msgq)
{
    if (!msgq) return false;
    if (msgq->mem_owned) return true;
    return MSGQ_GET_MAGIC(msgq) == MAGIC;
}

void ek_os_msgq_destroy(ek_os_msgq_t *msgq)
{
    if (ek_arch_in_isr()) return;
    if (!_msgq_valid(msgq)) return;

    uint32_t flags = ek_os_kernel_spin_lock();
    bool owned = msgq->mem_owned;

    bool woke = kernel_wake_all(&msgq->send_list, -ECANCELED);
    woke = kernel_wake_all(&msgq->recv_list, -ECANCELED) || woke;
    if (woke) kernel_schedule();
    if (!owned) MSGQ_SET_MAGIC(msgq, 0U);

    ek_os_kernel_spin_unlock(flags);

    if (owned) ek_free(msgq);
}

int ek_os_msgq_send(ek_os_msgq_t *msgq, const void *item, int64_t timeout)
{
    if (!_msgq_valid(msgq)) return -ENOENT;
    if (!item) return -EINVAL;

    if (ek_arch_in_isr()) timeout = 0;
    else
    {
        os_thread_t *cur = g_cur_thrd;
        if (!cur || kernel_is_idle_thread(cur)) return -EPERM;
    }

    int64_t deadline = (timeout > 0) ? (int64_t)ek_os_kernel_time() + timeout : EK_OS_FOREVER;

    for (;;)
    {
        uint32_t flags = ek_os_kernel_spin_lock();

        if (msgq->count < msgq->cap)
        {
            memcpy(msgq->buf + msgq->head * msgq->item_size, item, msgq->item_size);
            msgq->head = EK_INC_MOD(msgq->head, msgq->cap);
            msgq->count++;

            bool woke = kernel_wake_one(&msgq->recv_list, 0);
            if (woke) kernel_schedule();
            ek_os_kernel_spin_unlock(flags);
            return 0;
        }

        if (!timeout)
        {
            ek_os_kernel_spin_unlock(flags);
            return -EAGAIN;
        }

        int64_t wait = timeout;
        if (timeout > 0)
        {
            wait = deadline - (int64_t)ek_os_kernel_time();
            if (wait <= 0)
            {
                ek_os_kernel_spin_unlock(flags);
                return -ETIMEDOUT;
            }
        }

        int err = kernel_wait_on_locked(&msgq->send_list, wait, flags);
        if (err < 0) return err;
    }
}

int ek_os_msgq_recv(ek_os_msgq_t *msgq, void *item, int64_t timeout)
{
    if (ek_arch_in_isr()) return -EPERM;
    if (!_msgq_valid(msgq)) return -ENOENT;
    if (!item) return -EINVAL;

    os_thread_t *cur = g_cur_thrd;
    if (!cur || kernel_is_idle_thread(cur)) return -EPERM;

    int64_t deadline = (timeout > 0) ? (int64_t)ek_os_kernel_time() + timeout : EK_OS_FOREVER;

    for (;;)
    {
        uint32_t flags = ek_os_kernel_spin_lock();

        if (msgq->count)
        {
            memcpy(item, msgq->buf + msgq->tail * msgq->item_size, msgq->item_size);
            msgq->tail = EK_INC_MOD(msgq->tail, msgq->cap);
            msgq->count--;

            bool woke = kernel_wake_one(&msgq->send_list, 0);
            if (woke) kernel_schedule();
            ek_os_kernel_spin_unlock(flags);
            return 0;
        }

        if (!timeout)
        {
            ek_os_kernel_spin_unlock(flags);
            return -EAGAIN;
        }

        int64_t wait = timeout;
        if (timeout > 0)
        {
            wait = deadline - (int64_t)ek_os_kernel_time();
            if (wait <= 0)
            {
                ek_os_kernel_spin_unlock(flags);
                return -ETIMEDOUT;
            }
        }

        int err = kernel_wait_on_locked(&msgq->recv_list, wait, flags);
        if (err < 0) return err;
    }
}

int ek_os_msgq_count(ek_os_msgq_t *msgq)
{
    if (!_msgq_valid(msgq)) return -ENOENT;

    uint32_t flags = ek_os_kernel_spin_lock();
    int count = (int)msgq->count;
    ek_os_kernel_spin_unlock(flags);

    return count;
}

#endif /* EKCFG_RTOS == 1 */
