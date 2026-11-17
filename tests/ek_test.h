/**
 * @file ek_test.h
 * @brief 主机单元测试用的极简断言框架（无第三方依赖）
 */

#ifndef EK_TEST_H
#define EK_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static int ek_test_failures;

#define CHECK(cond)                                                          \
    do                                                                       \
    {                                                                        \
        if (!(cond))                                                         \
        {                                                                    \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            ek_test_failures++;                                              \
        }                                                                    \
    } while (0)

#define CHECK_EQ(actual, expected)                                                                 \
    do                                                                                             \
    {                                                                                              \
        const long _a = (long)(actual);                                                            \
        const long _e = (long)(expected);                                                          \
        if (_a != _e)                                                                              \
        {                                                                                          \
            printf("    FAIL %s:%d: %s == %s (got %ld, expected %ld)\n", __FILE__, __LINE__, #actual, \
                   #expected, _a, _e);                                                             \
            ek_test_failures++;                                                                    \
        }                                                                                          \
    } while (0)

#define RUN_TEST(fn)                                                                   \
    do                                                                                 \
    {                                                                                  \
        const int _before = ek_test_failures;                                          \
        fn();                                                                          \
        printf("%s %s\n", (_before == ek_test_failures) ? "[  OK  ]" : "[ FAIL ]", #fn); \
    } while (0)

#define TEST_RESULT() (ek_test_failures == 0 ? 0 : 1)

#endif /* EK_TEST_H */
