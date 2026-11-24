/**
 * @file workq.h
 * @brief RTOS 工作队列（Work Queue）接口
 * @author N1netyNine99
 *
 * 提供基于单工作线程的异步任务队列与延时执行机制：
 * 多个工作项（ek_os_work_t / ek_os_delay_work_t）可共享同一工作队列线程的栈空间，
 * 由工作线程按序串行调用回调函数。
 *
 * @note 静态创建的对象在 destroy 后仍占用调用者缓冲区，内核通过内部生命周期标记
 *       拒绝后续访问，避免 use-after-free。
 */

#ifndef EK_OS_WORKQ_H
#define EK_OS_WORKQ_H

#include "ek/conf_internal.h"

#if EKCFG_RTOS == 1

#    include "ek/core/def.h"
#    include "ek/container/list.h"
#    include "ek/os/kernel.h"
#    include "ek/os/thread.h"

#    ifdef __cplusplus
extern "C"
{
#    endif

/**
 * @brief 工作队列对象
 */
typedef struct ek_os_workq ek_os_workq_t;

/**
 * @brief 工作项对象
 */
typedef struct ek_os_work ek_os_work_t;

/**
 * @brief 延时工作项对象
 */
typedef struct ek_os_work_delay ek_os_work_delay_t;

/**
 * @brief 工作项回调函数原型
 * @param work 正在执行的 work 节点指针（可用 EK_CONTAINER_OF 找回宿主结构）
 */
typedef void (*ek_os_work_fn_t)(ek_os_work_t *work);

/**
 * @brief 工作项状态位掩码
 */
enum ek_os_work_state_e
{
    EK_OS_WORK_PENDING = (1U << 0), /**< 已提交并挂在 ready_list 等待执行 */
    EK_OS_WORK_DELAYED = (1U << 1), /**< 已调度并挂在 delay_list 等待到期 */
    EK_OS_WORK_RUNNING = (1U << 2), /**< 正在被工作线程执行回调 */
    EK_OS_WORK_CANCELING = (1U << 3), /**< 正在同步取消中，禁止再次入队 */
    EK_OS_WORK_BOOSTED = (1U << 4), /**< 本次提交触发了工作队列提权 */
};

/**
 * @brief 工作队列控制块
 */
struct ek_os_workq
{
    ek_list_node_t ready_list; /**< 待立即执行的 work 链表（FIFO） */
    ek_list_node_t delay_list; /**< 延时执行的 delay_work 链表（按到期时刻升序） */
    ek_list_node_t worker_wait; /**< 工作线程空转时的阻塞等待队列 */
    ek_list_node_t sync_wait; /**< 同步等待队列（供 cancel_sync / flush 使用） */
    ek_os_work_t *cur_work; /**< 当前正在执行的 work，空闲时为 NULL */
    int worker_tid; /**< 后端工作线程 TID */
    bool mem_owned; /**< true 表示控制块与线程栈由内核堆分配；false 表示位于调用者静态缓冲区 */
    bool stop; /**< 停机标志，置位后拒绝新提交并通知工作线程退出 */
};

/**
 * @brief 普通工作项节点
 */
struct ek_os_work
{
    ek_list_node_t node; /**< 侵入式队列节点 */
    ek_os_work_fn_t fn; /**< 工作项回调函数 */
    ek_os_workq_t *q; /**< 当前所属工作队列 */
    uint8_t flag; /**< 状态位掩码，取值见 enum ek_os_work_state_e */
};

/**
 * @brief 延时工作项节点
 */
struct ek_os_work_delay
{
    struct ek_os_work work; /**< 内嵌普通工作项（必须为首成员） */
    int64_t deadline; /**< 绝对到期时刻（内核微秒时基） */
};

/** @brief 静态缓冲区头部生命周期标记字节数（含对齐填充） */
#    define _EK_OS_WORKQ_STATIC_HDR_SIZE EK_ALIGN_UP(sizeof(uint32_t), alignof(ek_os_workq_t))

/** @brief 静态缓冲区中队列控制块（含隐藏头）占用的 8 字节对齐空间 */
#    define _EK_OS_WORKQ_STATIC_CTRL_SIZE EK_ALIGN_UP(_EK_OS_WORKQ_STATIC_HDR_SIZE + sizeof(ek_os_workq_t), 8)

/** @brief 静态创建工作队列缓冲区所需的对齐要求（字节） */
#    define EK_OS_WORKQ_STATIC_ALIGN (EK_MAX(alignof(ek_os_workq_t), 8U))

/**
 * @brief 静态创建工作队列所需的最小缓冲区字节数
 * @param stack_size 工作线程栈大小（字节，不得小于 EKCFG_OS_THREAD_MIN_STACK）
 *
 * 缓冲区前部预留隐藏生命周期标记头与工作队列控制块，后部容纳工作线程栈与 TCB。
 * 缓冲区起始地址必须按 EK_OS_WORKQ_STATIC_ALIGN 对齐。
 */
#    define EK_OS_WORKQ_STATIC_BUF_SIZE(stack_size) \
        (_EK_OS_WORKQ_STATIC_CTRL_SIZE + EK_ALIGN_UP((stack_size), 8) + EK_OS_THREAD_TCB_SIZE)

/**
 * @brief 动态创建工作队列及其后台工作线程
 * @param name       工作线程名称
 * @param prio       工作线程基础优先级（数值越小优先级越高）
 * @param stack_size 工作线程栈大小（字节，不得小于 EKCFG_OS_THREAD_MIN_STACK）
 * @return 成功返回工作队列句柄，失败返回 NULL
 *
 * @note 控制块与工作线程栈均由内核堆分配，使用完毕后需调用 ek_os_workq_destroy() 销毁回收。
 * @note 不允许在中断上下文调用。
 */
ek_os_workq_t *ek_os_workq_create(const char *name, uint8_t prio, size_t stack_size);

/**
 * @brief 在调用者提供的缓冲区上静态创建工作队列及其后台工作线程
 * @param buf  缓冲区起始地址，必须按 EK_OS_WORKQ_STATIC_ALIGN 对齐
 * @param size 缓冲区字节数，不得小于 EK_OS_WORKQ_STATIC_BUF_SIZE(EKCFG_OS_THREAD_MIN_STACK)
 * @param name 工作线程名称
 * @param prio 工作线程基础优先级（数值越小优先级越高）
 * @return 成功返回工作队列句柄，失败返回 NULL
 *
 * @note 缓冲区由调用者持有；destroy 会先等待工作线程退出并清除隐藏生命周期标记，不会释放缓冲区。
 * @note 不允许在中断上下文调用。
 */
ek_os_workq_t *ek_os_workq_create_static(void *buf, size_t size, const char *name, uint8_t prio);

/**
 * @brief 停止并销毁工作队列
 * @param q 工作队列句柄
 *
 * @note 设置停机标志并唤醒后台工作线程，阻塞等待工作线程彻底退出并回收线程资源；
 *       动态对象在此释放控制块与线程栈内存；静态对象清除生命周期标记。
 * @note 不允许在中断上下文及该工作队列自身线程内调用。
 */
void ek_os_workq_destroy(ek_os_workq_t *q);

#    if EKCFG_STATIC_ALLOC == 1
#        include "ek/mem/static_alloc.h"

/**
 * @brief 静态声明一个工作队列并自动注册初始化
 * @param handle     工作队列句柄变量名（static ek_os_workq_t *，初始化后指向静态缓冲区内的控制块）
 * @param prio       工作线程基础优先级（数值越小优先级越高）
 * @param stack_size 工作线程栈大小（字节，不得小于 EKCFG_OS_THREAD_MIN_STACK）
 *
 * @note 必须在 .c 文件作用域使用，且同一翻译单元内重复使用 handle 会重定义。
 * @note 句柄在 ek_static_alloc_init() 执行前为 NULL，不可提前使用。
 */
#        define EK_DEFINE_OS_WORKQ(handle, prio, stack_size)                                                        \
            static union                                                                                            \
            {                                                                                                       \
                ek_os_workq_t workq;                                                                                \
                uint64_t align; /* 确保同时满足 alignof(ek_os_workq_t) 与 8 字节栈对齐 */                           \
                uint8_t storage[EK_OS_WORKQ_STATIC_BUF_SIZE(stack_size)];                                           \
            } handle##_mem;                                                                                         \
            static ek_os_workq_t *handle;                                                                           \
            static int _static_alloc_init_##handle(void)                                                            \
            {                                                                                                       \
                handle =                                                                                            \
                    ek_os_workq_create_static(handle##_mem.storage, sizeof(handle##_mem.storage), #handle, (prio)); \
                return handle ? 0 : -ENOMEM;                                                                        \
            }                                                                                                       \
            EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_OS_WORKQ, _static_alloc_init_##handle)
#    endif /* EKCFG_STATIC_ALLOC == 1 */

#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_RTOS == 1 */

#endif // EK_OS_WORKQ_H
