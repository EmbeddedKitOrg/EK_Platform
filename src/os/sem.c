/**
 * @file sem.c
 * @brief RTOS 计数信号量实现
 * @author N1netyNine99
 */

#include "ek/os/sem.h"

#if EKCFG_RTOS == 1

#    include "os_priv.h"
#    include "ek/os/kernel.h"
#    include "ek/mem/heap.h"
#    include "ek/core/arch.h"

#    define MAGIC                 0x53454D50UL
#    define SEM_HDR               ((size_t)(EK_OS_SEM_STATIC_BUF_SIZE - sizeof(ek_os_sem_t)))
#    define SEM_FROM_BUF(buf)     ((ek_os_sem_t *)(uintptr_t)((uintptr_t)(buf) + SEM_HDR))
#    define SEM_TO_BUF(sem)       ((void *)(uintptr_t)((uintptr_t)(sem) - SEM_HDR))
#    define SEM_GET_MAGIC(sem)    (*(const uint32_t *)(uintptr_t)((uintptr_t)(sem) - SEM_HDR))
#    define SEM_SET_MAGIC(sem, v) (*(uint32_t *)(uintptr_t)((uintptr_t)(sem) - SEM_HDR) = (uint32_t)(v))

ek_os_sem_t *ek_os_sem_create(size_t init)
{
    if (ek_arch_in_isr()) return NULL;

    ek_os_sem_t *sem = ek_malloc(sizeof(*sem));
    if (!sem) return NULL;

    sem->mem_owned = true;
    sem->count = init;
    ek_list_init(&sem->wait_list);

    return sem;
}

ek_os_sem_t *ek_os_sem_create_static(size_t init, void *buf, size_t size)
{
    if (!buf || size < EK_OS_SEM_STATIC_BUF_SIZE) return NULL;
    if (((uintptr_t)buf % alignof(ek_os_sem_t)) != 0U) return NULL;
    if (ek_arch_in_isr()) return NULL;

    ek_os_sem_t *sem = SEM_FROM_BUF(buf);
    SEM_SET_MAGIC(sem, MAGIC);

    sem->mem_owned = false;
    sem->count = init;
    ek_list_init(&sem->wait_list);
    return sem;
}

static inline bool _sem_valid(const ek_os_sem_t *sem)
{
    if (!sem) return false;
    if (sem->mem_owned) return true;
    return SEM_GET_MAGIC(sem) == MAGIC;
}

void ek_os_sem_destroy(ek_os_sem_t *sem)
{
    if (!_sem_valid(sem)) return;
    if (ek_arch_in_isr()) return;

    bool owned = sem->mem_owned;

    uint32_t flags = ek_os_kernel_spin_lock();

    bool woke = kernel_wake_all(&sem->wait_list, -ECANCELED);
    if (woke) kernel_schedule();
    if (!owned) SEM_SET_MAGIC(sem, 0U);

    ek_os_kernel_spin_unlock(flags);

    if (owned) ek_free(sem);
}

int ek_os_sem_give(ek_os_sem_t *sem)
{
    if (!_sem_valid(sem)) return -ENOENT;

    uint32_t flags = ek_os_kernel_spin_lock();

    bool woke = false;
    if (ek_list_is_empty(&sem->wait_list) && sem->count < SIZE_MAX)
    {
        sem->count++;
    }
    else woke = kernel_wake_one(&sem->wait_list, 0);

    if (woke) kernel_schedule();

    ek_os_kernel_spin_unlock(flags);

    return 0;
}

int ek_os_sem_take(ek_os_sem_t *sem, int64_t timeout)
{
    if (!_sem_valid(sem)) return -ENOENT;
    if (ek_arch_in_isr()) return -EPERM;

    uint32_t flags = ek_os_kernel_spin_lock();

    if (sem->count)
    {
        sem->count--;
        ek_os_kernel_spin_unlock(flags);
        return 0;
    }

    if (!timeout)
    {
        ek_os_kernel_spin_unlock(flags);
        return -EAGAIN;
    }

    return kernel_wait_on_locked(&sem->wait_list, timeout, flags);
}

int ek_os_sem_count(ek_os_sem_t *sem)
{
    if (!_sem_valid(sem)) return -ENOENT;
    return sem->count;
}

#endif
