#include "ek_test.h"
#include "ek/sched/ek_picothread.h"
#include "ek/mem/ek_heap.h"

/* 推进到 tick=now：反复调度直到没有就绪任务（每个 tick 最多 64 次，防止忙任务卡死测试） */
static void run_at(uint32_t now)
{
    for (int i = 0; i < 64; i++)
    {
        const uint32_t r = ek_pt_schedule(now);
        const uint32_t ready_ret = (now == 0U) ? 1U : now;
        if (r != ready_ret) break;
    }
}

static void run_range(uint32_t from, uint32_t to)
{
    for (uint32_t t = from; t != to; t++) run_at(t);
}

/* ---------------- 信号量直接移交 ---------------- */

static ek_pt_sem_t s_sem;
static int s_take_err;
static int s_take_got;
static uint32_t s_take_tick;
static uint32_t s_now;

static void taker_task(ek_pt_t *pt, void *arg)
{
    uint32_t timeout = (uint32_t)(uintptr_t)arg;
    EK_PT_BEGIN(pt);
    EK_PT_SEM_TAKE(&s_sem, timeout, s_take_err);
    s_take_got = (s_take_err == 0);
    s_take_tick = s_now;
    EK_PT_SUSPEND(NULL);
    EK_PT_END(pt);
}

static void giver_task(ek_pt_t *pt, void *arg)
{
    (void)arg;
    EK_PT_BEGIN(pt);
    EK_PT_DELAY(5);
    EK_PT_SEM_GIVE(&s_sem);
    EK_PT_SUSPEND(NULL);
    EK_PT_END(pt);
}

static void run_with_now(uint32_t from, uint32_t to)
{
    for (uint32_t t = from; t != to; t++)
    {
        s_now = t;
        run_at(t);
    }
}

static void test_sem_give_is_handed_to_waiter(void)
{
    ek_pt_sem_init_static(&s_sem, 0);
    s_take_err = 999;
    s_take_got = 0;

    ek_pt_t *taker = ek_pt_create(taker_task, 1, (void *)(uintptr_t)100U);
    ek_pt_t *giver = ek_pt_create(giver_task, 2, NULL);
    run_with_now(0, 20);

    /* 旧实现：give 被丢失，taker 一直等到第 100 tick 超时 */
    CHECK_EQ(s_take_got, 1);
    CHECK_EQ(s_take_err, 0);
    CHECK(s_take_tick <= 6U);
    CHECK_EQ(s_sem.count, 0);

    ek_pt_destroy(taker);
    ek_pt_destroy(giver);
}

static ek_pt_sem_t s_sem2;
static int s_e1, s_e2;

static void stale_task(ek_pt_t *pt, void *arg)
{
    (void)arg;
    EK_PT_BEGIN(pt);
    EK_PT_SEM_TAKE(&s_sem2, 3, s_e1); /* 超时 */
    EK_PT_SEM_GIVE(&s_sem2);
    EK_PT_SEM_TAKE(&s_sem2, 3, s_e2); /* 立即成功 */
    EK_PT_SUSPEND(NULL);
    EK_PT_END(pt);
}

static void test_err_not_stale_after_timeout(void)
{
    ek_pt_sem_init_static(&s_sem2, 0);
    s_e1 = s_e2 = 999;
    ek_pt_t *t = ek_pt_create(stale_task, 0, NULL);
    run_range(100, 120);

    CHECK_EQ(s_e1, -ETIMEDOUT);
    /* 旧实现：e2 会沿用上一次的 -ETIMEDOUT，但信号量其实已经被拿走 */
    CHECK_EQ(s_e2, 0);
    CHECK_EQ(s_sem2.count, 0);
    ek_pt_destroy(t);
}

/* ---------------- 挂起/销毁正在等待的任务 ---------------- */

static void test_suspend_waiter_then_give(void)
{
    ek_pt_sem_init_static(&s_sem, 0);
    s_take_err = 999;
    s_take_got = 0;

    ek_pt_t *taker = ek_pt_create(taker_task, 1, (void *)(uintptr_t)EK_PT_WAIT_FOREVER);
    run_range(200, 202);
    CHECK_EQ(taker->state, EK_PT_STATE_BLOCK);

    CHECK(ek_pt_suspend(taker) == NULL); /* 挂起其他任务：不需要让出 */
    CHECK_EQ(taker->state, EK_PT_STATE_SUSPEND);

    /* 旧实现：等待队列里还挂着已挂起的任务，give 时对已摘下的节点再次 remove → 空指针 */
    ek_pt_sem_give(&s_sem);
    CHECK_EQ(s_sem.count, 1); /* 没有有效等待者，计数增加 */
    CHECK_EQ(taker->state, EK_PT_STATE_SUSPEND);

    EK_PT_RESUME(taker);
    run_range(202, 204);
    CHECK_EQ(s_take_err, -EINTR);
    CHECK_EQ(s_take_got, 0);

    ek_pt_destroy(taker);
}

static void test_destroy_waiter(void)
{
    ek_pt_sem_init_static(&s_sem, 0);
    ek_pt_t *taker = ek_pt_create(taker_task, 1, (void *)(uintptr_t)50U);
    run_range(300, 302);
    ek_pt_destroy(taker);
    ek_pt_sem_give(&s_sem);
    CHECK_EQ(s_sem.count, 1);
    CHECK(ek_list_is_empty(&s_sem.wait_list));
}

/* ---------------- 优先级 ---------------- */

static int s_order[4];
static int s_order_n;

static void prio_waiter(ek_pt_t *pt, void *arg)
{
    static int err_by_prio[8];
    EK_PT_BEGIN(pt);
    EK_PT_SEM_TAKE(&s_sem, EK_PT_WAIT_FOREVER, err_by_prio[pt->prio]);
    if (err_by_prio[pt->prio] == 0) s_order[s_order_n++] = (int)(uintptr_t)arg;
    EK_PT_SUSPEND(NULL);
    EK_PT_END(pt);
}

static void test_sem_wakes_highest_priority_first(void)
{
    ek_pt_sem_init_static(&s_sem, 0);
    s_order_n = 0;
    ek_pt_t *low = ek_pt_create(prio_waiter, 5, (void *)(uintptr_t)5U);
    ek_pt_t *high = ek_pt_create(prio_waiter, 1, (void *)(uintptr_t)1U);
    run_range(400, 402);

    ek_pt_sem_give(&s_sem);
    run_range(402, 404);
    ek_pt_sem_give(&s_sem);
    run_range(404, 406);

    CHECK_EQ(s_order_n, 2);
    CHECK_EQ(s_order[0], 1);
    CHECK_EQ(s_order[1], 5);
    ek_pt_destroy(low);
    ek_pt_destroy(high);
}

/* ---------------- tick 回绕 ---------------- */

static uint32_t s_wake_at;
static int s_woke;

static void delay_task(ek_pt_t *pt, void *arg)
{
    (void)arg;
    EK_PT_BEGIN(pt);
    EK_PT_DELAY(32);
    s_woke = 1;
    s_wake_at = s_now;
    EK_PT_SUSPEND(NULL);
    EK_PT_END(pt);
}

static void test_delay_across_tick_wrap(void)
{
    s_woke = 0;
    ek_pt_t *t = ek_pt_create(delay_task, 0, NULL);
    const uint32_t start = 0xFFFFFFF0UL;
    s_now = start;
    run_at(start); /* 任务开始延时，唤醒点 = 0x10（回绕后） */

    /* 旧实现直接比较 tick > now：回绕前的 now 都"大于" 0x10，任务会被立刻唤醒 */
    run_with_now(start + 1U, 0x0FU);
    CHECK_EQ(s_woke, 0);
    run_with_now(0x0FU, 0x12U);
    CHECK_EQ(s_woke, 1);
    CHECK_EQ(s_wake_at, 0x10U);
    ek_pt_destroy(t);
}

static void test_schedule_return_values(void)
{
    /* 没有任务：0 表示可以无限睡眠 */
    CHECK_EQ(ek_pt_schedule(500), 0);

    s_woke = 0;
    ek_pt_t *t = ek_pt_create(delay_task, 0, NULL);
    /* now == 0 且有就绪任务：不能返回 0（会被误认为无限睡眠） */
    CHECK(ek_pt_schedule(0) != 0U);
    ek_pt_destroy(t);
}

/* ---------------- 消息队列直接移交 ---------------- */

static ek_pt_msg_t *s_msg;
static uint32_t s_rx_buf;
static int s_rx_err;
static uint32_t s_rx_vals[8];
static int s_rx_n;

static void receiver_task(ek_pt_t *pt, void *arg)
{
    (void)arg;
    EK_PT_BEGIN(pt);
    while (s_rx_n < 4)
    {
        EK_PT_MSG_RECV(s_msg, &s_rx_buf, EK_PT_WAIT_FOREVER, s_rx_err);
        if (s_rx_err != 0) break;
        s_rx_vals[s_rx_n++] = s_rx_buf;
    }
    EK_PT_SUSPEND(NULL);
    EK_PT_END(pt);
}

static uint32_t s_tx_vals[4] = { 11, 22, 33, 44 };
static int s_tx_err[4];
static int s_tx_i;

static void sender_task(ek_pt_t *pt, void *arg)
{
    (void)arg;
    EK_PT_BEGIN(pt);
    for (s_tx_i = 0; s_tx_i < 4; s_tx_i++)
    {
        EK_PT_MSG_SEND(s_msg, &s_tx_vals[s_tx_i], EK_PT_WAIT_FOREVER, s_tx_err[s_tx_i]);
    }
    EK_PT_SUSPEND(NULL);
    EK_PT_END(pt);
}

static void test_msg_handoff_both_directions(void)
{
    /* 容量 1：发送方会因满而等待，接收方也会因空而等待 */
    s_msg = ek_pt_msg_create(sizeof(uint32_t), 1);
    CHECK(s_msg != NULL);
    if (s_msg == NULL) return;
    s_rx_n = 0;

    ek_pt_t *rx = ek_pt_create(receiver_task, 2, NULL);
    run_range(600, 602); /* 接收方先进入等待 */
    ek_pt_t *tx = ek_pt_create(sender_task, 1, NULL);
    run_range(602, 640);

    CHECK_EQ(s_rx_n, 4);
    for (int i = 0; i < 4; i++)
    {
        CHECK_EQ(s_rx_vals[i], s_tx_vals[i]);
        CHECK_EQ(s_tx_err[i], 0);
    }
    CHECK(ek_ringbuf_empty(&s_msg->rb));

    ek_pt_destroy(rx);
    ek_pt_destroy(tx);
    ek_pt_msg_destroy(s_msg);
}

static void test_msg_recv_timeout(void)
{
    s_msg = ek_pt_msg_create(sizeof(uint32_t), 2);
    s_rx_n = 0;
    s_rx_err = 999;
    ek_pt_t *rx = ek_pt_create(receiver_task, 2, NULL);
    run_range(700, 703);
    CHECK_EQ(s_rx_n, 0);

    /* 销毁队列：等待者以 -ECANCELED 醒来，不会访问已释放的队列 */
    ek_pt_msg_destroy(s_msg);
    s_msg = NULL;
    run_at(703);
    CHECK_EQ(s_rx_err, -ECANCELED);
    ek_pt_destroy(rx);
}

int main(void)
{
    ek_heap_init();
    ek_pt_init();

    RUN_TEST(test_sem_give_is_handed_to_waiter);
    RUN_TEST(test_err_not_stale_after_timeout);
    RUN_TEST(test_suspend_waiter_then_give);
    RUN_TEST(test_destroy_waiter);
    RUN_TEST(test_sem_wakes_highest_priority_first);
    RUN_TEST(test_delay_across_tick_wrap);
    RUN_TEST(test_schedule_return_values);
    RUN_TEST(test_msg_handoff_both_directions);
    RUN_TEST(test_msg_recv_timeout);
    return TEST_RESULT();
}
