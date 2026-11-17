#include "ek_test.h"
#include "ek/container/ek_ringbuf.h"
#include "ek/container/ek_stack.h"
#include "ek/container/ek_snapshot.h"
#include "ek/mem/ek_heap.h"

static void test_ringbuf_wraps_many_times(void)
{
    ek_ringbuf_t *rb = ek_ringbuf_create(sizeof(uint32_t), 3);
    CHECK(rb != NULL);
    if (rb == NULL) return;

    uint32_t next_in = 0U, next_out = 0U;
    for (int round = 0; round < 100; round++)
    {
        while (ek_ringbuf_write(rb, &next_in) == 0) next_in++;
        CHECK(ek_ringbuf_full(rb));
        uint32_t v;
        CHECK_EQ(ek_ringbuf_peek(rb, &v), 0);
        CHECK_EQ(v, next_out);
        /* 每轮只读出 2 个，让读写索引持续错开并反复回绕 */
        for (int k = 0; k < 2; k++)
        {
            CHECK_EQ(ek_ringbuf_read(rb, &v), 0);
            CHECK_EQ(v, next_out);
            next_out++;
        }
    }
    uint32_t v;
    while (ek_ringbuf_read(rb, &v) == 0)
    {
        CHECK_EQ(v, next_out);
        next_out++;
    }
    CHECK_EQ(next_out, next_in);
    CHECK_EQ(ek_ringbuf_read(rb, &v), -ENODATA);
    ek_ringbuf_destroy(rb);
}

static void test_spsc_capacity_and_order(void)
{
    ek_ringbuf_spsc_t *rb = ek_ringbuf_create_spsc(sizeof(uint16_t), 5);
    CHECK(rb != NULL);
    if (rb == NULL) return;

    uint16_t in = 0U, out = 0U, v;
    for (int round = 0; round < 50; round++)
    {
        int written = 0;
        while (ek_ringbuf_write_spsc(rb, &in) == 0)
        {
            in++;
            written++;
        }
        /* 无锁 SPSC 实际容量为 cap - 1 */
        CHECK(written == 4 || round > 0);
        CHECK(ek_ringbuf_full_spsc(rb));
        CHECK_EQ(ek_ringbuf_read_spsc(rb, &v), 0);
        CHECK_EQ(v, out);
        out++;
    }
    while (ek_ringbuf_read_spsc(rb, &v) == 0)
    {
        CHECK_EQ(v, out);
        out++;
    }
    CHECK_EQ(out, in);
    ek_ringbuf_destroy_spsc(rb);
}

static void test_snapshot_rejects_zero_id(void)
{
    ek_snapshot_t *s = ek_snapshot_create(sizeof(uint32_t));
    CHECK(s != NULL);
    if (s == NULL) return;

    uint32_t v = 42U, out = 0U;
    CHECK_EQ(ek_snapshot_get(s, &out), -ENODATA);
    CHECK_EQ(ek_snapshot_set(s, &v, 0U), -EINVAL);
    CHECK_EQ(ek_snapshot_set(s, &v, 7U), 0);
    CHECK_EQ(ek_snapshot_get(s, &out), 0);
    CHECK_EQ(out, 42);
    ek_snapshot_destroy(s);
}

static void test_stack_lifo(void)
{
    ek_stack_t *sk = ek_stack_create(sizeof(int), 4);
    CHECK(sk != NULL);
    if (sk == NULL) return;

    for (int i = 0; i < 4; i++) CHECK_EQ(ek_stack_push(sk, &i), 0);
    int extra = 99;
    CHECK_EQ(ek_stack_push(sk, &extra), -ENOSPC);
    for (int i = 3; i >= 0; i--)
    {
        int v = -1;
        CHECK_EQ(ek_stack_pop(sk, &v), 0);
        CHECK_EQ(v, i);
    }
    ek_stack_destroy(sk);
}

int main(void)
{
    ek_heap_init();
    RUN_TEST(test_ringbuf_wraps_many_times);
    RUN_TEST(test_spsc_capacity_and_order);
    RUN_TEST(test_snapshot_rejects_zero_id);
    RUN_TEST(test_stack_lifo);
    return TEST_RESULT();
}
