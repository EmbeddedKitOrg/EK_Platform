/**
 * @file ek_newlib_port.c
 * @brief newlib/nanolibc syscall 适配
 *
 * 为裸机 newlib 提供最小系统调用桩：
 * - _write → ek_port_io_fputc
 * - _read / _close / _fstat / _isatty / _lseek → 最小实现
 * - _sbrk → 使用链接脚本的 __heap_start / __heap_end（弱引用）
 *
 * 全部为弱符号，BSP 可以按需覆盖。
 */

#include "ek/ek_conf_internal.h"

#if EKCFG_NEWLIB == 1

#    include <errno.h>
#    include <stddef.h>
#    include <sys/stat.h>
#    include <sys/types.h>

#    include "ek/core/ek_def.h"
#    include "ek/core/ek_io.h"

/* newlib 的 __libc_init_array() 会调用 _init/_fini；自定义 startup 没有提供 */
__EK_WEAK void _init(void)
{
}
__EK_WEAK void _fini(void)
{
}

__EK_WEAK void _exit(int status)
{
    __EK_UNUSED(status);
    while (1)
    {
    }
}

__EK_WEAK int _kill(int pid, int sig)
{
    __EK_UNUSED(pid);
    __EK_UNUSED(sig);
    errno = ENOSYS;
    return -1;
}

__EK_WEAK int _getpid(void)
{
    return 1;
}

__EK_WEAK int _write(int fd, const void *buf, size_t len)
{
    __EK_UNUSED(fd);
    const char *p = (const char *)buf;
    for (size_t i = 0; i < len; i++)
    {
        if (ek_port_io_fputc(p[i]) < 0) return -1;
    }
    return (int)len;
}

__EK_WEAK int _read(int fd, void *buf, size_t len)
{
    __EK_UNUSED(fd);
    __EK_UNUSED(buf);
    __EK_UNUSED(len);
    errno = ENOSYS;
    return -1;
}

__EK_WEAK int _close(int fd)
{
    __EK_UNUSED(fd);
    return -1;
}

__EK_WEAK int _fstat(int fd, struct stat *st)
{
    __EK_UNUSED(fd);
    if (!st)
    {
        errno = EINVAL;
        return -1;
    }
    st->st_mode = S_IFCHR;
    return 0;
}

__EK_WEAK int _isatty(int fd)
{
    __EK_UNUSED(fd);
    return 1;
}

__EK_WEAK off_t _lseek(int fd, off_t offset, int whence)
{
    __EK_UNUSED(fd);
    __EK_UNUSED(offset);
    __EK_UNUSED(whence);
    return 0;
}

extern char __heap_start[] __attribute__((weak));
extern char __heap_end[] __attribute__((weak));

__EK_WEAK void *_sbrk(ptrdiff_t incr)
{
    static char *brk;

    if (!__heap_start || !__heap_end)
    {
        errno = ENOMEM;
        return (void *)-1;
    }
    if (!brk) brk = __heap_start;
    if (brk + incr > __heap_end)
    {
        errno = ENOMEM;
        return (void *)-1;
    }

    char *prev = brk;
    brk += incr;
    return prev;
}

#endif /* EKCFG_NEWLIB */
