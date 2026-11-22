/**
 * @file mutex.c
 * @brief RTOS 互斥量实现（支持可重入加锁与优先级继承）
 * @author N1netyNine99
 */

#include "ek/os/mutex.h"

#if EKCFG_RTOS == 1

#    include "os_priv.h"
#    include "ek/mem/heap.h"
#    include "ek/os/kernel.h"

#    define MAGIC                     0x4D555458UL
#    define MUTEX_HDR                 ((size_t)(EK_OS_MUTEX_STATIC_BUF_SIZE - sizeof(ek_os_mutex_t)))
#    define MUTEX_FROM_BUF(buf)       ((ek_os_mutex_t *)(uintptr_t)((uintptr_t)(buf) + MUTEX_HDR))
#    define MUTEX_TO_BUF(mutex)       ((void *)(uintptr_t)((uintptr_t)(mutex) - MUTEX_HDR))
#    define MUTEX_GET_MAGIC(mutex)    (*(const uint32_t *)(uintptr_t)((uintptr_t)(mutex) - MUTEX_HDR))
#    define MUTEX_SET_MAGIC(mutex, v) (*(uint32_t *)(uintptr_t)((uintptr_t)(mutex) - MUTEX_HDR) = (uint32_t)(v))

ek_os_mutex_t *ek_os_mutex_create(void)
{
    if (ek_arch_in_isr()) return NULL;

    ek_os_mutex_t *mutex = ek_malloc(sizeof(*mutex));
    if (!mutex) return NULL;

    mutex->lock_count = 0;
    mutex->mem_owned = true;
    mutex->owner = NULL;
    ek_list_init(&mutex->wait_list);
    ek_list_init(&mutex->owner_node);

    return mutex;
}

ek_os_mutex_t *ek_os_mutex_create_static(void *buf, size_t size)
{
    if (ek_arch_in_isr()) return NULL;
    if (!buf || size < EK_OS_MUTEX_STATIC_BUF_SIZE) return NULL;
    if (((uintptr_t)buf % alignof(ek_os_mutex_t)) != 0U) return NULL;

    ek_os_mutex_t *mutex = MUTEX_FROM_BUF(buf);
    MUTEX_SET_MAGIC(mutex, MAGIC);

    mutex->lock_count = 0;
    mutex->mem_owned = false;
    mutex->owner = NULL;
    ek_list_init(&mutex->wait_list);
    ek_list_init(&mutex->owner_node);

    return mutex;
}

static inline bool _mutex_valid(const ek_os_mutex_t *mutex)
{
    if (!mutex) return false;
    if (mutex->mem_owned) return true;
    return MUTEX_GET_MAGIC(mutex) == MAGIC;
}

bool mutex_valid(const ek_os_mutex_t *mutex)
{
    return _mutex_valid(mutex);
}

void kernel_mutex_release_locked(ek_os_mutex_t *mutex)
{
    os_thread_t *cur = (os_thread_t *)mutex->owner;
    mutex->lock_count = 0U;
    ek_list_remove(&mutex->owner_node);

    if (!ek_list_is_empty(&mutex->wait_list))
    {
        os_thread_t *next = EK_CONTAINER_OF(ek_list_get_first(&mutex->wait_list), os_thread_t, wait_node);
        mutex->owner = next;
        mutex->lock_count = 1U;
        ek_list_insert_tail(&next->owned_locks, &mutex->owner_node);
        kernel_thread_unblock(next, 0);
    }
    else
    {
        mutex->owner = NULL;
    }

    kernel_thread_recalc_prio(cur);
}

void ek_os_mutex_destroy(ek_os_mutex_t *mutex)
{
    if (ek_arch_in_isr()) return;
    if (!_mutex_valid(mutex)) return;

    uint32_t flags = ek_os_kernel_spin_lock();
    bool owned = mutex->mem_owned;
    os_thread_t *owner = (os_thread_t *)mutex->owner;

    if (owner)
    {
        ek_list_remove(&mutex->owner_node);
        mutex->owner = NULL;
        mutex->lock_count = 0U;
    }

    bool woke = kernel_wake_all(&mutex->wait_list, -ECANCELED);
    if (owner) kernel_thread_recalc_prio(owner);
    if (woke || owner) kernel_schedule();
    if (!owned) MUTEX_SET_MAGIC(mutex, 0U);

    ek_os_kernel_spin_unlock(flags);

    if (owned) ek_free(mutex);
}

int ek_os_mutex_lock(ek_os_mutex_t *mutex, int64_t timeout)
{
    if (ek_arch_in_isr()) return -EPERM;
    if (!_mutex_valid(mutex)) return -ENOENT;

    uint32_t flags = ek_os_kernel_spin_lock();

    os_thread_t *cur = g_cur_thrd;
    if (!cur || kernel_is_idle_thread(cur))
    {
        ek_os_kernel_spin_unlock(flags);
        return -EPERM;
    }

    os_thread_t *owner = (os_thread_t *)mutex->owner;
    if (!owner)
    {
        mutex->owner = cur;
        mutex->lock_count = 1U;
        ek_list_insert_tail(&cur->owned_locks, &mutex->owner_node);
        ek_os_kernel_spin_unlock(flags);
        return 0;
    }

    if (owner == cur)
    {
        int err = 0;
        if (mutex->lock_count < UINT32_MAX) mutex->lock_count++;
        else err = -EOVERFLOW;
        ek_os_kernel_spin_unlock(flags);
        return err;
    }

    if (!timeout)
    {
        ek_os_kernel_spin_unlock(flags);
        return -EAGAIN;
    }

    cur->wait_err = 0;
    cur->waiting_lock = mutex;

    kernel_wait_enqueue_locked(&mutex->wait_list);
    kernel_thread_recalc_prio(owner);
    kernel_thread_block(cur, timeout);
    kernel_schedule();
    ek_os_kernel_spin_unlock(flags);
    ek_arch_isb();

    flags = ek_os_kernel_spin_lock();
    cur->waiting_lock = NULL;
    int err = cur->wait_err;
    ek_os_kernel_spin_unlock(flags);

    return err;
}

int ek_os_mutex_unlock(ek_os_mutex_t *mutex)
{
    if (ek_arch_in_isr()) return -EPERM;
    if (!_mutex_valid(mutex)) return -ENOENT;

    uint32_t flags = ek_os_kernel_spin_lock();

    os_thread_t *cur = g_cur_thrd;
    if (!cur || (os_thread_t *)mutex->owner != cur)
    {
        ek_os_kernel_spin_unlock(flags);
        return -EPERM;
    }

    if (--mutex->lock_count > 0U)
    {
        ek_os_kernel_spin_unlock(flags);
        return 0;
    }

    kernel_mutex_release_locked(mutex);
    kernel_schedule();
    ek_os_kernel_spin_unlock(flags);

    return 0;
}

#endif /* EKCFG_RTOS == 1 */
