/**
 * @file condvar.c
 * @brief RTOS 条件变量实现
 * @author N1netyNine99
 */

#include "ek/os/condvar.h"

#if EKCFG_RTOS == 1

#    include "os_priv.h"
#    include "ek/os/kernel.h"
#    include "ek/mem/heap.h"
#    include "ek/core/arch.h"

#    define MAGIC                         0x434F4E44UL
#    define CONDVAR_HDR                   ((size_t)(EK_OS_CONDVAR_STATIC_BUF_SIZE - sizeof(ek_os_condvar_t)))
#    define CONDVAR_FROM_BUF(buf)         ((ek_os_condvar_t *)(uintptr_t)((uintptr_t)(buf) + CONDVAR_HDR))
#    define CONDVAR_TO_BUF(condvar)       ((void *)(uintptr_t)((uintptr_t)(condvar) - CONDVAR_HDR))
#    define CONDVAR_GET_MAGIC(condvar)    (*(const uint32_t *)(uintptr_t)((uintptr_t)(condvar) - CONDVAR_HDR))
#    define CONDVAR_SET_MAGIC(condvar, v) (*(uint32_t *)(uintptr_t)((uintptr_t)(condvar) - CONDVAR_HDR) = (uint32_t)(v))

ek_os_condvar_t *ek_os_condvar_create(void)
{
    if (ek_arch_in_isr()) return NULL;

    ek_os_condvar_t *condvar = ek_malloc(sizeof(*condvar));
    if (!condvar) return NULL;

    condvar->mem_owned = true;
    ek_list_init(&condvar->wait_list);

    return condvar;
}

ek_os_condvar_t *ek_os_condvar_create_static(void *buf, size_t size)
{
    if (ek_arch_in_isr()) return NULL;
    if (!buf || size < EK_OS_CONDVAR_STATIC_BUF_SIZE) return NULL;
    if (((uintptr_t)buf % alignof(ek_os_condvar_t)) != 0U) return NULL;

    ek_os_condvar_t *condvar = CONDVAR_FROM_BUF(buf);

    condvar->mem_owned = false;
    ek_list_init(&condvar->wait_list);

    CONDVAR_SET_MAGIC(condvar, MAGIC);

    return condvar;
}

static inline bool _condvar_valid(const ek_os_condvar_t *condvar)
{
    if (!condvar) return false;
    if (condvar->mem_owned) return true;
    return CONDVAR_GET_MAGIC(condvar) == MAGIC;
}

void ek_os_condvar_destroy(ek_os_condvar_t *condvar)
{
    if (ek_arch_in_isr()) return;
    if (!_condvar_valid(condvar)) return;

    uint32_t flags = ek_os_kernel_spin_lock();

    bool owned = condvar->mem_owned;

    bool woke = kernel_wake_all(&condvar->wait_list, -ECANCELED);
    if (woke) kernel_schedule();
    if (!owned) CONDVAR_SET_MAGIC(condvar, 0U);

    ek_os_kernel_spin_unlock(flags);

    if (owned) ek_free(condvar);
}

int ek_os_condvar_wait(ek_os_condvar_t *condvar, ek_os_mutex_t *mutex, int64_t timeout)
{
    if (ek_arch_in_isr()) return -EPERM;
    if (!_condvar_valid(condvar)) return -ENOENT;
    if (!mutex_valid(mutex)) return -ENOENT;

    uint32_t flags = ek_os_kernel_spin_lock();

    const os_thread_t *cur = g_cur_thrd;

    if (!cur || kernel_is_idle_thread(cur) || (os_thread_t *)mutex->owner != cur)
    {
        ek_os_kernel_spin_unlock(flags);
        return -EPERM;
    }

    if (!timeout)
    {
        ek_os_kernel_spin_unlock(flags);
        return -ETIMEDOUT;
    }

    if (mutex->lock_count > 1U)
    {
        ek_os_kernel_spin_unlock(flags);
        return -EDEADLK;
    }

    // 在同一自旋锁临界区内原子释放互斥量并挂入等待队列，防止唤醒丢失
    kernel_mutex_release_locked(mutex);
    int err = kernel_wait_on_locked(&condvar->wait_list, timeout, flags);

    // 返回前必须重新持有互斥量；若重加锁期间被挂起打断则继续等待
    int lock_err;
    do
    {
        lock_err = ek_os_mutex_lock(mutex, EK_OS_FOREVER);
    } while (lock_err == -EINTR);
    if (lock_err < 0) return lock_err;

    return err;
}

int ek_os_condvar_signal(ek_os_condvar_t *condvar)
{
    if (!_condvar_valid(condvar)) return -ENOENT;

    uint32_t flags = ek_os_kernel_spin_lock();

    if (ek_list_is_empty(&condvar->wait_list))
    {
        ek_os_kernel_spin_unlock(flags);
        return 0;
    }

    bool woke = kernel_wake_one(&condvar->wait_list, 0);
    if (woke) kernel_schedule();

    ek_os_kernel_spin_unlock(flags);

    return 0;
}

int ek_os_condvar_broadcast(ek_os_condvar_t *condvar)
{
    if (!_condvar_valid(condvar)) return -ENOENT;

    uint32_t flags = ek_os_kernel_spin_lock();

    if (ek_list_is_empty(&condvar->wait_list))
    {
        ek_os_kernel_spin_unlock(flags);
        return 0;
    }

    bool woke = kernel_wake_all(&condvar->wait_list, 0);
    if (woke) kernel_schedule();

    ek_os_kernel_spin_unlock(flags);

    return 0;
}

#endif /* EKCFG_RTOS == 1 */
