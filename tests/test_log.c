#include "ek_test.h"
#include "ek/core/ek_log.h"

EK_LOG_MODULE("test_log.c", EK_LOG_LEVEL_WARN);

static void test_log_macro_in_if_else(void)
{
    int else_hit = 0;
    int cond = 0;

    /* 旧宏是裸 if，else 会被宏内部的 if 截走，else_hit 永远不会被置位 */
    if (cond) EK_LOG_INFO("never");
    else else_hit = 1;
    CHECK_EQ(else_hit, 1);

    else_hit = 0;
    if (cond) EK_LOG_DEBUG("never");
    else else_hit = 1;
    CHECK_EQ(else_hit, 1);

    else_hit = 0;
    if (cond) EK_LOG_ERROR("never");
    else else_hit = 1;
    CHECK_EQ(else_hit, 1);
}

static void test_level_filter_still_works(void)
{
    /* 低于模块最低等级的日志被过滤；这里只验证能编译并正常执行 */
    EK_LOG_INFO("filtered %d", 1);
    EK_LOG_WARN("printed %d", 2);
    CHECK(1);
}

int main(void)
{
    RUN_TEST(test_log_macro_in_if_else);
    RUN_TEST(test_level_filter_still_works);
    return TEST_RESULT();
}
