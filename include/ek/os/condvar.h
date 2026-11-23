/**
 * @file condvar.h
 * @brief RTOS 条件变量接口
 * @author N1netyNine99
 *
 * 提供条件变量（condition variable）：与互斥量（ek_os_mutex_t）配合使用，
 * 支持原子释放互斥量并带超时阻塞等待、唤醒单个等待者（signal）及唤醒全部等待者（broadcast），
 * 等待队列按线程优先级升序排列。
 *
 * @note 静态创建的对象在 destroy 后仍占用调用者缓冲区，内核通过内部生命周期标记
 *       拒绝后续访问，避免 use-after-free。
 */

#ifndef EK_OS_CONDVAR_H
#define EK_OS_CONDVAR_H

#include "ek/conf_internal.h"

#if EKCFG_RTOS == 1

#    include "ek/core/def.h"
#    include "ek/container/list.h"
#    include "ek/os/kernel.h"
#    include "ek/os/mutex.h"

#    ifdef __cplusplus
extern "C"
{
#    endif

/**
 * @brief 条件变量对象
 */
typedef struct ek_os_condvar ek_os_condvar_t;

/**
 * @brief 条件变量控制块
 */
struct ek_os_condvar
{
    bool mem_owned; /**< true 表示控制块由内核堆分配，destroy 时释放；false 表示位于调用者缓冲区 */
    ek_list_node_t wait_list; /**< 等待本条件变量的线程链表，按线程有效优先级升序 */
};

/**
 * @brief 静态创建条件变量所需的最小缓冲区字节数
 *
 * 缓冲区前部会预留一个按 alignof(ek_os_condvar_t) 对齐的生命周期标记头，
 * 剩余部分容纳控制块。缓冲区起始地址必须按 alignof(ek_os_condvar_t) 对齐。
 */
#    define EK_OS_CONDVAR_STATIC_BUF_SIZE \
        (sizeof(ek_os_condvar_t) + EK_ALIGN_UP(sizeof(uint32_t), alignof(ek_os_condvar_t)))

/**
 * @brief 动态创建条件变量
 * @return 成功返回条件变量句柄，失败返回 NULL
 *
 * @note 控制块由内核堆分配，使用完毕后需调用 ek_os_condvar_destroy() 释放。
 * @note 不允许在中断上下文调用。
 */
ek_os_condvar_t *ek_os_condvar_create(void);

/**
 * @brief 在调用者提供的缓冲区上创建条件变量
 * @param buf  缓冲区起始地址，必须按 alignof(ek_os_condvar_t) 对齐
 * @param size 缓冲区字节数，不得小于 EK_OS_CONDVAR_STATIC_BUF_SIZE
 * @return 成功返回条件变量句柄，失败返回 NULL
 *
 * @note 缓冲区由调用者持有；destroy 只清除生命周期标记，不会释放缓冲区。
 * @note 不允许在中断上下文调用。
 */
ek_os_condvar_t *ek_os_condvar_create_static(void *buf, size_t size);

/**
 * @brief 销毁条件变量
 * @param condvar 条件变量句柄
 *
 * @note 会唤醒所有阻塞等待者，并让它们从 ek_os_condvar_wait() 得到 -ECANCELED。
 * @note 动态对象在此释放控制块内存；静态对象只清除生命周期标记。
 * @note 不允许在中断上下文调用；调用前应确保没有其它上下文仍在操作该条件变量。
 */
void ek_os_condvar_destroy(ek_os_condvar_t *condvar);

/**
 * @brief 等待条件变量成立（原子释放互斥量并阻塞，返回前重新加锁）
 * @param condvar 条件变量句柄
 * @param mutex   调用者当前持有的互斥量（不可重入嵌套超过 1 层）
 * @param timeout 相对超时时长（单位为内核时间基计数，推荐通过 EK_OS_MSEC()/EK_OS_SEC() 构造）；传 EK_OS_FOREVER 表示永久等待，传 0 立即超时返回 -ETIMEDOUT
 * @return 0 被正常唤醒且已重新持有 mutex
 * @retval -ENOENT    condvar 或 mutex 句柄无效/已销毁
 * @retval -EPERM     在中断上下文调用、空闲线程调用，或当前线程未持有该 mutex
 * @retval -EDEADLK   mutex 重入加锁嵌套层数大于 1
 * @retval -ETIMEDOUT 等待超时（或 timeout == 0）
 * @retval -EINTR     等待期间被挂起打断
 * @retval -ECANCELED 等待期间 condvar 被销毁，或重新加锁期间 mutex 被销毁
 *
 * @note 内部会在自旋锁临界区内原子释放 mutex 并挂入等待队列，唤醒或超时返回前会重新调用
 *       ek_os_mutex_lock() 获取 mutex（重加锁期间忽略 -EINTR 重试）。
 * @note 调用者应使用 while (!cond) 循环检查谓词条件，防止虚假唤醒或锁竞争抢占。
 * @note 不允许在中断上下文调用。
 */
int ek_os_condvar_wait(ek_os_condvar_t *condvar, ek_os_mutex_t *mutex, int64_t timeout);

/**
 * @brief 唤醒条件变量等待队列中的一个等待者
 * @param condvar 条件变量句柄
 * @return 0 成功
 * @retval -ENOENT 句柄无效或已销毁
 *
 * @note 若存在阻塞等待者，唤醒其中最高优先级的等待者；等待队列为空时直接返回 0（不记忆信号）。
 * @note 允许在中断上下文调用。
 */
int ek_os_condvar_signal(ek_os_condvar_t *condvar);

/**
 * @brief 唤醒条件变量等待队列中的全部等待者
 * @param condvar 条件变量句柄
 * @return 0 成功
 * @retval -ENOENT 句柄无效或已销毁
 *
 * @note 等待队列为空时直接返回 0（不记忆信号）。
 * @note 允许在中断上下文调用。
 */
int ek_os_condvar_broadcast(ek_os_condvar_t *condvar);

#    if EKCFG_STATIC_ALLOC == 1
#        include "ek/mem/static_alloc.h"

/**
 * @brief 静态声明一个条件变量并自动注册初始化
 * @param handle 条件变量句柄变量名（static ek_os_condvar_t *，初始化后指向静态缓冲区内的控制块）
 *
 * @note 缓冲区按 alignof(ek_os_condvar_t) 对齐，大小为 EK_OS_CONDVAR_STATIC_BUF_SIZE。
 * @note 必须在 .c 文件作用域使用，且同一翻译单元内重复使用 handle 会重定义。
 * @note 句柄在 ek_static_alloc_init() 执行前为 NULL，不可提前使用。
 */
#        define EK_DEFINE_OS_CONDVAR(handle)                                                              \
            static union                                                                                  \
            {                                                                                             \
                ek_os_condvar_t condvar; /* 抬高 union 的对齐到 alignof(ek_os_condvar_t) */               \
                uint8_t storage[EK_OS_CONDVAR_STATIC_BUF_SIZE];                                           \
            } handle##_mem;                                                                               \
            static ek_os_condvar_t *handle;                                                               \
            static int _static_alloc_init_##handle(void)                                                  \
            {                                                                                             \
                handle = ek_os_condvar_create_static(handle##_mem.storage, sizeof(handle##_mem.storage)); \
                return handle ? 0 : -ENOMEM;                                                              \
            }                                                                                             \
            EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_OS_CONDVAR, _static_alloc_init_##handle)
#    endif /* EKCFG_STATIC_ALLOC == 1 */

#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_RTOS == 1 */

#endif // EK_OS_CONDVAR_H
