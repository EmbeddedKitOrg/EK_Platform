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
#    include "ek/core/assert.h"

#    define TID_SLOT_BITS       6
#    define TID_SLOT_MASK       ((1U << TID_SLOT_BITS) - 1U)
#    define MAKE_TID(gen, slot) ((int)(((uint32_t)(gen) << TID_SLOT_BITS) | (uint32_t)(slot)))
#    define TID_SLOT(tid)       ((uint32_t)(tid) & TID_SLOT_MASK)

static_assert(EKCFG_OS_THREADS_MAX <= (1U << TID_SLOT_BITS), "EKCFG_OS_THREADS_MAX exceeds TID_SLOT_BITS capacity");
static_assert(EKCFG_OS_THREAD_MIN_PRIO < 32, "EKCFG_OS_THREAD_MIN_PRIO must be < 32");
static_assert(offsetof(os_thread_t, arch.sp) == 0, "arch.sp must be at offset 0");

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
    ek_os_thread_exit(NULL);
}

int kernel_alloc_thread_locked(os_thread_t *thrd)
{
    if (!thrd) return -EINVAL;

    for (int i = 0; i < EKCFG_OS_THREADS_MAX; i++)
    {
        if (!s_all_threads[i].thrd)
        {
            s_all_threads[i].thrd = thrd;
            if (++s_all_threads[i].gen == 0) s_all_threads[i].gen = 1;
            int tid = MAKE_TID(s_all_threads[i].gen, i);
            thrd->tid = tid;
            return tid;
        }
    }
    return -ENOSPC;
}

void kernel_free_thread_locked(int tid)
{
    if (tid <= 0) return;

    uint32_t slot = TID_SLOT(tid);
    if (slot >= EKCFG_OS_THREADS_MAX) return;

    os_thread_t *thrd = s_all_threads[slot].thrd;
    if (thrd && thrd->tid == tid)
    {
        s_all_threads[slot].thrd = NULL;
    }
}

os_thread_t *kernel_tid_to_thread_locked(int tid)
{
    if (tid <= 0) return NULL;

    uint32_t slot = TID_SLOT(tid);
    if (slot >= EKCFG_OS_THREADS_MAX) return NULL;

    os_thread_t *thrd = s_all_threads[slot].thrd;
    if (!thrd || thrd->tid != tid)
    {
        return NULL;
    }

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

static inline void _clear_ready_bit(uint8_t prio)
{
    if (s_ready_thrds[prio].next && ek_list_is_empty(&s_ready_thrds[prio]))
    {
        s_ready_bitmap &= ~(1UL << prio);
    }
}

void kernel_thread_dead_locked(os_thread_t *thrd, void *retval)
{
    if (thrd->sched.state == EK_OS_THREAD_DEAD || thrd->sched.state == EK_OS_THREAD_UNUSED) return;

    thrd->retval = retval;
    thrd->sched.state = EK_OS_THREAD_DEAD;
    thrd->lock.waiting_lock = NULL;
    ek_list_remove(&thrd->sched.state_node);
    ek_list_remove(&thrd->wait.node);
    _clear_ready_bit(thrd->sched.prio);

    ek_list_node_t *pos = NULL;
    ek_list_node_t *n = NULL;
    ek_list_foreach_safe(pos, n, &thrd->lock.owned_list)
    {
        ek_os_mutex_t *m = EK_CONTAINER_OF(pos, ek_os_mutex_t, owner_node);
        ek_list_remove(&m->owner_node);
        m->owner = NULL;
        m->lock_count = 0U;
        kernel_wake_all_locked(&m->wait_list, -ECANCELED);
    }

    kernel_wake_all_locked(&thrd->join_wait, 0);
}

void kernel_thread_ready_locked(os_thread_t *thrd)
{
    if (thrd->sched.state == EK_OS_THREAD_DEAD) return;

    if (!s_ready_thrds[thrd->sched.prio].next) ek_list_init(&s_ready_thrds[thrd->sched.prio]);
    thrd->sched.state = EK_OS_THREAD_READY;
    ek_list_remove(&thrd->sched.state_node);
    ek_list_insert_tail(&s_ready_thrds[thrd->sched.prio], &thrd->sched.state_node);
    s_ready_bitmap |= (1UL << thrd->sched.prio);
}

void kernel_thread_run_locked(os_thread_t *thrd)
{
    thrd->sched.state = EK_OS_THREAD_RUNNING;
    ek_list_remove(&thrd->sched.state_node);
    _clear_ready_bit(thrd->sched.prio);
}

static inline bool _is_block_head(const os_thread_t *thrd)
{
    return (thrd->sched.state == EK_OS_THREAD_BLOCK) && !ek_list_is_empty(&s_block_thrds) &&
           (ek_list_get_first(&s_block_thrds) == &thrd->sched.state_node);
}

static void _block_timer_sync(void)
{
    if (!s_timer_ops.now || !s_timer_ops.set_timeout) return;

    int64_t delta = EK_OS_FOREVER;
    if (!ek_list_is_empty(&s_block_thrds))
    {
        os_thread_t *head = EK_CONTAINER_OF(ek_list_get_first(&s_block_thrds), os_thread_t, sched.state_node);
        int64_t now = (int64_t)s_timer_ops.now();
        delta = (head->wait.deadline > now) ? (head->wait.deadline - now) : 0;
    }
    s_timer_ops.set_timeout(delta);
}

void kernel_thread_suspend_locked(os_thread_t *thrd)
{
    if (thrd->sched.state == EK_OS_THREAD_DEAD || thrd->sched.state == EK_OS_THREAD_UNUSED ||
        thrd->sched.state == EK_OS_THREAD_SUSPEND)
    {
        return;
    }

    bool was_block_head = _is_block_head(thrd);
    if (thrd->sched.state == EK_OS_THREAD_BLOCK)
    {
        thrd->wait.deadline = EK_OS_FOREVER;
        thrd->wait.err = -EINTR;
    }

    ek_os_mutex_t *wlock = (ek_os_mutex_t *)thrd->lock.waiting_lock;
    thrd->sched.state = EK_OS_THREAD_SUSPEND;
    thrd->lock.waiting_lock = NULL;
    ek_list_remove(&thrd->sched.state_node);
    ek_list_remove(&thrd->wait.node);
    ek_list_insert_tail(&s_suspend_thrds, &thrd->sched.state_node);
    _clear_ready_bit(thrd->sched.prio);

    if (wlock && wlock->owner) kernel_thread_recalc_prio_locked((os_thread_t *)wlock->owner);
    if (was_block_head) _block_timer_sync();
}

void kernel_thread_resume_locked(os_thread_t *thrd)
{
    if (thrd->sched.state == EK_OS_THREAD_SUSPEND)
    {
        kernel_thread_ready_locked(thrd);
    }
}

void kernel_thread_set_prio_locked(os_thread_t *thrd, uint8_t prio)
{
    if (thrd->sched.state == EK_OS_THREAD_DEAD || thrd->sched.state == EK_OS_THREAD_UNUSED) return;

    uint8_t new_prio = EK_MIN(prio, (uint8_t)EKCFG_OS_THREAD_MIN_PRIO);
    if (thrd->sched.prio == new_prio) return;

    uint8_t old_prio = thrd->sched.prio;
    thrd->sched.prio = new_prio;
    if (thrd->sched.state == EK_OS_THREAD_READY)
    {
        ek_list_remove(&thrd->sched.state_node);
        _clear_ready_bit(old_prio);
        kernel_thread_ready_locked(thrd);
    }
}

static void _requeue_wait_list(ek_list_node_t *wait_list, os_thread_t *thrd)
{
    ek_list_remove(&thrd->wait.node);
    ek_list_node_t *pnode = NULL;
    ek_list_foreach(pnode, wait_list)
    {
        os_thread_t *pthrd = EK_CONTAINER_OF(pnode, os_thread_t, wait.node);
        if (thrd->sched.prio < pthrd->sched.prio) break;
    }
    ek_list_insert_before(pnode, &thrd->wait.node);
}

void kernel_thread_recalc_prio_locked(os_thread_t *thrd)
{
    for (int depth = 0; thrd && depth < EKCFG_OS_THREADS_MAX; depth++)
    {
        if (thrd->sched.state == EK_OS_THREAD_DEAD || thrd->sched.state == EK_OS_THREAD_UNUSED) break;

        uint8_t target = thrd->sched.base_prio;
        ek_list_node_t *node = NULL;
        ek_list_foreach(node, &thrd->lock.owned_list)
        {
            ek_os_mutex_t *m = EK_CONTAINER_OF(node, ek_os_mutex_t, owner_node);
            if (!ek_list_is_empty(&m->wait_list))
            {
                os_thread_t *top = EK_CONTAINER_OF(ek_list_get_first(&m->wait_list), os_thread_t, wait.node);
                if (top->sched.prio < target) target = top->sched.prio;
            }
        }

        if (target == thrd->sched.prio) break;

        kernel_thread_set_prio_locked(thrd, target);

        ek_os_mutex_t *wlock = (ek_os_mutex_t *)thrd->lock.waiting_lock;
        if (!wlock) break;

        _requeue_wait_list(&wlock->wait_list, thrd);
        thrd = (os_thread_t *)wlock->owner;
    }
}

void kernel_thread_block_locked(os_thread_t *thrd, int64_t timeout)
{
    thrd->sched.state = EK_OS_THREAD_BLOCK;
    ek_list_remove(&thrd->sched.state_node);
    _clear_ready_bit(thrd->sched.prio);

    if (timeout < 0 || !s_timer_ops.now || !s_timer_ops.set_timeout)
    {
        thrd->wait.deadline = EK_OS_FOREVER;
        return;
    }

    int64_t now = (int64_t)s_timer_ops.now();
    thrd->wait.deadline = (timeout > INT64_MAX - now) ? INT64_MAX : (now + timeout);

    ek_list_node_t *pnode;
    ek_list_foreach(pnode, &s_block_thrds)
    {
        os_thread_t *pthrd = EK_CONTAINER_OF(pnode, os_thread_t, sched.state_node);
        if (thrd->wait.deadline < pthrd->wait.deadline) break;
    }

    ek_list_insert_before(pnode, &thrd->sched.state_node);

    if (ek_list_get_first(&s_block_thrds) == &thrd->sched.state_node)
    {
        s_timer_ops.set_timeout(thrd->wait.deadline - now);
    }
}

void kernel_thread_unblock_locked(os_thread_t *thrd, int err)
{
    if (thrd->sched.state != EK_OS_THREAD_BLOCK) return;

    bool was_head = _is_block_head(thrd);

    ek_list_remove(&thrd->wait.node);
    thrd->lock.waiting_lock = NULL;
    thrd->wait.deadline = EK_OS_FOREVER;
    thrd->wait.err = err;
    kernel_thread_ready_locked(thrd);

    if (was_head)
    {
        _block_timer_sync();
    }
}

void kernel_wait_enqueue_locked(ek_list_node_t *wait_list)
{
    os_thread_t *cur = g_cur_thrd;
    ek_list_node_t *pnode = NULL;
    ek_list_foreach(pnode, wait_list)
    {
        os_thread_t *pthrd = EK_CONTAINER_OF(pnode, os_thread_t, wait.node);
        if (cur->sched.prio < pthrd->sched.prio) break;
    }
    ek_list_insert_before(pnode, &cur->wait.node);
}

int kernel_wait_on_locked(ek_list_node_t *wait_list, int64_t timeout, uint32_t irq_flags)
{
    os_thread_t *cur = g_cur_thrd;
    if (!cur || kernel_is_idle_thread(cur))
    {
        ek_os_kernel_spin_unlock(irq_flags);
        return -EPERM;
    }

    cur->wait.err = 0;
    kernel_wait_enqueue_locked(wait_list);

    kernel_thread_block_locked(cur, timeout);
    kernel_schedule_locked();
    ek_os_kernel_spin_unlock(irq_flags);
    ek_arch_isb();

    return cur->wait.err;
}

bool kernel_wake_one_locked(ek_list_node_t *wait_list, int err)
{
    if (ek_list_is_empty(wait_list)) return false;

    os_thread_t *thrd = EK_CONTAINER_OF(ek_list_get_first(wait_list), os_thread_t, wait.node);
    kernel_thread_unblock_locked(thrd, err);
    return true;
}

bool kernel_wake_all_locked(ek_list_node_t *wait_list, int err)
{
    bool woke = false;
    ek_list_node_t *pos = NULL;
    ek_list_node_t *n = NULL;

    ek_list_foreach_safe(pos, n, wait_list)
    {
        os_thread_t *thrd = EK_CONTAINER_OF(pos, os_thread_t, wait.node);
        kernel_thread_unblock_locked(thrd, err);
        woke = true;
    }

    return woke;
}

void kernel_wait_morph_to_mutex_locked(os_thread_t *thrd, ek_os_mutex_t *mutex)
{
    // 已收到条件信号进入无限等锁阶段，必须先解除原条件等待的超时定时器
    if (thrd->wait.deadline != EK_OS_FOREVER)
    {
        bool was_block_head = _is_block_head(thrd);
        ek_list_remove(&thrd->sched.state_node);
        thrd->wait.deadline = EK_OS_FOREVER;
        if (was_block_head) _block_timer_sync();
    }

    thrd->wait.data = NULL;
    thrd->wait.err = 0;
    thrd->lock.waiting_lock = mutex;

    _requeue_wait_list(&mutex->wait_list, thrd);
    if (mutex->owner)
    {
        kernel_thread_recalc_prio_locked((os_thread_t *)mutex->owner);
    }
}

static os_thread_t *_get_next_thread_locked(void)
{
    if (s_ready_bitmap == 0)
    {
        return s_idle_thrd ? s_idle_thrd : g_cur_thrd;
    }

    int prio = __builtin_ctz(s_ready_bitmap);
    ek_list_node_t *node = ek_list_get_first(&s_ready_thrds[prio]);
    os_thread_t *thrd = EK_CONTAINER_OF(node, os_thread_t, sched.state_node);
    kernel_thread_run_locked(thrd);

    return thrd;
}

void kernel_schedule_locked(void)
{
    if (!g_cur_thrd) return;

    os_thread_t *cur = g_next_thrd ? g_next_thrd : g_cur_thrd;
    if (cur->sched.state == EK_OS_THREAD_RUNNING)
    {
        if (s_ready_bitmap && !cur->sched.sched_lock)
        {
            int highest_prio = __builtin_ctz(s_ready_bitmap);
            if ((uint8_t)highest_prio < cur->sched.prio)
            {
                kernel_thread_ready_locked(cur);
                g_next_thrd = _get_next_thread_locked();
                ek_arch_switch_yield();
            }
        }
        return;
    }

    g_next_thrd = _get_next_thread_locked();
    if (g_next_thrd != g_cur_thrd)
    {
        ek_arch_switch_yield();
    }
}

void kernel_thread_yield_locked(void)
{
    os_thread_t *cur = g_next_thrd ? g_next_thrd : g_cur_thrd;
    if (!cur || cur->sched.state != EK_OS_THREAD_RUNNING || cur->sched.sched_lock) return;

    /* 同优先级无人竞争时无需轮转，避免无谓的 PendSV 往返 */
    ek_list_node_t *head = &s_ready_thrds[cur->sched.prio];
    if (!head->next || ek_list_is_empty(head)) return;

    kernel_thread_ready_locked(cur);
    g_next_thrd = _get_next_thread_locked();
    ek_arch_switch_yield();
}

#    if EKCFG_OS_KERNEL_STACKOVERFLOW_CHECK > 0

__EK_WEAK void ek_os_kernel_stackoverflow_hook(int tid, const char *name)
{
    __EK_UNUSED(tid);
    __EK_UNUSED(name);

    // 处于 PendSV 关中断上下文（MSP 栈），兜底 abort 确保断言关闭时也能停机
    ek_assert(0 && "stack overflow");
    abort();
}

void kernel_stackoverflow_check(os_thread_t *thrd)
{
    if (!thrd || !thrd->stack.base || !thrd->stack.size) return;

    uintptr_t base = (uintptr_t)thrd->stack.base;
    uintptr_t top = base + thrd->stack.size;
    uintptr_t sp = thrd->arch.sp;

    // 级别 1 与级别 2 通用：边界指针与对齐检查
    if (sp < base || sp > top || (sp & 0x3U))
    {
        ek_os_kernel_stackoverflow_hook(thrd->tid, thrd->name);
        return;
    }

#        if EKCFG_OS_KERNEL_STACKOVERFLOW_CHECK == 2
    // 级别 2：检查栈底 16 字节魔数哨兵，捕获切出前已返回的瞬时溢栈
    const uint32_t *guard = (const uint32_t *)EK_ALIGN_UP(base, sizeof(uint32_t));
    size_t max_words = (top > (uintptr_t)guard) ? ((top - (uintptr_t)guard) / sizeof(uint32_t)) : 0U;
    size_t n = EK_MIN((size_t)EK_OS_STACK_GUARD_WORDS, max_words);

    for (size_t i = 0U; i < n; i++)
    {
        if (guard[i] != EK_OS_STACK_MAGIC_WORD)
        {
            ek_os_kernel_stackoverflow_hook(thrd->tid, thrd->name);
            return;
        }
    }

    // 切出瞬间 O(1) 刷新当前栈深记录，完整峰值由查询 API 按需扫栈得出
    size_t used = (size_t)(top - sp);
    if (used > thrd->debug.high_water_mark)
    {
        thrd->debug.high_water_mark = used;
    }
#        endif
}

#    endif

__EK_WEAK void ek_os_kernel_idle_hook(void)
{
}

static void *_idle_thread(void *arg)
{
    __EK_UNUSED(arg);
    for (;;)
    {
        ek_os_kernel_idle_hook();
        ek_arch_idle();
    }

    __builtin_unreachable();
}

static uint8_t s_idle_stack[EKCFG_OS_IDLE_THREAD_STACK + sizeof(os_thread_t)] __EK_ALIGNED(8);

__EK_NO_RETURN void ek_os_kernel_start(const ek_os_timer_ops_t *ops)
{
    if (!ops || !ops->now || !ops->set_timeout) abort();

    s_timer_ops = *ops;

    int tid = ek_os_thread_create_static(
        "kernel idle", _idle_thread, NULL, EKCFG_OS_THREAD_MIN_PRIO, s_idle_stack, sizeof(s_idle_stack));

    if (tid > 0)
    {
        uint32_t flags = ek_os_kernel_spin_lock();
        s_idle_thrd = kernel_tid_to_thread_locked(tid);
        g_cur_thrd = _get_next_thread_locked();
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
        os_thread_t *thrd = EK_CONTAINER_OF(pos, os_thread_t, sched.state_node);
        if (thrd->wait.deadline > now) break;

        ek_os_mutex_t *wlock = (ek_os_mutex_t *)thrd->lock.waiting_lock;
        thrd->lock.waiting_lock = NULL;
        ek_list_remove(&thrd->sched.state_node);
        ek_list_remove(&thrd->wait.node);
        thrd->wait.deadline = EK_OS_FOREVER;
        thrd->wait.err = -ETIMEDOUT;
        kernel_thread_ready_locked(thrd);

        if (wlock && wlock->owner) kernel_thread_recalc_prio_locked((os_thread_t *)wlock->owner);
        woke = true;
    }

    if (ek_list_is_empty(&s_block_thrds))
    {
        s_timer_ops.set_timeout(-1);
    }
    else
    {
        os_thread_t *head = EK_CONTAINER_OF(ek_list_get_first(&s_block_thrds), os_thread_t, sched.state_node);
        s_timer_ops.set_timeout(head->wait.deadline - now);
    }

    if (woke) kernel_schedule_locked();
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

    if (cur->sched.sched_lock < UINT8_MAX) cur->sched.sched_lock++;
    int depth = cur->sched.sched_lock;

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

    if (!cur->sched.sched_lock)
    {
        ek_os_kernel_spin_unlock(flags);
        return 0;
    }

    cur->sched.sched_lock--;
    int depth = cur->sched.sched_lock;

    if (!depth) kernel_schedule_locked();
    ek_os_kernel_spin_unlock(flags);

    return depth;
}

#endif /* EKCFG_RTOS == 1 */
