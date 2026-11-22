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

    os_thread_t *cur = g_cur_thrd;

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

    // 记录绑定的互斥量供 signal/broadcast 原地 Wait Morphing，同一临界区内原子释锁并阻塞
    cur->wait.data = mutex;
    kernel_mutex_release_locked(mutex);
    int err = kernel_wait_on_locked(&condvar->wait_list, timeout, flags);

    flags = ek_os_kernel_spin_lock();
    bool morphed = (cur->wait.data == NULL);
    cur->wait.data = NULL;
    bool already_owned = mutex_valid(mutex) && ((os_thread_t *)mutex->owner == cur);
    ek_os_kernel_spin_unlock(flags);

    if (already_owned) return 0;

    // 已通过 Wait Morphing 收到信号，若等锁期间被挂起打断则保留信号并重试加锁
    if (morphed && err == -EINTR) err = 0;

    int lock_err;
    do
    {
        lock_err = ek_os_mutex_lock(mutex, EK_OS_FOREVER);
    } while (lock_err == -EINTR);
    if (lock_err < 0) return lock_err;

    return err;
}

static bool _condvar_wake_one_locked(ek_os_condvar_t *condvar)
{
    os_thread_t *thrd = EK_CONTAINER_OF(ek_list_get_first(&condvar->wait_list), os_thread_t, wait.node);
    ek_os_mutex_t *mtx = (ek_os_mutex_t *)thrd->wait.data;

    // 若绑定的互斥量正被持有，直接原地转移到互斥量等待队列，避免无效唤醒折返
    if (mtx && mutex_valid(mtx) && mtx->owner != NULL)
    {
        kernel_wait_morph_to_mutex_locked(thrd, mtx);
        return ((os_thread_t *)mtx->owner != g_cur_thrd);
    }

    kernel_thread_unblock(thrd, 0);
    return true;
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

    if (_condvar_wake_one_locked(condvar)) kernel_schedule();

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

    bool need_sched = false;
    while (!ek_list_is_empty(&condvar->wait_list))
    {
        if (_condvar_wake_one_locked(condvar)) need_sched = true;
    }
    if (need_sched) kernel_schedule();

    ek_os_kernel_spin_unlock(flags);

    return 0;
}

#endif /* EKCFG_RTOS == 1 */
