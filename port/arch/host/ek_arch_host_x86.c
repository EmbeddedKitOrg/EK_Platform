/**
 * @file ek_arch_host_x86.c
 * @brief 架构相关底层操作实现
 * @author N1netyNine99
 *
 * x86 Host 兼容层
 */
#include "ek/core/ek_arch.h"

#if EKCFG_ARCH == 1

void ek_arch_isb(void)
{
    __EK_ASM("" :: : "memory");
}
void ek_arch_dsb(void)
{
    __EK_ASM("" :: : "memory");
}
void ek_arch_dmb(void)
{
    __EK_ASM("" :: : "memory");
}

uint32_t ek_arch_irq_lock(void)
{
    return 0;
}

void ek_arch_irq_unlock(uint32_t flags)
{
    __EK_UNUSED(flags);
}

// host 构建没有异常向量表
void ek_arch_redirect_vtor(void)
{
}

int ek_arch_irq_install(int32_t irq, ek_arch_isr_t handler)
{
    __EK_UNUSED(irq);
    __EK_UNUSED(handler);
    return -ENOSYS;
}

#endif // EKCFG_ARCH == 1
