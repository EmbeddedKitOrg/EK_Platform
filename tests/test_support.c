/**
 * @file test_support.c
 * @brief 所有测试程序共用的桩函数
 */

#include <stdio.h>
#include <stdlib.h>

#include "ek/core/ek_assert.h"

/* 断言失败直接退出，避免 ek_assert_fault 的死循环把测试卡住 */
void ek_assert_hook(const char *file, uint32_t line, const char *expr)
{
    printf("ASSERT FAILED %s:%lu: %s\n", file, (unsigned long)line, expr);
    exit(2);
}
