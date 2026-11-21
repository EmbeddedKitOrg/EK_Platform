/**
 * @file sem.h
 * @brief RTOS 计数信号量接口
 * @author N1netyNine99
 *
 * 提供计数信号量（counting semaphore）：支持内核堆分配与调用者缓冲区两种创建方式，
 * 支持带超时的阻塞等待，等待队列按线程优先级升序排列。
 *
 * @note 静态创建的对象在 destroy 后仍占用调用者缓冲区，内核通过内部生命周期标记
 *       拒绝后续访问，避免 use-after-free。
 */

#ifndef EK_OS_SEM_H
#define EK_OS_SEM_H

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
 * @brief 信号量对象
 */
typedef struct ek_os_sem ek_os_sem_t;

/**
 * @brief 信号量控制块
 */
struct ek_os_sem
{
    bool mem_owned; /**< true 表示控制块由内核堆分配，destroy 时释放；false 表示位于调用者缓冲区 */
    size_t count; /**< 当前可用计数 */
    ek_list_node_t wait_list; /**< 等待本信号量的线程链表，按线程优先级升序 */
};

/**
 * @brief 静态创建信号量所需的最小缓冲区字节数
 *
 * 缓冲区前部会预留一个按 _Alignof(ek_os_sem_t) 对齐的生命周期标记头，
 * 剩余部分容纳控制块。缓冲区起始地址必须按 _Alignof(ek_os_sem_t) 对齐。
 */
#    define EK_OS_SEM_STATIC_BUF_SIZE (sizeof(ek_os_sem_t) + EK_ALIGN_UP(sizeof(uint32_t), _Alignof(ek_os_sem_t)))

/**
 * @brief 动态创建信号量
 * @param init 初始计数
 * @return 成功返回信号量句柄，失败返回 NULL
 *
 * @note 控制块由内核堆分配，使用完毕后需调用 ek_os_sem_destroy() 释放。
 * @note 不允许在中断上下文调用。
 */
ek_os_sem_t *ek_os_sem_create(size_t init);

/**
 * @brief 在调用者提供的缓冲区上创建信号量
 * @param init 初始计数
 * @param buf  缓冲区起始地址，必须按 _Alignof(ek_os_sem_t) 对齐
 * @param size 缓冲区字节数，不得小于 EK_OS_SEM_STATIC_BUF_SIZE
 * @return 成功返回信号量句柄，失败返回 NULL
 *
 * @note 缓冲区由调用者持有；destroy 只清除生命周期标记，不会释放缓冲区。
 * @note 不允许在中断上下文调用。
 */
ek_os_sem_t *ek_os_sem_create_static(size_t init, void *buf, size_t size);

/**
 * @brief 销毁信号量
 * @param sem 信号量句柄
 *
 * @note 会唤醒所有阻塞等待者，并让它们从 ek_os_sem_take() 得到 -ECANCELED。
 * @note 动态对象在此释放控制块内存；静态对象只清除生命周期标记。
 * @note 不允许在中断上下文调用；调用前应确保没有其它上下文仍在操作该信号量。
 */
void ek_os_sem_destroy(ek_os_sem_t *sem);

/**
 * @brief 释放信号量（计数加一，或直接移交给等待者）
 * @param sem 信号量句柄
 * @return 0 成功
 * @retval -ENOENT 句柄无效或已销毁
 *
 * @note 存在阻塞等待者时，直接把信号移交给最高优先级的等待者，计数不增加。
 * @note 允许在中断上下文调用。
 */
int ek_os_sem_give(ek_os_sem_t *sem);

/**
 * @brief 获取信号量
 * @param sem     信号量句柄
 * @param timeout 相对超时时长（单位为内核时间基计数，推荐通过 EK_OS_MSEC()/EK_OS_SEC() 构造）；传 EK_OS_FOREVER 表示永久等待，传 0 表示不等待
 * @return 0 成功获取
 * @retval -ENOENT    句柄无效或已销毁
 * @retval -EPERM     在中断上下文调用，或需阻塞等待时处于空闲线程/内核未启动状态
 * @retval -EAGAIN    超时为 0 且当前无可用计数
 * @retval -ETIMEDOUT 等待超时
 * @retval -EINTR     等待期间被挂起打断
 * @retval -ECANCELED 等待期间信号量被销毁
 *
 * @note 不允许在中断上下文调用。
 */
int ek_os_sem_take(ek_os_sem_t *sem, int64_t timeout);

/**
 * @brief 查询当前可用计数
 * @param sem 信号量句柄
 * @return 当前可用计数；句柄无效或已销毁返回 -ENOENT
 */
int ek_os_sem_count(ek_os_sem_t *sem);

#    if EKCFG_STATIC_ALLOC == 1
#        include "ek/mem/static_alloc.h"

/**
 * @brief 静态声明一个信号量并自动注册初始化
 * @param handle     信号量句柄变量名（static ek_os_sem_t *，初始化后指向静态缓冲区内的控制块）
 * @param init_count 初始计数
 *
 * @note 缓冲区按 _Alignof(ek_os_sem_t) 对齐，大小为 EK_OS_SEM_STATIC_BUF_SIZE。
 * @note 必须在 .c 文件作用域使用，且同一翻译单元内重复使用 handle 会重定义。
 * @note 句柄在 ek_static_alloc_init() 执行前为 NULL，不可提前使用。
 */
#        define EK_DEFINE_OS_SEM(handle, init_count)                                                                \
            static union                                                                                            \
            {                                                                                                       \
                ek_os_sem_t sem; /* 抬高 union 的对齐到 _Alignof(ek_os_sem_t) */                                    \
                uint8_t storage[EK_OS_SEM_STATIC_BUF_SIZE];                                                         \
            } handle##_mem;                                                                                         \
            static ek_os_sem_t *handle;                                                                             \
            static int _static_alloc_init_##handle(void)                                                            \
            {                                                                                                       \
                handle = ek_os_sem_create_static((init_count), handle##_mem.storage, sizeof(handle##_mem.storage)); \
                return handle ? 0 : -ENOMEM;                                                                        \
            }                                                                                                       \
            EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_OS_SEM, _static_alloc_init_##handle)
#    endif /* EKCFG_STATIC_ALLOC == 1 */

#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_RTOS == 1 */

#endif // EK_OS_SEM_H
