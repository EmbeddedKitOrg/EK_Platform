#include "ek_picothread.h"
#if EKCFG_PICOTHREAD == 1

#    include "ek_heap.h"
#    include "ek_assert.h"
#    include "ek_export.h"

__EK_STATIC_INLINE void _ready_enqueue(ek_pt_t *pt);
__EK_STATIC_INLINE void _ready_dequeue(ek_pt_t *pt);
__EK_STATIC_INLINE void _insert_event_by_prio(ek_list_node_t *list, ek_pt_t *pt);
__EK_STATIC_INLINE void _insert_state_by_time(ek_list_node_t *list, ek_pt_t *pt);
static ek_pt_t *_pt_next_ready(void);

#    define PT_PRIO_LEVELS ((uint8_t)(EKCFG_PT_PRIO_LOWEST + 1U))

volatile static bool s_init = false;
static ek_list_node_t s_ready_list[PT_PRIO_LEVELS];
static uint32_t s_ready_mask;
static ek_list_node_t s_block_list;
static ek_pt_t *s_cur_pt;
static ek_pt_t *s_next_pt;
static uint32_t s_schedule_now;

void ek_pt_init(void)
{
    if (s_init == true)
    {
        return;
    }
    s_init = true;
    s_ready_mask = 0U;
    for (uint8_t i = 0; i < PT_PRIO_LEVELS; i++) ek_list_init(&s_ready_list[i]);
    ek_list_init(&s_block_list);
}

EK_EXPORT_COMPONENTS(ek_pt_init, 0);

#    if EKCFG_STATIC_ALLOC == 1
int ek_pt_init_static(ek_pt_t *pt, ek_pt_cb_t cb, uint8_t prio, void *arg)
{
    if (pt == NULL || cb == NULL || s_init == false || prio > EKCFG_PT_PRIO_LOWEST) return -EINVAL;

    pt->cb = cb;
    pt->arg = arg;
    pt->prio = prio;
    pt->state = EK_PT_STATE_READY;
    pt->tick = 0;
    pt->line = 0;
    ek_list_init(&pt->event_node);
    ek_list_init(&pt->state_node);
#        if EKCFG_PICOTHREAD_SEM || EKCFG_PICOTHREAD_MSG
    pt->wait_result = 0;
#        endif
#        if EKCFG_PICOTHREAD_SEM
    pt->wait_sem = NULL;
#        endif
#        if EKCFG_PICOTHREAD_MSG
    pt->wait_msg = NULL;
#        endif

    _ready_enqueue(pt);
    return 0;
}

void ek_pt_deinit_static(ek_pt_t *pt)
{
    if (pt == NULL || pt == s_cur_pt) return;

    if (pt->state == EK_PT_STATE_READY) _ready_dequeue(pt);
    else if (pt->state == EK_PT_STATE_BLOCK) ek_list_remove(&pt->state_node);
#        if EKCFG_PICOTHREAD_SEM
    if (pt->wait_sem != NULL)
    {
        ek_list_remove(&pt->event_node);
        pt->wait_sem = NULL;
    }
#        endif
#        if EKCFG_PICOTHREAD_MSG
    if (pt->wait_msg != NULL)
    {
        ek_list_remove(&pt->event_node);
        pt->wait_msg = NULL;
    }
#        endif
#        if EKCFG_PICOTHREAD_SEM || EKCFG_PICOTHREAD_MSG
    pt->wait_result = -ECANCELED;
#        endif
    pt->state = EK_PT_STATE_SUSPEND;
}
#    endif /* EKCFG_STATIC_ALLOC */

ek_pt_t *ek_pt_create(ek_pt_cb_t cb, uint8_t prio, void *arg)
{
    ek_assert_param(s_init == true);
    if (cb == NULL || prio > EKCFG_PT_PRIO_LOWEST) return NULL;
    ek_pt_t *pt = ek_malloc(sizeof(*pt));
    if (pt == NULL) return NULL;
    pt->cb = cb;
    pt->arg = arg;
    pt->prio = prio;
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
#    endif /* EKCFG_PICOTHREAD_SEM */
#    if EKCFG_PICOTHREAD_MSG
    pt->wait_msg = NULL;
#    endif /* EKCFG_PICOTHREAD_MSG */

    _ready_enqueue(pt);

    return pt;
}

void ek_pt_destroy(ek_pt_t *pt)
{
    if (pt == NULL || pt == s_cur_pt) return;

    if (pt->state == EK_PT_STATE_READY) _ready_dequeue(pt);
    else if (pt->state == EK_PT_STATE_BLOCK) ek_list_remove(&pt->state_node);

#    if EKCFG_PICOTHREAD_SEM
    // 从信号量等待链表移除
    if (pt->wait_sem != NULL)
    {
        ek_list_remove(&pt->event_node);
        pt->wait_sem = NULL;
    }
#    endif
#    if EKCFG_PICOTHREAD_MSG
    // 从消息队列等待链表移除
    if (pt->wait_msg != NULL)
    {
        ek_list_remove(&pt->event_node);
        pt->wait_msg = NULL;
    }
#    endif
#    if EKCFG_PICOTHREAD_SEM || EKCFG_PICOTHREAD_MSG
    pt->wait_result = -ECANCELED;
#    endif

    ek_free(pt);
}

uint32_t ek_pt_schedule(uint32_t now)
{
    ek_assert_param(s_init == true);

    s_schedule_now = now;

    // 将阻塞链表中到期任务移回就绪链表
    ek_list_node_t *pos, *n;
    ek_list_foreach_safe(pos, n, &s_block_list)
    {
        ek_pt_t *pt = EK_CONTAINER_OF(pos, ek_pt_t, state_node);
        if (pt->tick > now)
        {
            break;
        }
        ek_list_remove(pos);
        pt->state = EK_PT_STATE_READY;
#    if EKCFG_PICOTHREAD_SEM || EKCFG_PICOTHREAD_MSG
        // 超时退出时清理事件等待链表
#        if EKCFG_PICOTHREAD_SEM
        if (pt->wait_sem != NULL)
        {
            ek_list_remove(&pt->event_node);
            pt->wait_sem = NULL;
            pt->wait_result = -ETIMEDOUT;
        }
#        endif
#        if EKCFG_PICOTHREAD_MSG
        if (pt->wait_msg != NULL)
        {
            ek_list_remove(&pt->event_node);
            pt->wait_msg = NULL;
            pt->wait_result = -ETIMEDOUT;
        }
#        endif
#    endif
        _ready_enqueue(pt);
    }

    // 取下一个就绪任务
    s_next_pt = _pt_next_ready();
    if (s_next_pt == NULL)
    {
        // 无就绪任务，跳过执行
        s_cur_pt = NULL;
    }
    else
    {
        s_cur_pt = s_next_pt;
        _ready_dequeue(s_cur_pt);
        s_cur_pt->state = EK_PT_STATE_RUNNING;
        s_cur_pt->cb(s_cur_pt, s_cur_pt->arg);

        // 回调后根据状态重新分流
        if (s_cur_pt->state == EK_PT_STATE_RUNNING)
        {
            s_cur_pt->state = EK_PT_STATE_READY;
            _ready_enqueue(s_cur_pt);
        }
        else if (s_cur_pt->state == EK_PT_STATE_BLOCK)
        {
            _insert_state_by_time(&s_block_list, s_cur_pt);
        }
    }

    // 有就绪任务则返回当前 tick，主循环不应睡眠
    if (s_ready_mask != 0U) return s_schedule_now;

    // 返回阻塞链表最早唤醒 tick
    // 空则 0 表示可无限休眠
    if (ek_list_is_empty(&s_block_list)) return 0;
    ek_list_node_t *head = ek_list_get_first(&s_block_list);
    ek_pt_t *first_blocked = EK_CONTAINER_OF(head, ek_pt_t, state_node);
    return first_blocked->tick;
}

void ek_pt_block(ek_pt_t *pt, uint32_t xtick)
{
    ek_assert_param(s_init == true);
    if (pt == NULL) return;
    pt->tick = (xtick == (uint32_t)-1) ? (uint32_t)-1 : s_schedule_now + xtick;
    if (pt == s_cur_pt)
    {
        pt->state = EK_PT_STATE_BLOCK;
        return;
    }

    if (pt->state == EK_PT_STATE_READY) _ready_dequeue(pt);
    else if (pt->state == EK_PT_STATE_BLOCK) ek_list_remove(&pt->state_node);
    pt->state = EK_PT_STATE_BLOCK;
    _insert_state_by_time(&s_block_list, pt);
}

ek_pt_t *ek_pt_suspend(ek_pt_t *pt)
{
    // 传入 NULL 表示挂起自己
    ek_pt_t *pt_to_suspend = pt == NULL ? s_cur_pt : pt;

    ek_assert_param(s_init == true);
    if (pt_to_suspend == NULL) return NULL;

    if (pt_to_suspend == s_cur_pt)
    {
        pt_to_suspend->state = EK_PT_STATE_SUSPEND;
        return pt_to_suspend;
    }

    if (pt_to_suspend->state == EK_PT_STATE_READY) _ready_dequeue(pt_to_suspend);
    else if (pt_to_suspend->state == EK_PT_STATE_BLOCK) ek_list_remove(&pt_to_suspend->state_node);
    pt_to_suspend->state = EK_PT_STATE_SUSPEND;
    return NULL;
}

void ek_pt_resume(ek_pt_t *pt)
{
    ek_assert_param(s_init == true);
    if (pt == NULL || pt->state != EK_PT_STATE_SUSPEND) return;

    pt->state = EK_PT_STATE_READY;
    _ready_enqueue(pt);
}

ek_pt_t *ek_pt_active(void)
{
    return s_cur_pt;
}

#    if EKCFG_PICOTHREAD_SEM == 1
#        if EKCFG_STATIC_ALLOC == 1
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

    ek_list_node_t *pos, *n;
    ek_list_foreach_safe(pos, n, &sem->wait_list)
    {
        ek_pt_t *pt = EK_CONTAINER_OF(pos, ek_pt_t, event_node);
        ek_list_remove(pos);
        ek_list_remove(&pt->state_node);
        pt->wait_sem = NULL;
        pt->wait_result = -ECANCELED;
        _ready_enqueue(pt);
        pt->state = EK_PT_STATE_READY;
    }
}
#        endif /* EKCFG_STATIC_ALLOC */

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
    ek_list_node_t *pos, *n;
    ek_list_foreach_safe(pos, n, &sem->wait_list)
    {
        ek_pt_t *pt = EK_CONTAINER_OF(pos, ek_pt_t, event_node);
        ek_list_remove(pos);
        ek_list_remove(&pt->state_node);
        pt->wait_sem = NULL;
        pt->wait_result = -ECANCELED;
        _ready_enqueue(pt);
        pt->state = EK_PT_STATE_READY;
    }

    ek_free(sem);
}

bool ek_pt_sem_take(ek_pt_sem_t *sem)
{
    if (sem == NULL) return false;
    ek_assert_param(s_cur_pt != NULL);

    if (sem->count)
    {
        sem->count--;
        return true;
    }
    s_cur_pt->wait_sem = sem;
    _insert_event_by_prio(&sem->wait_list, s_cur_pt);
    return false;
}

void ek_pt_sem_give(ek_pt_sem_t *sem)
{
    if (sem == NULL) return;

    // 首先判断是否有等待的任务
    // 如果有，则取出优先级最高
    // 删除事件节点的所在链表位置
    // 设置为就绪状态
    if (!ek_list_is_empty(&sem->wait_list))
    {
        ek_list_node_t *node = ek_list_get_first(&sem->wait_list);
        ek_pt_t *pt = EK_CONTAINER_OF(node, ek_pt_t, event_node);
        ek_list_remove(node);
        ek_list_remove(&pt->state_node);
        pt->wait_sem = NULL;
        pt->wait_result = 0;
        _ready_enqueue(pt);
        pt->state = EK_PT_STATE_READY;
        return;
    }

    // 如果没有，则增加计数值
    sem->count++;
}

#    endif /* EKCFG_PICOTHREAD_SEM */

#    if EKCFG_PICOTHREAD_MSG == 1
#        include "ek_ringbuf.h"

#        if EKCFG_STATIC_ALLOC == 1
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

    ek_list_node_t *pos, *n;
    ek_list_foreach_safe(pos, n, &msg->recv_wait)
    {
        ek_pt_t *pt = EK_CONTAINER_OF(pos, ek_pt_t, event_node);
        ek_list_remove(pos);
        ek_list_remove(&pt->state_node);
        pt->wait_msg = NULL;
        pt->wait_result = -ECANCELED;
        _ready_enqueue(pt);
        pt->state = EK_PT_STATE_READY;
    }

    ek_list_foreach_safe(pos, n, &msg->send_wait)
    {
        ek_pt_t *pt = EK_CONTAINER_OF(pos, ek_pt_t, event_node);
        ek_list_remove(pos);
        ek_list_remove(&pt->state_node);
        pt->wait_msg = NULL;
        pt->wait_result = -ECANCELED;
        _ready_enqueue(pt);
        pt->state = EK_PT_STATE_READY;
    }

    ek_ringbuf_deinit_static(&msg->rb);
}
#        endif /* EKCFG_STATIC_ALLOC */
ek_pt_msg_t *ek_pt_msg_create(size_t item_size, uint32_t item_amount)
{
    if (item_size == 0U || item_amount == 0U) return NULL;
    // 防止总大小溢出
    if (item_amount > (SIZE_MAX - sizeof(ek_pt_msg_t)) / item_size) return NULL;

    ek_pt_msg_t *msg = (ek_pt_msg_t *)ek_malloc(sizeof(ek_pt_msg_t) + (size_t)item_amount * item_size);
    if (msg == NULL) return NULL;

    msg->rb.cap = item_amount;
    msg->rb.item_size = item_size;
    msg->rb.read_idx = 0U;
    msg->rb.write_idx = 0U;
    msg->rb.item_amount = 0U;
#        if EKCFG_RTOS == 1
    EK_LOCK_INIT(msg->rb.lock);
#        endif
    ek_list_init(&msg->recv_wait);
    ek_list_init(&msg->send_wait);
    return msg;
}

void ek_pt_msg_destroy(ek_pt_msg_t *msg)
{
    if (msg == NULL) return;

    ek_list_node_t *pos, *n;
    ek_list_foreach_safe(pos, n, &msg->recv_wait)
    {
        ek_pt_t *pt = EK_CONTAINER_OF(pos, ek_pt_t, event_node);
        ek_list_remove(pos);
        ek_list_remove(&pt->state_node);
        pt->wait_msg = NULL;
        pt->wait_result = -ECANCELED;
        _ready_enqueue(pt);
        pt->state = EK_PT_STATE_READY;
    }

    ek_list_foreach_safe(pos, n, &msg->send_wait)
    {
        ek_pt_t *pt = EK_CONTAINER_OF(pos, ek_pt_t, event_node);
        ek_list_remove(pos);
        ek_list_remove(&pt->state_node);
        pt->wait_msg = NULL;
        pt->wait_result = -ECANCELED;
        _ready_enqueue(pt);
        pt->state = EK_PT_STATE_READY;
    }

    ek_free(msg);
}

bool ek_pt_msg_send(ek_pt_msg_t *msg, const void *data)
{
    if (msg == NULL || data == NULL) return false;
    ek_assert_param(s_cur_pt != NULL);

    int ret = ek_ringbuf_write(&msg->rb, data);
    if (ret == 0)
    {
        // 写入成功，唤醒一个等待接收的任务
        if (!ek_list_is_empty(&msg->recv_wait))
        {
            ek_list_node_t *node = ek_list_get_first(&msg->recv_wait);
            ek_pt_t *pt = EK_CONTAINER_OF(node, ek_pt_t, event_node);
            ek_list_remove(node);
            ek_list_remove(&pt->state_node);
            pt->wait_msg = NULL;
            pt->wait_result = 0;
            _ready_enqueue(pt);
            pt->state = EK_PT_STATE_READY;
        }
        return true;
    }

    // 缓冲区满，挂到发送等待队列
    s_cur_pt->wait_msg = msg;
    _insert_event_by_prio(&msg->send_wait, s_cur_pt);
    return false;
}

bool ek_pt_msg_recv(ek_pt_msg_t *msg, void *data)
{
    if (msg == NULL) return false;
    ek_assert_param(s_cur_pt != NULL);

    int ret = ek_ringbuf_read(&msg->rb, data);
    if (ret == 0)
    {
        // 读取成功，唤醒一个等待发送的任务
        if (!ek_list_is_empty(&msg->send_wait))
        {
            ek_list_node_t *node = ek_list_get_first(&msg->send_wait);
            ek_pt_t *pt = EK_CONTAINER_OF(node, ek_pt_t, event_node);
            ek_list_remove(node);
            ek_list_remove(&pt->state_node);
            pt->wait_msg = NULL;
            pt->wait_result = 0;
            _ready_enqueue(pt);
            pt->state = EK_PT_STATE_READY;
        }
        return true;
    }

    // 缓冲区空，挂到接收等待队列
    s_cur_pt->wait_msg = msg;
    _insert_event_by_prio(&msg->recv_wait, s_cur_pt);
    return false;
}

#    endif /* EKCFG_PICOTHREAD_MSG */

__EK_STATIC_INLINE void _ready_enqueue(ek_pt_t *pt)
{
    const uint8_t prio = pt->prio;
    ek_list_insert_tail(&s_ready_list[prio], &pt->state_node);
    s_ready_mask |= (1UL << prio);
}

__EK_STATIC_INLINE void _ready_dequeue(ek_pt_t *pt)
{
    const uint8_t prio = pt->prio;
    ek_list_remove(&pt->state_node);
    if (ek_list_is_empty(&s_ready_list[prio])) s_ready_mask &= ~(1UL << prio);
}

__EK_STATIC_INLINE void _insert_event_by_prio(ek_list_node_t *list, ek_pt_t *pt)
{
    if (ek_list_is_empty(list))
    {
        ek_list_insert_head(list, &pt->event_node);
        return;
    }
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

__EK_STATIC_INLINE void _insert_state_by_time(ek_list_node_t *list, ek_pt_t *pt)
{
    if (ek_list_is_empty(list))
    {
        ek_list_insert_head(list, &pt->state_node);
        return;
    }
    ek_list_node_t *i;
    ek_list_foreach(i, list)
    {
        ek_pt_t *cur_pt = EK_CONTAINER_OF(i, ek_pt_t, state_node);
        if (cur_pt->tick > pt->tick || (cur_pt->tick == pt->tick && cur_pt->prio > pt->prio))
        {
            ek_list_insert_before(i, &pt->state_node);
            return;
        }
    }
    ek_list_insert_tail(list, &pt->state_node);
}

static ek_pt_t *_pt_next_ready(void)
{
    if (s_ready_mask == 0U) return NULL;
    const uint8_t prio = (uint8_t)__builtin_ctz(s_ready_mask);
    ek_list_node_t *head = ek_list_get_first(&s_ready_list[prio]);
    return EK_CONTAINER_OF(head, ek_pt_t, state_node);
}

#endif // EKCFG_PICOTHREAD
