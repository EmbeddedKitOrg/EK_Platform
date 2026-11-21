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
#    define EK_OS_MSEC(ms) \
        ((int64_t)(ms) * (int64_t)(EKCFG_OS_KERNEL_TIMEBASE_HZ) / 1000LL) /**< 毫秒转为内核时间基计数 */
#    define EK_OS_SEC(s) ((int64_t)(s) * (int64_t)(EKCFG_OS_KERNEL_TIMEBASE_HZ)) /**< 秒转为内核时间基计数 */

/**
 * @brief 内核时间源与定时唤醒操作表（由 BSP/Port 实现）
 */
typedef struct
{
    uint64_t (*now)(void); /**< 获取当前单调 64 位时间（单位：1 / EKCFG_OS_KERNEL_TIMEBASE_HZ 秒） */
    void (*set_timeout)(
        int64_t delta); /**< 预约 delta 个内核时间基周期后触发 ek_os_kernel_announce_isr()；< 0 表示停表 */
} ek_os_timer_ops_t;

/**
 * @brief 进入内核临界区（关闭中断并保存进入前的状态）
 *
 * 所有会操作就绪队列、超时链表、等待队列等共享调度状态的内核路径都必须调用。
 * 支持嵌套：内层返回的 flags 必须原样传回 ek_os_kernel_spin_unlock()。
 *
 * @return 进入前的中断状态：Cortex-M 上即 PRIMASK 寄存器的值——0 表示此前中断是开启的，
 *         非 0 表示此前已经处于关中断状态；该值必须原样传回 ek_os_kernel_spin_unlock()，
 *         不能自行构造，否则嵌套临界区会提前打开中断。
 */
__EK_STATIC_INLINE uint32_t ek_os_kernel_spin_lock(void)
{
    return ek_arch_irq_lock();
}

/**
 * @brief 退出内核临界区（恢复进入前的中断状态）
 *
 * @param flags ek_os_kernel_spin_lock() 返回的进入前中断状态（Cortex-M 上为 PRIMASK）；
 *              原样传回以恢复进入该层临界区前的中断开关状态
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
 * @return 当前 64 位时间戳（单位为内核时间基计数；未配置时间源时返回 0）
 */
uint64_t ek_os_kernel_time(void);

/**
 * @brief 定时器超时/闹钟中断入口（由 BSP 定时器 ISR 清标志后调用）
 */
void ek_os_kernel_announce_isr(void);

/**
 * @brief 返回当前运行线程的 tid
 * @return 当前运行线程的 tid（正整数，含空闲线程）；内核尚未启动时返回 -ESRCH
 */
int ek_os_kernel_current_tid(void);

/**
 * @brief 锁定当前线程的调度器（禁止被抢占，但不关闭中断）
 * @return 加锁后的嵌套层数（1..255）；失败返回负 errno
 * @retval -EPERM 在中断上下文调用
 * @retval -ESRCH 内核尚未启动
 *
 * @note 可嵌套，必须与 ek_os_kernel_sched_unlock() 成对调用。
 * @note 只阻止「更高优先级线程就绪」引发的抢占；当前线程主动阻塞（如等待信号量）仍会切走。
 * @note 中断照常响应，只是不会因就绪队列变化而在返回时切换线程。
 * @note 层数饱和于 UINT8_MAX，不会回绕。
 */
int ek_os_kernel_sched_lock(void);

/**
 * @brief 解除当前线程的调度器锁
 * @return 解锁后的剩余嵌套层数（0 表示已完全解锁）；失败返回负 errno
 * @retval -EPERM 在中断上下文调用
 * @retval -ESRCH 内核尚未启动
 *
 * @note 层数归零时会立即补一次调度判决，使锁期间就绪的更高优先级线程得以运行。
 * @note 对未加锁的线程调用不会报错，直接返回 0 且不触发调度。
 */
int ek_os_kernel_sched_unlock(void);

#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_RTOS == 1 */

#endif // EK_OS_KERNEL_H
