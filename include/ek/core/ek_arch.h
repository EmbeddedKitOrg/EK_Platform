#ifndef EK_ARCH_H
#define EK_ARCH_H

#include "ek/ek_conf_internal.h"
#include "ek/core/ek_def.h"

#if defined(__arm__) || defined(__thumb__)

static inline void ek_arch_isb(void)
{
    __EK_ASM("isb 0xF" :: : "memory");
}

static inline void ek_arch_dsb(void)
{
    __EK_ASM("dsb 0xF" :: : "memory");
}

static inline void ek_arch_dmb(void)
{
    __EK_ASM("dmb 0xF" :: : "memory");
}

static inline uint32_t ek_arch_irq_save(void)
{
    uint32_t result;
    __EK_ASM("mrs %0, primask \n cpsid i" : "=r"(result)::"memory");
    return result;
}

static inline void ek_arch_irq_restore(uint32_t flags)
{
    __EK_ASM("msr primask, %0" ::"r"(flags) : "memory");
}

#elif defined(__riscv)

static inline void ek_arch_isb(void)
{
    __EK_ASM("fence.i" :: : "memory");
}

static inline void ek_arch_dsb(void)
{
    __EK_ASM("fence" :: : "memory");
}

static inline void ek_arch_dmb(void)
{
    __EK_ASM("fence rw, rw" :: : "memory");
}

static inline uint32_t ek_arch_irq_save(void)
{
    uint32_t flags;
    __EK_ASM("csrrci %0, mstatus, 8" : "=r"(flags)::"memory");
    return flags;
}

static inline void ek_arch_irq_restore(uint32_t flags)
{
    __EK_ASM("csrs mstatus, %0" ::"r"(flags & 8) : "memory");
}

#else

static inline void ek_arch_isb(void)
{
    __EK_ASM("" :: : "memory");
}
static inline void ek_arch_dsb(void)
{
    __EK_ASM("" :: : "memory");
}
static inline void ek_arch_dmb(void)
{
    __EK_ASM("" :: : "memory");
}
static inline uint32_t ek_arch_irq_save(void)
{
    return 0;
}
static inline void ek_arch_irq_restore(uint32_t flags)
{
    __EK_UNUSED(flags);
}

#endif

#endif // EK_ARCH_H
