/**
 * @file kernel.h
 * @brief 无滴答 RTOS 内核调度与时间管理接口
 * @author N1netyNine99
 */

#ifndef EK_OS_KERNEL_H
#define EK_OS_KERNEL_H

#include "ek/conf_internal.h"

#if EKCFG_RTOS == 1

#    include "ek/core/def.h"
#    include "ek/core/arch.h"

#    ifdef __cplusplus
extern "C"
{
#    endif

#    define EK_OS_FOREVER ((int64_t)(-1)) /**< 永久阻塞超时值 */

/**
 * @brief 内核时间源与定时唤醒操作表（由 BSP/Port 实现）
 */
typedef struct
{
    uint64_t (*now)(void); /**< 获取当前单调 64 位时间 */
    void (*set_timeout)(int64_t delta); /**< 预约 delta 后触发 ek_os_kernel_announce_isr()；< 0 表示停表 */
} ek_os_timer_ops_t;

/**
 * @brief 进入内核临界区（关闭中断并保存进入前的状态）
 *
 * 所有会操作就绪队列、超时链表、等待队列等共享调度状态的内核路径都必须调用。
 * 支持嵌套：内层返回的 flags 必须原样传回 ek_os_kernel_spin_unlock()。
 *
 * @return 进入前的中断状态标志，必须与 ek_os_kernel_spin_unlock() 配对使用
 */
__EK_STATIC_INLINE uint32_t ek_os_kernel_spin_lock(void)
{
    return ek_arch_irq_lock();
}

/**
 * @brief 退出内核临界区（恢复进入前的中断状态）
 *
 * @param flags ek_os_kernel_spin_lock() 返回的状态标志
 *
 * @note 若这是最外层临界区且期间已置位 PendSV，本调用会直接触发上下文切换。
 */
__EK_STATIC_INLINE void ek_os_kernel_spin_unlock(uint32_t flags)
{
    ek_arch_irq_unlock(flags);
}

/**
 * @brief 启动内核调度器（永不返回）
 * @param ops 时间源与单次闹钟操作表（不可为 NULL，且 now/set_timeout 均须有效）
 */
__EK_NO_RETURN void ek_os_kernel_start(const ek_os_timer_ops_t *ops);

/**
 * @brief 获取内核单调当前时间
 * @return 当前 64 位时间戳（未配置时间源时返回 0）
 */
uint64_t ek_os_kernel_time(void);

/**
 * @brief 定时器超时/闹钟中断入口（由 BSP 定时器 ISR 清标志后调用）
 */
void ek_os_kernel_announce_isr(void);

/**
 * @brief 返回当前工作线程的 tid
 * @return > 0 表示当前工作线程的 tid，< 0 表示内核没就绪
 */
int ek_os_kernel_current_tid(void);

#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_RTOS == 1 */

#endif // EK_OS_KERNEL_H
