/**
 * @file ek_io.h
 * @brief 标准输入输出接口
 * @author N1netyNine99
 *
 * 提供可配置的标准输入输出封装，支持 lwprintf、picolibc 与标准 libc 后端
 *
 * @note lwprintf 与 picolibc 后端使用前需实现 ek_port_io_fputc() 函数，用于底层字符输出
 */

#ifndef EK_IO_H
#define EK_IO_H

#include "ek_conf_internal.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if EKCFG_IO_LWPRTF == 1

#    include "lwprintf.h"

/**
 * @brief 底层字符输出函数声明
 * @note 用户需要实现此函数，用于底层字符输出（如 UART）
 * @return 写入的字符
 */
int ek_port_io_fputc(int ch);

/**
 * @brief 初始化 I/O 系统
 * @note 初始化 lwprintf 库，设置输出函数
 */
void ek_io_init(void);

#    define ek_printf    lwprintf
#    define ek_vprintf   lwvprintf
#    define ek_sprintf   lwsprintf
#    define ek_snprintf  lwsnprintf
#    define ek_vsnprintf lwvsnprintf

#elif EKCFG_PICOLIBC == 1

#    include <stdio.h>

/**
 * @brief 底层字符输出函数声明
 * @note 用户需要实现此函数，用于底层字符输出（如 UART）
 * @return 写入的字符
 */
int ek_port_io_fputc(int ch);

/**
 * @brief 初始化 I/O 系统（picolibc 模式下无需初始化）
 */
void ek_io_init(void);

#    define ek_printf    printf
#    define ek_vprintf   vprintf
#    define ek_sprintf   sprintf
#    define ek_snprintf  snprintf
#    define ek_vsnprintf vsnprintf

#else

#    include <stdio.h>

void ek_io_init(void);

#    define EK_IO_FPUTC()

// 如果不需要使用lwprintf 需要补全下列的宏
// #    define ek_printf
// #    define ek_vsprintf
// #    define ek_sprintf
// #    define ek_snprintf
// #    define ek_vsnprintf

#endif /* EKCFG_IO_LWPRTF */

#ifdef __cplusplus
}
#endif

#endif
