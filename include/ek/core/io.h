/**
 * @file io.h
 * @brief 标准输入输出接口
 * @author N1netyNine99
 *
 * 提供可配置的标准输入输出封装，支持 lwprintf、picolibc 与标准 libc 后端：
 * - 开启 VFS（EKCFG_VFS == 1）时，标准流直接路由到 VFS fd 0/1/2，可通过 ek_vfs_bind_fd() 热重定向；
 *   若配置了 EKCFG_IO_CONSOLE_PATH，ek_io_init() 会自动将 stdin/stdout/stderr 绑定到该设备。
 * - 关闭 VFS（EKCFG_VFS == 0）时，可通过 ek_io_set_ops() 注册轻量字符/块操作表。
 */

#ifndef EK_IO_H
#define EK_IO_H

#include "ek/conf_internal.h"
#include "ek/core/def.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define EK_IO_STDIN  (0) /**< 标准输入描述符 */
#define EK_IO_STDOUT (1) /**< 标准输出描述符 */
#define EK_IO_STDERR (2) /**< 标准错误描述符 */

/**
 * @brief 初始化 I/O 子系统
 *
 * 开启 VFS 且定义了 EKCFG_IO_CONSOLE_PATH 时，自动将标准输入输出错误流绑定到该设备；
 * 启用 lwprintf 时完成全局输出回调注册。通常由 EK_EXPORT_COMPONENT 自动调用。
 */
void ek_io_init(void);

/**
 * @brief 向指定标准流写入数据块
 * @param fd  目标描述符（EK_IO_STDOUT / EK_IO_STDERR 等）
 * @param buf 待写入数据缓冲区
 * @param len 期望写入字节数
 * @return 实际写入字节数（>= 0），负数 errno 失败
 */
ssize_t ek_io_write(int fd, const void *buf, size_t len);

/**
 * @brief 从指定标准流读取数据块
 * @param fd  目标描述符（EK_IO_STDIN 等）
 * @param buf 接收数据缓冲区
 * @param len 期望读取字节数
 * @return 实际读取字节数（>= 0，0 表示 EOF），负数 errno 失败
 */
ssize_t ek_io_read(int fd, void *buf, size_t len);

/**
 * @brief 向指定标准流写入单个字符
 * @param fd 目标描述符（EK_IO_STDOUT / EK_IO_STDERR 等）
 * @param ch 待输出字符
 * @return 成功返回写入的无符号字符值（0..255），失败返回 -1
 */
int ek_io_putc(int fd, int ch);

/**
 * @brief 从指定标准流读取单个字符
 * @param fd 目标描述符（EK_IO_STDIN 等）
 * @return 成功返回读取的无符号字符值（0..255），无数据或失败返回 -1
 */
int ek_io_getc(int fd);

#if EKCFG_VFS == 0

/**
 * @brief 无 VFS 模式下的轻量级标准 IO 操作表
 */
typedef struct
{
    int (*put_ch)(int fd, int ch); /**< 单字符输出（可选，若为空则回退到 write） */
    int (*get_ch)(int fd); /**< 单字符输入（可选，若为空则回退到 read） */
    ssize_t (*write)(int fd, const void *buf, size_t len); /**< 块输出（可选，若为空则回退到 put_ch） */
    ssize_t (*read)(int fd, void *buf, size_t len); /**< 块输入（可选，若为空则回退到 get_ch） */
} ek_io_ops_t;

/**
 * @brief 注册无 VFS 模式下的标准 IO 操作表
 * @param ops 操作表指针（调用期间须保持有效，传 NULL 表示清空为丢弃模式）
 */
void ek_io_set_ops(const ek_io_ops_t *ops);

/**
 * @brief 获取当前已注册的无 VFS 标准 IO 操作表
 * @return 当前操作表指针，未设置时返回 NULL
 */
const ek_io_ops_t *ek_io_get_ops(void);

#endif /* EKCFG_VFS == 0 */

#if EKCFG_IO_LWPRTF == 1

#    include "lwprintf.h"

#    define ek_printf    lwprintf
#    define ek_vprintf   lwvprintf
#    define ek_sprintf   lwsprintf
#    define ek_snprintf  lwsnprintf
#    define ek_vsnprintf lwvsnprintf

#else

#    include <stdio.h>

#    define ek_printf    printf
#    define ek_vprintf   vprintf
#    define ek_sprintf   sprintf
#    define ek_snprintf  snprintf
#    define ek_vsnprintf vsnprintf

#endif /* EKCFG_IO_LWPRTF */

#ifdef __cplusplus
}
#endif

#endif /* EK_IO_H */
