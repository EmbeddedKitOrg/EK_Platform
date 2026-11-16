/**
 * @file kernel.c
 * @brief 无滴答 RTOS 内核调度器、阻塞超时与等待队列实现
 * @author N1netyNine99
 */

#include "os_priv.h"

#if EKCFG_RTOS == 1

#    include "ek/os/kernel.h"
#    include "ek/mem/heap.h"
#    include "ek/core/def.h"

#    define TID_SLOT_BITS       6
#    define TID_SLOT_MASK       ((1U << TID_SLOT_BITS) - 1U)
#    define MAKE_TID(gen, slot) ((int)(((uint32_t)(gen) << TID_SLOT_BITS) | (uint32_t)(slot)))
#    define TID_SLOT(tid)       ((uint32_t)(tid) & TID_SLOT_MASK)

static_assert(EKCFG_OS_THREADS_MAX <= (1U << TID_SLOT_BITS), "EKCFG_OS_THREADS_MAX exceeds TID_SLOT_BITS capacity");
static_assert(EKCFG_OS_THREAD_MIN_PRIO < 32, "EKCFG_OS_THREAD_MIN_PRIO must be < 32");
static_assert(offsetof(os_thread_t, sp) == 0, "sp must be at offset 0");

typedef struct
{
    os_thread_t *thrd;
    uint16_t gen;
} thread_slot_t;

static thread_slot_t s_all_threads[EKCFG_OS_THREADS_MAX];
static ek_list_node_t s_ready_thrds[EKCFG_OS_THREAD_MIN_PRIO + 1];
static uint32_t s_ready_bitmap;
static ek_list_node_t s_block_thrds = EK_LIST_HEAD_INIT(s_block_thrds);
static ek_list_node_t s_suspend_thrds = EK_LIST_HEAD_INIT(s_suspend_thrds);
static os_thread_t *s_idle_thrd;
static ek_os_timer_ops_t s_timer_ops;

os_thread_t *g_next_thrd;
os_thread_t *g_cur_thrd;

void kernel_thread_exit(void)
{
    ek_os_thread_exit(0);
}


int kernel_alloc_thread(os_thread_t *thrd)
{
    if (!thrd) return -EINVAL;

    uint32_t flags = ek_os_kernel_spin_lock();
    for (int i = 0; i < EKCFG_OS_THREADS_MAX; i++)
    {
        if (!s_all_threads[i].thrd)
        {
            s_all_threads[i].thrd = thrd;
            if (++s_all_threads[i].gen == 0) s_all_threads[i].gen = 1;
            int tid = MAKE_TID(s_all_threads[i].gen, i);
            thrd->tid = tid;
            ek_os_kernel_spin_unlock(flags);
            return tid;
        }
    }
    ek_os_kernel_spin_unlock(flags);
    return -ENOSPC;
}

void kernel_free_thread(int tid)
{
    if (tid <= 0) return;

    uint32_t slot = TID_SLOT(tid);
    if (slot >= EKCFG_OS_THREADS_MAX) return;

    uint32_t flags = ek_os_kernel_spin_lock();
    os_thread_t *thrd = s_all_threads[slot].thrd;
    if (thrd && thrd->tid == tid)
    {
        s_all_threads[slot].thrd = NULL;
    }
    ek_os_kernel_spin_unlock(flags);
}

os_thread_t *kernel_tid_to_thread(int tid)
{
    if (tid <= 0) return NULL;

    uint32_t slot = TID_SLOT(tid);
    if (slot >= EKCFG_OS_THREADS_MAX) return NULL;

    uint32_t flags = ek_os_kernel_spin_lock();
    os_thread_t *thrd = s_all_threads[slot].thrd;
    if (!thrd || thrd->tid != tid)
    {
        thrd = NULL;
    }
    ek_os_kernel_spin_unlock(flags);

    return thrd;
}

int kernel_thread_to_tid(os_thread_t *thrd)
{
    return thrd ? thrd->tid : -EINVAL;
}

bool kernel_is_idle_thread(const os_thread_t *thrd)
{
    return thrd && (thrd == s_idle_thrd);
}

static inline void _kernel_clear_ready_bit(uint8_t prio)
{
    if (s_ready_thrds[prio].next && ek_list_is_empty(&s_ready_thrds[prio]))
    {
        s_ready_bitmap &= ~(1UL << prio);
    }
}

void kernel_thread_dead(os_thread_t *thrd, int err)
{
    uint32_t flags = ek_os_kernel_spin_lock();
    if (thrd->state == EK_OS_THREAD_DEAD || thrd->state == EK_OS_THREAD_UNUSED)
    {
        ek_os_kernel_spin_unlock(flags);
        return;
    }

    thrd->wait_err = (err < 0) ? 0 : err;
    thrd->state = EK_OS_THREAD_DEAD;
    ek_list_remove(&thrd->state_node);
    ek_list_remove(&thrd->wait_node);
    _kernel_clear_ready_bit(thrd->prio);
    kernel_wake_all(&thrd->join_wait, 0);
    ek_os_kernel_spin_unlock(flags);
}

void kernel_thread_ready(os_thread_t *thrd)
{
    uint32_t flags = ek_os_kernel_spin_lock();
    if (thrd->state == EK_OS_THREAD_DEAD)
    {
        ek_os_kernel_spin_unlock(flags);
        return;
    }
    if (!s_ready_thrds[thrd->prio].next) ek_list_init(&s_ready_thrds[thrd->prio]);
    thrd->state = EK_OS_THREAD_READY;
    ek_list_remove(&thrd->state_node);
    ek_list_insert_tail(&s_ready_thrds[thrd->prio], &thrd->state_node);
    s_ready_bitmap |= (1UL << thrd->prio);
    ek_os_kernel_spin_unlock(flags);
}

void kernel_thread_run(os_thread_t *thrd)
{
    uint32_t flags = ek_os_kernel_spin_lock();
    thrd->state = EK_OS_THREAD_RUNNING;
    ek_list_remove(&thrd->state_node);
    _kernel_clear_ready_bit(thrd->prio);
    ek_os_kernel_spin_unlock(flags);
}

static inline bool _kernel_is_block_head(const os_thread_t *thrd)
{
    return (thrd->state == EK_OS_THREAD_BLOCK) && !ek_list_is_empty(&s_block_thrds) &&
           (ek_list_get_first(&s_block_thrds) == &thrd->state_node);
}

static void _kernel_block_timer_sync(void)
{
    if (!s_timer_ops.now || !s_timer_ops.set_timeout) return;

    int64_t delta = EK_OS_FOREVER;
    if (!ek_list_is_empty(&s_block_thrds))
    {
        os_thread_t *head = EK_CONTAINER_OF(ek_list_get_first(&s_block_thrds), os_thread_t, state_node);
        int64_t now = (int64_t)s_timer_ops.now();
        delta = (head->deadline > now) ? (head->deadline - now) : 0;
    }
    s_timer_ops.set_timeout(delta);
}

void kernel_thread_suspend(os_thread_t *thrd)
{
    uint32_t flags = ek_os_kernel_spin_lock();
    if (thrd->state == EK_OS_THREAD_DEAD || thrd->state == EK_OS_THREAD_UNUSED || thrd->state == EK_OS_THREAD_SUSPEND)
    {
        ek_os_kernel_spin_unlock(flags);
        return;
    }

    bool was_block_head = _kernel_is_block_head(thrd);
    if (thrd->state == EK_OS_THREAD_BLOCK)
    {
        thrd->deadline = EK_OS_FOREVER;
        thrd->wait_err = -EINTR;
    }

    thrd->state = EK_OS_THREAD_SUSPEND;
    ek_list_remove(&thrd->state_node);
    ek_list_remove(&thrd->wait_node);
    ek_list_insert_tail(&s_suspend_thrds, &thrd->state_node);
    _kernel_clear_ready_bit(thrd->prio);
    if (was_block_head) _kernel_block_timer_sync();
    ek_os_kernel_spin_unlock(flags);
}

void kernel_thread_resume(os_thread_t *thrd)
{
    uint32_t flags = ek_os_kernel_spin_lock();
    if (thrd->state == EK_OS_THREAD_SUSPEND)
    {
        kernel_thread_ready(thrd);
    }
    ek_os_kernel_spin_unlock(flags);
}

void kernel_thread_set_prio(os_thread_t *thrd, uint8_t prio)
{
    uint32_t flags = ek_os_kernel_spin_lock();
    if (thrd->state == EK_OS_THREAD_DEAD || thrd->state == EK_OS_THREAD_UNUSED)
    {
        ek_os_kernel_spin_unlock(flags);
        return;
    }

    uint8_t new_prio = EK_MIN(prio, (uint8_t)EKCFG_OS_THREAD_MIN_PRIO);
    if (thrd->prio == new_prio)
    {
        ek_os_kernel_spin_unlock(flags);
        return;
    }

    uint8_t old_prio = thrd->prio;
    thrd->prio = new_prio;
    if (thrd->state == EK_OS_THREAD_READY)
    {
        ek_list_remove(&thrd->state_node);
        _kernel_clear_ready_bit(old_prio);
        kernel_thread_ready(thrd);
    }
    ek_os_kernel_spin_unlock(flags);
}

void kernel_thread_block(os_thread_t *thrd, int64_t timeout)
{
    uint32_t flags = ek_os_kernel_spin_lock();

    thrd->state = EK_OS_THREAD_BLOCK;
    ek_list_remove(&thrd->state_node);
    _kernel_clear_ready_bit(thrd->prio);

    if (timeout < 0 || !s_timer_ops.now || !s_timer_ops.set_timeout)
    {
        thrd->deadline = EK_OS_FOREVER;
        ek_os_kernel_spin_unlock(flags);
        return;
    }

    int64_t now = (int64_t)s_timer_ops.now();
    thrd->deadline = (timeout > INT64_MAX - now) ? INT64_MAX : (now + timeout);

    ek_list_node_t *pnode;
    ek_list_foreach(pnode, &s_block_thrds)
    {
        os_thread_t *pthrd = EK_CONTAINER_OF(pnode, os_thread_t, state_node);
        if (thrd->deadline < pthrd->deadline) break;
    }

    ek_list_insert_before(pnode, &thrd->state_node);

    if (ek_list_get_first(&s_block_thrds) == &thrd->state_node)
    {
        s_timer_ops.set_timeout(thrd->deadline - now);
    }

    ek_os_kernel_spin_unlock(flags);
}

void kernel_thread_unblock(os_thread_t *thrd, int err)
{
    uint32_t flags = ek_os_kernel_spin_lock();
    if (thrd->state != EK_OS_THREAD_BLOCK)
    {
        ek_os_kernel_spin_unlock(flags);
        return;
    }

    bool was_head = _kernel_is_block_head(thrd);

    ek_list_remove(&thrd->wait_node);
    thrd->deadline = EK_OS_FOREVER;
    thrd->wait_err = err;
    kernel_thread_ready(thrd);

    if (was_head)
    {
        _kernel_block_timer_sync();
    }

    ek_os_kernel_spin_unlock(flags);
}

int kernel_wait_on_locked(ek_list_node_t *wait_list, int64_t timeout, uint32_t irq_flags)
{
    os_thread_t *cur = g_cur_thrd;
    if (!cur || kernel_is_idle_thread(cur))
    {
        ek_os_kernel_spin_unlock(irq_flags);
        return -EPERM;
    }

    cur->wait_err = 0;

    ek_list_node_t *pnode = NULL;
    ek_list_foreach(pnode, wait_list)
    {
        os_thread_t *pthrd = EK_CONTAINER_OF(pnode, os_thread_t, wait_node);
        if (cur->prio < pthrd->prio) break;
    }
    ek_list_insert_before(pnode, &cur->wait_node);

    kernel_thread_block(cur, timeout);
    kernel_schedule();
    ek_os_kernel_spin_unlock(irq_flags);
    ek_arch_isb();

    return cur->wait_err;
}

bool kernel_wake_one(ek_list_node_t *wait_list, int err)
{
    uint32_t flags = ek_os_kernel_spin_lock();
    if (ek_list_is_empty(wait_list))
    {
        ek_os_kernel_spin_unlock(flags);
        return false;
    }

    os_thread_t *thrd = EK_CONTAINER_OF(ek_list_get_first(wait_list), os_thread_t, wait_node);
    kernel_thread_unblock(thrd, err);
    ek_os_kernel_spin_unlock(flags);
    return true;
}

bool kernel_wake_all(ek_list_node_t *wait_list, int err)
{
    uint32_t flags = ek_os_kernel_spin_lock();
    bool woke = false;
    ek_list_node_t *pos = NULL;
    ek_list_node_t *n = NULL;

    ek_list_foreach_safe(pos, n, wait_list)
    {
        os_thread_t *thrd = EK_CONTAINER_OF(pos, os_thread_t, wait_node);
        kernel_thread_unblock(thrd, err);
        woke = true;
    }

    ek_os_kernel_spin_unlock(flags);
    return woke;
}

static os_thread_t *_kernel_get_next_thread(void)
{
    uint32_t flags = ek_os_kernel_spin_lock();
    if (s_ready_bitmap == 0)
    {
        os_thread_t *fallback = s_idle_thrd ? s_idle_thrd : g_cur_thrd;
        ek_os_kernel_spin_unlock(flags);
        return fallback;
    }

    int prio = __builtin_ctz(s_ready_bitmap);
    ek_list_node_t *node = ek_list_get_first(&s_ready_thrds[prio]);
    os_thread_t *thrd = EK_CONTAINER_OF(node, os_thread_t, state_node);
    kernel_thread_run(thrd);
    ek_os_kernel_spin_unlock(flags);

    return thrd;
}

void kernel_schedule(void)
{
    uint32_t flags = ek_os_kernel_spin_lock();
    if (!g_cur_thrd)
    {
        ek_os_kernel_spin_unlock(flags);
        return;
    }

    os_thread_t *cur = g_next_thrd ? g_next_thrd : g_cur_thrd;
    if (cur->state == EK_OS_THREAD_RUNNING)
    {
        if (s_ready_bitmap != 0)
        {
            if (cur->sched_lock > 0U)
            {
                ek_os_kernel_spin_unlock(flags);
                return;
            }

            int highest_prio = __builtin_ctz(s_ready_bitmap);
            if ((uint8_t)highest_prio < cur->prio)
            {
                kernel_thread_ready(cur);
                g_next_thrd = _kernel_get_next_thread();
                ek_arch_switch_yield();
            }
        }
        ek_os_kernel_spin_unlock(flags);
        return;
    }

    g_next_thrd = _kernel_get_next_thread();
    if (g_next_thrd != g_cur_thrd)
    {
        ek_arch_switch_yield();
    }
    ek_os_kernel_spin_unlock(flags);
}

static int _kernel_idle_thread(void *arg)
{
    __EK_UNUSED(arg);
    for (;;)
    {
        ek_arch_idle();
    }

    __builtin_unreachable();
}

static uint8_t _kernel_idle_stack[EKCFG_OS_IDLE_THREAD_STACK + sizeof(os_thread_t)] __EK_ALIGNED(8);

__EK_NO_RETURN void ek_os_kernel_start(const ek_os_timer_ops_t *ops)
{
    if (!ops || !ops->now || !ops->set_timeout) abort();

    s_timer_ops = *ops;

    int tid = ek_os_thread_create_static("kernel idle",
                                         _kernel_idle_thread,
                                         NULL,
                                         EKCFG_OS_THREAD_MIN_PRIO,
                                         _kernel_idle_stack,
                                         sizeof(_kernel_idle_stack));

    if (tid > 0)
    {
        uint32_t flags = ek_os_kernel_spin_lock();
        s_idle_thrd = kernel_tid_to_thread(tid);
        g_cur_thrd = _kernel_get_next_thread();
        g_next_thrd = g_cur_thrd;
        ek_os_kernel_spin_unlock(flags);
        ek_arch_sched_start();

        __builtin_unreachable();
    }

    abort();
}

uint64_t ek_os_kernel_time(void)
{
    return s_timer_ops.now ? s_timer_ops.now() : 0;
}

void ek_os_kernel_announce_isr(void)
{
    if (!s_timer_ops.now || !s_timer_ops.set_timeout) return;

    uint32_t flags = ek_os_kernel_spin_lock();
    int64_t now = (int64_t)s_timer_ops.now();
    bool woke = false;

    ek_list_node_t *pos = NULL;
    ek_list_node_t *n = NULL;
    ek_list_foreach_safe(pos, n, &s_block_thrds)
    {
        os_thread_t *thrd = EK_CONTAINER_OF(pos, os_thread_t, state_node);
        if (thrd->deadline > now) break;

        ek_list_remove(&thrd->state_node);
        ek_list_remove(&thrd->wait_node);
        thrd->deadline = EK_OS_FOREVER;
        thrd->wait_err = -ETIMEDOUT;
        kernel_thread_ready(thrd);
        woke = true;
    }

    if (ek_list_is_empty(&s_block_thrds))
    {
        s_timer_ops.set_timeout(-1);
    }
    else
    {
        os_thread_t *head = EK_CONTAINER_OF(ek_list_get_first(&s_block_thrds), os_thread_t, state_node);
        s_timer_ops.set_timeout(head->deadline - now);
    }

    if (woke)
    {
        kernel_schedule();
    }
    ek_os_kernel_spin_unlock(flags);
}

int ek_os_kernel_current_tid(void)
{
    return g_cur_thrd ? g_cur_thrd->tid : -ESRCH;
}

int ek_os_kernel_sched_lock(void)
{
    if (ek_arch_in_isr()) return -EPERM;

    uint32_t flags = ek_os_kernel_spin_lock();

    os_thread_t *cur = g_cur_thrd;
    if (!cur)
    {
        ek_os_kernel_spin_unlock(flags);
        return -ESRCH;
    }

    if (cur->sched_lock < UINT8_MAX) cur->sched_lock++;
    int depth = cur->sched_lock;

    ek_os_kernel_spin_unlock(flags);

    return depth;
}

int ek_os_kernel_sched_unlock(void)
{
    if (ek_arch_in_isr()) return -EPERM;

    uint32_t flags = ek_os_kernel_spin_lock();

    os_thread_t *cur = g_cur_thrd;
    if (!cur)
    {
        ek_os_kernel_spin_unlock(flags);
        return -ESRCH;
    }

    if (!cur->sched_lock)
    {
        ek_os_kernel_spin_unlock(flags);
        return 0;
    }

    cur->sched_lock--;
    int depth = cur->sched_lock;

    ek_os_kernel_spin_unlock(flags);

    if (!depth) kernel_schedule();
    return depth;
}

#endif /* EKCFG_RTOS == 1 */
