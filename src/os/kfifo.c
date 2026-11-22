/**
 * @file kfifo.c
 * @brief RTOS 侵入式零拷贝 FIFO 队列实现
 * @author N1netyNine99
 */

#include "ek/os/kfifo.h"

#if EKCFG_RTOS == 1

#    include "os_priv.h"
#    include "ek/core/arch.h"
#    include "ek/mem/heap.h"
#    include "ek/os/kernel.h"

#    define MAGIC                     0x4649464FUL
#    define KFIFO_HDR                 ((size_t)(EK_OS_KFIFO_STATIC_BUF_SIZE - sizeof(ek_os_kfifo_t)))
#    define KFIFO_FROM_BUF(buf)       ((ek_os_kfifo_t *)(uintptr_t)((uintptr_t)(buf) + KFIFO_HDR))
#    define KFIFO_TO_BUF(kfifo)       ((void *)(uintptr_t)((uintptr_t)(kfifo) - KFIFO_HDR))
#    define KFIFO_GET_MAGIC(kfifo)    (*(const uint32_t *)(uintptr_t)((uintptr_t)(kfifo) - KFIFO_HDR))
#    define KFIFO_SET_MAGIC(kfifo, v) (*(uint32_t *)(uintptr_t)((uintptr_t)(kfifo) - KFIFO_HDR) = (uint32_t)(v))

ek_os_kfifo_t *ek_os_kfifo_create(void)
{
    if (ek_arch_in_isr()) return NULL;

    ek_os_kfifo_t *fifo = ek_malloc(sizeof(*fifo));
    if (!fifo) return NULL;

    fifo->mem_owned = true;
    fifo->head = fifo->tail = NULL;
    ek_list_init(&fifo->wait_list);

    return fifo;
}

ek_os_kfifo_t *ek_os_kfifo_create_static(void *buf, size_t size)
{
    if (ek_arch_in_isr()) return NULL;
    if (!buf || size < EK_OS_KFIFO_STATIC_BUF_SIZE) return NULL;
    if (((uintptr_t)buf % alignof(ek_os_kfifo_t)) != 0U) return NULL;

    ek_os_kfifo_t *fifo = KFIFO_FROM_BUF(buf);

    fifo->mem_owned = false;
    fifo->head = fifo->tail = NULL;
    ek_list_init(&fifo->wait_list);
    KFIFO_SET_MAGIC(fifo, MAGIC);

    return fifo;
}

static inline bool _kfifo_valid(const ek_os_kfifo_t *fifo)
{
    if (!fifo) return false;
    if (!fifo->mem_owned) return KFIFO_GET_MAGIC(fifo) == MAGIC;
    return true;
}

void ek_os_kfifo_destroy(ek_os_kfifo_t *fifo)
{
    if (ek_arch_in_isr()) return;
    if (!_kfifo_valid(fifo)) return;

    uint32_t flags = ek_os_kernel_spin_lock();
    bool owned = fifo->mem_owned;
    bool woke = kernel_wake_all_locked(&fifo->wait_list, -ECANCELED);
    if (woke) kernel_schedule_locked();

    if (!owned) KFIFO_SET_MAGIC(fifo, 0);

    ek_os_kernel_spin_unlock(flags);

    if (owned) ek_free(fifo);
}

int ek_os_kfifo_put(ek_os_kfifo_t *fifo, void *data)
{
    if (!_kfifo_valid(fifo)) return -ENOENT;
    if (!data || ((uintptr_t)data % alignof(ek_os_kfifo_node_t)) != 0U) return -EINVAL;

    uint32_t flags = ek_os_kernel_spin_lock();

    if (!ek_list_is_empty(&fifo->wait_list))
    {
        os_thread_t *thrd = EK_CONTAINER_OF(ek_list_get_first(&fifo->wait_list), os_thread_t, wait.node);
        thrd->wait.data = data;

        bool woke = kernel_wake_one_locked(&fifo->wait_list, 0);
        if (woke) kernel_schedule_locked();

        ek_os_kernel_spin_unlock(flags);
        return 0;
    }

    ek_os_kfifo_node_t *node = (ek_os_kfifo_node_t *)data;
    node->next = NULL;

    if (fifo->tail) fifo->tail->next = node;
    else fifo->head = node;
    fifo->tail = node;

    ek_os_kernel_spin_unlock(flags);
    return 0;
}

int ek_os_kfifo_get(ek_os_kfifo_t *fifo, void **data, int64_t timeout)
{
    if (ek_arch_in_isr() && timeout != 0) return -EPERM;
    if (!_kfifo_valid(fifo)) return -ENOENT;
    if (!data) return -EINVAL;

    uint32_t flags = ek_os_kernel_spin_lock();

    if (fifo->head)
    {
        ek_os_kfifo_node_t *node = fifo->head;
        fifo->head = node->next;
        if (!fifo->head) fifo->tail = NULL;
        node->next = NULL;

        ek_os_kernel_spin_unlock(flags);
        *data = node;
        return 0;
    }

    if (!timeout)
    {
        ek_os_kernel_spin_unlock(flags);
        return -EAGAIN;
    }

    os_thread_t *cur = g_cur_thrd;
    if (!cur || kernel_is_idle_thread(cur))
    {
        ek_os_kernel_spin_unlock(flags);
        return -EPERM;
    }

    cur->wait.data = NULL;
    int err = kernel_wait_on_locked(&fifo->wait_list, timeout, flags);
    if (!err) *data = cur->wait.data;

    return err;
}

int ek_os_kfifo_peek(const ek_os_kfifo_t *fifo, void **data)
{
    if (!_kfifo_valid(fifo)) return -ENOENT;
    if (!data) return -EINVAL;

    uint32_t flags = ek_os_kernel_spin_lock();
    ek_os_kfifo_node_t *node = fifo->head;
    ek_os_kernel_spin_unlock(flags);

    if (!node) return -EAGAIN;

    *data = node;
    return 0;
}

bool ek_os_kfifo_is_empty(const ek_os_kfifo_t *fifo)
{
    if (!_kfifo_valid(fifo)) return true;

    uint32_t flags = ek_os_kernel_spin_lock();
    bool empty = (fifo->head == NULL);
    ek_os_kernel_spin_unlock(flags);

    return empty;
}

#endif /* EKCFG_RTOS == 1 */
