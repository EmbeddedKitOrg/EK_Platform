/**
 * @file arch.c
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

struct cm_stack_frame
{
    struct
    {
#    if defined(__FPU_PRESENT) && (__FPU_PRESENT == 1) && defined(__VFP_FP__) && !defined(__SOFTFP__)
        uint32_t exc_return;
#    endif
        uint32_t r4;
        uint32_t r5;
        uint32_t r6;
        uint32_t r7;
        uint32_t r8;
        uint32_t r9;
        uint32_t r10;
        uint32_t r11;
    } sf_save;

    struct
    {
        uint32_t r0;
        uint32_t r1;
        uint32_t r2;
        uint32_t r3;
        uint32_t r12;
        uint32_t lr;
        uint32_t pc;
        uint32_t xpsr;
    } hw_save;
};

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

void ek_arch_idle(void)
{
    __WFI();
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

bool ek_arch_in_isr(void)
{
    return (__get_IPSR() & 0x1FFU) != 0U;
}

__EK_NO_RETURN void ek_arch_sched_start(void)
{
    NVIC_SetPriority(PendSV_IRQn, 0xFFU);
    NVIC_SetPriority(SVCall_IRQn, 0x00U);

    __enable_irq();
    __EK_ASM("svc 0");

    __builtin_unreachable();
}

#    if defined(EKCFG_RTOS) && (EKCFG_RTOS == 1)

#        include "ek/os/thread.h"

extern void ek_arch_start_first_thread(void);
extern void ek_arch_switch_thread(void);
extern void ek_os_thread_entry_wrapper(ek_os_thread_fn_t entry, void *arg);

EK_DEFINE_ARCH_IRQ(SVCall_IRQn, ek_arch_start_first_thread);
EK_DEFINE_ARCH_IRQ(PendSV_IRQn, ek_arch_switch_thread);

void *ek_arch_stack_init(void *stack_top, ek_os_thread_fn_t entry, void *arg, void (*exit)(void))
{
    uintptr_t sp = (uintptr_t)EK_ALIGN_DOWN(stack_top, 8);
    struct cm_stack_frame *frame = (struct cm_stack_frame *)(sp - sizeof(*frame));

    memset(frame, 0, sizeof(*frame));

#        if defined(__FPU_PRESENT) && (__FPU_PRESENT == 1) && defined(__VFP_FP__) && !defined(__SOFTFP__)
    frame->sf_save.exc_return = 0xFFFFFFFDU;
#        endif

    frame->hw_save.r0 = (uint32_t)(uintptr_t)entry;
    frame->hw_save.r1 = (uint32_t)(uintptr_t)arg;
    frame->hw_save.lr = (uint32_t)(uintptr_t)exit;
    frame->hw_save.pc = (uint32_t)(uintptr_t)ek_os_thread_entry_wrapper;
    frame->hw_save.xpsr = 0x01000000U; // bit24 (Thumb 位) 必须为 1

    return (void *)frame;
}

void ek_arch_switch_yield(void)
{
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    __DSB();
    __ISB();
}

#    endif

#endif // EKCFG_ARCH == 1
