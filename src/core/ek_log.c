/**
 * @file ek_log.c
 * @brief 日志系统实现
 * @author N1netyNine99
 */

#include "ek_log.h"

#if EKCFG_LOG == 1

#    define EK_LOG_COLOR_NONE  "\033[0;0m"
#    define EK_LOG_COLOR_INFO  "\033[94m"
#    define EK_LOG_COLOR_DEBUG "\033[92m"
#    define EK_LOG_COLOR_WARN  "\033[33m"
#    define EK_LOG_COLOR_ERROR "\033[91m"
#    define EK_LOG_COLOR_FATAL "\033[30;41m"

#    if (EKCFG_LOG_COLOR == 1)

static const char *const s_log_color_table[EK_LOG_LEVEL_MAX] = {
    EK_LOG_COLOR_NONE, EK_LOG_COLOR_DEBUG, EK_LOG_COLOR_INFO, EK_LOG_COLOR_WARN, EK_LOG_COLOR_ERROR, EK_LOG_COLOR_FATAL,
};

#    endif /* (EKCFG_LOG_COLOR == 1) */

static const char *s_log_type_table[EK_LOG_LEVEL_MAX] = { "None", "Debug", "Info", "Warn", "Error", "Fatal" };

static char s_log_buffer[EKCFG_LOG_BUF_SIZE];

__EK_WEAK uint32_t ek_port_log_get_tick(void)
{
    return 0;
}

void ek_log_printf(const char *tag, uint32_t line, ek_log_level_t type, uint32_t tick, const char *fmt, ...)
{
#    if (EKCFG_LOG_COLOR == 1)
    ek_printf(
        "%s[%s/%s L:%" PRIu32 ",T:%" PRIu32 "]:", s_log_color_table[type], s_log_type_table[type], tag, line, tick);
#    else /* EKCFG_LOG_COLOR == 1 */
    ek_printf("[%s/%s L:%" PRIu32 ",T:%" PRIu32 "]:", s_log_type_table[type], tag, line, tick);
#    endif /* EKCFG_LOG_COLOR == 1 */

    va_list args;
    va_start(args, fmt);
    ek_vsnprintf(s_log_buffer, EKCFG_LOG_BUF_SIZE - 1, fmt, args);
    va_end(args);

    ek_printf("%s", s_log_buffer);

#    if (EKCFG_LOG_COLOR == 1)
    ek_printf(EK_LOG_COLOR_NONE); // 恢复日志颜色
#    endif /* EKCFG_LOG_COLOR == 1 */

    ek_printf(CRLF); // 换行符
}

#endif /* EKCFG_LOG */
