/**
 * @file ek_arch.c
 * @brief 架构相关底层操作实现
 * @author N1netyNine99
 *
 * ARM 分支提供向量表重定向；RISC-V 和 host 分支的 VTOR 重定向为空实现。
 */

#include "ek/core/ek_arch.h"

#if EKCFG_ARCH == 1

#include "ek/core/ek_assert.h"

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
#    elif defined(__riscv)

// RISC-V 使用 mtvec 配置异常入口，不使用 VTOR
void ek_arch_redirect_vtor(void)
{
}

#    else

// host 构建没有异常向量表
void ek_arch_redirect_vtor(void)
{
}

#    endif

#endif // EKCFG_ARCH == 1
