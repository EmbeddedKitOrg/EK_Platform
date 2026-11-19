/**
 * @file arch.h
 * @brief 架构相关底层操作
 * @author N1netyNine99
 *
 * 提供 CPU 屏障和临界区保存/恢复；ARM Cortex-M 额外提供向量表重定向。
 * RISC-V 和 host 分支不提供 VTOR 重定向。
 */

#ifndef EK_ARCH_H
#define EK_ARCH_H

#include "ek/conf_internal.h"

#if EKCFG_ARCH == 1

#    include "ek/core/def.h"
#    include "ek/core/export.h"

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
 * @note EKCFG_EXPORT == 1 时由 EK_EXPORT_BOOT 在启动阶段自动调用；
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
void ek_arch_isb(void);

/**
 * @brief 数据同步屏障
 */
void ek_arch_dsb(void);

/**
 * @brief 数据内存屏障
 */
void ek_arch_dmb(void);

/**
 * @brief 关闭中断并保存进入前的状态
 *
 * @return ARM: PRIMASK；RISC-V: mstatus；host: 0
 */
uint32_t ek_arch_irq_lock(void);

/**
 * @brief 恢复进入临界区前的中断状态
 *
 * @param flags ek_arch_irq_lock() 返回的状态值
 *
 * @note host 分支没有中断状态，解锁为空操作。
 */
void ek_arch_irq_unlock(uint32_t flags);

/**
 * @brief 在无可调度线程时让 CPU 进入低功耗等待
 *
 * Cortex-M 实现为 WFI：任意使能的中断都会把 CPU 唤醒，因此调用前无需再检查
 * PendSV 是否挂起。RISC-V 实现为 WFI，host 分支为空操作。
 *
 * @note 调用时应已开中断，否则会永久休眠；内核 idle 线程在每次 GC 之后调用。
 */
void ek_arch_idle(void);

/**
 * @brief 判断当前是否处于中断
 *
 * @return true 表示在中断中， false 表示不在中断
 */
bool ek_arch_in_isr(void);

/**
 * @brief 启动操作系统调度器并切入首个线程
 *
 * @note Cortex-M 分支配置 PendSV/SVC 优先级、开中断并触发 SVC 进入首个线程，永不返回。
 */
__EK_NO_RETURN void ek_arch_sched_start(void);

#    if (defined(__arm__) || defined(__thumb__)) && (EKCFG_EXPORT == 1 || EKCFG_STATIC_ALLOC == 1)

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
 * @note EKCFG_EXPORT == 1 时通过 EK_EXPORT_BOOT 在 BOOT 阶段调用
 *       ek_arch_irq_install()，顺序位于 ek_arch_redirect_vtor() 之后、
 *       BOARD 阶段初始化之前。
 * @note EKCFG_EXPORT == 0 且 EKCFG_STATIC_ALLOC == 1 时退回
 *       ek_static_alloc 路径，在 COMPONENT 阶段安装。
 * @note 只安装向量，不设置优先级、不使能中断；优先级仍由 vendor/CMSIS 配置。
 * @note 必须在 .c 文件作用域使用；内部用 __COUNTER__ 生成唯一符号，
 *       没有 __COUNTER__ 时退回 __LINE__，此时同一行只能出现一次。
 */
#        define EK_DEFINE_ARCH_IRQ(irqn, handler)           _EK_DEFINE_ARCH_IRQ_IMPL(_EK_ARCH_UNIQUE_ID, irqn, handler)
#        define _EK_DEFINE_ARCH_IRQ_IMPL(id, irqn, handler) _EK_DEFINE_ARCH_IRQ_IMPL2(id, irqn, handler)

#        if EKCFG_EXPORT == 1
#            define _EK_DEFINE_ARCH_IRQ_IMPL2(id, irqn, handler)     \
                static void _ek_arch_irq_init_##id(void)             \
                {                                                    \
                    ek_arch_irq_install((int32_t)(irqn), (handler)); \
                }                                                    \
                EK_EXPORT_BOOT(_ek_arch_irq_init_##id, EK_EXPORT_ORDER_BOOT_ARCH_IRQ)
#        else
#            include "ek/mem/static_alloc.h"
#            define _EK_DEFINE_ARCH_IRQ_IMPL2(id, irqn, handler)            \
                static int _static_alloc_init_arch_irq_##id(void)           \
                {                                                           \
                    return ek_arch_irq_install((int32_t)(irqn), (handler)); \
                }                                                           \
                EK_STATIC_ALLOC_REGISTER(                                   \
                    arch_irq_##id, EK_STATIC_ALLOC_ORDER_ARCH_IRQ, _static_alloc_init_arch_irq_##id)
#        endif
#    endif /* ARM && (EKCFG_EXPORT || EKCFG_STATIC_ALLOC) */

#    if EKCFG_RTOS == 1
/**
 * @brief 在线程栈顶构造初始寄存器上下文
 *
 * @param stack_top 线程可用栈区最高地址（函数内部会向下做架构要求的对齐）
 * @param entry     线程入口函数
 * @param arg       传递给线程入口函数的参数
 * @param exit      线程入口函数意外返回时的兜底退出函数
 * @return 构造完成后的初始栈顶指针（写入 TCB 的 sp）
 *
 * @note 内部伪造中断返回现场，供首次上下文切换直接恢复并跳转至 entry。
 */
void *ek_arch_stack_init(void *stack_top, void *(*entry)(void *), void *arg, void (*exit)(void));

/**
 * @brief 触发一次线程上下文切换请求
 *
 * @note Cortex-M 分支通过置位 PendSV 悬起异常，在退出临界区或中断后完成切换。
 */
void ek_arch_switch_yield(void);
#    endif

#    ifdef __cplusplus
}
#    endif

#endif // EKCFG_ARCH == 1

#endif // EK_ARCH_H
