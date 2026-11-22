/**
 * @file kfifo.h
 * @brief RTOS 侵入式零拷贝 FIFO 队列接口
 * @author N1netyNine99
 *
 * 提供基于侵入式单链表的先进先出（FIFO）队列：
 * 入队与出队均为 O(1) 零拷贝传递，不预分配节点、不限制元素大小或条数，
 * 适合在 ISR 与线程之间传递大块数据帧或对象池缓冲区的所有权。
 *
 * @note 传入队列的每个数据块首字段必须预留为 ek_os_kfifo_node_t（且起始地址按
 *       alignof(ek_os_kfifo_node_t) 对齐），在经 ek_os_kfifo_get() 取出前不可修改该节点，
 *       也不得将同一节点重复入队。
 * @note 队列本身不拥有数据块内存；destroy 仅唤醒阻塞等待者并返回 -ECANCELED，
 *       队列中残留的数据块需由调用者负责排空或回收。
 * @note 静态创建的对象在 destroy 后仍占用调用者缓冲区，内核通过内部生命周期标记
 *       拒绝后续访问，避免 use-after-free。
 */

#ifndef EK_OS_KFIFO_H
#define EK_OS_KFIFO_H

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
 * @brief 侵入式 FIFO 链表节点（必须嵌入用户数据结构的首字段）
 */
typedef struct ek_os_kfifo_node ek_os_kfifo_node_t;

/**
 * @brief FIFO 队列对象
 */
typedef struct ek_os_kfifo ek_os_kfifo_t;

/**
 * @brief 侵入式 FIFO 单链表节点结构
 */
struct ek_os_kfifo_node
{
    struct ek_os_kfifo_node *next; /**< 指向下一个入队元素，由内核在入队/出队时维护 */
};

/**
 * @brief FIFO 队列控制块
 */
struct ek_os_kfifo
{
    bool mem_owned; /**< true 表示控制块由内核堆分配，destroy 时释放；false 表示位于调用者缓冲区 */
    ek_os_kfifo_node_t *head; /**< 队头指针（下一个出队元素） */
    ek_os_kfifo_node_t *tail; /**< 队尾指针（用于 O(1) 尾插） */
    ek_list_node_t wait_list; /**< 队列空时阻塞的接收者链表，按线程有效优先级升序 */
};

/**
 * @brief 静态创建 FIFO 队列所需的最小缓冲区字节数
 *
 * 缓冲区前部会预留一个按 alignof(ek_os_kfifo_t) 对齐的生命周期标记头，
 * 剩余部分容纳控制块。缓冲区起始地址必须按 alignof(ek_os_kfifo_t) 对齐。
 */
#    define EK_OS_KFIFO_STATIC_BUF_SIZE (sizeof(ek_os_kfifo_t) + EK_ALIGN_UP(sizeof(uint32_t), alignof(ek_os_kfifo_t)))

/**
 * @brief 动态创建侵入式 FIFO 队列
 * @return 成功返回队列句柄，失败返回 NULL
 *
 * @note 控制块由内核堆分配，使用完毕后需调用 ek_os_kfifo_destroy() 释放。
 * @note 不允许在中断上下文调用。
 */
ek_os_kfifo_t *ek_os_kfifo_create(void);

/**
 * @brief 在调用者提供的缓冲区上创建侵入式 FIFO 队列
 * @param buf  缓冲区起始地址，必须按 alignof(ek_os_kfifo_t) 对齐
 * @param size 缓冲区字节数，不得小于 EK_OS_KFIFO_STATIC_BUF_SIZE
 * @return 成功返回队列句柄，失败返回 NULL
 *
 * @note 缓冲区由调用者持有；destroy 只清除生命周期标记，不会释放缓冲区。
 * @note 不允许在中断上下文调用。
 */
ek_os_kfifo_t *ek_os_kfifo_create_static(void *buf, size_t size);

/**
 * @brief 销毁侵入式 FIFO 队列
 * @param fifo FIFO 队列句柄
 *
 * @note 会唤醒所有阻塞等待的接收者，并让它们从 ek_os_kfifo_get() 得到 -ECANCELED。
 * @note 动态对象在此释放控制块内存；静态对象只清除生命周期标记。
 * @note 不会释放队列中尚未取走的用户数据块；不允许在中断上下文调用。
 */
void ek_os_kfifo_destroy(ek_os_kfifo_t *fifo);

/**
 * @brief 将数据块零拷贝入队到 FIFO 尾部
 * @param fifo FIFO 队列句柄
 * @param data 待入队数据块地址，首字段必须为 ek_os_kfifo_node_t 且按 alignof(ek_os_kfifo_node_t) 对齐
 * @return 0 成功
 * @retval -ENOENT 句柄无效或已销毁
 * @retval -EINVAL data 为 NULL 或未按 alignof(ek_os_kfifo_node_t) 对齐
 *
 * @note 若已有线程阻塞在 ek_os_kfifo_get()，data 会跳过链表直接交接给最高优先级等待者并立即唤醒它。
 * @note 允许在中断上下文调用；数据块内存必须持续有效直到被消费取出，且禁止重复入队。
 */
int ek_os_kfifo_put(ek_os_kfifo_t *fifo, void *data);

/**
 * @brief 从 FIFO 头部零拷贝取出一个数据块
 * @param fifo    FIFO 队列句柄
 * @param data    输出参数，用于接收取出的数据块地址
 * @param timeout 队列空时的等待时长（单位为内核时间基计数，推荐通过 EK_OS_MSEC()/EK_OS_SEC() 构造）；
 *                传 EK_OS_FOREVER 表示永久等待，传 0 表示非阻塞尝试
 * @return 0 成功
 * @retval -ENOENT    句柄无效或已销毁
 * @retval -EINVAL    data 出参为空指针
 * @retval -EPERM     在中断上下文传入非零 timeout，或在空闲线程上下文调用
 * @retval -EAGAIN    队列为空且 timeout == 0
 * @retval -ETIMEDOUT 等待超时
 * @retval -EINTR     等待期间被挂起打断
 * @retval -ECANCELED 等待期间队列被销毁
 *
 * @note 允许在中断上下文传 timeout == 0 进行非阻塞取出；带超时阻塞只允许在线程上下文调用。
 */
int ek_os_kfifo_get(ek_os_kfifo_t *fifo, void **data, int64_t timeout);

/**
 * @brief 查看 FIFO 队头数据块地址但不将其弹出队列
 * @param fifo FIFO 队列句柄
 * @param data 输出参数，用于接收队头数据块地址
 * @return 0 成功
 * @retval -ENOENT 句柄无效或已销毁
 * @retval -EINVAL data 出参为空指针
 * @retval -EAGAIN 队列为空
 *
 * @note 允许在中断上下文调用；仅在当前队列存在已排队节点时返回，不会阻塞。
 */
int ek_os_kfifo_peek(const ek_os_kfifo_t *fifo, void **data);

/**
 * @brief 查询 FIFO 队列当前是否为空
 * @param fifo FIFO 队列句柄
 * @return true 队列为空或句柄无效，false 队列中至少有一个元素
 *
 * @note 允许在中断上下文调用。
 */
bool ek_os_kfifo_is_empty(const ek_os_kfifo_t *fifo);

#    if EKCFG_STATIC_ALLOC == 1
#        include "ek/mem/static_alloc.h"

/**
 * @brief 静态声明一个侵入式 FIFO 队列并自动注册初始化
 * @param handle FIFO 句柄变量名（static ek_os_kfifo_t *，初始化后指向静态缓冲区内的控制块）
 *
 * @note 缓冲区按 alignof(ek_os_kfifo_t) 对齐，大小为 EK_OS_KFIFO_STATIC_BUF_SIZE。
 * @note 必须在 .c 文件作用域使用，且同一翻译单元内重复使用 handle 会重定义。
 * @note 句柄在 ek_static_alloc_init() 执行前为 NULL，不可提前使用。
 */
#        define EK_DEFINE_OS_KFIFO(handle)                                                              \
            static union                                                                                \
            {                                                                                           \
                ek_os_kfifo_t kfifo; /* 抬高 union 的对齐到 alignof(ek_os_kfifo_t) */                   \
                uint8_t storage[EK_OS_KFIFO_STATIC_BUF_SIZE];                                           \
            } handle##_mem;                                                                             \
            static ek_os_kfifo_t *handle;                                                               \
            static int _static_alloc_init_##handle(void)                                                \
            {                                                                                           \
                handle = ek_os_kfifo_create_static(handle##_mem.storage, sizeof(handle##_mem.storage)); \
                return handle ? 0 : -ENOMEM;                                                            \
            }                                                                                           \
            EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_OS_KFIFO, _static_alloc_init_##handle)
#    endif /* EKCFG_STATIC_ALLOC == 1 */

#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_RTOS == 1 */

#endif // EK_OS_KFIFO_H
