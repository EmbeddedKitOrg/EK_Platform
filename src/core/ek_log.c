/**
 * @file ek_log.c
 * @brief 日志系统实现
 * @author N1netyNine99
 */

#include "ek/core/ek_log.h"

#if EKCFG_LOG == 1

#    define COLOR_NONE  "\033[0;0m"
#    define COLOR_INFO  "\033[94m"
#    define COLOR_DEBUG "\033[92m"
#    define COLOR_WARN  "\033[33m"
#    define COLOR_ERROR "\033[91m"
#    define COLOR_FATAL "\033[30;41m"

#    if (EKCFG_LOG_COLOR == 1)

static const char *const s_log_color_table[EK_LOG_LEVEL_MAX] = {
    COLOR_NONE, COLOR_DEBUG, COLOR_INFO, COLOR_WARN, COLOR_ERROR, COLOR_FATAL,
};

#    endif /* (EKCFG_LOG_COLOR == 1) */

static const char *s_log_type_table[EK_LOG_LEVEL_MAX] = { "None", "Debug", "Info", "Warn", "Error", "Fatal" };

static char s_log_buffer[EKCFG_LOG_BUF_SIZE];

__EK_WEAK uint32_t ek_port_log_get_tick(void)
{
    return 0;
}

void ek_log_printf(const char *tag, uint32_t line, ek_log_level_e type, uint32_t tick, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ek_vsnprintf(s_log_buffer, sizeof(s_log_buffer), fmt, args);
    va_end(args);

#    if EKCFG_LOG_COLOR == 1
    ek_printf("%s[%s/%s L:%" PRIu32 ",T:%" PRIu32 "]:%s" COLOR_NONE CRLF,
              s_log_color_table[type],
              s_log_type_table[type],
              tag,
              line,
              tick,
              s_log_buffer);
#    else /* EKCFG_LOG_COLOR == 1 */
    ek_printf("[%s/%s L:%" PRIu32 ",T:%" PRIu32 "]:%s" CRLF, s_log_type_table[type], tag, line, tick, s_log_buffer);
#    endif /* EKCFG_LOG_COLOR == 1 */
}

#endif /* EKCFG_LOG */
