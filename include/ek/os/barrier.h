/**
 * @file barrier.h
 * @brief RTOS 屏障接口
 * @author N1netyNine99
 *
 * 提供可复用周期性屏障（cyclic barrier）：允许多个线程在同步点等待，
 * 当达到预期数量（expected）后统一唤醒全部等待者并自动进入下一轮（generation 递增），
 * 支持带超时等待与空闲时动态重置预期数量。
 *
 * @note 静态创建的对象在 destroy 后仍占用调用者缓冲区，内核通过内部生命周期标记
 *       拒绝后续访问，避免 use-after-free。
 */

#ifndef EK_OS_BARRIER_H
#define EK_OS_BARRIER_H

#include "ek/conf_internal.h"

#if EKCFG_RTOS == 1

#    include "ek/core/def.h"
#    include "ek/container/list.h"
#    include "ek/os/kernel.h"

#    ifdef __cplusplus
extern "C"
{
#    endif

/**
 * @brief 屏障对象
 */
typedef struct ek_os_barrier ek_os_barrier_t;

/**
 * @brief 屏障控制块
 */
struct ek_os_barrier
{
    bool mem_owned; /**< true 表示控制块由内核堆分配，destroy 时释放；false 表示位于调用者缓冲区 */
    uint32_t count; /**< 本轮已到达并等待在屏障处的线程计数 */
    uint32_t expected; /**< 本轮触发放行所需的线程总数量（必须大于 0） */
    uint32_t generation; /**< 轮次代号，每次放行或 reset 递增，用于防跨轮次误回滚计数 */
    ek_list_node_t wait_list; /**< 等待本屏障的线程链表，按线程有效优先级升序 */
};

/**
 * @brief 静态创建屏障所需的最小缓冲区字节数
 *
 * 缓冲区前部会预留一个按 alignof(ek_os_barrier_t) 对齐的生命周期标记头，
 * 剩余部分容纳控制块。缓冲区起始地址必须按 alignof(ek_os_barrier_t) 对齐。
 */
#    define EK_OS_BARRIER_STATIC_BUF_SIZE \
        (sizeof(ek_os_barrier_t) + EK_ALIGN_UP(sizeof(uint32_t), alignof(ek_os_barrier_t)))

/**
 * @brief 动态创建屏障
 * @param expected 触发一次放行所需的等待线程数量（必须大于 0）
 * @return 成功返回屏障句柄，失败（如内存不足、expected 为 0 或在 ISR 中调用）返回 NULL
 *
 * @note 控制块由内核堆分配，使用完毕后需调用 ek_os_barrier_destroy() 释放。
 * @note 不允许在中断上下文调用。
 */
ek_os_barrier_t *ek_os_barrier_create(uint32_t expected);

/**
 * @brief 在调用者提供的缓冲区上创建屏障
 * @param buf      缓冲区起始地址，必须按 alignof(ek_os_barrier_t) 对齐
 * @param size     缓冲区字节数，不得小于 EK_OS_BARRIER_STATIC_BUF_SIZE
 * @param expected 触发一次放行所需的等待线程数量（必须大于 0）
 * @return 成功返回屏障句柄，失败返回 NULL
 *
 * @note 缓冲区由调用者持有；destroy 只清除生命周期标记，不会释放缓冲区。
 * @note 不允许在中断上下文调用。
 */
ek_os_barrier_t *ek_os_barrier_create_static(void *buf, size_t size, uint32_t expected);

/**
 * @brief 销毁屏障
 * @param barrier 屏障句柄
 *
 * @note 会唤醒所有阻塞等待者，并让它们从 ek_os_barrier_wait() 得到 -ECANCELED。
 * @note 动态对象在此释放控制块内存；静态对象只清除生命周期标记。
 * @note 不允许在中断上下文调用；调用前应确保没有其它上下文仍在操作该屏障。
 */
void ek_os_barrier_destroy(ek_os_barrier_t *barrier);

/**
 * @brief 重置屏障的新一轮预期等待线程数量
 * @param barrier  屏障句柄
 * @param expected 新的预期线程数量（必须大于 0）
 * @return 0 成功
 * @retval -ENOENT 句柄无效或已销毁
 * @retval -EINVAL expected 为 0
 * @retval -EBUSY  当前存在等待者或本轮已有线程到达计数未清零
 *
 * @note 仅应在无线程处于等待状态时调用；重置成功会递增 generation。
 */
int ek_os_barrier_reset(ek_os_barrier_t *barrier, uint32_t expected);

/**
 * @brief 等待直到 expected 个线程全部到达屏障
 * @param barrier 屏障句柄
 * @param timeout 相对超时时长（单位为内核时间基计数，推荐通过 EK_OS_MSEC()/EK_OS_SEC() 构造）；传 EK_OS_FOREVER 表示永久等待，传 0 且尚未凑齐时立即返回 -EAGAIN
 * @return 0 全部线程已到达并成功放行
 * @retval -ENOENT    句柄无效或已销毁
 * @retval -EPERM     在中断上下文或空闲线程调用
 * @retval -EINVAL    屏障配置或内部计数状态异常
 * @retval -EAGAIN    timeout == 0 且尚未凑齐预期线程数
 * @retval -ETIMEDOUT 等待超时（已到达计数会在仍在同一轮时自动退还）
 * @retval -EINTR     等待期间被挂起打断（已到达计数会在仍在同一轮时自动退还）
 * @retval -ECANCELED 等待期间屏障被销毁
 *
 * @note 若本线程恰好是第 expected 个到达的线程，即使 timeout == 0 也会立即清零 count、递增 generation 并唤醒所有等待者返回 0。
 * @note 不允许在中断上下文调用。
 */
int ek_os_barrier_wait(ek_os_barrier_t *barrier, int64_t timeout);

#    if EKCFG_STATIC_ALLOC == 1
#        include "ek/mem/static_alloc.h"

/**
 * @brief 静态声明一个屏障并自动注册初始化
 * @param handle   屏障句柄变量名（static ek_os_barrier_t *，初始化后指向静态缓冲区内的控制块）
 * @param expected 触发一次放行所需的等待线程数量（必须大于 0）
 *
 * @note 缓冲区按 alignof(ek_os_barrier_t) 对齐，大小为 EK_OS_BARRIER_STATIC_BUF_SIZE。
 * @note 必须在 .c 文件作用域使用，且同一翻译单元内重复使用 handle 会重定义。
 * @note 句柄在 ek_static_alloc_init() 执行前为 NULL，不可提前使用。
 */
#        define EK_DEFINE_OS_BARRIER(handle, expected)                                                                \
            static union                                                                                              \
            {                                                                                                         \
                ek_os_barrier_t barrier; /* 抬高 union 的对齐到 alignof(ek_os_barrier_t) */                           \
                uint8_t storage[EK_OS_BARRIER_STATIC_BUF_SIZE];                                                       \
            } handle##_mem;                                                                                           \
            static ek_os_barrier_t *handle;                                                                           \
            static int _static_alloc_init_##handle(void)                                                              \
            {                                                                                                         \
                handle = ek_os_barrier_create_static(handle##_mem.storage, sizeof(handle##_mem.storage), (expected)); \
                return handle ? 0 : -ENOMEM;                                                                          \
            }                                                                                                         \
            EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_OS_BARRIER, _static_alloc_init_##handle)
#    endif /* EKCFG_STATIC_ALLOC == 1 */

#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_RTOS == 1 */

#endif // EK_OS_BARRIER_H
