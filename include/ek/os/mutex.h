/**
 * @file mutex.h
 * @brief RTOS 互斥量接口（支持可重入加锁与优先级继承）
 * @author N1netyNine99
 */

#ifndef EK_OS_MUTEX_H
#define EK_OS_MUTEX_H

#include "ek/conf_internal.h"

#if EKCFG_RTOS == 1

#    include "ek/core/def.h"
#    include "ek/container/list.h"

#    ifdef __cplusplus
extern "C"
{
#    endif

/**
 * @brief 互斥量对象
 */
typedef struct ek_os_mutex ek_os_mutex_t;

/**
 * @brief 互斥量控制块
 */
struct ek_os_mutex
{
    bool mem_owned; /**< true 表示控制块由内核堆分配，destroy 时释放；false 表示位于调用者缓冲区 */
    void *owner; /**< 当前持有者线程指针 (os_thread_t *)；NULL 表示未加锁 */
    uint32_t lock_count; /**< 重入加锁计数；0 表示空闲，同一线程重复 lock 时递增 */
    ek_list_node_t wait_list; /**< 等待本互斥量的线程链表，按线程有效优先级升序 */
    ek_list_node_t owner_node; /**< 挂入持有者线程 lock.owned_list 链表的侵入式节点 */
};

/**
 * @brief 静态创建互斥量所需的最小缓冲区字节数
 */
#    define EK_OS_MUTEX_STATIC_BUF_SIZE (sizeof(ek_os_mutex_t) + EK_ALIGN_UP(sizeof(uint32_t), alignof(ek_os_mutex_t)))

/**
 * @brief 动态创建互斥量
 * @return 成功返回互斥量句柄，失败返回 NULL
 * @note 不允许在中断上下文调用。
 */
ek_os_mutex_t *ek_os_mutex_create(void);

/**
 * @brief 在调用者提供的缓冲区上创建互斥量
 * @param buf  缓冲区起始地址，必须按 alignof(ek_os_mutex_t) 对齐
 * @param size 缓冲区字节数，不得小于 EK_OS_MUTEX_STATIC_BUF_SIZE
 * @return 成功返回互斥量句柄，失败返回 NULL
 * @note 缓冲区由调用者持有；destroy 只清除生命周期标记，不会释放缓冲区。
 * @note 不允许在中断上下文调用。
 */
ek_os_mutex_t *ek_os_mutex_create_static(void *buf, size_t size);

/**
 * @brief 销毁互斥量（唤醒所有阻塞等待者返回 -ECANCELED，并重新计算原持有者的优先级）
 * @param mutex 互斥量句柄
 * @note 不允许在中断上下文调用。
 */
void ek_os_mutex_destroy(ek_os_mutex_t *mutex);

/**
 * @brief 获取互斥量（支持同一线程重入加锁与优先级继承）
 * @param mutex   互斥量句柄
 * @param timeout 相对超时时长（单位为内核时间基计数，推荐通过 EK_OS_MSEC()/EK_OS_SEC() 构造）；传 EK_OS_FOREVER 表示永久等待，传 0 表示非阻塞试探
 * @return 0 成功获取，负数错误码失败（-ENOENT/-EPERM/-EAGAIN/-ETIMEDOUT/-EINTR/-ECANCELED/-EOVERFLOW）
 * @note 不允许在中断上下文或空闲线程上下文调用。
 */
int ek_os_mutex_lock(ek_os_mutex_t *mutex, int64_t timeout);

/**
 * @brief 非阻塞尝试获取互斥量
 * @param mutex 互斥量句柄
 * @return 0 成功获取，-EAGAIN 锁已被其它线程持有，或其它负数错误码
 * @note 不允许在中断上下文或空闲线程上下文调用。
 */
static inline int ek_os_mutex_trylock(ek_os_mutex_t *mutex)
{
    return ek_os_mutex_lock(mutex, 0);
}

/**
 * @brief 释放互斥量（仅持有者可调用；计数归 0 时移交所有权给最高优先级等待者并重算原持有者优先级）
 * @param mutex 互斥量句柄
 * @return 0 成功，负数错误码失败（-ENOENT/-EPERM）
 * @note 不允许在中断上下文调用。
 */
int ek_os_mutex_unlock(ek_os_mutex_t *mutex);

#    if EKCFG_STATIC_ALLOC == 1
#        include "ek/mem/static_alloc.h"

/**
 * @brief 静态声明一个互斥量并自动注册初始化
 * @param handle 互斥量句柄变量名（static ek_os_mutex_t *）
 */
#        define EK_DEFINE_OS_MUTEX(handle)                                                              \
            static union                                                                                \
            {                                                                                           \
                ek_os_mutex_t mutex;                                                                    \
                uint8_t storage[EK_OS_MUTEX_STATIC_BUF_SIZE];                                           \
            } handle##_mem;                                                                             \
            static ek_os_mutex_t *handle;                                                               \
            static int _static_alloc_init_##handle(void)                                                \
            {                                                                                           \
                handle = ek_os_mutex_create_static(handle##_mem.storage, sizeof(handle##_mem.storage)); \
                return handle ? 0 : -ENOMEM;                                                            \
            }                                                                                           \
            EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_OS_MUTEX, _static_alloc_init_##handle)
#    endif /* EKCFG_STATIC_ALLOC == 1 */

#    ifdef __cplusplus
}
#    endif

#endif

#endif // EK_OS_MUTEX_H
