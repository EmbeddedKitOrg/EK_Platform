/**
 * @file ek_critical.c
 * @brief 临界区默认实现
 */

#include "ek/core/ek_critical.h"

/* 嵌套深度与最外层进入前的中断状态；只在中断已关闭时读写，无需额外保护 */
static uint32_t s_critical_nest;
static uint32_t s_critical_saved;

#if defined(__CC_ARM)

/* ARM Compiler 5：与 CMSIS 相同，通过命名寄存器变量直接读写 PRIMASK */
__EK_WEAK void ek_critical_enter(void)
{
    register uint32_t primask_reg __asm("primask");
    const uint32_t primask = primask_reg;
    __disable_irq();
    if (s_critical_nest++ == 0U) s_critical_saved = primask;
}

__EK_WEAK void ek_critical_exit(void)
{
    if (s_critical_nest == 0U) return;
    if (--s_critical_nest == 0U)
    {
        register uint32_t primask_reg __asm("primask");
        primask_reg = s_critical_saved;
    }
}

#elif (defined(__GNUC__) || defined(__clang__)) && defined(__arm__) && defined(__ARM_ARCH_PROFILE) && \
    (__ARM_ARCH_PROFILE == 'M')

/* Cortex-M：GCC / armclang */
__EK_WEAK void ek_critical_enter(void)
{
    uint32_t primask;
    __asm volatile("mrs %0, primask\n\tcpsid i" : "=r"(primask)::"memory");
    if (s_critical_nest++ == 0U) s_critical_saved = primask;
}

__EK_WEAK void ek_critical_exit(void)
{
    if (s_critical_nest == 0U) return;
    if (--s_critical_nest == 0U)
    {
        const uint32_t primask = s_critical_saved;
        __asm volatile("msr primask, %0" ::"r"(primask) : "memory");
    }
}

#elif (defined(__GNUC__) || defined(__clang__)) && defined(__riscv)

/* RISC-V 机器模式：清除 mstatus.MIE（bit 3） */
__EK_WEAK void ek_critical_enter(void)
{
    unsigned long mstatus;
    __asm volatile("csrrci %0, mstatus, 8" : "=r"(mstatus)::"memory");
    if (s_critical_nest++ == 0U) s_critical_saved = (uint32_t)(mstatus & 8UL);
}

__EK_WEAK void ek_critical_exit(void)
{
    if (s_critical_nest == 0U) return;
    if (--s_critical_nest == 0U && s_critical_saved != 0U)
    {
        __asm volatile("csrsi mstatus, 8" ::: "memory");
    }
}

#else

/* 未知平台（含主机测试）：只维护嵌套计数并加编译器屏障 */
__EK_WEAK void ek_critical_enter(void)
{
    s_critical_nest++;
    s_critical_saved = 0U;
    EK_COMPILER_BARRIER();
}

__EK_WEAK void ek_critical_exit(void)
{
    EK_COMPILER_BARRIER();
    if (s_critical_nest != 0U) s_critical_nest--;
}

#endif
