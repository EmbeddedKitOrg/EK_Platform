#include "ek_test.h"
#include "ek/container/ek_list.h"

static void test_remove_is_idempotent(void)
{
    ek_list_node_t head, a, b;
    ek_list_init(&head);
    ek_list_init(&a);
    ek_list_init(&b);

    CHECK(!ek_list_node_is_linked(&a));
    ek_list_insert_tail(&head, &a);
    ek_list_insert_tail(&head, &b);
    CHECK(ek_list_node_is_linked(&a));

    ek_list_remove(&a);
    CHECK(!ek_list_node_is_linked(&a));
    /* 旧实现把 prev/next 置 NULL，第二次移除会解引用空指针 */
    ek_list_remove(&a);
    CHECK(ek_list_get_first(&head) == &b);
    CHECK(ek_list_get_last(&head) == &b);

    ek_list_remove(&b);
    CHECK(ek_list_is_empty(&head));
}

static void test_insert_order(void)
{
    ek_list_node_t head, n[4];
    ek_list_init(&head);
    ek_list_insert_tail(&head, &n[1]);
    ek_list_insert_head(&head, &n[0]);
    ek_list_insert_tail(&head, &n[3]);
    ek_list_insert_before(&n[3], &n[2]);

    ek_list_node_t *pos;
    int i = 0;
    ek_list_foreach(pos, &head)
    {
        CHECK(pos == &n[i]);
        i++;
    }
    CHECK_EQ(i, 4);
}

int main(void)
{
    RUN_TEST(test_remove_is_idempotent);
    RUN_TEST(test_insert_order);
    return TEST_RESULT();
}
