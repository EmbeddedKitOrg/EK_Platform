#include "ek_test.h"
#include "ek/sched/ek_evoke.h"
#include "ek/mem/ek_heap.h"

/* ---------------- 模拟硬件：真实时间 + 一次性低功耗定时器 + 一个外部中断 ---------------- */

static uint32_t g_real_now;       /* 真实时间 */
static bool g_timer_armed;
static uint32_t g_timer_armed_at; /* 装载时的真实时间 */
static uint32_t g_timer_span;
static bool g_idle;               /* 进入睡眠时既没有定时器也没有外部中断可等 */
static int g_critical_depth_in_sleep = -1;

static bool g_irq_pending;        /* 计划中的外部中断 */
static uint32_t g_irq_at;
static void (*g_irq_handler)(void);

static int g_crit_depth;

void ek_evoke_enter_critical(void)
{
    g_crit_depth++;
}

void ek_evoke_exit_critical(void)
{
    g_crit_depth--;
}

void ek_evoke_set_timer(uint32_t xtick)
{
    g_timer_armed = true;
    g_timer_armed_at = g_real_now;
    g_timer_span = xtick;
}

uint32_t ek_evoke_get_timer_elapsed(void)
{
    if (!g_timer_armed) return 0U;
    return g_real_now - g_timer_armed_at;
}

/* 睡眠：时间推进到最近的中断（定时器到期或外部中断），并执行对应的中断服务 */
static void fake_sleep(void)
{
    g_critical_depth_in_sleep = g_crit_depth;

    const uint32_t timer_due = g_timer_armed_at + g_timer_span;
    const bool timer_first = g_timer_armed && (!g_irq_pending || EK_TICK_AFTER_EQ(g_irq_at, timer_due));

    if (timer_first)
    {
        g_real_now = timer_due;
        g_timer_armed = false;
        ek_evoke_delay_timer_callback();
    }
    else if (g_irq_pending)
    {
        g_real_now = g_irq_at;
        g_irq_pending = false;
        g_irq_handler();
    }
    else
    {
        g_idle = true;
    }
}

void ek_evoke_light_sleep(void)
{
    fake_sleep();
}

void ek_evoke_deep_sleep(void)
{
    fake_sleep();
}

/* 运行直到调度器真正空闲（没有定时器、没有外部中断可等） */
static void run_until_idle(int max_rounds)
{
    g_idle = false;
    for (int i = 0; i < max_rounds && !g_idle; i++) ek_evoke_run_once();
}

/* ---------------- 回调记录 ---------------- */

typedef struct
{
    int runs;
    uint32_t real[8];
} rec_t;

static void record_cb(ek_evoke_event_t *evt, void *arg)
{
    (void)evt;
    rec_t *r = (rec_t *)arg;
    if (r->runs < 8) r->real[r->runs] = g_real_now;
    r->runs++;
}

/* ---------------- 用例 ---------------- */

static void test_publish_during_callback_not_lost(void)
{
    rec_t rec = { 0 };
    ek_evoke_event_t *evt = ek_evoke_event_create(0);
    ek_evoke_task_t *tsk = ek_evoke_task_create(record_cb, &rec);
    ek_evoke_event_subscribe(tsk, evt);

    /* 第一次 publish 让任务就绪；后两次没有等待者，只能累加计数 */
    ek_evoke_event_publish(evt, NULL);
    ek_evoke_event_publish(evt, NULL);
    ek_evoke_event_publish(evt, NULL);
    run_until_idle(5);

    /* 旧实现：任务跑一次后直接挂回等待链表，积压的 2 次计数要等之后再 publish 才会被消费 */
    CHECK_EQ(rec.runs, 3);
    CHECK_EQ(evt->count, 0);
    CHECK_EQ(tsk->state, EK_EVOKE_STATE_WAITING);

    ek_evoke_task_destroy(tsk);
    ek_evoke_event_destroy(evt);
}

static ek_evoke_task_t *s_self;
static int s_self_runs;

static void destroy_self_cb(ek_evoke_event_t *evt, void *arg)
{
    (void)evt;
    (void)arg;
    s_self_runs++;
    ek_evoke_task_destroy(s_self); /* 回调里销毁自己 */
}

static void test_destroy_self_in_callback(void)
{
    ek_evoke_event_t *evt = ek_evoke_event_create(0);
    s_self = ek_evoke_task_create(destroy_self_cb, NULL);
    s_self_runs = 0;
    ek_evoke_event_subscribe(s_self, evt);
    ek_evoke_event_publish(evt, NULL);
    run_until_idle(5);
    CHECK_EQ(s_self_runs, 1);

    /* 任务已销毁：再次发布只会累加计数，不会访问已释放的任务 */
    ek_evoke_event_publish(evt, NULL);
    run_until_idle(5);
    CHECK_EQ(s_self_runs, 1);
    CHECK_EQ(evt->count, 1);
    ek_evoke_event_destroy(evt);
}

static ek_evoke_event_t *s_eb;

static void irq_post_defer_b(void)
{
    CHECK_EQ(ek_evoke_event_defer_from_isr(s_eb, NULL, 10, false), 0);
}

static void test_defer_inserted_mid_flight_does_not_drift(void)
{
    rec_t ra = { 0 }, rb = { 0 };
    ek_evoke_event_t *ea = ek_evoke_event_create(0);
    s_eb = ek_evoke_event_create(0);
    ek_evoke_task_t *ta = ek_evoke_task_create(record_cb, &ra);
    ek_evoke_task_t *tb = ek_evoke_task_create(record_cb, &rb);
    ek_evoke_event_subscribe(ta, ea);
    ek_evoke_event_subscribe(tb, s_eb);

    const uint32_t t0 = g_real_now;
    ek_evoke_event_defer(ea, NULL, 100, false); /* A：t0 + 100 */

    /* 定时器运行到第 30 tick 时，外部中断投递 B：再过 10 tick */
    g_irq_pending = true;
    g_irq_at = t0 + 30U;
    g_irq_handler = irq_post_defer_b;

    run_until_idle(20);

    CHECK_EQ(rb.runs, 1);
    CHECK_EQ(rb.real[0] - t0, 40);
    CHECK_EQ(ra.runs, 1);
    /* 旧实现：重装定时器时丢掉了已走过的 30 tick，A 被推迟到 t0 + 130 */
    CHECK_EQ(ra.real[0] - t0, 100);

    ek_evoke_task_destroy(ta);
    ek_evoke_task_destroy(tb);
    ek_evoke_event_destroy(ea);
    ek_evoke_event_destroy(s_eb);
}

static void test_defer_across_tick_wrap(void)
{
    rec_t r = { 0 };
    ek_evoke_event_t *evt = ek_evoke_event_create(0);
    ek_evoke_task_t *tsk = ek_evoke_task_create(record_cb, &r);
    ek_evoke_event_subscribe(tsk, evt);

    /* 把调度器时钟推进到回绕前 0x20 */
    const uint32_t target = 0xFFFFFFE0UL;
    while (ek_evoke_get_tick() != target)
    {
        uint32_t gap = target - ek_evoke_get_tick();
        if (gap > 0x40000000UL) gap = 0x40000000UL;
        ek_evoke_event_defer(evt, NULL, gap, false);
        run_until_idle(10);
    }
    r.runs = 0;

    const uint32_t start_real = g_real_now;
    ek_evoke_event_defer(evt, NULL, 0x40, false); /* 唤醒时刻回绕到 0x20 */
    run_until_idle(10);

    /* 旧实现用 >= 比较：回绕后的唤醒时刻被当成"已经过去"，立刻触发 */
    CHECK_EQ(r.runs, 1);
    CHECK_EQ(r.real[0] - start_real, 0x40);
    CHECK_EQ(ek_evoke_get_tick(), 0x20);

    ek_evoke_task_destroy(tsk);
    ek_evoke_event_destroy(evt);
}

static void test_destroy_event_purges_defers(void)
{
    rec_t r = { 0 };
    ek_evoke_event_t *evt = ek_evoke_event_create(0);
    ek_evoke_task_t *tsk = ek_evoke_task_create(record_cb, &r);
    ek_evoke_event_subscribe(tsk, evt);
    ek_evoke_event_defer(evt, NULL, 50, false);

    ek_evoke_event_destroy(evt); /* 旧实现：延时请求仍指向已释放的事件 */
    run_until_idle(10);
    CHECK_EQ(r.runs, 0);
    CHECK_EQ(tsk->state, EK_EVOKE_STATE_IDLE);
    ek_evoke_task_destroy(tsk);
}

static void test_isr_queue_holds_configured_capacity(void)
{
    rec_t r = { 0 };
    ek_evoke_event_t *evt = ek_evoke_event_create(0);
    ek_evoke_task_t *tsk = ek_evoke_task_create(record_cb, &r);
    ek_evoke_event_subscribe(tsk, evt);

    /* 旧实现 SPSC 容量少一个，只能放 EK_EVOKE_MAX_ISR_REQ - 1 个请求 */
    for (int i = 0; i < EK_EVOKE_MAX_ISR_REQ; i++) CHECK_EQ(ek_evoke_event_publish_from_isr(evt, NULL), 0);
    CHECK_EQ(ek_evoke_event_publish_from_isr(evt, NULL), -ENOSPC);

    run_until_idle(10);
    CHECK_EQ(r.runs, EK_EVOKE_MAX_ISR_REQ);
    ek_evoke_task_destroy(tsk);
    ek_evoke_event_destroy(evt);
}

static void test_sleep_called_inside_critical(void)
{
    rec_t r = { 0 };
    ek_evoke_event_t *evt = ek_evoke_event_create(0);
    ek_evoke_task_t *tsk = ek_evoke_task_create(record_cb, &r);
    ek_evoke_event_subscribe(tsk, evt);
    ek_evoke_event_defer(evt, NULL, 5, false);

    g_critical_depth_in_sleep = -1;
    run_until_idle(10);
    /* 检查"无事可做"与进入睡眠在同一临界区内，避免丢失唤醒 */
    CHECK_EQ(g_critical_depth_in_sleep, 1);
    CHECK_EQ(g_crit_depth, 0);

    ek_evoke_task_destroy(tsk);
    ek_evoke_event_destroy(evt);
}

int main(void)
{
    ek_heap_init();
    ek_evoke_init();

    RUN_TEST(test_publish_during_callback_not_lost);
    RUN_TEST(test_destroy_self_in_callback);
    RUN_TEST(test_defer_inserted_mid_flight_does_not_drift);
    RUN_TEST(test_defer_across_tick_wrap);
    RUN_TEST(test_destroy_event_purges_defers);
    RUN_TEST(test_isr_queue_holds_configured_capacity);
    RUN_TEST(test_sleep_called_inside_critical);
    return TEST_RESULT();
}
