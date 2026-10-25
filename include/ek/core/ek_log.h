/**
 * @file ek_log.h
 * @brief 日志系统
 * @author N1netyNine99
 *
 * 提供分级日志输出功能，支持：
 * - 多级别日志（DEBUG、INFO、WARN、ERROR）
 * - 文件名、行号、时间戳输出
 * - 彩色打印（可选）
 * - 格式化输出
 *
 * @note 使用前需实现 ek_port_log_get_tick() 函数，用于获取系统时间戳
 */

#ifndef EK_LOG_H
#define EK_LOG_H

#include "ek_conf_internal.h"

#if EKCFG_LOG == 1

#    include "ek_io.h"
#    include "ek_def.h"

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
} ek_log_level_t;

/**
 * @brief 定义文件标签和最小日志等级
 * @param tag 标签字符串
 * @param level 最低日志等级
 * @note 在源文件开头使用，用于标识日志来源
 * @example
 * EK_LOG_FILE_TAG("main.c");
 */
#    define EK_LOG_MODULE(tag, level)            \
        static const char *_EK_LOG_TAG_ = (tag); \
        static const ek_log_level_t _EK_LOG_MIN_LEVEL_ = (level)

/**
 * @brief 定义文件标签并且默认日志全部输出
 * @param tag 标签字符串
 * @note 用于兼容旧的接口
 * @example
 * EK_LOG_FILE_TAG("main.c");
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
void ek_log_printf(const char *tag, uint32_t line, ek_log_level_t type, uint32_t tick, const char *fmt, ...);

/**
 * @brief 获取系统时间戳
 * @note 本身为弱定义，用户可以自己实现
 * @return 时间戳
 */
uint32_t ek_port_log_get_tick(void);

/**
 * @brief DEBUG 级别日志
 * @param ... 格式化字符串和参数
 * @note 仅在 EKCFG_LOG_DEBUG == 1 时有效
 */
#    if (EKCFG_LOG_DEBUG == 1)
#        define EK_LOG_DEBUG(...)                         \
            if (_EK_LOG_MIN_LEVEL_ <= EK_LOG_LEVEL_DEBUG) \
            ek_log_printf(_EK_LOG_TAG_, __LINE__, EK_LOG_LEVEL_DEBUG, ek_port_log_get_tick(), __VA_ARGS__)
#    else
#        define EK_LOG_DEBUG(...)
#    endif

/**
 * @brief 普通日志（无级别标识）
 * @param ... 格式化字符串和参数
 */
#    define EK_LOG(...)                              \
        if (_EK_LOG_MIN_LEVEL_ <= EK_LOG_LEVEL_NONE) \
        ek_log_printf(_EK_LOG_TAG_, __LINE__, EK_LOG_LEVEL_NONE, ek_port_log_get_tick(), __VA_ARGS__)

/**
 * @brief INFO 级别日志
 * @param ... 格式化字符串和参数
 */
#    define EK_LOG_INFO(...)                         \
        if (_EK_LOG_MIN_LEVEL_ <= EK_LOG_LEVEL_INFO) \
        ek_log_printf(_EK_LOG_TAG_, __LINE__, EK_LOG_LEVEL_INFO, ek_port_log_get_tick(), __VA_ARGS__)

/**
 * @brief WARN 级别日志
 * @param ... 格式化字符串和参数
 */
#    define EK_LOG_WARN(...)                         \
        if (_EK_LOG_MIN_LEVEL_ <= EK_LOG_LEVEL_WARN) \
        ek_log_printf(_EK_LOG_TAG_, __LINE__, EK_LOG_LEVEL_WARN, ek_port_log_get_tick(), __VA_ARGS__)

/**
 * @brief ERROR 级别日志
 * @param ... 格式化字符串和参数
 */
#    define EK_LOG_ERROR(...)                         \
        if (_EK_LOG_MIN_LEVEL_ <= EK_LOG_LEVEL_ERROR) \
        ek_log_printf(_EK_LOG_TAG_, __LINE__, EK_LOG_LEVEL_ERROR, ek_port_log_get_tick(), __VA_ARGS__)
/**
 * @brief FATAL 级别日志
 * @param ... 格式化字符串和参数
 */
#    define EK_LOG_FATAL(...)                                                                               \
        if (_EK_LOG_MIN_LEVEL_ <= EK_LOG_LEVEL_FATAL)                                                       \
        {                                                                                                   \
            ek_log_printf(_EK_LOG_TAG_, __LINE__, EK_LOG_LEVEL_FATAL, ek_port_log_get_tick(), __VA_ARGS__); \
            while (1);                                                                                      \
        }
#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_LOG */

#endif // __SER_LOG_H
