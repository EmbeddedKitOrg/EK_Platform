/**
 * @file picolibc_port.c
 * @brief picolibc 适配层
 * @author N1netyNine99
 *
 * 统一 picolibc 与 ek_utils 的内存分配器和字符输出：
 * - A (EKCFG_HEAP_TLSF=1): picolibc 的 malloc/free/realloc → ek_malloc/ek_free/ek_realloc (TLSF)
 * - B (EKCFG_HEAP_TLSF=0): ek_malloc/ek_free/ek_realloc → picolibc 的 malloc/free/realloc
 * - stdin/stdout/stderr → ek_io_getc / ek_io_putc（由 VFS fd 0/1/2 或 ek_io_ops_t 承载）
 */

#include "ek/conf_internal.h"

#if EKCFG_PICOLIBC == 1

#    include "ek/mem/heap.h"
#    include "ek/core/io.h"

#    if EKCFG_HEAP_TLSF == 1

void *malloc(size_t size)
{
    return ek_malloc(size);
}

void free(void *ptr)
{
    ek_free(ptr);
}

void *realloc(void *ptr, size_t size)
{
    return ek_realloc(ptr, size);
}

#    else /* EKCFG_HEAP_TLSF == 0 */

void *ek_malloc(size_t size)
{
    return malloc(size);
}

void ek_free(void *ptr)
{
    free(ptr);
}

void *ek_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

#    endif /* EKCFG_HEAP_TLSF */

static int _ek_picolibc_stdin_getc(FILE *file)
{
    __EK_UNUSED(file);
    int ch = ek_io_getc(EK_IO_STDIN);
    return (ch >= 0) ? ch : _FDEV_EOF;
}

static int _ek_picolibc_stdout_putc(char ch, FILE *file)
{
    __EK_UNUSED(file);
    return (ek_io_putc(EK_IO_STDOUT, (unsigned char)ch) >= 0) ? (unsigned char)ch : _FDEV_EOF;
}

static int _ek_picolibc_stderr_putc(char ch, FILE *file)
{
    __EK_UNUSED(file);
    return (ek_io_putc(EK_IO_STDERR, (unsigned char)ch) >= 0) ? (unsigned char)ch : _FDEV_EOF;
}

static FILE __stdin = FDEV_SETUP_STREAM(NULL, _ek_picolibc_stdin_getc, NULL, _FDEV_SETUP_READ);
static FILE __stdout = FDEV_SETUP_STREAM(_ek_picolibc_stdout_putc, NULL, NULL, _FDEV_SETUP_WRITE);
static FILE __stderr = FDEV_SETUP_STREAM(_ek_picolibc_stderr_putc, NULL, NULL, _FDEV_SETUP_WRITE);

FILE *const stdin = &__stdin;
FILE *const stdout = &__stdout;
FILE *const stderr = &__stderr;

__EK_WEAK void _exit(int status)
{
    __EK_UNUSED(status);
    while (1)
    {
    }
}

#endif /* EKCFG_PICOLIBC */
