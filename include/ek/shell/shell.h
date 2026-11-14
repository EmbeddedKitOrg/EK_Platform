/**
 * @file shell.h
 * @brief 嵌入式命令行解释器 (Shell)
 * @author N1netyNine99
 *
 * 提供零动态内存开销的嵌入式命令行交互环境，支持：
 * - 链接器段自动导出静态命令
 * - 运行时动态注册与注销命令
 * - VT100 / ANSI 控制台光标与按键解析
 * - 原地分词解析命令行参数 (argc/argv)
 * - 基于 VFS 文件描述符的输入输出解耦
 *
 * @note 依赖 EKCFG_VFS=1
 */

#ifndef SHELL_H
#define SHELL_H

#include "ek/conf_internal.h"

#if EKCFG_SHELL == 1

#    include "ek/core/def.h"

#    ifdef __cplusplus
extern "C"
{
#    endif /* __cplusplus */

/**
 * @brief Shell 配置结构体前置声明
 */
typedef struct ek_shell_cfg ek_shell_cfg_t;

/**
 * @brief Shell 命令结构体前置声明
 */
typedef struct ek_shell_cmd ek_shell_cmd_t;

/**
 * @brief Shell 命令处理函数类型
 *
 * @param argc 参数个数（包含命令名本身）
 * @param argv 参数字符串数组
 * @param sh 传递的 shell 的指针
 * @return 0 表示成功，非 0 或负数错误码表示执行失败
 */
typedef int (*ek_shell_cmd_fn_t)(int argc, char *argv[], void *sh);

/**
 * @brief Shell 初始化配置
 */
struct ek_shell_cfg
{
    int in_fd; /**< 输入流文件描述符（如 VFS UART 读端） */
    int out_fd; /**< 输出流文件描述符（如 VFS UART 写端） */
};

/**
 * @brief Shell 命令条目描述
 */
struct ek_shell_cmd
{
    const char *name; /**< 命令名称（触发关键字） */
    ek_shell_cmd_fn_t fn; /**< 命令回调函数指针 */
};

/**
 * @brief 静态导出 Shell 命令
 *
 * 将命令条目置入 `.ek_shell_cmd` 链接器段，系统启动后自动识别。
 *
 * @param name 命令名称标识符（如 help、ls，无需双引号）
 * @param fn   命令处理函数指针
 */
#    define EK_DEFINE_SHELL_CMD(cmd_name, cmd_fn)                                                        \
        const ek_shell_cmd_t ek_shell_cmd_##cmd_name __attribute__((used, section(".ek_shell_cmd"))) = { \
            .name = #cmd_name,                                                                           \
            .fn = cmd_fn,                                                                                \
        }

/**
 * @brief 初始化 Shell 环境
 *
 * 绑定输入输出文件描述符，初始化内部行缓冲及提示符。
 *
 * @param cfg 配置结构体指针
 * @return 0 成功，-EINVAL 参数无效
 */
int ek_shell_init(ek_shell_cfg_t *cfg);

/**
 * @brief 从输入流读取并驱动 Shell 处理步进
 *
 * 从配置的输入文件描述符读取待处理数据并送入解析状态机。
 *
 * @param buf  临时接收缓冲区指针
 * @param size 缓冲区最大字节数
 * @return 成功处理的字节数，0 表示无数据，负数表示 POSIX 错误码
 */
int ek_shell_step(char *buf, size_t size);

/**
 * @brief 动态注册 Shell 命令
 *
 * 在堆内存中分配节点并插入动态命令链表。
 *
 * @param name 命令名称
 * @param fn   命令处理函数指针
 * @return 0 成功，-EINVAL 参数无效，-ENOMEM 内存不足
 */
int ek_shell_cmd_register(const char *name, ek_shell_cmd_fn_t fn);

/**
 * @brief 动态注销 Shell 命令
 *
 * 从动态命令链表中移除指定名称的命令并释放内存。
 *
 * @param name 待注销的命令名称
 */
void ek_shell_cmd_unregister(const char *name);

/**
 * @brief 直接向 Shell 输入数据
 *
 * 用于中断接收、DMA 回调或外部数据流直接送入字符。
 *
 * @param data 待输入的字节流指针
 * @param len  数据字节长度
 * @return 0 成功，-EINVAL 参数无效
 */
int ek_shell_input(const void *data, size_t len);

#    ifdef __cplusplus
}
#    endif /* __cplusplus */

#endif // EKCFG_SHELL == 1

#endif // SHELL_H
