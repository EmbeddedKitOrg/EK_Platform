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
 * @brief 异常处理函数类型
 */
typedef void (*ek_arch_isr_t)(void);

/**
 * @brief 重定向异常向量表
 *
 * ARM Cortex-M 分支把 Flash 向量表复制到 SRAM，并将 VTOR 指向 SRAM 副本。
 * RISC-V 和 host 分支为空实现。
 *
 * @note ARM 分支要求链接脚本提供 _flash_vtor_start/_flash_vtor_end 和
 *       _sram_vtor_start/_sram_vtor_end，且两块区域大小一致。
 * @note EKCFG_EXPORT == 1 时由 EK_EXPORT_HARDWARE 在启动阶段自动调用；
 *       重复调用是幂等的。
 */
void ek_arch_redirect_vtor(void);

/**
 * @brief 把异常处理函数安装到当前向量表
 *
 * @param irq     外部 IRQ 编号，或负的核心异常编号（-14..-1）
 * @param handler 异常处理函数，不能为 NULL
 * @return 0 成功；负数 errno 失败
 * @retval -EINVAL 参数非法
 * @retval -EAGAIN VTOR 尚未指向 SRAM 向量表
 *
 * @note 仅修改向量表，不设置优先级、不使能中断。
 * @note EKCFG_EXPORT == 1 时启动阶段会自动重定向 VTOR；
 *       未启用 EKCFG_EXPORT 时需先手动调用 ek_arch_redirect_vtor()。
 */
int ek_arch_irq_install(int32_t irq, ek_arch_isr_t handler);

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

#    if EKCFG_STATIC_ALLOC == 1 && (defined(__arm__) || defined(__thumb__))
#        include "ek/mem/ek_static_alloc.h"

#        if defined(__COUNTER__)
#            define _EK_ARCH_UNIQUE_ID __COUNTER__
#        else
#            define _EK_ARCH_UNIQUE_ID __LINE__
#        endif

/**
 * @brief 静态声明并自动安装一个中断处理函数
 *
 * @param irqn    外部 IRQ 编号，或负的核心异常编号
 * @param handler 中断处理函数
 *
 * @note 启动时通过 ek_static_alloc 调用 ek_arch_irq_install()。
 * @note 只安装向量，不设置优先级、不使能中断；优先级仍由 vendor/CMSIS 配置。
 * @note 必须在 .c 文件作用域使用；内部用 __COUNTER__ 生成唯一符号，
 *       没有 __COUNTER__ 时退回 __LINE__，此时同一行只能出现一次。
 */
#        define EK_DEFINE_ARCH_IRQ(irqn, handler) \
            _EK_DEFINE_ARCH_IRQ_IMPL(_EK_ARCH_UNIQUE_ID, irqn, handler)
#        define _EK_DEFINE_ARCH_IRQ_IMPL(id, irqn, handler) \
            _EK_DEFINE_ARCH_IRQ_IMPL2(id, irqn, handler)
#        define _EK_DEFINE_ARCH_IRQ_IMPL2(id, irqn, handler)                 \
            static int _static_alloc_init_arch_irq_##id(void)               \
            {                                                               \
                return ek_arch_irq_install((int32_t)(irqn), (handler));     \
            }                                                               \
            EK_STATIC_ALLOC_REGISTER(arch_irq_##id, EK_STATIC_ALLOC_ORDER_ARCH_IRQ, _static_alloc_init_arch_irq_##id)
#    endif /* EKCFG_STATIC_ALLOC && ARM */

#    ifdef __cplusplus
}
#    endif

#endif // EKCFG_ARCH == 1

#endif // EK_ARCH_H
