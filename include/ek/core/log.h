/**
 * @file log.h
 * @brief 日志系统
 * @author N1netyNine99
 *
 * 提供分级日志输出功能，支持：
 * - 多级别日志（NONE、DEBUG、INFO、WARN、ERROR、FATAL）
 * - 文件名、行号、时间戳输出
 * - 彩色打印（可选）
 * - 格式化输出
 *
 * @note 使用前需实现 ek_port_log_get_tick() 函数，用于获取系统时间戳
 */

#ifndef EK_LOG_H
#define EK_LOG_H

#include "ek/conf_internal.h"

#if EKCFG_LOG == 1

#    include "ek/core/io.h"
#    include "ek/core/def.h"

/**
 * @brief 是否启用彩色打印
 * @note 1 = 启用 ANSI 颜色代码，0 = 纯文本输出
 */
#    ifndef EKCFG_LOG_COLOR
#        define EKCFG_LOG_COLOR (0)
#    endif /* EKCFG_LOG_COLOR   */

/**
 * @brief 是否启用 DEBUG 级别日志
 * @note 1 = 启用，0 = 禁用（可减小代码体积）
 */
#    ifndef EKCFG_LOG_DEBUG
#        define EKCFG_LOG_DEBUG (1)
#    endif /* EKCFG_LOG_DEBUG   */

/**
 * @brief 日志缓冲区大小
 * @note 单条日志的最大长度（字节）
 */
#    ifndef EKCFG_LOG_BUF_SIZE
#        define EKCFG_LOG_BUF_SIZE (256)
#    endif /* EKCFG_LOG_BUF_SIZE     */

/**
 * @brief 日志级别枚举
 */
typedef enum
{
    EK_LOG_LEVEL_NONE = 0, /**< 无级别（普通输出） */
    EK_LOG_LEVEL_DEBUG, /**< 调试信息 */
    EK_LOG_LEVEL_INFO, /**< 一般信息 */
    EK_LOG_LEVEL_WARN, /**< 警告信息 */
    EK_LOG_LEVEL_ERROR, /**< 错误信息 */
    EK_LOG_LEVEL_FATAL, /**< 致命错误信息 */

    EK_LOG_LEVEL_MAX, /**< 最大级别数 */
} ek_log_level_e;

/**
 * @brief 定义当前源文件的日志标签和最低输出级别
 * @param tag 标签字符串
 * @param level 最低日志级别（ek_log_level_e）
 * @note 在源文件开头使用，用于标识日志来源并过滤低于该级别的日志
 * @code
 * EK_LOG_MODULE("main.c", EK_LOG_LEVEL_INFO);
 * @endcode
 */
#    define EK_LOG_MODULE(tag, level)            \
        static const char *__ek_log_tag = (tag); \
        static const uint8_t __ek_log_min_level = (uint8_t)(level)

/**
 * @brief 定义文件标签并允许输出所有级别日志
 * @param tag 标签字符串
 * @note 用于兼容旧接口，等价于 EK_LOG_MODULE(tag, EK_LOG_LEVEL_NONE)
 * @code
 * EK_LOG_FILE_TAG("main.c");
 * @endcode
 */
#    define EK_LOG_FILE_TAG(tag) EK_LOG_MODULE(tag, EK_LOG_LEVEL_NONE)

#    ifdef __cplusplus
extern "C"
{
#    endif

/**
 * @brief 日志输出函数（内部使用）
 * @param tag 文件标签
 * @param line 行号
 * @param type 日志级别
 * @param tick 时间戳
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void ek_log_printf(const char *tag, uint32_t line, ek_log_level_e type, uint32_t tick, const char *fmt, ...);

/**
 * @brief 获取系统时间戳
 * @note 默认实现为弱函数（返回 0），用户可覆盖此函数以提供系统时间戳
 * @return 时间戳
 */
uint32_t ek_port_log_get_tick(void);

/**
 * @brief DEBUG 级别日志
 * @param ... 格式化字符串和参数
 * @note 仅在 EKCFG_LOG_DEBUG == 1 时有效
 */
#    if (EKCFG_LOG_DEBUG == 1)
#        define EK_LOG_DEBUG(...)                                                                                   \
            do                                                                                                      \
            {                                                                                                       \
                if (__ek_log_min_level <= (uint8_t)EK_LOG_LEVEL_DEBUG)                                              \
                    ek_log_printf(__ek_log_tag, __LINE__, EK_LOG_LEVEL_DEBUG, ek_port_log_get_tick(), __VA_ARGS__); \
            } while (0)
#    else

#        define EK_LOG_DEBUG(...)
#    endif

/**
 * @brief 普通日志（无级别标识）
 * @param ... 格式化字符串和参数
 */
#    define EK_LOG(...)                                                                                        \
        do                                                                                                     \
        {                                                                                                      \
            if (__ek_log_min_level <= (uint8_t)EK_LOG_LEVEL_NONE)                                              \
                ek_log_printf(__ek_log_tag, __LINE__, EK_LOG_LEVEL_NONE, ek_port_log_get_tick(), __VA_ARGS__); \
        } while (0)

/**
 * @brief INFO 级别日志
 * @param ... 格式化字符串和参数
 */
#    define EK_LOG_INFO(...)                                                                                   \
        do                                                                                                     \
        {                                                                                                      \
            if (__ek_log_min_level <= (uint8_t)EK_LOG_LEVEL_INFO)                                              \
                ek_log_printf(__ek_log_tag, __LINE__, EK_LOG_LEVEL_INFO, ek_port_log_get_tick(), __VA_ARGS__); \
        } while (0)

/**
 * @brief WARN 级别日志
 * @param ... 格式化字符串和参数
 */
#    define EK_LOG_WARN(...)                                                                                   \
        do                                                                                                     \
        {                                                                                                      \
            if (__ek_log_min_level <= (uint8_t)EK_LOG_LEVEL_WARN)                                              \
                ek_log_printf(__ek_log_tag, __LINE__, EK_LOG_LEVEL_WARN, ek_port_log_get_tick(), __VA_ARGS__); \
        } while (0)

/**
 * @brief ERROR 级别日志
 * @param ... 格式化字符串和参数
 */
#    define EK_LOG_ERROR(...)                                                                                   \
        do                                                                                                      \
        {                                                                                                       \
            if (__ek_log_min_level <= (uint8_t)EK_LOG_LEVEL_ERROR)                                              \
                ek_log_printf(__ek_log_tag, __LINE__, EK_LOG_LEVEL_ERROR, ek_port_log_get_tick(), __VA_ARGS__); \
        } while (0)

/**
 * @brief FATAL 级别日志
 * @param ... 格式化字符串和参数
 * @warning 当该级别允许输出时，打印日志后会进入 while (1) 死循环，永不返回
 */
#    define EK_LOG_FATAL(...)                                                                                   \
        do                                                                                                      \
        {                                                                                                       \
            if (__ek_log_min_level <= (uint8_t)EK_LOG_LEVEL_FATAL)                                              \
            {                                                                                                   \
                ek_log_printf(__ek_log_tag, __LINE__, EK_LOG_LEVEL_FATAL, ek_port_log_get_tick(), __VA_ARGS__); \
                while (1);                                                                                      \
            }                                                                                                   \
        } while (0)

#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_LOG */

#endif /* EK_LOG_H */
