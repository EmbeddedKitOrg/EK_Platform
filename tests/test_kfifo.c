#include "ek_test.h"
#include "ek/container/ek_kfifo.h"

typedef struct
{
    uint32_t id;
    ek_kfifo_node_t node; /* 故意不放在第一个成员，验证 container_of 偏移 */
    uint8_t payload[16];
} frame_t;

static void test_put_get_fifo_order(void)
{
    ek_kfifo_t q;
    frame_t f[4];
    CHECK_EQ(ek_kfifo_init(&q), 0);
    CHECK(ek_kfifo_is_empty(&q));
    CHECK(ek_kfifo_get(&q) == NULL);
    CHECK(ek_kfifo_get_entry(&q, frame_t, node) == NULL);

    for (uint32_t i = 0; i < 4; i++)
    {
        f[i].id = i;
        CHECK_EQ(ek_kfifo_put(&q, &f[i].node), 0);
    }
    CHECK_EQ(ek_kfifo_count(&q), 4);
    CHECK(ek_kfifo_peek_entry(&q, frame_t, node) == &f[0]);
    CHECK(ek_kfifo_peek_tail(&q) == &f[3].node);

    for (uint32_t i = 0; i < 4; i++)
    {
        frame_t *got = ek_kfifo_get_entry(&q, frame_t, node);
        CHECK(got == &f[i]); /* 零拷贝：拿回的就是原对象 */
    }
    CHECK(ek_kfifo_is_empty(&q));
    CHECK(ek_kfifo_peek_head(&q) == NULL);
    CHECK(ek_kfifo_peek_tail(&q) == NULL);
}

static void test_put_front_and_double_put(void)
{
    EK_DEFINE_KFIFO(q);
    frame_t a, b, c;
    CHECK_EQ(ek_kfifo_put(&q, &a.node), 0);
    CHECK_EQ(ek_kfifo_put(&q, &a.node), -EINVAL); /* 连续两次入队同一节点被拒绝 */
    CHECK_EQ(ek_kfifo_put(&q, &b.node), 0);
    CHECK_EQ(ek_kfifo_put_front(&q, &c.node), 0);
    CHECK_EQ(ek_kfifo_put_front(&q, &c.node), -EINVAL);
    CHECK_EQ(ek_kfifo_count(&q), 3);

    CHECK(ek_kfifo_get(&q) == &c.node);
    CHECK(ek_kfifo_get(&q) == &a.node);
    CHECK(ek_kfifo_get(&q) == &b.node);
    CHECK(ek_kfifo_get(&q) == NULL);

    /* 空队列 put_front 后 tail 也要正确 */
    CHECK_EQ(ek_kfifo_put_front(&q, &a.node), 0);
    CHECK(ek_kfifo_peek_tail(&q) == &a.node);
    CHECK_EQ(ek_kfifo_put(&q, &b.node), 0);
    CHECK(ek_kfifo_get(&q) == &a.node);
    CHECK(ek_kfifo_get(&q) == &b.node);
}

static void test_get_all_and_put_list(void)
{
    EK_DEFINE_KFIFO(q);
    EK_DEFINE_KFIFO(q2);
    frame_t f[5];
    for (uint32_t i = 0; i < 5; i++)
    {
        f[i].id = i;
        ek_kfifo_put(&q, &f[i].node);
    }

    uint32_t n = 0;
    ek_kfifo_node_t *chain = ek_kfifo_get_all(&q, &n);
    CHECK_EQ(n, 5);
    CHECK(ek_kfifo_is_empty(&q));

    ek_kfifo_node_t *pos, *nx;
    uint32_t expect = 0;
    ek_kfifo_chain_foreach_safe(pos, nx, chain)
    {
        CHECK_EQ(ek_kfifo_entry(pos, frame_t, node)->id, expect);
        expect++;
        ek_kfifo_put(&q2, pos); /* 遍历中重新入队到另一个 FIFO */
    }
    CHECK_EQ(expect, 5);
    CHECK_EQ(ek_kfifo_count(&q2), 5);

    /* put_list：把 q2 整条链拼回 q 的尾部 */
    frame_t head;
    head.id = 100;
    ek_kfifo_put(&q, &head.node);
    chain = ek_kfifo_get_all(&q2, &n);
    CHECK_EQ(ek_kfifo_put_list(&q, chain, &f[4].node), 0);
    CHECK_EQ(ek_kfifo_count(&q), 6);
    CHECK(ek_kfifo_get(&q) == &head.node);
    for (uint32_t i = 0; i < 5; i++) CHECK(ek_kfifo_get(&q) == &f[i].node);

    /* last 不在链上 */
    f[0].node.next = NULL;
    CHECK_EQ(ek_kfifo_put_list(&q, &f[0].node, &f[1].node), -EINVAL);
    CHECK(ek_kfifo_is_empty(&q));
}

static void test_remove(void)
{
    EK_DEFINE_KFIFO(q);
    frame_t f[4];
    for (int i = 0; i < 4; i++) ek_kfifo_put(&q, &f[i].node);

    CHECK(ek_kfifo_remove(&q, &f[1].node)); /* 中间 */
    CHECK(ek_kfifo_remove(&q, &f[3].node)); /* 队尾 */
    CHECK(!ek_kfifo_remove(&q, &f[3].node));
    CHECK(ek_kfifo_peek_tail(&q) == &f[2].node);
    CHECK(ek_kfifo_remove(&q, &f[0].node)); /* 队首 */
    CHECK_EQ(ek_kfifo_count(&q), 1);
    CHECK(ek_kfifo_peek_head(&q) == &f[2].node);

    /* 删除后继续入队，tail 必须正确 */
    ek_kfifo_put(&q, &f[0].node);
    CHECK(ek_kfifo_get(&q) == &f[2].node);
    CHECK(ek_kfifo_get(&q) == &f[0].node);
    CHECK(ek_kfifo_remove(&q, &f[0].node) == false);
}

static void test_zero_copy_pool_pipeline(void)
{
    static frame_t pool[8];
    EK_DEFINE_KFIFO(free_q);
    EK_DEFINE_KFIFO(rx_q);

    CHECK_EQ(EK_KFIFO_FILL(&free_q, pool, node), 0);
    CHECK_EQ(ek_kfifo_count(&free_q), 8);

    /* "ISR" 从空闲池取帧、写入、投递；"主循环" 处理后归还 */
    uint32_t produced = 0, consumed = 0;
    for (int round = 0; round < 20; round++)
    {
        frame_t *f;
        while ((f = ek_kfifo_get_entry(&free_q, frame_t, node)) != NULL)
        {
            CHECK(f >= &pool[0] && f <= &pool[7]);
            f->id = produced++;
            f->payload[0] = (uint8_t)f->id;
            ek_kfifo_put(&rx_q, &f->node);
        }
        CHECK_EQ(ek_kfifo_count(&rx_q), 8);
        while ((f = ek_kfifo_get_entry(&rx_q, frame_t, node)) != NULL)
        {
            CHECK_EQ(f->id, consumed);
            CHECK_EQ(f->payload[0], (uint8_t)consumed);
            consumed++;
            ek_kfifo_put(&free_q, &f->node);
        }
    }
    CHECK_EQ(produced, consumed);
    CHECK_EQ(ek_kfifo_count(&free_q), 8);

    CHECK_EQ(ek_kfifo_fill(&free_q, pool, sizeof(pool[0]), 0, 0), -EINVAL);
    CHECK_EQ(ek_kfifo_fill(&free_q, pool, sizeof(pool[0]), 1, sizeof(pool[0])), -EINVAL);
}

static void test_null_args(void)
{
    ek_kfifo_node_t n;
    CHECK_EQ(ek_kfifo_init(NULL), -EINVAL);
    CHECK_EQ(ek_kfifo_put(NULL, &n), -EINVAL);
    CHECK(ek_kfifo_get(NULL) == NULL);
    uint32_t cnt = 123;
    CHECK(ek_kfifo_get_all(NULL, &cnt) == NULL);
    CHECK_EQ(cnt, 0);
    CHECK(ek_kfifo_is_empty(NULL));
}

int main(void)
{
    RUN_TEST(test_put_get_fifo_order);
    RUN_TEST(test_put_front_and_double_put);
    RUN_TEST(test_get_all_and_put_list);
    RUN_TEST(test_remove);
    RUN_TEST(test_zero_copy_pool_pipeline);
    RUN_TEST(test_null_args);
    return TEST_RESULT();
}
