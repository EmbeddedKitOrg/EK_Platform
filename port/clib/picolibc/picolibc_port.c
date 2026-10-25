/**
 * @file ek_picolibc_port.c
 * @brief picolibc 适配层
 * @author N1netyNine99
 *
 * 统一 picolibc 与 ek_utils 的内存分配器和字符输出：
 * - A (EKCFG_HEAP_TLSF=1): picolibc 的 malloc/free/realloc → ek_malloc/ek_free/ek_realloc (TLSF)
 * - B (EKCFG_HEAP_TLSF=0): ek_malloc/ek_free/ek_realloc → picolibc 的 malloc/free/realloc
 * - stdout: putchar → ek_port_io_fputc
 */

#include "ek_conf_internal.h"

#if EKCFG_PICOLIBC == 1

#    include "ek_heap.h"
#    include "ek_io.h"

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

static int _ek_picolibc_putc(char ch, FILE *file)
{
    __EK_UNUSED(file);
    return ek_port_io_fputc(ch);
}

static FILE __stdio = FDEV_SETUP_STREAM(_ek_picolibc_putc, NULL, NULL, _FDEV_SETUP_WRITE);

FILE *const stdout = &__stdio;
FILE *const stderr = &__stdio;

void _exit(int status)
{
    __EK_UNUSED(status);
    while (1)
    {
    }
}

#endif /* EKCFG_PICOLIBC */
