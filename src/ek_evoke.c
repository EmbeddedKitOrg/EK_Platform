/**
 * @file ek_evoke.c
 * @brief 协作式事件驱动任务调度器实现
 * @author N1netyNine99
 */

#include "ek_evoke.h"

#if EKCFG_RTOS == 0 && EKCFG_EVOKE == 1

#    include "ek_ringbuf.h"
#    include "ek_heap.h"
#    include "ek_export.h"
#    include "ek_log.h"

EK_LOG_FILE_TAG("ek_evoke.c");

#    define ISR_REQ_PUBLISH       (0x01)
#    define ISR_REQ_PUBLISH_DEALY (0x02)
#    define ISR_REQ_BROADCAST     (0x04)

typedef uint8_t _isr_req_type_t;

typedef struct
{
    _isr_req_type_t type;
    ek_evoke_event_t *evt;
    void *payload;
    uint32_t delay;
} _isr_req_t;

typedef struct
{
    ek_list_node_t node;
    ek_evoke_event_t *evt;
    void *payload;
    uint32_t wakeup_tick;
    bool broadcast;
} _defer_req_t;

static volatile uint32_t s_sleep_lock;
static volatile uint32_t s_event_tick_base;
static volatile uint32_t s_event_tick_diff;
static volatile uint32_t s_defer_earliest_tick;
static volatile bool s_defer_evt_wakeup;

static _defer_req_t s_defer_req_pool[EK_EVOKE_MAX_DEFER_REQ];
static ek_list_node_t s_defer_pool_free_list;
static union
{
    ek_ringbuf_spsc_t rb;
    _isr_req_t align_; /* 把 union 对齐抬到元素对齐 */
    uint8_t storage[sizeof(ek_ringbuf_spsc_t) + sizeof(_isr_req_t) * EK_EVOKE_MAX_ISR_REQ];
} s_isr_fifo_mem;
static ek_ringbuf_spsc_t *const s_isr_fifo = &s_isr_fifo_mem.rb;

static ek_list_node_t s_ready_task_list;
static ek_list_node_t s_defer_evt_list;

static _defer_req_t *_defer_req_malloc(void);
static void _defer_req_free(_defer_req_t *req);
static bool _ek_evoke_can_deep_sleep(void);

__EK_STATIC_INLINE void _ek_evoke_set_timer(uint32_t xtick)
{
    ek_evoke_set_timer(xtick);
    s_event_tick_diff = xtick;
}

void ek_evoke_sleep_lock(void)
{
    s_sleep_lock++;
}

void ek_evoke_sleep_unlock(void)
{
    if (s_sleep_lock) s_sleep_lock--;
    else EK_LOG_WARN("the sleep lock is 0, fail to decrease the sleep lock");
}

void ek_evoke_delay_timer_callback(void)
{
    s_defer_evt_wakeup = true;
    s_event_tick_base += s_event_tick_diff;
    s_event_tick_diff = 0;
}

void ek_evoke_init(void)
{
    ek_list_init(&s_ready_task_list);
    ek_list_init(&s_defer_evt_list);
    ek_list_init(&s_defer_pool_free_list);

    for (size_t i = 0; i < EK_EVOKE_MAX_DEFER_REQ; i++)
    {
        ek_list_insert_tail(&s_defer_pool_free_list, &s_defer_req_pool[i].node);
    }

    ek_ringbuf_init_spsc_static(s_isr_fifo, sizeof(_isr_req_t), EK_EVOKE_MAX_ISR_REQ);

    s_defer_earliest_tick = UINT32_MAX;
    s_event_tick_base = 0;
    s_sleep_lock = 0;
    s_defer_evt_wakeup = false;
}

EK_EXPORT_COMPONENTS(ek_evoke_init, 0);

#    if EKCFG_STATIC_ALLOC == 1
int ek_evoke_task_init_static(ek_evoke_task_t *tsk, ek_evoke_cb_t cb, void *arg)
{
    if (tsk == NULL || cb == NULL) return -EINVAL;

    ek_list_init(&tsk->node);
    tsk->cb = cb;
    tsk->arg = arg;
    tsk->state = EK_EVOKE_STATE_IDLE;
    tsk->wait_event = NULL;
    return 0;
}

void ek_evoke_task_deinit_static(ek_evoke_task_t *tsk)
{
    if (tsk == NULL) return;
    if (tsk->state != EK_EVOKE_STATE_IDLE)
    {
        ek_list_remove(&tsk->node);
    }
    tsk->state = EK_EVOKE_STATE_IDLE;
    tsk->wait_event = NULL;
    tsk->cb = NULL;
    tsk->arg = NULL;
}

int ek_evoke_event_init_static(ek_evoke_event_t *evt, uint32_t init)
{
    if (evt == NULL) return -EINVAL;

    ek_list_init(&evt->wait_list);
    evt->count = init;
    evt->data = NULL;
    return 0;
}

void ek_evoke_event_deinit_static(ek_evoke_event_t *evt)
{
    if (evt == NULL) return;
    while (!ek_list_is_empty(&evt->wait_list))
    {
        ek_list_node_t *node = ek_list_get_first(&evt->wait_list);
        ek_evoke_task_t *tsk = ek_list_container(node, ek_evoke_task_t, node);
        ek_list_remove(&tsk->node);
        tsk->wait_event = NULL;
        tsk->state = EK_EVOKE_STATE_IDLE;
    }
    evt->count = 0U;
    evt->data = NULL;
}
#    endif /* EKCFG_STATIC_ALLOC */

ek_evoke_task_t *ek_evoke_task_create(ek_evoke_cb_t cb, void *arg)
{
    if (cb == NULL) return NULL;

    ek_evoke_task_t *tsk = ek_malloc(sizeof(*tsk));
    if (tsk == NULL) return NULL;

    ek_list_init(&tsk->node);
    tsk->cb = cb;
    tsk->arg = arg;
    tsk->state = EK_EVOKE_STATE_IDLE;
    tsk->wait_event = NULL;

    return tsk;
}

void ek_evoke_task_destroy(ek_evoke_task_t *tsk)
{
    if (tsk == NULL) return;
    if (tsk->state != EK_EVOKE_STATE_IDLE)
    {
        ek_list_remove(&tsk->node);
    }
    ek_free(tsk);
}

ek_evoke_event_t *ek_evoke_event_create(uint32_t init)
{
    ek_evoke_event_t *evt = ek_malloc(sizeof(*evt));
    if (evt == NULL) return NULL;

    ek_list_init(&evt->wait_list);
    evt->count = init;
    evt->data = NULL;

    return evt;
}

void ek_evoke_event_destroy(ek_evoke_event_t *evt)
{
    if (evt == NULL) return;
    while (!ek_list_is_empty(&evt->wait_list))
    {
        ek_list_node_t *node = ek_list_get_first(&evt->wait_list);
        ek_evoke_task_t *tsk = ek_list_container(node, ek_evoke_task_t, node);
        ek_list_remove(&tsk->node);
        tsk->wait_event = NULL;
        tsk->state = EK_EVOKE_STATE_IDLE;
    }
    ek_free(evt);
}

bool ek_evoke_event_subscribe(ek_evoke_task_t *tsk, ek_evoke_event_t *evt)
{
    if (tsk == NULL || evt == NULL) return false;

    tsk->wait_event = evt;
    // 等待的事件中有计数量
    // 且没有其他任务已经在等待这个事件
    // 则可以直接减少计数量并且运行任务
    if (evt->count && ek_list_is_empty(&evt->wait_list))
    {
        evt->count--;
        ek_list_insert_tail(&s_ready_task_list, &tsk->node);
        tsk->state = EK_EVOKE_STATE_READY;

        return true;
    }

    // 否则说明需要等待事件发布
    ek_list_insert_tail(&evt->wait_list, &tsk->node);
    tsk->state = EK_EVOKE_STATE_WAITTING;

    return false;
}

void ek_evoke_event_broadcast(ek_evoke_event_t *evt, void *payload)
{
    if (evt == NULL) return;

    evt->data = payload;
    // 没有任务等待这个事件
    // 增加计数值
    if (ek_list_is_empty(&evt->wait_list))
    {
        evt->count++;
        return;
    }

    // 唤醒所有等待该事件的任务
    while (!ek_list_is_empty(&evt->wait_list))
    {
        ek_list_node_t *node = ek_list_get_first(&evt->wait_list);
        ek_evoke_task_t *tsk = ek_list_container(node, ek_evoke_task_t, node);
        ek_list_remove(node);
        ek_list_insert_tail(&s_ready_task_list, node);
        tsk->state = EK_EVOKE_STATE_READY;
    }
}

void ek_evoke_event_publish(ek_evoke_event_t *evt, void *payload)
{
    if (evt == NULL) return;

    evt->data = payload;
    // 只唤醒第一个等待该事件的任务
    if (!ek_list_is_empty(&evt->wait_list))
    {
        ek_list_node_t *node = ek_list_get_first(&evt->wait_list);
        ek_evoke_task_t *tsk = ek_list_container(node, ek_evoke_task_t, node);
        ek_list_remove(node);
        ek_list_insert_tail(&s_ready_task_list, node);
        tsk->state = EK_EVOKE_STATE_READY;

        return;
    }

    // 如果没有任务等待，就增加计数值
    evt->count++;
}

void ek_evoke_event_defer(ek_evoke_event_t *evt, void *payload, uint32_t delay, bool broadcast)
{
    if (evt == NULL) return;

    if (!delay)
    {
        if (broadcast) return ek_evoke_event_broadcast(evt, payload);
        else return ek_evoke_event_publish(evt, payload);
    }

    _defer_req_t *req = _defer_req_malloc();
    if (req == NULL)
    {
        EK_LOG_WARN("the defer request pool is empty, fail to create a defer request");
        return;
    }
    uint32_t wakeup_tick = delay + s_event_tick_base;

    req->evt = evt;
    req->broadcast = broadcast;
    req->payload = payload;
    req->wakeup_tick = wakeup_tick;

    ek_list_node_t *pos;
    ek_list_foreach(pos, &s_defer_evt_list)
    {
        _defer_req_t *pos_req = ek_list_container(pos, _defer_req_t, node);
        if (pos_req->wakeup_tick >= wakeup_tick) break;
    }
    ek_list_insert_before(pos, &req->node);
}

int ek_evoke_event_broadcast_from_isr(ek_evoke_event_t *evt, void *payload)
{
    if (evt == NULL) return -EINVAL;

    ek_evoke_enter_critical();

    _isr_req_t req = {
        .type = ISR_REQ_BROADCAST,
        .evt = evt,
        .payload = payload,
    };
    int err = ek_ringbuf_write_spsc(s_isr_fifo, &req);
    EK_ERR_GOTO(err, defer);

defer:
    ek_evoke_exit_critical();
    return err;
}

int ek_evoke_event_publish_from_isr(ek_evoke_event_t *evt, void *payload)
{
    if (evt == NULL) return -EINVAL;

    ek_evoke_enter_critical();

    _isr_req_t req = {
        .type = ISR_REQ_PUBLISH,
        .evt = evt,
        .payload = payload,
    };
    int err = ek_ringbuf_write_spsc(s_isr_fifo, &req);
    EK_ERR_GOTO(err, defer);

defer:
    ek_evoke_exit_critical();
    return err;
}

int ek_evoke_event_defer_from_isr(ek_evoke_event_t *evt, void *payload, uint32_t delay, bool broadcast)
{
    if (evt == NULL) return -EINVAL;

    ek_evoke_enter_critical();

    _isr_req_t req = {
        .type = (broadcast == true) ? (ISR_REQ_PUBLISH_DEALY | ISR_REQ_BROADCAST)
                                    : (ISR_REQ_PUBLISH_DEALY | ISR_REQ_PUBLISH),
        .evt = evt,
        .payload = payload,
        .delay = delay,
    };
    int err = ek_ringbuf_write_spsc(s_isr_fifo, &req);
    EK_ERR_GOTO(err, defer);

defer:
    ek_evoke_exit_critical();
    return err;
}

void ek_evoke_event_loop(void)
{
    while (1)
    {
        // 先处理是否有来自中断的请求
        // 从中断请求fifo中读取
        while (!ek_ringbuf_empty_spsc(s_isr_fifo))
        {
            _isr_req_t req = { 0 };
            if (ek_ringbuf_read_spsc(s_isr_fifo, &req) == 0)
            {
                if (req.type & ISR_REQ_PUBLISH)
                {
                    if (req.type & ISR_REQ_PUBLISH_DEALY) ek_evoke_event_defer(req.evt, req.payload, req.delay, false);
                    else ek_evoke_event_publish(req.evt, req.payload);
                }
                else if (req.type & ISR_REQ_BROADCAST)
                {
                    if (req.type & ISR_REQ_PUBLISH_DEALY) ek_evoke_event_defer(req.evt, req.payload, req.delay, true);
                    else ek_evoke_event_broadcast(req.evt, req.payload);
                }
            }
        }

        // 首先检查就绪链表是否为空
        // 如果不为空则执行所有到期任务
        // 如果为空就直接去检查延时链表
        while (!ek_list_is_empty(&s_ready_task_list))
        {
            ek_list_node_t *node = ek_list_get_first(&s_ready_task_list);
            ek_evoke_task_t *tsk = ek_list_container(node, ek_evoke_task_t, node);
            ek_list_remove(&tsk->node);
            tsk->state = EK_EVOKE_STATE_RUNNING;
            tsk->cb(tsk->wait_event, tsk->arg);
            ek_list_insert_tail(&tsk->wait_event->wait_list, &tsk->node);
            tsk->state = EK_EVOKE_STATE_WAITTING;
        }

        // 检查延时链表
        // 如果有延时的事件，则取出最早的唤醒事件然后设置中断时间
        if (!ek_list_is_empty(&s_defer_evt_list))
        {
            ek_list_node_t *node = ek_list_get_first(&s_defer_evt_list);
            _defer_req_t *req = ek_list_container(node, _defer_req_t, node);
            if (req->wakeup_tick != s_defer_earliest_tick)
            {
                s_defer_earliest_tick = req->wakeup_tick;
                _ek_evoke_set_timer(s_defer_earliest_tick - s_event_tick_base);
            }
        }

        // 检查睡眠锁和最近的延时唤醒时间，决定进入哪种睡眠状态
        if (_ek_evoke_can_deep_sleep()) ek_evoke_deep_sleep();
        else ek_evoke_light_sleep();

        // 中断唤醒，要去检查是否有延时事件被唤醒
        // 如果有，则依次唤醒
        // 并且要去检查是否有其他同样唤醒时间的事件
        // 如果有，一并发布
        if (s_defer_evt_wakeup)
        {
            s_defer_evt_wakeup = false;

            while (!ek_list_is_empty(&s_defer_evt_list))
            {
                ek_list_node_t *node = ek_list_get_first(&s_defer_evt_list);
                _defer_req_t *req = ek_list_container(node, _defer_req_t, node);

                if (req->wakeup_tick > s_defer_earliest_tick) break;

                ek_list_remove(node);
                if (req->broadcast) ek_evoke_event_broadcast(req->evt, req->payload);
                else ek_evoke_event_publish(req->evt, req->payload);
                _defer_req_free(req);
            }
            s_defer_earliest_tick = UINT32_MAX;
        }
    }
}
static bool _ek_evoke_can_deep_sleep(void)
{
    if (s_sleep_lock) return false;
    if (ek_list_is_empty(&s_defer_evt_list)) return true;

    uint32_t idle_tick = s_defer_earliest_tick - s_event_tick_base;
    return idle_tick >= EKCFG_EVOKE_MIN_DEEPSLEEP_TICK;
}

static _defer_req_t *_defer_req_malloc(void)
{
    if (!ek_list_is_empty(&s_defer_pool_free_list))
    {
        ek_list_node_t *node = ek_list_get_first(&s_defer_pool_free_list);
        ek_list_remove(node);
        _defer_req_t *req = ek_list_container(node, _defer_req_t, node);
        return req;
    }
    return NULL;
}

static void _defer_req_free(_defer_req_t *req)
{
    ek_list_insert_tail(&s_defer_pool_free_list, &req->node);
}

__EK_WEAK void ek_evoke_enter_critical(void)
{
}

__EK_WEAK void ek_evoke_exit_critical(void)
{
}

__EK_WEAK void ek_evoke_light_sleep(void)
{
}

__EK_WEAK void ek_evoke_deep_sleep(void)
{
}

__EK_WEAK void ek_evoke_set_timer(uint32_t xtick)
{
    __EK_UNUSED(xtick);
}

#endif // EKCFG_RTOS == 0 && EKCFG_EVOKE == 1
