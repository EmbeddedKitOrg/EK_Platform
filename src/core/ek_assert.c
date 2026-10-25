/**
 * @file ek_assert.c
 * @brief 断言机制实现
 * @author N1netyNine99
 */

#include "ek_assert.h"

#if EKCFG_ASSERT

#    include "ek_log.h"

void ek_assert_fault(const char *file, uint32_t line, const char *expr)
{
#    if EKCFG_ASSERT_LOG == 1
    EK_LOG_FILE_TAG("ek_assert.c");
    EK_LOG_ERROR("file:%s,line:%" PRIu32 ",expr: %s", EK_GET_FILE_NAME(file), line, expr);
#    else
    __EK_UNUSED(file);
    __EK_UNUSED(line);
    __EK_UNUSED(expr);
#    endif /* EKCFG_ASSERT_LOG */

    ek_assert_hook(file, line, expr);

    while (1)
    {
    }
}

__EK_WEAK void ek_assert_hook(const char *file, uint32_t line, const char *expr)
{
    __EK_UNUSED(file);
    __EK_UNUSED(line);
    __EK_UNUSED(expr);
}

#endif // EKCFG_ASSERT
