/**
 * @file ek_arch_riscv_generic.c
 * @brief 架构相关底层操作实现
 * @author N1netyNine99
 *
 * RISCV 通用架构
 */
#include "ek/core/ek_arch.h"

#if EKCFG_ARCH == 1

void ek_arch_redirect_vtor(void)
{
}

int ek_arch_irq_install(int32_t irq, ek_arch_isr_t handler)
{
    __EK_UNUSED(irq);
    __EK_UNUSED(handler);
    return -ENOSYS;
}

void ek_arch_isb(void)
{
    __EK_ASM("fence.i" :: : "memory");
}

void ek_arch_dsb(void)
{
    __EK_ASM("fence" :: : "memory");
}

void ek_arch_dmb(void)
{
    __EK_ASM("fence rw, rw" :: : "memory");
}

uint32_t ek_arch_irq_lock(void)
{
    uint32_t flags;
    __EK_ASM("csrrci %0, mstatus, 8" : "=r"(flags)::"memory");
    return flags;
}

void ek_arch_irq_unlock(uint32_t flags)
{
    __EK_ASM("csrs mstatus, %0" ::"r"(flags & 8U) : "memory");
}

#endif // EKCFG_ARCH == 1
