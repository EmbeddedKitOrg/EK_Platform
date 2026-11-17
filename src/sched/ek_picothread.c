#include "ek/sched/ek_picothread.h"
#if EKCFG_PICOTHREAD == 1

#    include "ek/mem/ek_heap.h"
#    include "ek/core/ek_assert.h"
#    include "ek/core/ek_export.h"

#    define PT_PRIO_LEVELS ((uint8_t)(EKCFG_PT_PRIO_LOWEST + 1U))

static volatile bool s_init = false;
static ek_list_node_t s_ready_list[PT_PRIO_LEVELS];
static uint32_t s_ready_mask;
static ek_list_node_t s_block_list;
static ek_pt_t *s_cur_pt;
static uint32_t s_schedule_now;

static void _ready_enqueue(ek_pt_t *pt);
static void _ready_dequeue(ek_pt_t *pt);
static void _insert_block_by_time(ek_pt_t *pt);
static ek_pt_t *_pt_next_ready(void);
static void _pt_setup(ek_pt_t *pt, ek_pt_cb_t cb, uint8_t prio, void *arg);
static void _pt_detach_state(ek_pt_t *pt);
static void _pt_teardown(ek_pt_t *pt);
#    if EKCFG_PICOTHREAD_SEM || EKCFG_PICOTHREAD_MSG
static void _insert_event_by_prio(ek_list_node_t *list, ek_pt_t *pt);
static void _pt_cancel_wait(ek_pt_t *pt, int result);
static void _pt_wake(ek_pt_t *pt, int result);
static void _pt_wake_all(ek_list_node_t *wait_list, int result);
static void _pt_set_wait_error(int err);
#    endif

void ek_pt_init(void)
{
    if (s_init == true)
    {
        return;
    }
    s_init = true;
    s_ready_mask = 0U;
    s_cur_pt = NULL;
    for (uint8_t i = 0; i < PT_PRIO_LEVELS; i++) ek_list_init(&s_ready_list[i]);
    ek_list_init(&s_block_list);
}

EK_EXPORT_COMPONENTS(ek_pt_init, 0);

int ek_pt_init_static(ek_pt_t *pt, ek_pt_cb_t cb, uint8_t prio, void *arg)
{
    if (pt == NULL || cb == NULL || s_init == false || prio > EKCFG_PT_PRIO_LOWEST) return -EINVAL;

    _pt_setup(pt, cb, prio, arg);
    return 0;
}

void ek_pt_deinit_static(ek_pt_t *pt)
{
    if (pt == NULL || pt == s_cur_pt) return;
    _pt_teardown(pt);
}

ek_pt_t *ek_pt_create(ek_pt_cb_t cb, uint8_t prio, void *arg)
{
    ek_assert_param(s_init == true);
    if (cb == NULL || s_init == false || prio > EKCFG_PT_PRIO_LOWEST) return NULL;
    ek_pt_t *pt = ek_malloc(sizeof(*pt));
    if (pt == NULL) return NULL;

    _pt_setup(pt, cb, prio, arg);
    return pt;
}

void ek_pt_destroy(ek_pt_t *pt)
{
    if (pt == NULL || pt == s_cur_pt) return;

    _pt_teardown(pt);
    ek_free(pt);
}

uint32_t ek_pt_schedule(uint32_t now)
{
    ek_assert_param(s_init == true);

    s_schedule_now = now;

    // 将阻塞链表中到期任务移回就绪链表（链表按唤醒 tick 升序，回绕安全比较）
    ek_list_node_t *pos, *n;
    ek_list_foreach_safe(pos, n, &s_block_list)
    {
        ek_pt_t *pt = EK_CONTAINER_OF(pos, ek_pt_t, state_node);
        if (EK_TICK_AFTER(pt->tick, now))
        {
            break;
        }
        ek_list_remove(pos);
#    if EKCFG_PICOTHREAD_SEM || EKCFG_PICOTHREAD_MSG
        // 超时退出：从事件等待队列摘除并标记超时（普通延时任务不受影响）
        _pt_cancel_wait(pt, -ETIMEDOUT);
#    endif
        pt->state = EK_PT_STATE_READY;
        _ready_enqueue(pt);
    }

    // 取下一个就绪任务执行
    ek_pt_t *next = _pt_next_ready();
    if (next != NULL)
    {
        s_cur_pt = next;
        _ready_dequeue(next);
        next->state = EK_PT_STATE_RUNNING;
        next->cb(next, next->arg);

        // 回调后根据状态重新分流
        if (next->state == EK_PT_STATE_RUNNING)
        {
            next->state = EK_PT_STATE_READY;
            _ready_enqueue(next);
        }
        else if (next->state == EK_PT_STATE_BLOCK && !next->forever)
        {
            _insert_block_by_time(next);
        }
        // SUSPEND / 永久阻塞：不挂到任何调度链表
    }
    // 回调结束后清空当前任务，主循环里才能正常销毁刚运行过的任务
    s_cur_pt = NULL;

    // 有就绪任务则返回当前 tick，主循环不应睡眠（避开表示"无限睡眠"的 0）
    if (s_ready_mask != 0U) return (now == 0U) ? 1U : now;

    // 返回阻塞链表最早唤醒 tick；空则 0 表示可无限休眠
    if (ek_list_is_empty(&s_block_list)) return 0U;
    ek_pt_t *first_blocked = EK_CONTAINER_OF(ek_list_get_first(&s_block_list), ek_pt_t, state_node);
    return (first_blocked->tick == 0U) ? 0xFFFFFFFFUL : first_blocked->tick;
}

void ek_pt_block(ek_pt_t *pt, uint32_t xtick)
{
    ek_assert_param(s_init == true);
    if (pt == NULL) return;

    const bool forever = (xtick == EK_PT_WAIT_FOREVER);

    if (pt == s_cur_pt)
    {
        // 当前任务：回调返回后由 ek_pt_schedule 挂到阻塞链表
        pt->tick = s_schedule_now + xtick;
        pt->forever = forever ? 1U : 0U;
        pt->state = EK_PT_STATE_BLOCK;
        return;
    }

    _pt_detach_state(pt);
    pt->tick = s_schedule_now + xtick;
    pt->forever = forever ? 1U : 0U;
    pt->state = EK_PT_STATE_BLOCK;
    if (!forever) _insert_block_by_time(pt);
}

ek_pt_t *ek_pt_suspend(ek_pt_t *pt)
{
    // 传入 NULL 表示挂起自己
    ek_pt_t *target = (pt == NULL) ? s_cur_pt : pt;

    ek_assert_param(s_init == true);
    if (target == NULL) return NULL;

#    if EKCFG_PICOTHREAD_SEM || EKCFG_PICOTHREAD_MSG
    // 挂起会打断正在进行的事件等待，否则事件到来时会唤醒一个已挂起的任务
    _pt_cancel_wait(target, -EINTR);
#    endif

    if (target == s_cur_pt)
    {
        target->forever = 0U;
        target->state = EK_PT_STATE_SUSPEND;
        return target;
    }

    _pt_detach_state(target);
    target->state = EK_PT_STATE_SUSPEND;
    return NULL;
}

void ek_pt_resume(ek_pt_t *pt)
{
    ek_assert_param(s_init == true);
    if (pt == NULL || pt->state != EK_PT_STATE_SUSPEND) return;

    if (pt == s_cur_pt)
    {
        // 回调里先挂起自己又恢复：撤销挂起，回调返回后按正常流程回到就绪链表
        pt->state = EK_PT_STATE_RUNNING;
        return;
    }

    pt->state = EK_PT_STATE_READY;
    _ready_enqueue(pt);
}

ek_pt_t *ek_pt_active(void)
{
    return s_cur_pt;
}

#    if EKCFG_PICOTHREAD_SEM == 1
int ek_pt_sem_init_static(ek_pt_sem_t *sem, uint8_t count)
{
    if (sem == NULL) return -EINVAL;
    sem->count = count;
    ek_list_init(&sem->wait_list);
    return 0;
}

void ek_pt_sem_deinit_static(ek_pt_sem_t *sem)
{
    if (sem == NULL) return;
    _pt_wake_all(&sem->wait_list, -ECANCELED);
    sem->count = 0U;
}

ek_pt_sem_t *ek_pt_sem_create(uint8_t count)
{
    ek_pt_sem_t *sem = ek_malloc(sizeof(*sem));
    if (sem == NULL) return NULL;
    sem->count = count;
    ek_list_init(&sem->wait_list);
    return sem;
}

void ek_pt_sem_destroy(ek_pt_sem_t *sem)
{
    if (sem == NULL) return;

    // 唤醒所有等待此信号量的任务
    _pt_wake_all(&sem->wait_list, -ECANCELED);
    ek_free(sem);
}

bool ek_pt_sem_take(ek_pt_sem_t *sem)
{
    if (sem == NULL)
    {
        _pt_set_wait_error(-EINVAL);
        return false;
    }
    ek_assert_param(s_cur_pt != NULL);

    if (sem->count)
    {
        sem->count--;
        return true;
    }
    if (s_cur_pt == NULL) return false;

    s_cur_pt->wait_sem = sem;
    _insert_event_by_prio(&sem->wait_list, s_cur_pt);
    return false;
}

void ek_pt_sem_give(ek_pt_sem_t *sem)
{
    if (sem == NULL) return;

    // 有等待者：把这次释放直接移交给优先级最高的等待者，计数不变。
    // 等待者醒来后不再重新 take，避免"唤醒了却又拿不到"的丢失问题。
    if (!ek_list_is_empty(&sem->wait_list))
    {
        ek_pt_t *pt = EK_CONTAINER_OF(ek_list_get_first(&sem->wait_list), ek_pt_t, event_node);
        _pt_wake(pt, 0);
        return;
    }

    // 没有等待者则增加计数（饱和，防止 uint8_t 回绕成 0）
    if (sem->count < UINT8_MAX) sem->count++;
}

#    endif /* EKCFG_PICOTHREAD_SEM */

#    if EKCFG_PICOTHREAD_MSG == 1

int ek_pt_msg_init_static(ek_pt_msg_t *msg, size_t item_size, uint32_t item_amount)
{
    if (msg == NULL) return -EINVAL;
    EK_ERR_RETURN(ek_ringbuf_init_static(&msg->rb, item_size, item_amount));
    ek_list_init(&msg->recv_wait);
    ek_list_init(&msg->send_wait);
    return 0;
}

void ek_pt_msg_deinit_static(ek_pt_msg_t *msg)
{
    if (msg == NULL) return;

    _pt_wake_all(&msg->recv_wait, -ECANCELED);
    _pt_wake_all(&msg->send_wait, -ECANCELED);
    ek_ringbuf_deinit_static(&msg->rb);
}

ek_pt_msg_t *ek_pt_msg_create(size_t item_size, uint32_t item_amount)
{
    if (item_size == 0U || item_amount == 0U) return NULL;
    // 防止总大小溢出
    if (item_amount > (SIZE_MAX - sizeof(ek_pt_msg_t)) / item_size) return NULL;

    ek_pt_msg_t *msg = (ek_pt_msg_t *)ek_malloc(sizeof(ek_pt_msg_t) + (size_t)item_amount * item_size);
    if (msg == NULL) return NULL;

    if (ek_pt_msg_init_static(msg, item_size, item_amount) != 0)
    {
        ek_free(msg);
        return NULL;
    }
    return msg;
}

void ek_pt_msg_destroy(ek_pt_msg_t *msg)
{
    if (msg == NULL) return;

    ek_pt_msg_deinit_static(msg);
    ek_free(msg);
}

bool ek_pt_msg_send(ek_pt_msg_t *msg, const void *data)
{
    if (msg == NULL || data == NULL)
    {
        _pt_set_wait_error(-EINVAL);
        return false;
    }

    // 有任务在等待接收（此时缓冲区必为空）：直接拷进它的缓冲区并唤醒
    if (!ek_list_is_empty(&msg->recv_wait))
    {
        ek_pt_t *pt = EK_CONTAINER_OF(ek_list_get_first(&msg->recv_wait), ek_pt_t, event_node);
        if (pt->wait_buf != NULL) memcpy(pt->wait_buf, data, msg->rb.item_size);
        _pt_wake(pt, 0);
        return true;
    }

    if (ek_ringbuf_write(&msg->rb, data) == 0) return true;

    // 缓冲区满，挂到发送等待队列；接收方取走数据后会把 data 补进缓冲区
    ek_assert_param(s_cur_pt != NULL);
    if (s_cur_pt == NULL) return false;
    s_cur_pt->wait_msg = msg;
    s_cur_pt->wait_buf = (void *)(uintptr_t)data;
    _insert_event_by_prio(&msg->send_wait, s_cur_pt);
    return false;
}

bool ek_pt_msg_recv(ek_pt_msg_t *msg, void *data)
{
    if (msg == NULL)
    {
        _pt_set_wait_error(-EINVAL);
        return false;
    }

    if (ek_ringbuf_read(&msg->rb, data) == 0)
    {
        // 腾出了一个槽位：把优先级最高的等待发送者的数据补进来并唤醒它
        if (!ek_list_is_empty(&msg->send_wait))
        {
            ek_pt_t *pt = EK_CONTAINER_OF(ek_list_get_first(&msg->send_wait), ek_pt_t, event_node);
            (void)ek_ringbuf_write(&msg->rb, pt->wait_buf);
            _pt_wake(pt, 0);
        }
        return true;
    }

    // 缓冲区空，挂到接收等待队列；发送方会把数据直接拷进 data
    ek_assert_param(s_cur_pt != NULL);
    if (s_cur_pt == NULL) return false;
    s_cur_pt->wait_msg = msg;
    s_cur_pt->wait_buf = data;
    _insert_event_by_prio(&msg->recv_wait, s_cur_pt);
    return false;
}

#    endif /* EKCFG_PICOTHREAD_MSG */

/* ========== 内部函数 ========== */

static void _pt_setup(ek_pt_t *pt, ek_pt_cb_t cb, uint8_t prio, void *arg)
{
    pt->cb = cb;
    pt->arg = arg;
    pt->prio = prio;
    pt->forever = 0U;
    pt->state = EK_PT_STATE_READY;
    pt->tick = 0;
    pt->line = 0;
    ek_list_init(&pt->event_node);
    ek_list_init(&pt->state_node);
#    if EKCFG_PICOTHREAD_SEM || EKCFG_PICOTHREAD_MSG
    pt->wait_result = 0;
#    endif
#    if EKCFG_PICOTHREAD_SEM
    pt->wait_sem = NULL;
#    endif
#    if EKCFG_PICOTHREAD_MSG
    pt->wait_msg = NULL;
    pt->wait_buf = NULL;
#    endif

    _ready_enqueue(pt);
}

/* 从就绪/阻塞链表摘下（不处理事件等待） */
static void _pt_detach_state(ek_pt_t *pt)
{
    if (pt->state == EK_PT_STATE_READY) _ready_dequeue(pt);
    else if (pt->state == EK_PT_STATE_BLOCK) ek_list_remove(&pt->state_node); // 永久阻塞时节点自环，移除是空操作
    pt->forever = 0U;
}

static void _pt_teardown(ek_pt_t *pt)
{
#    if EKCFG_PICOTHREAD_SEM || EKCFG_PICOTHREAD_MSG
    _pt_cancel_wait(pt, -ECANCELED);
#    endif
    _pt_detach_state(pt);
    pt->state = EK_PT_STATE_SUSPEND;
}

#    if EKCFG_PICOTHREAD_SEM || EKCFG_PICOTHREAD_MSG
/* 若任务正在等待事件，则从事件等待队列摘下并记录结果 */
static void _pt_cancel_wait(ek_pt_t *pt, int result)
{
#        if EKCFG_PICOTHREAD_SEM
    if (pt->wait_sem != NULL)
    {
        ek_list_remove(&pt->event_node);
        pt->wait_sem = NULL;
        pt->wait_result = result;
    }
#        endif
#        if EKCFG_PICOTHREAD_MSG
    if (pt->wait_msg != NULL)
    {
        ek_list_remove(&pt->event_node);
        pt->wait_msg = NULL;
        pt->wait_buf = NULL;
        pt->wait_result = result;
    }
#        endif
}

/* 结束一个等待者的事件等待并让它就绪 */
static void _pt_wake(ek_pt_t *pt, int result)
{
    _pt_cancel_wait(pt, result);
    if (pt->state == EK_PT_STATE_BLOCK)
    {
        _pt_detach_state(pt);
        pt->state = EK_PT_STATE_READY;
        _ready_enqueue(pt);
    }
}

static void _pt_wake_all(ek_list_node_t *wait_list, int result)
{
    while (!ek_list_is_empty(wait_list))
    {
        ek_pt_t *pt = EK_CONTAINER_OF(ek_list_get_first(wait_list), ek_pt_t, event_node);
        _pt_wake(pt, result);
    }
}

static void _pt_set_wait_error(int err)
{
    if (s_cur_pt != NULL) s_cur_pt->wait_result = err;
}

static void _insert_event_by_prio(ek_list_node_t *list, ek_pt_t *pt)
{
    ek_list_node_t *i;
    ek_list_foreach(i, list)
    {
        ek_pt_t *cur_pt = EK_CONTAINER_OF(i, ek_pt_t, event_node);
        if (cur_pt->prio > pt->prio)
        {
            ek_list_insert_before(i, &pt->event_node);
            return;
        }
    }
    ek_list_insert_tail(list, &pt->event_node);
}
#    endif /* EKCFG_PICOTHREAD_SEM || EKCFG_PICOTHREAD_MSG */

static void _ready_enqueue(ek_pt_t *pt)
{
    const uint8_t prio = pt->prio;
    ek_list_insert_tail(&s_ready_list[prio], &pt->state_node);
    s_ready_mask |= (1UL << prio);
}

static void _ready_dequeue(ek_pt_t *pt)
{
    const uint8_t prio = pt->prio;
    ek_list_remove(&pt->state_node);
    if (ek_list_is_empty(&s_ready_list[prio])) s_ready_mask &= ~(1UL << prio);
}

static void _insert_block_by_time(ek_pt_t *pt)
{
    ek_list_node_t *i;
    ek_list_foreach(i, &s_block_list)
    {
        ek_pt_t *cur_pt = EK_CONTAINER_OF(i, ek_pt_t, state_node);
        if (EK_TICK_AFTER(cur_pt->tick, pt->tick) || (cur_pt->tick == pt->tick && cur_pt->prio > pt->prio))
        {
            ek_list_insert_before(i, &pt->state_node);
            return;
        }
    }
    ek_list_insert_tail(&s_block_list, &pt->state_node);
}

static ek_pt_t *_pt_next_ready(void)
{
    if (s_ready_mask == 0U) return NULL;
    const uint8_t prio = (uint8_t)ek_ctz32(s_ready_mask);
    ek_list_node_t *head = ek_list_get_first(&s_ready_list[prio]);
    return EK_CONTAINER_OF(head, ek_pt_t, state_node);
}

#endif // EKCFG_PICOTHREAD
