/**
 * @file ek_io.c
 * @brief 标准输入输出实现
 * @author N1netyNine99
 */

#include "ek_io.h"

#if EKCFG_IO_LWPRTF == 1

#    include "ek_def.h"
#    include "ek_export.h"

/* 前向声明 */
static int _ek_io_printf(int ch, lwprintf_t *lwp);

__EK_WEAK int ek_port_io_fputc(int ch)
{
    __EK_UNUSED(ch);
    return ch;
}

void ek_io_init(void)
{
    lwprintf_init(_ek_io_printf);
}

EK_EXPORT_COMPONENTS(ek_io_init, 0);

/* 静态函数实现 */
static int _ek_io_printf(int ch, lwprintf_t *lwp)
{
    __EK_UNUSED(lwp);
    if (ch != '\0')
    {
        ek_port_io_fputc(ch);
    }
    return ch;
}

#elif EKCFG_PICOLIBC == 1

#    include "../inc/ek_def.h"

__EK_WEAK int ek_port_io_fputc(int ch)
{
    __EK_UNUSED(ch);
    return ch;
}

void ek_io_init(void)
{
}

#else

void ek_io_init(void)
{
}

#endif
