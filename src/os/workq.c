#include "ek/os/workq.h"

#if EKCFG_RTOS == 1

#    include "os_priv.h"
#    include "ek/core/arch.h"
#    include "ek/core/assert.h"
#    include "ek/mem/heap.h"
#    include "ek/os/kernel.h"
#    include "ek/os/thread.h"

#    define MAGIC                 0x57524B51UL
#    define WORKQ_HDR             ((size_t)_EK_OS_WORKQ_STATIC_HDR_SIZE)
#    define WORKQ_FROM_BUF(buf)   ((ek_os_workq_t *)(uintptr_t)((uintptr_t)(buf) + WORKQ_HDR))
#    define WORKQ_GET_MAGIC(q)    (*(const uint32_t *)(uintptr_t)((uintptr_t)(q) - WORKQ_HDR))
#    define WORKQ_SET_MAGIC(q, v) (*(uint32_t *)(uintptr_t)((uintptr_t)(q) - WORKQ_HDR) = (uint32_t)(v))

static void *_workq_thread(void *arg)
{
    ek_os_workq_t *q = (ek_os_workq_t *)arg;
    ek_assert(q != NULL);

    for (;;)
    {
        uint32_t flags = ek_os_kernel_spin_lock();
        int64_t now = (int64_t)ek_os_kernel_time();

        // 将所有已到期的延时工作项按序搬运到就绪队列
        if (!ek_list_is_empty(&q->delay_list))
        {
            ek_list_node_t *pnode = NULL;
            ek_list_node_t *temp_node = NULL;

            ek_list_foreach_safe(pnode, temp_node, &q->delay_list)
            {
                ek_os_work_t *base = EK_CONTAINER_OF(pnode, ek_os_work_t, node);
                ek_os_work_delay_t *dwork = EK_CONTAINER_OF(base, ek_os_work_delay_t, work);

                if ((now - dwork->deadline) < 0) break;

                ek_list_remove(pnode);
                base->flag = (uint8_t)((base->flag & ~EK_OS_WORK_DELAYED) | EK_OS_WORK_PENDING);
                ek_list_insert_tail(&q->ready_list, pnode);
            }
        }

        if (!ek_list_is_empty(&q->ready_list))
        {
            ek_list_node_t *node = ek_list_pop_first(&q->ready_list);
            ek_os_work_t *work = EK_CONTAINER_OF(node, ek_os_work_t, node);
            ek_assert(work != NULL);

            q->cur_work = work;
            // 不往 work->flag 写入 RUNNING 位且保留 work->q：
            // 运行态由 q->cur_work == work 判定，避免回调自释放后 fn() 返回再次解引用 work
            // 同时便于 cancel_sync 通过 work->q 找到队列
            work->flag &= ~EK_OS_WORK_PENDING;

            ek_os_kernel_spin_unlock(flags);

            work->fn(work);

            flags = ek_os_kernel_spin_lock();
            q->cur_work = NULL;

            if (kernel_wake_all_locked(&q->sync_wait, 0))
            {
                kernel_schedule_locked();
            }

            ek_os_kernel_spin_unlock(flags);
            continue;
        }

        if (q->stop)
        {
            ek_os_kernel_spin_unlock(flags);
            break;
        }

        int64_t timeout = EK_OS_FOREVER;
        if (!ek_list_is_empty(&q->delay_list))
        {
            ek_os_work_t *base = EK_CONTAINER_OF(ek_list_get_first(&q->delay_list), ek_os_work_t, node);
            ek_os_work_delay_t *head = EK_CONTAINER_OF(base, ek_os_work_delay_t, work);
            timeout = (head->deadline > now) ? (head->deadline - now) : 0;
        }

        int err = kernel_wait_on_locked(&q->worker_wait, timeout, flags);

        if (err < 0)
        {
            // TODO worker 线程的错误处理
        }
    }

    return NULL;
}

static void _setup_workq(ek_os_workq_t *q, bool mem_owned)
{
    ek_list_init(&q->ready_list);
    ek_list_init(&q->delay_list);
    ek_list_init(&q->worker_wait);
    ek_list_init(&q->sync_wait);
    q->cur_work = NULL;
    q->worker_tid = -1;
    q->mem_owned = mem_owned;
    q->stop = false;
}
ek_os_workq_t *ek_os_workq_create(const char *name, uint8_t prio, size_t stack_size)
{
    if (ek_arch_in_isr()) return NULL;
    if (!name || stack_size < EKCFG_OS_THREAD_MIN_STACK) return NULL;

    ek_os_workq_t *q = ek_malloc(sizeof(*q));
    if (!q) return NULL;

    _setup_workq(q, true);

    int tid = ek_os_thread_create(name, _workq_thread, q, prio, stack_size);
    if (tid <= 0)
    {
        ek_free(q);
        return NULL;
    }

    uint32_t flags = ek_os_kernel_spin_lock();
    q->worker_tid = tid;
    ek_os_kernel_spin_unlock(flags);

    return q;
}

ek_os_workq_t *ek_os_workq_create_static(void *buf, size_t size, const char *name, uint8_t prio)
{
    if (ek_arch_in_isr()) return NULL;
    if (!buf || !name) return NULL;
    if (size < EK_OS_WORKQ_STATIC_BUF_SIZE(EKCFG_OS_THREAD_MIN_STACK)) return NULL;
    if (((uintptr_t)buf % EK_OS_WORKQ_STATIC_ALIGN) != 0U) return NULL;

    ek_os_workq_t *q = WORKQ_FROM_BUF(buf);
    _setup_workq(q, false);

    // 必须在创建工作线程前写入魔数，防止高优先级工作线程立即抢占时校验失效
    WORKQ_SET_MAGIC(q, MAGIC);

    void *thrd_buf = (void *)((uintptr_t)buf + _EK_OS_WORKQ_STATIC_CTRL_SIZE);
    size_t thrd_size = size - _EK_OS_WORKQ_STATIC_CTRL_SIZE;

    int tid = ek_os_thread_create_static(name, _workq_thread, q, prio, thrd_buf, thrd_size);
    if (tid <= 0)
    {
        WORKQ_SET_MAGIC(q, 0U);
        return NULL;
    }

    uint32_t flags = ek_os_kernel_spin_lock();
    q->worker_tid = tid;
    ek_os_kernel_spin_unlock(flags);

    return q;
}

static inline bool _workq_valid(const ek_os_workq_t *q)
{
    if (!q) return false;
    if (q->mem_owned) return true;
    return WORKQ_GET_MAGIC(q) == MAGIC;
}

void ek_os_workq_destroy(ek_os_workq_t *q)
{
    if (ek_arch_in_isr()) return;
    if (!_workq_valid(q)) return;

    uint32_t flags = ek_os_kernel_spin_lock();

    if (q->stop || (q->worker_tid > 0 && ek_os_kernel_current_tid() == q->worker_tid))
    {
        ek_os_kernel_spin_unlock(flags);
        return;
    }

    q->stop = true;
    int tid = q->worker_tid;
    bool owned = q->mem_owned;

    if (kernel_wake_one_locked(&q->worker_wait, 0))
    {
        kernel_schedule_locked();
    }

    ek_os_kernel_spin_unlock(flags);

    // 同步等待工作线程退出，由内核回收其栈内存与线程表槽位
    if (tid > 0) ek_os_thread_join(tid, EK_OS_FOREVER, NULL);

    flags = ek_os_kernel_spin_lock();
    q->worker_tid = -1;

    ek_list_node_t *pos = NULL;
    ek_list_node_t *n = NULL;
    ek_list_foreach_safe(pos, n, &q->delay_list)
    {
        ek_os_work_t *work = EK_CONTAINER_OF(pos, ek_os_work_t, node);
        ek_list_remove(pos);
        work->flag &= ~EK_OS_WORK_DELAYED;
        work->q = NULL;
    }

    if (kernel_wake_all_locked(&q->sync_wait, -ECANCELED))
    {
        kernel_schedule_locked();
    }

    if (!owned) WORKQ_SET_MAGIC(q, 0U);
    ek_os_kernel_spin_unlock(flags);

    if (owned) ek_free(q);
}

int ek_os_work_init(ek_os_work_t *w, ek_os_work_fn_t fn)
{
    if (!w) return -ENOENT;
    if (!fn) return -EINVAL;

    w->fn = fn;
    ek_list_init(&w->node);
    w->flag = 0;
    w->q = NULL;

    return 0;
}

int ek_os_work_delay_init(ek_os_work_delay_t *w, ek_os_work_fn_t fn)
{
    if (!w) return -ENOENT;
    if (!fn) return -EINVAL;

    w->work.fn = fn;
    ek_list_init(&w->work.node);
    w->work.flag = 0;
    w->work.q = NULL;
    w->deadline = 0;

    return 0;
}

static inline int _workq_ready_locked(const ek_os_workq_t *wq)
{
    if (!_workq_valid(wq)) return -ENOENT;
    if (wq->stop) return -ECANCELED;

    os_thread_t *thrd = kernel_tid_to_thread_locked(wq->worker_tid);
    if (!thrd) return -ENOENT;
    if (thrd->sched.state == EK_OS_THREAD_DEAD || thrd->sched.state == EK_OS_THREAD_SUSPEND)
    {
        return -EPERM;
    }

    return 0;
}

static inline void _submit_locked(ek_os_workq_t *wq, ek_os_work_t *w)
{
    w->flag |= EK_OS_WORK_PENDING;
    w->q = wq;
    ek_list_insert_tail(&wq->ready_list, &w->node);

    if (kernel_wake_one_locked(&wq->worker_wait, 0))
    {
        kernel_schedule_locked();
    }
}

static inline void _delay_insert_locked(ek_os_workq_t *wq, ek_os_work_delay_t *w, int64_t delay)
{
    int64_t ddl = (int64_t)ek_os_kernel_time() + delay;

    ek_list_node_t *pnode = NULL;
    ek_list_foreach(pnode, &wq->delay_list)
    {
        ek_os_work_t *base = EK_CONTAINER_OF(pnode, ek_os_work_t, node);
        ek_os_work_delay_t *dwork = EK_CONTAINER_OF(base, ek_os_work_delay_t, work);

        if ((ddl - dwork->deadline) < 0) break;
    }

    w->deadline = ddl;
    w->work.flag |= EK_OS_WORK_DELAYED;
    w->work.q = wq;
    ek_list_insert_before(pnode, &w->work.node);

    // 新节点抢占了延时队头时，唤醒睡眠中的工作线程重算最近超时
    if (ek_list_get_first(&wq->delay_list) == &w->work.node)
    {
        if (kernel_wake_one_locked(&wq->worker_wait, 0))
        {
            kernel_schedule_locked();
        }
    }
}

int ek_os_workq_submit(ek_os_workq_t *wq, ek_os_work_t *w)
{
    if (!wq || !w) return -ENOENT;
    if (!w->fn) return -EINVAL;

    uint32_t flags = ek_os_kernel_spin_lock();

    int err = _workq_ready_locked(wq);
    if (err < 0)
    {
        ek_os_kernel_spin_unlock(flags);
        return err;
    }

    if (w->flag & (EK_OS_WORK_PENDING | EK_OS_WORK_DELAYED | EK_OS_WORK_CANCELING))
    {
        ek_os_kernel_spin_unlock(flags);
        return -EBUSY;
    }

    _submit_locked(wq, w);

    ek_os_kernel_spin_unlock(flags);
    return 0;
}

int ek_os_workq_schedule(ek_os_workq_t *wq, ek_os_work_delay_t *w, int64_t delay)
{
    if (!wq || !w) return -ENOENT;
    if (!w->work.fn) return -EINVAL;

    uint32_t flags = ek_os_kernel_spin_lock();

    int err = _workq_ready_locked(wq);
    if (err < 0)
    {
        ek_os_kernel_spin_unlock(flags);
        return err;
    }

    if (w->work.flag & (EK_OS_WORK_PENDING | EK_OS_WORK_DELAYED | EK_OS_WORK_CANCELING))
    {
        ek_os_kernel_spin_unlock(flags);
        return -EBUSY;
    }

    if (delay <= 0) _submit_locked(wq, &w->work);
    else _delay_insert_locked(wq, w, delay);

    ek_os_kernel_spin_unlock(flags);
    return 0;
}

int ek_os_workq_reschedule(ek_os_workq_t *wq, ek_os_work_delay_t *w, int64_t delay)
{
    if (!wq || !w) return -ENOENT;
    if (!w->work.fn) return -EINVAL;

    uint32_t flags = ek_os_kernel_spin_lock();

    int err = _workq_ready_locked(wq);
    if (err < 0)
    {
        ek_os_kernel_spin_unlock(flags);
        return err;
    }

    if (w->work.flag & (EK_OS_WORK_PENDING | EK_OS_WORK_CANCELING))
    {
        ek_os_kernel_spin_unlock(flags);
        return -EBUSY;
    }

    if (w->work.flag & EK_OS_WORK_DELAYED)
    {
        if (w->work.q != wq)
        {
            ek_os_kernel_spin_unlock(flags);
            return -EINVAL;
        }
        ek_list_remove(&w->work.node);
        w->work.flag &= ~EK_OS_WORK_DELAYED;
    }

    if (delay <= 0) _submit_locked(wq, &w->work);
    else _delay_insert_locked(wq, w, delay);

    ek_os_kernel_spin_unlock(flags);
    return 0;
}

#endif /* EKCFG_RTOS == 1 */
