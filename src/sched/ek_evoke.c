/**
 * @file ek_evoke.c
 * @brief 协作式事件驱动任务调度器实现
 * @author N1netyNine99
 */

#include "ek/sched/ek_evoke.h"

#if EKCFG_RTOS == 0 && EKCFG_EVOKE == 1

#    include "ek/container/ek_ringbuf.h"
#    include "ek/mem/ek_heap.h"
#    include "ek/core/ek_export.h"
#    include "ek/core/ek_log.h"
#    include "ek/core/ek_critical.h"

EK_LOG_FILE_TAG("ek_evoke.c");

#    define ISR_REQ_PUBLISH       (0x01)
#    define ISR_REQ_PUBLISH_DELAY (0x02)
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

/*
 * 虚拟时钟
 * --------
 * 延时定时器在深度睡眠期间仍然运行，而 SysTick 等系统节拍会停，因此调度器的时间
 * 只由延时定时器推进：
 *   now = s_tick_base + min(ek_evoke_get_timer_elapsed(), s_timer_span)
 * - s_tick_base：最近一次装载或到期时的时刻
 * - s_timer_span：当前装载的定时长度，0 表示定时器未运行
 * 重新装载定时器前先把已经走过的时间并入 s_tick_base，所以中途插入更早的
 * 延时请求不会让已排队的请求整体推迟。
 */
static volatile uint32_t s_sleep_lock;
static volatile uint32_t s_tick_base;
static volatile uint32_t s_timer_span;
static volatile bool s_timer_fired;
static uint32_t s_timer_target;

static _defer_req_t s_defer_req_pool[EK_EVOKE_MAX_DEFER_REQ];
static ek_list_node_t s_defer_pool_free_list;
static union
{
    ek_ringbuf_spsc_t rb;
    _isr_req_t align_; /* 把 union 对齐抬到元素对齐 */
    uint8_t storage[sizeof(ek_ringbuf_spsc_t) + sizeof(_isr_req_t) * (EK_EVOKE_MAX_ISR_REQ + 1)];
} s_isr_fifo_mem;
static ek_ringbuf_spsc_t *const s_isr_fifo = &s_isr_fifo_mem.rb;

static ek_list_node_t s_ready_task_list;
static ek_list_node_t s_defer_evt_list;

/* 正在执行回调的任务；回调里销毁自己时延迟到回调返回后再释放 */
static ek_evoke_task_t *s_cur_task;
static bool s_cur_task_free;

static _defer_req_t *_defer_req_malloc(void);
static void _defer_req_free(_defer_req_t *req);
static uint32_t _now_locked(void);
static bool _subscribe(ek_evoke_task_t *tsk, ek_evoke_event_t *evt);
static void _detach_event(ek_evoke_event_t *evt);
static void _drain_isr_requests(void);
static void _run_ready_tasks(void);
static bool _process_due_defers(void);
static void _update_timer(void);
static bool _can_deep_sleep_locked(void);

/* ========== 时间 ========== */

uint32_t ek_evoke_get_tick(void)
{
    ek_evoke_enter_critical();
    const uint32_t now = _now_locked();
    ek_evoke_exit_critical();
    return now;
}

void ek_evoke_delay_timer_callback(void)
{
    if (s_timer_span == 0U) return; /* 没有装载的定时器：视为伪触发 */
    s_tick_base += s_timer_span;
    s_timer_span = 0U;
    s_timer_fired = true;
}

/* ========== 睡眠锁 ========== */

void ek_evoke_sleep_lock(void)
{
    ek_evoke_enter_critical();
    s_sleep_lock++;
    ek_evoke_exit_critical();
}

void ek_evoke_sleep_unlock(void)
{
    bool underflow = false;
    ek_evoke_enter_critical();
    if (s_sleep_lock) s_sleep_lock--;
    else underflow = true;
    ek_evoke_exit_critical();
    if (underflow) EK_LOG_WARN("the sleep lock is 0, fail to decrease the sleep lock");
}

/* ========== 初始化 ========== */

void ek_evoke_init(void)
{
    ek_list_init(&s_ready_task_list);
    ek_list_init(&s_defer_evt_list);
    ek_list_init(&s_defer_pool_free_list);

    for (size_t i = 0; i < EK_EVOKE_MAX_DEFER_REQ; i++)
    {
        ek_list_insert_tail(&s_defer_pool_free_list, &s_defer_req_pool[i].node);
    }

    /* SPSC 环形缓冲区实际可存 cap - 1 项，多留一个槽位保证能存满 EK_EVOKE_MAX_ISR_REQ 个请求 */
    ek_ringbuf_init_spsc_static(s_isr_fifo, sizeof(_isr_req_t), EK_EVOKE_MAX_ISR_REQ + 1);

    s_tick_base = 0U;
    s_timer_span = 0U;
    s_timer_target = 0U;
    s_timer_fired = false;
    s_sleep_lock = 0U;
    s_cur_task = NULL;
    s_cur_task_free = false;
}

EK_EXPORT_COMPONENTS(ek_evoke_init, 0);

/* ========== 任务/事件 创建/销毁 ========== */

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
    ek_list_remove(&tsk->node); /* 节点未挂链时是空操作 */
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
    _detach_event(evt);
    evt->count = 0U;
    evt->data = NULL;
}

ek_evoke_task_t *ek_evoke_task_create(ek_evoke_cb_t cb, void *arg)
{
    if (cb == NULL) return NULL;

    ek_evoke_task_t *tsk = ek_malloc(sizeof(*tsk));
    if (tsk == NULL) return NULL;

    (void)ek_evoke_task_init_static(tsk, cb, arg);
    return tsk;
}

void ek_evoke_task_destroy(ek_evoke_task_t *tsk)
{
    if (tsk == NULL) return;

    if (tsk == s_cur_task)
    {
        /* 在自己的回调里销毁自己：回调返回后再释放，避免释放后继续访问 */
        tsk->state = EK_EVOKE_STATE_IDLE;
        tsk->wait_event = NULL;
        s_cur_task_free = true;
        return;
    }

    ek_list_remove(&tsk->node);
    ek_free(tsk);
}

ek_evoke_event_t *ek_evoke_event_create(uint32_t init)
{
    ek_evoke_event_t *evt = ek_malloc(sizeof(*evt));
    if (evt == NULL) return NULL;

    (void)ek_evoke_event_init_static(evt, init);
    return evt;
}

void ek_evoke_event_destroy(ek_evoke_event_t *evt)
{
    if (evt == NULL) return;
    _detach_event(evt);
    ek_free(evt);
}

/* ========== 事件订阅/分发 ========== */

bool ek_evoke_event_subscribe(ek_evoke_task_t *tsk, ek_evoke_event_t *evt)
{
    if (tsk == NULL || evt == NULL) return false;

    /* 已经在等待/就绪的任务改订其他事件时先摘下，避免同一节点挂在两条链表上 */
    ek_list_remove(&tsk->node);
    return _subscribe(tsk, evt);
}

void ek_evoke_event_broadcast(ek_evoke_event_t *evt, void *payload)
{
    if (evt == NULL) return;

    evt->data = payload;
    // 没有任务等待这个事件：增加计数值
    if (ek_list_is_empty(&evt->wait_list))
    {
        evt->count++;
        return;
    }

    // 唤醒所有等待该事件的任务
    while (!ek_list_is_empty(&evt->wait_list))
    {
        ek_list_node_t *node = ek_list_get_first(&evt->wait_list);
        ek_evoke_task_t *tsk = EK_CONTAINER_OF(node, ek_evoke_task_t, node);
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
        ek_evoke_task_t *tsk = EK_CONTAINER_OF(node, ek_evoke_task_t, node);
        ek_list_remove(node);
        ek_list_insert_tail(&s_ready_task_list, node);
        tsk->state = EK_EVOKE_STATE_READY;

        return;
    }

    // 如果没有任务等待，就增加计数值；任务回调结束重新订阅时会消费它
    evt->count++;
}

void ek_evoke_event_defer(ek_evoke_event_t *evt, void *payload, uint32_t delay, bool broadcast)
{
    if (evt == NULL) return;

    if (!delay)
    {
        if (broadcast) ek_evoke_event_broadcast(evt, payload);
        else ek_evoke_event_publish(evt, payload);
        return;
    }

    _defer_req_t *req = _defer_req_malloc();
    if (req == NULL)
    {
        EK_LOG_WARN("the defer request pool is empty, fail to create a defer request");
        return;
    }
    const uint32_t wakeup_tick = ek_evoke_get_tick() + delay;

    req->evt = evt;
    req->broadcast = broadcast;
    req->payload = payload;
    req->wakeup_tick = wakeup_tick;

    // 按唤醒时刻升序插入（回绕安全）；同一时刻的请求保持先来先发
    ek_list_node_t *pos;
    ek_list_foreach(pos, &s_defer_evt_list)
    {
        _defer_req_t *pos_req = EK_CONTAINER_OF(pos, _defer_req_t, node);
        if (EK_TICK_AFTER(pos_req->wakeup_tick, wakeup_tick)) break;
    }
    ek_list_insert_before(pos, &req->node);
}

/* ========== ISR 接口 ========== */

static int _isr_post(const _isr_req_t *req)
{
    /* 多个中断都可能是生产者：写入过程放在临界区内，保证 SPSC 缓冲区只有一个写者 */
    ek_evoke_enter_critical();
    const int err = ek_ringbuf_write_spsc(s_isr_fifo, req);
    ek_evoke_exit_critical();
    return err;
}

int ek_evoke_event_broadcast_from_isr(ek_evoke_event_t *evt, void *payload)
{
    if (evt == NULL) return -EINVAL;

    _isr_req_t req = {
        .type = ISR_REQ_BROADCAST,
        .evt = evt,
        .payload = payload,
        .delay = 0U,
    };
    return _isr_post(&req);
}

int ek_evoke_event_publish_from_isr(ek_evoke_event_t *evt, void *payload)
{
    if (evt == NULL) return -EINVAL;

    _isr_req_t req = {
        .type = ISR_REQ_PUBLISH,
        .evt = evt,
        .payload = payload,
        .delay = 0U,
    };
    return _isr_post(&req);
}

int ek_evoke_event_defer_from_isr(ek_evoke_event_t *evt, void *payload, uint32_t delay, bool broadcast)
{
    if (evt == NULL) return -EINVAL;

    _isr_req_t req = {
        .type = (broadcast == true) ? (ISR_REQ_PUBLISH_DELAY | ISR_REQ_BROADCAST)
                                    : (ISR_REQ_PUBLISH_DELAY | ISR_REQ_PUBLISH),
        .evt = evt,
        .payload = payload,
        .delay = delay,
    };
    return _isr_post(&req);
}

/* ========== 事件主循环 ========== */

void ek_evoke_run_once(void)
{
    // 1. 中断请求 → 2. 到期的延时事件 → 3. 执行就绪任务
    _drain_isr_requests();
    (void)_process_due_defers();
    _run_ready_tasks();

    // 任务执行期间可能产生新的中断请求、就绪任务或已到期的延时事件，先处理完再考虑睡眠
    if (!ek_ringbuf_empty_spsc(s_isr_fifo) || !ek_list_is_empty(&s_ready_task_list)) return;
    if (_process_due_defers()) return;

    // 为最近的延时事件装载定时器
    _update_timer();

    /*
     * 检查与进入睡眠放在同一个临界区里：中断在检查之后到来时，中断请求会保持挂起，
     * WFI/WFE 会立即返回，不会出现"请求已入队却一直睡到下一个中断"的丢失唤醒。
     */
    ek_evoke_enter_critical();
    if (ek_ringbuf_empty_spsc(s_isr_fifo) && ek_list_is_empty(&s_ready_task_list) && !s_timer_fired)
    {
        if (_can_deep_sleep_locked()) ek_evoke_deep_sleep();
        else ek_evoke_light_sleep();
    }
    s_timer_fired = false;
    ek_evoke_exit_critical();
}

void ek_evoke_event_loop(void)
{
    for (;;)
    {
        ek_evoke_run_once();
    }
}

/* ========== 内部函数 ========== */

/* 调用方必须已进入临界区 */
static uint32_t _now_locked(void)
{
    uint32_t now = s_tick_base;
    if (s_timer_span != 0U)
    {
        uint32_t elapsed = ek_evoke_get_timer_elapsed();
        if (elapsed > s_timer_span) elapsed = s_timer_span;
        now += elapsed;
    }
    return now;
}

/* 任务节点必须未挂在任何链表上 */
static bool _subscribe(ek_evoke_task_t *tsk, ek_evoke_event_t *evt)
{
    tsk->wait_event = evt;
    // 事件有积压计数且没有其他任务在等：直接消费一次并就绪
    if (evt->count && ek_list_is_empty(&evt->wait_list))
    {
        evt->count--;
        ek_list_insert_tail(&s_ready_task_list, &tsk->node);
        tsk->state = EK_EVOKE_STATE_READY;
        return true;
    }

    // 否则等待事件发布
    ek_list_insert_tail(&evt->wait_list, &tsk->node);
    tsk->state = EK_EVOKE_STATE_WAITING;
    return false;
}

/* 事件失效（销毁/反初始化）时，清掉所有指向它的任务与延时请求 */
static void _detach_event(ek_evoke_event_t *evt)
{
    ek_list_node_t *pos, *n;

    while (!ek_list_is_empty(&evt->wait_list))
    {
        ek_evoke_task_t *tsk = EK_CONTAINER_OF(ek_list_get_first(&evt->wait_list), ek_evoke_task_t, node);
        ek_list_remove(&tsk->node);
        tsk->wait_event = NULL;
        tsk->state = EK_EVOKE_STATE_IDLE;
    }

    ek_list_foreach_safe(pos, n, &s_ready_task_list)
    {
        ek_evoke_task_t *tsk = EK_CONTAINER_OF(pos, ek_evoke_task_t, node);
        if (tsk->wait_event != evt) continue;
        ek_list_remove(pos);
        tsk->wait_event = NULL;
        tsk->state = EK_EVOKE_STATE_IDLE;
    }

    if (s_cur_task != NULL && s_cur_task->wait_event == evt) s_cur_task->wait_event = NULL;

    ek_list_foreach_safe(pos, n, &s_defer_evt_list)
    {
        _defer_req_t *req = EK_CONTAINER_OF(pos, _defer_req_t, node);
        if (req->evt != evt) continue;
        ek_list_remove(pos);
        _defer_req_free(req);
    }
}

static void _drain_isr_requests(void)
{
    _isr_req_t req;
    while (ek_ringbuf_read_spsc(s_isr_fifo, &req) == 0)
    {
        const bool broadcast = (req.type & ISR_REQ_BROADCAST) != 0U;
        if (req.type & ISR_REQ_PUBLISH_DELAY) ek_evoke_event_defer(req.evt, req.payload, req.delay, broadcast);
        else if (broadcast) ek_evoke_event_broadcast(req.evt, req.payload);
        else ek_evoke_event_publish(req.evt, req.payload);
    }
}

static void _run_ready_tasks(void)
{
    while (!ek_list_is_empty(&s_ready_task_list))
    {
        ek_list_node_t *node = ek_list_get_first(&s_ready_task_list);
        ek_evoke_task_t *tsk = EK_CONTAINER_OF(node, ek_evoke_task_t, node);
        ek_list_remove(node);

        tsk->state = EK_EVOKE_STATE_RUNNING;
        s_cur_task = tsk;
        s_cur_task_free = false;
        tsk->cb(tsk->wait_event, tsk->arg);
        s_cur_task = NULL;

        if (s_cur_task_free)
        {
            s_cur_task_free = false;
            ek_free(tsk);
            continue;
        }

        /*
         * 回调里改订了别的事件、反初始化了自己，状态就不再是 RUNNING，保持原样。
         * 否则重新订阅原事件——回调期间积压的计数会在这里被消费，不会丢失。
         */
        if (tsk->state != EK_EVOKE_STATE_RUNNING) continue;
        if (tsk->wait_event == NULL)
        {
            tsk->state = EK_EVOKE_STATE_IDLE;
            continue;
        }
        (void)_subscribe(tsk, tsk->wait_event);
    }
}

/* 发布所有已到期的延时事件，返回是否发布了至少一个 */
static bool _process_due_defers(void)
{
    if (ek_list_is_empty(&s_defer_evt_list)) return false;

    const uint32_t now = ek_evoke_get_tick();
    bool fired = false;

    while (!ek_list_is_empty(&s_defer_evt_list))
    {
        _defer_req_t *req = EK_CONTAINER_OF(ek_list_get_first(&s_defer_evt_list), _defer_req_t, node);
        if (EK_TICK_AFTER(req->wakeup_tick, now)) break;

        ek_list_remove(&req->node);
        ek_evoke_event_t *evt = req->evt;
        void *payload = req->payload;
        const bool broadcast = req->broadcast;
        _defer_req_free(req);

        if (broadcast) ek_evoke_event_broadcast(evt, payload);
        else ek_evoke_event_publish(evt, payload);
        fired = true;
    }
    return fired;
}

/* 为最早的延时请求装载定时器（已装载同一目标时不重复装载） */
static void _update_timer(void)
{
    if (ek_list_is_empty(&s_defer_evt_list)) return;

    const _defer_req_t *first = EK_CONTAINER_OF(ek_list_get_first(&s_defer_evt_list), _defer_req_t, node);
    const uint32_t target = first->wakeup_tick;

    ek_evoke_enter_critical();
    if (s_timer_span == 0U || s_timer_target != target)
    {
        /* 先把已经走过的时间并入基准，再按剩余时长重新装载 */
        const uint32_t now = _now_locked();
        const uint32_t span = EK_TICK_AFTER(target, now) ? (target - now) : 1U;
        s_tick_base = now;
        s_timer_span = span;
        s_timer_target = target;
        ek_evoke_set_timer(span);
    }
    ek_evoke_exit_critical();
}

/* 调用方必须已进入临界区 */
static bool _can_deep_sleep_locked(void)
{
    if (s_sleep_lock) return false;
    if (ek_list_is_empty(&s_defer_evt_list)) return true;

    const _defer_req_t *first = EK_CONTAINER_OF(ek_list_get_first(&s_defer_evt_list), _defer_req_t, node);
    const uint32_t now = _now_locked();
    if (!EK_TICK_AFTER(first->wakeup_tick, now)) return false;
    return (first->wakeup_tick - now) >= EKCFG_EVOKE_MIN_DEEPSLEEP_TICK;
}

static _defer_req_t *_defer_req_malloc(void)
{
    if (!ek_list_is_empty(&s_defer_pool_free_list))
    {
        ek_list_node_t *node = ek_list_get_first(&s_defer_pool_free_list);
        ek_list_remove(node);
        _defer_req_t *req = EK_CONTAINER_OF(node, _defer_req_t, node);
        return req;
    }
    return NULL;
}

static void _defer_req_free(_defer_req_t *req)
{
    ek_list_insert_tail(&s_defer_pool_free_list, &req->node);
}

/* ========== 弱函数默认实现 ========== */

__EK_WEAK void ek_evoke_enter_critical(void)
{
    ek_critical_enter();
}

__EK_WEAK void ek_evoke_exit_critical(void)
{
    ek_critical_exit();
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

__EK_WEAK uint32_t ek_evoke_get_timer_elapsed(void)
{
    return 0U;
}

#endif // EKCFG_RTOS == 0 && EKCFG_EVOKE == 1
