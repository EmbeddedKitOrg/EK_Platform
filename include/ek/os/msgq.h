/**
 * @file msgq.h
 * @brief RTOS 定长消息队列接口（拷贝语义，运行期零分配）
 * @author N1netyNine99
 *
 * 队列在创建时确定单条消息字节数与深度，运行期不再分配内存：
 * 发送把消息拷贝进空闲槽，接收从槽拷贝到调用者缓冲区。
 * 内存开销与时间复杂度都可预测，适合中断与线程之间传递小消息。
 *
 * @note 单条消息建议不超过 64 字节。大块数据请用 sizeof(void *) 传递句柄，
 *       由上层对象池管理生命周期，避免队列内部承担归属与释放语义。
 */

#ifndef EK_OS_MSGQ_H
#define EK_OS_MSGQ_H

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
 * @brief 消息队列对象
 */
typedef struct ek_os_msgq ek_os_msgq_t;

/**
 * @brief 消息队列控制块
 */
struct ek_os_msgq
{
    bool mem_owned; /**< true 表示控制块由内核堆分配，destroy 时释放；false 表示位于调用者缓冲区 */
    size_t item_size; /**< 单条消息字节数，同时作为槽步长 */
    size_t cap; /**< 队列深度（槽位总数） */
    size_t count; /**< 当前消息条数 */
    size_t head; /**< 下一个写入槽下标 */
    size_t tail; /**< 下一个读取槽下标 */
    ek_list_node_t recv_list; /**< 队列空时阻塞的接收者链表，按线程优先级升序 */
    ek_list_node_t send_list; /**< 队列满时阻塞的发送者链表，按线程优先级升序 */
    uint8_t *buf; /**< 数据槽区，cap * item_size 字节，位于控制块之后 */
};

/**
 * @brief 静态创建所需缓冲区中，生命周期标记头占用的字节数
 */
#    define EK_OS_MSGQ_HDR_SIZE (EK_ALIGN_UP(sizeof(uint32_t), alignof(ek_os_msgq_t)))

/**
 * @brief 计算静态创建消息队列所需的最小缓冲区字节数
 * @param item_size 单条消息字节数
 * @param depth     队列深度
 *
 * @note 缓冲区前部预留生命周期标记头，其后依次是控制块、至多 7 字节对齐填充和数据槽区。
 */
#    define EK_OS_MSGQ_STATIC_BUF_SIZE(item_size, depth) \
        (EK_OS_MSGQ_HDR_SIZE + sizeof(ek_os_msgq_t) + 7U + (item_size) * (depth))

/**
 * @brief 动态创建消息队列
 * @param item_size 单条消息字节数（须大于 0）
 * @param depth     队列深度，即最多缓存的未取走消息条数（须大于 0）
 * @return 成功返回消息队列句柄，失败返回 NULL
 *
 * @note 控制块与数据槽区由内核堆一次性分配，使用完毕后需调用 ek_os_msgq_destroy() 释放。
 * @note 不允许在中断上下文调用。
 */
ek_os_msgq_t *ek_os_msgq_create(size_t item_size, size_t depth);

/**
 * @brief 在调用者提供的缓冲区上创建消息队列
 * @param item_size 单条消息字节数（须大于 0）
 * @param depth     队列深度（须大于 0）
 * @param buf       缓冲区起始地址，必须按 alignof(ek_os_msgq_t) 对齐
 * @param size      缓冲区字节数，不得小于 EK_OS_MSGQ_STATIC_BUF_SIZE(item_size, depth)
 * @return 成功返回消息队列句柄，失败返回 NULL
 *
 * @note 缓冲区由调用者持有；destroy 只清除生命周期标记，不会释放缓冲区。
 * @note 不允许在中断上下文调用。
 */
ek_os_msgq_t *ek_os_msgq_create_static(size_t item_size, size_t depth, void *buf, size_t size);

/**
 * @brief 销毁消息队列
 * @param msgq 消息队列句柄
 *
 * @note 会唤醒所有阻塞的发送者与接收者，并让它们得到 -ECANCELED。
 * @note 动态对象在此释放控制块与槽区内存；静态对象只清除生命周期标记。
 * @note 不允许在中断上下文调用；调用前应确保没有其它上下文仍在操作该队列。
 */
void ek_os_msgq_destroy(ek_os_msgq_t *msgq);

/**
 * @brief 向队列发送一条消息
 * @param msgq    消息队列句柄
 * @param item    待发送消息的地址，长度由创建时的 item_size 决定
 * @param timeout 队列满时的等待时长（单位为内核时间基计数，推荐通过 EK_OS_MSEC()/EK_OS_SEC() 构造）；传 EK_OS_FOREVER 表示永久等待，传 0 表示不等待
 * @return 0 成功
 * @retval -ENOENT    句柄无效或已销毁
 * @retval -EINVAL    item 为空
 * @retval -EPERM     在内核未启动或空闲线程上下文调用
 * @retval -EAGAIN    队列已满且不等待（timeout == 0 或中断上下文调用）
 * @retval -ETIMEDOUT 等待期间队列始终没有空位
 * @retval -EINTR     等待期间被挂起打断
 * @retval -ECANCELED 等待期间队列被销毁
 *
 * @note 允许在中断上下文调用：此时退化为非阻塞发送，队列满返回 -EAGAIN。
 * @note 消息按值拷贝进队列，函数返回后调用者可立即复用或释放 item 指向的内存。
 */
int ek_os_msgq_send(ek_os_msgq_t *msgq, const void *item, int64_t timeout);

/**
 * @brief 从队列接收一条消息
 * @param msgq    消息队列句柄
 * @param item    接收缓冲区地址，长度由创建时的 item_size 决定
 * @param timeout 队列空时的等待时长（单位为内核时间基计数，推荐通过 EK_OS_MSEC()/EK_OS_SEC() 构造）；传 EK_OS_FOREVER 表示永久等待，传 0 表示不等待
 * @return 0 成功
 * @retval -ENOENT    句柄无效或已销毁
 * @retval -EINVAL    item 为空
 * @retval -EPERM     在中断上下文、空闲线程或内核未启动时调用
 * @retval -EAGAIN    队列为空且不等待（timeout == 0）
 * @retval -ETIMEDOUT 等待期间队列始终没有消息
 * @retval -EINTR     等待期间被挂起打断
 * @retval -ECANCELED 等待期间队列被销毁
 *
 * @note 不允许在中断上下文调用。
 * @note 严格按先进先出顺序取走消息。
 */
int ek_os_msgq_recv(ek_os_msgq_t *msgq, void *item, int64_t timeout);

/**
 * @brief 查询队列中当前缓存的消息条数
 * @param msgq 消息队列句柄
 * @return 当前消息条数（0..cap）；句柄无效或已销毁返回 -ENOENT
 */
int ek_os_msgq_count(ek_os_msgq_t *msgq);

#    if EKCFG_STATIC_ALLOC == 1
#        include "ek/mem/static_alloc.h"

/**
 * @brief 静态声明一个消息队列并自动注册初始化
 * @param handle    消息队列句柄变量名（static ek_os_msgq_t *）
 * @param item_size 单条消息字节数（须为常量表达式）
 * @param depth     队列深度（须为常量表达式）
 *
 * @note 缓冲区按 alignof(ek_os_msgq_t) 对齐，大小为 EK_OS_MSGQ_STATIC_BUF_SIZE(item_size, depth)。
 * @note 必须在 .c 文件作用域使用，且同一翻译单元内重复使用 handle 会重定义。
 * @note 句柄在 ek_static_alloc_init() 执行前为 NULL，不可提前使用。
 */
#        define EK_DEFINE_OS_MSGQ(handle, item_size, depth)                                    \
            static union                                                                       \
            {                                                                                  \
                ek_os_msgq_t msgq; /* 抬高 union 的对齐到 alignof(ek_os_msgq_t) */             \
                uint8_t storage[EK_OS_MSGQ_STATIC_BUF_SIZE(item_size, depth)];                 \
            } handle##_mem;                                                                    \
            static ek_os_msgq_t *handle;                                                       \
            static int _static_alloc_init_##handle(void)                                       \
            {                                                                                  \
                handle = ek_os_msgq_create_static(                                             \
                    (item_size), (depth), handle##_mem.storage, sizeof(handle##_mem.storage)); \
                return handle ? 0 : -ENOMEM;                                                   \
            }                                                                                  \
            EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_OS_MSGQ, _static_alloc_init_##handle)
#    endif /* EKCFG_STATIC_ALLOC == 1 */

#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_RTOS == 1 */

#endif // EK_OS_MSGQ_H
