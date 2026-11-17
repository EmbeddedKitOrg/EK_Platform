/**
 * @file thread.c
 * @brief RTOS 线程创建、挂起、恢复、优先级调整与终止实现
 * @author N1netyNine99
 */

#include "ek/os/thread.h"

#if EKCFG_RTOS == 1

#    include "os_priv.h"
#    include "ek/mem/heap.h"
#    include "ek/os/kernel.h"
#    include "ek/core/assert.h"

#    if EKCFG_STATIC_ALLOC == 1
static_assert(EK_OS_THREAD_TCB_SIZE >= sizeof(os_thread_t), "EK_OS_THREAD_TCB_SIZE is smaller than os_thread_t");
#    endif

void thread_entry_wrapper(ek_os_thread_fn_t entry, void *arg)
{
    void *ret = entry(arg);
    ek_os_thread_exit(ret);
}

static int _thread_setup(os_thread_t *thrd,
                         const char *name,
                         ek_os_thread_fn_t fn,
                         void *arg,
                         uint8_t prio,
                         void *stack_base,
                         size_t stack_size,
                         void *stack_top,
                         bool stack_owned)
{
    memset(thrd, 0, sizeof(*thrd));

    size_t name_len = strlen(name);
    if (name_len >= EKCFG_OS_THREAD_NAME_LEN)
    {
        name_len = EKCFG_OS_THREAD_NAME_LEN - 1;
    }

    memcpy(thrd->name, name, name_len);
    thrd->name[name_len] = '\0';
    thrd->stack_base = stack_base;
    thrd->stack_size = stack_size;
    thrd->stack_owned = stack_owned;
    thrd->fn = fn;
    thrd->arg = arg;
    // 数值越低 优先级越高
    thrd->base_prio = EK_MIN(prio, (uint8_t)EKCFG_OS_THREAD_MIN_PRIO);
    thrd->sp = (uintptr_t)ek_arch_stack_init(stack_top, (void (*)(void *))fn, arg, kernel_thread_exit);
    thrd->state = EK_OS_THREAD_READY;
    thrd->sched_lock = 0;
    thrd->deadline = 0;
    thrd->prio = thrd->base_prio;
    thrd->waiting_lock = NULL;

    ek_list_init(&thrd->state_node);
    ek_list_init(&thrd->wait_node);
    ek_list_init(&thrd->join_wait);
    ek_list_init(&thrd->owned_locks);

    uint32_t flags = ek_os_kernel_spin_lock();
    int tid = kernel_alloc_thread(thrd);
    if (tid <= 0)
    {
        ek_os_kernel_spin_unlock(flags);
        return tid;
    }
    kernel_thread_ready(thrd);
    kernel_schedule();
    ek_os_kernel_spin_unlock(flags);

    return tid;
}

int ek_os_thread_create(const char *name, ek_os_thread_fn_t fn, void *arg, uint8_t prio, size_t size)
{
    if (ek_arch_in_isr()) return -EPERM;

    if (!name || !fn || size < EKCFG_OS_THREAD_MIN_STACK) return -EINVAL;

    size_t tcb_align = _Alignof(os_thread_t);
    size_t overhead = sizeof(os_thread_t) + tcb_align - 1U + 7U;
    if (size > SIZE_MAX - overhead) return -EINVAL;

    size_t stack_size = EK_ALIGN_UP(size, 8);
    uint8_t *buf = ek_malloc(sizeof(os_thread_t) + stack_size + tcb_align - 1U);
    if (!buf) return -ENOMEM;

    uintptr_t tcb_addr = EK_ALIGN_UP((uintptr_t)buf + stack_size, tcb_align);
    uintptr_t stack_top = EK_ALIGN_DOWN(tcb_addr, 8);
    os_thread_t *thrd = (os_thread_t *)tcb_addr;
    int tid =
        _thread_setup(thrd, name, fn, arg, prio, buf, (size_t)(stack_top - (uintptr_t)buf), (void *)stack_top, true);
    if (tid <= 0)
    {
        ek_free(buf);
    }

    return tid;
}

int ek_os_thread_create_static(
    const char *name, ek_os_thread_fn_t fn, void *arg, uint8_t prio, void *stack, size_t size)
{
    if (ek_arch_in_isr()) return -EPERM;

    if (!name || !fn || !stack || size < sizeof(os_thread_t) + EKCFG_OS_THREAD_MIN_STACK)
    {
        return -EINVAL;
    }

    uintptr_t stack_ptr = (uintptr_t)stack;
    if (size > UINTPTR_MAX - stack_ptr)
    {
        return -EINVAL;
    }

    uintptr_t mem_end = stack_ptr + size;
    uintptr_t tcb_addr = EK_ALIGN_DOWN(mem_end - sizeof(os_thread_t), _Alignof(os_thread_t));
    uintptr_t stack_top = EK_ALIGN_DOWN(tcb_addr, 8);
    uintptr_t stack_base = EK_ALIGN_UP(stack, 8);

    if (stack_top <= stack_base || (stack_top - stack_base) < EKCFG_OS_THREAD_MIN_STACK)
    {
        return -EINVAL;
    }

    os_thread_t *thrd = (os_thread_t *)tcb_addr;
    return _thread_setup(
        thrd, name, fn, arg, prio, (void *)stack_base, (size_t)(stack_top - stack_base), (void *)stack_top, false);
}

static int _thread_get(int tid, bool allow_idle, os_thread_t **out_thrd)
{
    os_thread_t *thrd = kernel_tid_to_thread(tid);
    if (!thrd || thrd->state == EK_OS_THREAD_DEAD || thrd->state == EK_OS_THREAD_UNUSED)
    {
        return -ESRCH;
    }
    if (!allow_idle && kernel_is_idle_thread(thrd))
    {
        return -EPERM;
    }
    *out_thrd = thrd;
    return 0;
}

int ek_os_thread_suspend(int tid)
{
    if (ek_arch_in_isr()) return -EPERM;

    os_thread_t *thrd = NULL;
    uint32_t flags = ek_os_kernel_spin_lock();
    int err = _thread_get(tid, false, &thrd);
    if (err == 0)
    {
        ek_assert(ek_list_is_empty(&thrd->owned_locks));

        kernel_thread_suspend(thrd);
        if (thrd == g_cur_thrd || thrd == g_next_thrd)
        {
            kernel_schedule();
        }
    }
    ek_os_kernel_spin_unlock(flags);
    return err;
}

int ek_os_thread_resume(int tid)
{
    if (ek_arch_in_isr()) return -EPERM;

    os_thread_t *thrd = NULL;
    uint32_t flags = ek_os_kernel_spin_lock();
    int err = _thread_get(tid, true, &thrd);
    if (err == 0 && thrd->state == EK_OS_THREAD_SUSPEND)
    {
        kernel_thread_resume(thrd);
        kernel_schedule();
    }
    ek_os_kernel_spin_unlock(flags);
    return err;
}

int ek_os_thread_set_prio(int tid, uint8_t prio)
{
    if (ek_arch_in_isr()) return -EPERM;

    os_thread_t *thrd = NULL;
    uint32_t flags = ek_os_kernel_spin_lock();
    int err = _thread_get(tid, false, &thrd);
    if (err == 0)
    {
        thrd->base_prio = EK_MIN(prio, (uint8_t)EKCFG_OS_THREAD_MIN_PRIO);
        kernel_thread_recalc_prio(thrd);
        kernel_schedule();
    }
    ek_os_kernel_spin_unlock(flags);
    return err;
}

int ek_os_thread_get_prio(int tid, uint8_t *prio)
{
    if (!prio) return -EINVAL;

    os_thread_t *thrd = NULL;
    uint32_t flags = ek_os_kernel_spin_lock();
    int err = _thread_get(tid, true, &thrd);
    if (!err) *prio = thrd->prio;
    ek_os_kernel_spin_unlock(flags);
    return err;
}

static inline void _thread_reap_locked(os_thread_t *thrd, uint32_t flags, void **retval)
{
    void *ret = thrd->retval;
    bool owned = thrd->stack_owned;
    void *buf = thrd->stack_base;

    kernel_free_thread(thrd->tid);
    thrd->state = EK_OS_THREAD_UNUSED;
    ek_os_kernel_spin_unlock(flags);

    if (owned) ek_free(buf);
    if (retval) *retval = ret;
}

int ek_os_thread_join(int tid, int64_t timeout, void **retval)
{
    if (ek_arch_in_isr()) return -EPERM;

    os_thread_t *cur = g_cur_thrd;
    if (!cur) return -EPERM;
    if (tid == cur->tid) return -EDEADLK;

    uint32_t flags = ek_os_kernel_spin_lock();
    os_thread_t *thrd = kernel_tid_to_thread(tid);
    if (!thrd || thrd->state == EK_OS_THREAD_UNUSED)
    {
        ek_os_kernel_spin_unlock(flags);
        return -ESRCH;
    }
    if (kernel_is_idle_thread(thrd))
    {
        ek_os_kernel_spin_unlock(flags);
        return -EPERM;
    }

    if (thrd->state == EK_OS_THREAD_DEAD)
    {
        _thread_reap_locked(thrd, flags, retval);
        return 0;
    }

    if (timeout == 0)
    {
        ek_os_kernel_spin_unlock(flags);
        return -EAGAIN;
    }

    if (!ek_list_is_empty(&thrd->join_wait))
    {
        ek_os_kernel_spin_unlock(flags);
        return -EBUSY;
    }

    int err = kernel_wait_on_locked(&thrd->join_wait, timeout, flags);
    if (err < 0) return err;

    flags = ek_os_kernel_spin_lock();
    thrd = kernel_tid_to_thread(tid);
    if (!thrd || thrd->state == EK_OS_THREAD_UNUSED)
    {
        ek_os_kernel_spin_unlock(flags);
        return -ESRCH;
    }
    _thread_reap_locked(thrd, flags, retval);
    return 0;
}

__EK_NO_RETURN void ek_os_thread_exit(void *retval)
{
    uint32_t flags = ek_os_kernel_spin_lock();
    os_thread_t *cur = g_cur_thrd;

    if (cur && !kernel_is_idle_thread(cur))
    {
        kernel_thread_dead(cur, retval);
        kernel_schedule();
    }
    ek_os_kernel_spin_unlock(flags);

    for (;;)
    {
    }
}

int ek_os_thread_sleep(int64_t timeout)
{
    if (ek_arch_in_isr()) return -EPERM;
    if (timeout < 0) return -EINVAL;
    if (timeout == 0) return 0;

    uint32_t flags = ek_os_kernel_spin_lock();
    os_thread_t *cur = g_cur_thrd;
    if (!cur || kernel_is_idle_thread(cur))
    {
        ek_os_kernel_spin_unlock(flags);
        return -EPERM;
    }

    kernel_thread_block(cur, timeout);
    kernel_schedule();
    ek_os_kernel_spin_unlock(flags);
    ek_arch_isb();

    return 0;
}

#endif /* EKCFG_RTOS == 1 */
