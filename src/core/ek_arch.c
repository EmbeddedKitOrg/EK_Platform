/**
 * @file ek_arch.c
 * @brief 架构相关底层操作实现
 * @author N1netyNine99
 *
 * ARM 分支提供向量表重定向；RISC-V 和 host 分支的 VTOR 重定向为空实现。
 */

#include "ek/core/ek_arch.h"

#if EKCFG_ARCH == 1

#    include "ek/core/ek_assert.h"

extern uint32_t _flash_vtor_start[];
extern uint32_t _flash_vtor_end[];
extern uint32_t _sram_vtor_start[];
extern uint32_t _sram_vtor_end[];

#    if defined(__arm__) || defined(__thumb__)

#        define SCB_VTOR (*(volatile uint32_t *)0xE000ED08UL)

void ek_arch_redirect_vtor(void)
{
    size_t size = (size_t)((uintptr_t)_flash_vtor_end - (uintptr_t)_flash_vtor_start);

    uint32_t flags = ek_arch_irq_lock();
    memcpy(_sram_vtor_start, _flash_vtor_start, size);

    SCB_VTOR = (uintptr_t)_sram_vtor_start;

    ek_arch_dsb();
    ek_arch_isb();

    ek_arch_irq_unlock(flags);
}

int ek_arch_irq_install(int32_t irq, ek_arch_isr_t handler)
{
    if (handler == NULL) return -EINVAL;
    if (irq < -14 || irq >= EKCFG_ARCH_IRQ_COUNT) return -EINVAL;
    if (SCB_VTOR != (uintptr_t)_sram_vtor_start) return -EAGAIN;

    uint32_t *table = (uint32_t *)(uintptr_t)SCB_VTOR;
    if (table == NULL) return -EINVAL;

    uint32_t flags = ek_arch_irq_lock();
    table[16 + irq] = (uint32_t)(uintptr_t)handler;
    ek_arch_dsb();
    ek_arch_isb();
    ek_arch_irq_unlock(flags);

    return 0;
}
#    elif defined(__riscv)

// RISC-V 使用 mtvec 配置异常入口，不使用 VTOR
void ek_arch_redirect_vtor(void)
{
}

int ek_arch_irq_install(int32_t irq, ek_arch_isr_t handler)
{
    __EK_UNUSED(irq);
    __EK_UNUSED(handler);
    return -ENOSYS;
}

#    else

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

#    endif

#endif // EKCFG_ARCH == 1
