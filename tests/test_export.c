/*
 * ek_export / ek_static_alloc 的执行顺序测试。
 *
 * 真实固件里条目放在 .ek_export_fn / .ek_static_alloc 段，边界符号由链接脚本提供。
 * 主机上没有这两个段，这里把边界符号重定义为指向测试数组的指针，然后直接 include 实现文件。
 */

#define EKCFG_EXPORT           (1)
#define EKCFG_EXPORT_AUTO_INIT (0)
#define EKCFG_STATIC_ALLOC     (1)

#include "ek_test.h"
#include "ek/core/ek_export.h"
#include "ek/mem/ek_static_alloc.h"

static int s_log[16];
static int s_log_n;

static void f0(void) { s_log[s_log_n++] = 0; }
static void f1(void) { s_log[s_log_n++] = 1; }
static void f2(void) { s_log[s_log_n++] = 2; }
static void f3(void) { s_log[s_log_n++] = 3; }
static void f4(void) { s_log[s_log_n++] = 4; }

/* 段内顺序故意打乱；f1 与 f3 的 (level, order) 相同，应保持段内先后 */
static const _ek_export_item_t s_export_items[] = {
    { 2, 1, f4 },
    { 0, 5, f1 },
    { 2, 0, f2 },
    { 0, 5, f3 },
    { 0, 0, f0 },
};

const _ek_export_item_t *test_export_begin = &s_export_items[0];
const _ek_export_item_t *test_export_end = &s_export_items[sizeof(s_export_items) / sizeof(s_export_items[0])];

#define _ek_export_fn_start (*test_export_begin)
#define _ek_export_fn_end   (*test_export_end)
#include "../src/core/ek_export.c"

static int a_init(void) { s_log[s_log_n++] = 10; return 0; }
static int b_init(void) { s_log[s_log_n++] = 11; return -ENOMEM; }
static int c_init(void) { s_log[s_log_n++] = 12; return -EINVAL; }

static const _ek_static_alloc_item_t s_alloc_items[] = {
    { 20, "c", c_init },
    { 10, "a", a_init },
    { 20, "b_never", NULL },
    { 15, "b", b_init },
};

const _ek_static_alloc_item_t *test_alloc_begin = &s_alloc_items[0];
const _ek_static_alloc_item_t *test_alloc_end = &s_alloc_items[sizeof(s_alloc_items) / sizeof(s_alloc_items[0])];

#define _ek_static_alloc_start (*test_alloc_begin)
#define _ek_static_alloc_end   (*test_alloc_end)
#include "../src/mem/ek_static_alloc.c"

static void test_export_order_and_once(void)
{
    s_log_n = 0;
    ek_export_init();
    CHECK_EQ(s_log_n, 5);
    CHECK_EQ(s_log[0], 0);
    CHECK_EQ(s_log[1], 1);
    CHECK_EQ(s_log[2], 3);
    CHECK_EQ(s_log[3], 2);
    CHECK_EQ(s_log[4], 4);

    /* 再次调用不会重复执行（避免重复初始化堆等模块） */
    ek_export_init();
    CHECK_EQ(s_log_n, 5);
}

static void test_static_alloc_order_and_first_error(void)
{
    s_log_n = 0;
    ek_static_alloc_init();
    CHECK_EQ(s_log_n, 3);
    CHECK_EQ(s_log[0], 10);
    CHECK_EQ(s_log[1], 11);
    CHECK_EQ(s_log[2], 12);
    CHECK_EQ(ek_static_alloc_get_init_error(), -ENOMEM);
}

int main(void)
{
    RUN_TEST(test_export_order_and_once);
    RUN_TEST(test_static_alloc_order_and_first_error);
    return TEST_RESULT();
}
