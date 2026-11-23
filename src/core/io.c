/**
 * @file io.c
 * @brief 标准输入输出实现
 * @author N1netyNine99
 */

#include "ek/core/io.h"
#include "ek/core/export.h"

#if EKCFG_VFS == 1

#    include "ek/vfs/vfs_core.h"

ssize_t ek_io_write(int fd, const void *buf, size_t len)
{
    if (!buf && len > 0U) return -EINVAL;
    if (len == 0U) return 0;
    return ek_vfs_write(fd, buf, len);
}

ssize_t ek_io_read(int fd, void *buf, size_t len)
{
    if (!buf && len > 0U) return -EINVAL;
    if (len == 0U) return 0;
    return ek_vfs_read(fd, buf, len);
}

int ek_io_putc(int fd, int ch)
{
    uint8_t c = (uint8_t)ch;
    return (ek_vfs_write(fd, &c, 1U) == 1) ? (int)c : -1;
}

int ek_io_getc(int fd)
{
    uint8_t c = 0U;
    return (ek_vfs_read(fd, &c, 1U) == 1) ? (int)c : -1;
}

#else /* EKCFG_VFS == 0 */

static const ek_io_ops_t *s_io_ops;

void ek_io_set_ops(const ek_io_ops_t *ops)
{
    s_io_ops = ops;
}

const ek_io_ops_t *ek_io_get_ops(void)
{
    return s_io_ops;
}

ssize_t ek_io_write(int fd, const void *buf, size_t len)
{
    if (!buf && len > 0U) return -EINVAL;
    if (len == 0U) return 0;

    const ek_io_ops_t *ops = s_io_ops;
    if (!ops) return (ssize_t)len;
    if (ops->write) return ops->write(fd, buf, len);

    if (ops->put_ch)
    {
        const uint8_t *p = (const uint8_t *)buf;
        for (size_t i = 0U; i < len; i++)
        {
            if (ops->put_ch(fd, p[i]) < 0) return (i > 0U) ? (ssize_t)i : -EIO;
        }
        return (ssize_t)len;
    }

    return (ssize_t)len;
}

ssize_t ek_io_read(int fd, void *buf, size_t len)
{
    if (!buf && len > 0U) return -EINVAL;
    if (len == 0U) return 0;

    const ek_io_ops_t *ops = s_io_ops;
    if (!ops) return 0;
    if (ops->read) return ops->read(fd, buf, len);

    if (ops->get_ch)
    {
        uint8_t *p = (uint8_t *)buf;
        size_t i = 0U;
        for (; i < len; i++)
        {
            int c = ops->get_ch(fd);
            if (c < 0) break;
            p[i] = (uint8_t)c;
        }
        return (ssize_t)i;
    }

    return 0;
}

int ek_io_putc(int fd, int ch)
{
    const ek_io_ops_t *ops = s_io_ops;
    uint8_t c = (uint8_t)ch;

    if (!ops) return (int)c;
    if (ops->put_ch) return ops->put_ch(fd, c);
    if (ops->write) return (ops->write(fd, &c, 1U) == 1) ? (int)c : -1;
    return (int)c;
}

int ek_io_getc(int fd)
{
    const ek_io_ops_t *ops = s_io_ops;
    if (!ops) return -1;
    if (ops->get_ch) return ops->get_ch(fd);

    if (ops->read)
    {
        uint8_t c = 0U;
        return (ops->read(fd, &c, 1U) == 1) ? (int)c : -1;
    }

    return -1;
}

#endif /* EKCFG_VFS */

#if EKCFG_IO_LWPRTF == 1

static int _ek_io_printf(int ch, lwprintf_t *lwp)
{
    __EK_UNUSED(lwp);
    if (ch != '\0')
    {
        ek_io_putc(EK_IO_STDOUT, ch);
    }
    return ch;
}

#endif /* EKCFG_IO_LWPRTF */

void ek_io_init(void)
{
#if EKCFG_VFS == 1 && defined(EKCFG_IO_CONSOLE_PATH)
    const char *console = EKCFG_IO_CONSOLE_PATH;
    if (console && console[0] == '/')
    {
        (void)ek_vfs_bind_stdio(console);
    }
#endif

#if EKCFG_IO_LWPRTF == 1
    lwprintf_init(_ek_io_printf);
#endif
}

#if EKCFG_EXPORT == 1
EK_EXPORT_COMPONENT(ek_io_init, EK_EXPORT_ORDER_COMPONENT_IO);
#endif
