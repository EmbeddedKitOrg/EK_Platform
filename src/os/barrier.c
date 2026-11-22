/**
 * @file barrier.c
 * @brief RTOS 屏障实现
 * @author N1netyNine99
 */

#include "ek/os/barrier.h"

#if EKCFG_RTOS == 1
#    include "os_priv.h"
#    include "ek/os/kernel.h"
#    include "ek/mem/heap.h"
#    include "ek/core/arch.h"

#    define MAGIC                         0x42415252UL
#    define BARRIER_HDR                   ((size_t)(EK_OS_BARRIER_STATIC_BUF_SIZE - sizeof(ek_os_barrier_t)))
#    define BARRIER_FROM_BUF(buf)         ((ek_os_barrier_t *)(uintptr_t)((uintptr_t)(buf) + BARRIER_HDR))
#    define BARRIER_TO_BUF(barrier)       ((void *)(uintptr_t)((uintptr_t)(barrier) - BARRIER_HDR))
#    define BARRIER_GET_MAGIC(barrier)    (*(const uint32_t *)(uintptr_t)((uintptr_t)(barrier) - BARRIER_HDR))
#    define BARRIER_SET_MAGIC(barrier, v) (*(uint32_t *)(uintptr_t)((uintptr_t)(barrier) - BARRIER_HDR) = (uint32_t)(v))

ek_os_barrier_t *ek_os_barrier_create(uint32_t expected)
{
    if (ek_arch_in_isr()) return NULL;
    if (!expected) return NULL;

    ek_os_barrier_t *barrier = ek_malloc(sizeof(*barrier));
    if (!barrier) return NULL;

    barrier->mem_owned = true;
    barrier->count = 0;
    barrier->generation = 0;
    barrier->expected = expected;
    ek_list_init(&barrier->wait_list);

    return barrier;
}

ek_os_barrier_t *ek_os_barrier_create_static(void *buf, size_t size, uint32_t expected)
{
    if (ek_arch_in_isr()) return NULL;
    if (!buf || size < EK_OS_BARRIER_STATIC_BUF_SIZE) return NULL;
    if (((uintptr_t)buf % alignof(ek_os_barrier_t)) != 0U) return NULL;
    if (!expected) return NULL;

    ek_os_barrier_t *barrier = BARRIER_FROM_BUF(buf);
    barrier->mem_owned = false;
    barrier->count = 0;
    barrier->generation = 0;
    barrier->expected = expected;
    ek_list_init(&barrier->wait_list);

    BARRIER_SET_MAGIC(barrier, MAGIC);

    return barrier;
}

static inline bool _barrier_valid(const ek_os_barrier_t *barrier)
{
    if (!barrier) return false;
    if (barrier->mem_owned) return true;
    return BARRIER_GET_MAGIC(barrier) == MAGIC;
}

void ek_os_barrier_destroy(ek_os_barrier_t *barrier)
{
    if (ek_arch_in_isr()) return;
    if (!_barrier_valid(barrier)) return;

    uint32_t flags = ek_os_kernel_spin_lock();

    bool owned = barrier->mem_owned;

    bool woke = kernel_wake_all(&barrier->wait_list, -ECANCELED);
    if (woke) kernel_schedule();

    if (!owned) BARRIER_SET_MAGIC(barrier, 0);

    ek_os_kernel_spin_unlock(flags);

    if (owned) ek_free(barrier);
}

int ek_os_barrier_reset(ek_os_barrier_t *barrier, uint32_t expected)
{
    if (!_barrier_valid(barrier)) return -ENOENT;
    if (!expected) return -EINVAL;

    uint32_t flags = ek_os_kernel_spin_lock();

    if (!ek_list_is_empty(&barrier->wait_list) || barrier->count)
    {
        ek_os_kernel_spin_unlock(flags);
        return -EBUSY;
    }

    barrier->expected = expected;
    barrier->generation++;

    ek_os_kernel_spin_unlock(flags);

    return 0;
}

int ek_os_barrier_wait(ek_os_barrier_t *barrier, int64_t timeout)
{
    if (!_barrier_valid(barrier)) return -ENOENT;
    if (ek_arch_in_isr()) return -EPERM;

    uint32_t flags = ek_os_kernel_spin_lock();

    const os_thread_t *cur = g_cur_thrd;
    if (!cur || kernel_is_idle_thread(cur))
    {
        ek_os_kernel_spin_unlock(flags);
        return -EPERM;
    }

    if (!barrier->expected || barrier->count >= barrier->expected)
    {
        ek_os_kernel_spin_unlock(flags);
        return -EINVAL;
    }

    if (barrier->count + 1U == barrier->expected)
    {
        barrier->count = 0U;
        barrier->generation++;
        bool woke = kernel_wake_all(&barrier->wait_list, 0);
        if (woke) kernel_schedule();

        ek_os_kernel_spin_unlock(flags);
        return 0;
    }

    if (!timeout)
    {
        ek_os_kernel_spin_unlock(flags);
        return -EAGAIN;
    }

    barrier->count++;
    uint32_t gen = barrier->generation;
    int err = kernel_wait_on_locked(&barrier->wait_list, timeout, flags);
    if (!err || err == -ECANCELED) return err;

    // 仅在同一代且对象仍有效时退还等待计数，避免跨轮次误扣
    flags = ek_os_kernel_spin_lock();
    if (_barrier_valid(barrier) && barrier->generation == gen && barrier->count > 0U)
    {
        barrier->count--;
    }
    ek_os_kernel_spin_unlock(flags);

    return err;
}

#endif /* EKCFG_RTOS == 1 */
