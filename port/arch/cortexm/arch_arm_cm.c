/**
 * @file arch_arm_cm.c
 * @brief 架构相关底层操作实现
 * @author N1netyNine99
 *
 * ARM 分支提供向量表重定向与中断安装
 */
#include "ek/core/arch.h"

#if EKCFG_ARCH == 1

#    include "ek/core/export.h"

extern uint32_t _flash_vtor_start[];
extern uint32_t _flash_vtor_end[];
extern uint32_t _sram_vtor_start[];
extern uint32_t _sram_vtor_end[];

#    ifndef EK_SOC_PATH
#        error "EK_SOC_PATH is not set: add ek_soc.h to the project and pass its path via CMake SOC."
#    endif

#    include "ek_soc.h"

static bool s_vtor_redirected;

void ek_arch_isb(void)
{
    __ISB();
}

void ek_arch_dsb(void)
{
    __DSB();
}

void ek_arch_dmb(void)
{
    __DMB();
}

uint32_t ek_arch_irq_lock(void)
{
    uint32_t flags = __get_PRIMASK();
    __disable_irq();
    return flags;
}

void ek_arch_irq_unlock(uint32_t flags)
{
    __set_PRIMASK(flags);
}

void ek_arch_redirect_vtor(void)
{
    if (s_vtor_redirected) return;

    size_t size = (size_t)((uintptr_t)_flash_vtor_end - (uintptr_t)_flash_vtor_start);

    uint32_t flags = ek_arch_irq_lock();
    memcpy(_sram_vtor_start, _flash_vtor_start, size);

    SCB->VTOR = (uintptr_t)_sram_vtor_start;

    ek_arch_dsb();
    ek_arch_isb();

    s_vtor_redirected = true;
    ek_arch_irq_unlock(flags);
}

EK_EXPORT_BOOT(ek_arch_redirect_vtor, EK_EXPORT_ORDER_BOOT_ARCH);

int ek_arch_irq_install(int32_t irq, ek_arch_isr_t handler)
{
    if (handler == NULL) return -EINVAL;
    if (irq < -14 || irq >= EKCFG_ARCH_IRQ_COUNT) return -EINVAL;
    if (SCB->VTOR != (uintptr_t)_sram_vtor_start) return -EAGAIN;

    uint32_t *table = (uint32_t *)(uintptr_t)SCB->VTOR;
    if (table == NULL) return -EINVAL;

    uint32_t flags = ek_arch_irq_lock();
    table[16 + irq] = (uint32_t)(uintptr_t)handler;
    ek_arch_dsb();
    ek_arch_isb();
    ek_arch_irq_unlock(flags);

    return 0;
}

#endif // EKCFG_ARCH == 1
