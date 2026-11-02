/**
 * @file ek_arch.h
 * @brief 架构相关底层操作
 * @author N1netyNine99
 *
 * 提供 CPU 屏障和临界区保存/恢复；ARM Cortex-M 额外提供向量表重定向。
 * RISC-V 和 host 分支不提供 VTOR 重定向。
 */

#ifndef EK_ARCH_H
#define EK_ARCH_H

#include "ek/ek_conf_internal.h"

#if EKCFG_ARCH == 1

#    include "ek/core/ek_def.h"

#    ifdef __cplusplus
extern "C"
{
#    endif

/**
 * @brief 重定向异常向量表
 *
 * ARM Cortex-M 分支把 Flash 向量表复制到 SRAM，并将 VTOR 指向 SRAM 副本。
 * RISC-V 和 host 分支为空实现。
 *
 * @note ARM 分支要求链接脚本提供 _flash_vtor_start/_flash_vtor_end 和
 *       _sram_vtor_start/_sram_vtor_end，且两块区域大小一致。
 */
void ek_arch_redirect_vtor(void);

/**
 * @brief 指令同步屏障
 */
static inline void ek_arch_isb(void);

/**
 * @brief 数据同步屏障
 */
static inline void ek_arch_dsb(void);

/**
 * @brief 数据内存屏障
 */
static inline void ek_arch_dmb(void);

/**
 * @brief 关闭中断并保存进入前的状态
 *
 * @return ARM: PRIMASK；RISC-V: mstatus；host: 0
 */
static inline uint32_t ek_arch_irq_lock(void);

/**
 * @brief 恢复进入临界区前的中断状态
 *
 * @param flags ek_arch_irq_lock() 返回的状态值
 *
 * @note host 分支没有中断状态，解锁为空操作。
 */
static inline void ek_arch_irq_unlock(uint32_t flags);

// ARM
#    if defined(__arm__) || defined(__thumb__)

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

static inline uint32_t ek_arch_irq_lock(void)
{
    uint32_t result;
    __EK_ASM("mrs %0, primask \n cpsid i" : "=r"(result)::"memory");
    return result;
}

static inline void ek_arch_irq_unlock(uint32_t flags)
{
    __EK_ASM("msr primask, %0" ::"r"(flags) : "memory");
}

// RISC-V
#    elif defined(__riscv)

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

static inline uint32_t ek_arch_irq_lock(void)
{
    uint32_t flags;

    // M 模式：清 mstatus.MIE，返回进入临界区前的 mstatus
    __EK_ASM("csrrci %0, mstatus, 8" : "=r"(flags)::"memory");
    return flags;
}

static inline void ek_arch_irq_unlock(uint32_t flags)
{
    // 只恢复 MIE，避免覆盖 mstatus 的其他位
    __EK_ASM("csrs mstatus, %0" ::"r"(flags & 8U) : "memory");
}

// Host
#    else

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

static inline uint32_t ek_arch_irq_lock(void)
{
    return 0;
}

static inline void ek_arch_irq_unlock(uint32_t flags)
{
    __EK_UNUSED(flags);
}

#    endif

#    ifdef __cplusplus
}
#    endif

#endif // EKCFG_ARCH == 1

#endif // EK_ARCH_H
