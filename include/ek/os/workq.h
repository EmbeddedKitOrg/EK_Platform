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
#    include "ek/core/arch.h"
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
};

/**
 * @brief 工作队列控制块
 */
struct ek_os_workq
{
    ek_list_node_t ready_list; /**< 待立即执行的 work 链表（FIFO） */
    ek_list_node_t delay_list; /**< 延时执行的 delay_work 链表（按到期时刻升序） */
    ek_list_node_t worker_wait; /**< 工作线程空转时的阻塞等待队列 */
    ek_list_node_t sync_wait; /**< 同步等待队列（供 cancel_sync 使用） */
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

/**
 * @brief 初始化普通工作项
 * @param w  工作项句柄
 * @param fn 工作项回调函数
 * @return 0 成功
 * @retval -ENOENT w 为空指针
 * @retval -EINVAL fn 为空指针
 */
int ek_os_work_init(ek_os_work_t *w, ek_os_work_fn_t fn);

/**
 * @brief 初始化延时工作项
 * @param w  延时工作项句柄
 * @param fn 工作项回调函数
 * @return 0 成功
 * @retval -ENOENT w 为空指针
 * @retval -EINVAL fn 为空指针
 */
int ek_os_work_delay_init(ek_os_work_delay_t *w, ek_os_work_fn_t fn);

/**
 * @brief 查询工作项当前状态位掩码
 * @param w 工作项句柄
 * @return 非负状态位掩码（取值见 enum ek_os_work_state_e 组合，空闲为 0）
 * @retval -ENOENT w 为空指针
 *
 * @note 允许在中断上下文调用。
 * @note EK_OS_WORK_RUNNING 由所属队列 cur_work 实时判定，并不写入 w->flag。
 */
int ek_os_work_state(const ek_os_work_t *w);

/**
 * @brief 将普通工作项立即提交到工作队列的就绪链表尾部
 * @param wq 工作队列句柄
 * @param w  工作项句柄（不可处于已排队或同步取消状态）
 * @return 0 成功
 * @retval -ENOENT    wq 或 w 无效/已销毁
 * @retval -EINVAL    w->fn 为空指针
 * @retval -ECANCELED 工作队列正在停机销毁
 * @retval -EPERM     工作线程已退出或处于挂起状态
 * @retval -EBUSY     工作项已处于排队或同步取消中
 *
 * @note 允许在中断上下文调用；支持在回调内部重新提交自身。
 */
int ek_os_workq_submit(ek_os_workq_t *wq, ek_os_work_t *w);

/**
 * @brief 调度延时工作项在指定时长后执行
 * @param wq    工作队列句柄
 * @param w     延时工作项句柄（不可处于已排队或同步取消状态）
 * @param delay 相对延时时长（单位为内核时间基计数，推荐通过 EK_OS_MSEC()/EK_OS_SEC() 构造）；<= 0 时立即提交
 * @return 0 成功
 * @retval -ENOENT    wq 或 w 无效/已销毁
 * @retval -EINVAL    w->work.fn 为空指针
 * @retval -ECANCELED 工作队列正在停机销毁
 * @retval -EPERM     工作线程已退出或处于挂起状态
 * @retval -EBUSY     工作项已处于排队或同步取消中
 *
 * @note 允许在中断上下文调用；delay <= 0 时直接入队 ready_list。
 */
int ek_os_workq_schedule(ek_os_workq_t *wq, ek_os_work_delay_t *w, int64_t delay);

/**
 * @brief 重新调度延时工作项（已处于延时等待状态时先摘除再按新延时重排）
 * @param wq    工作队列句柄
 * @param w     延时工作项句柄
 * @param delay 新的相对延时时长（单位为内核时间基计数，推荐通过 EK_OS_MSEC()/EK_OS_SEC() 构造）；<= 0 时立即提交
 * @return 0 成功
 * @retval -ENOENT    wq 或 w 无效/已销毁
 * @retval -EINVAL    w->work.fn 为空指针，或延时工作项原属于其它工作队列
 * @retval -ECANCELED 工作队列正在停机销毁
 * @retval -EPERM     工作线程已退出或处于挂起状态
 * @retval -EBUSY     工作项已进入就绪队列等待执行，或正在同步取消中
 *
 * @note 允许在中断上下文调用。
 */
int ek_os_workq_reschedule(ek_os_workq_t *wq, ek_os_work_delay_t *w, int64_t delay);

/**
 * @brief 非阻塞取消尚未开始执行的工作项
 * @param w 工作项句柄（延时工作项可传入 &dwork.work）
 * @return 0 成功取消或已脱离队列
 * @retval -ENOENT w 为空或未关联有效工作队列
 * @retval -EBUSY  工作项正在执行回调或正在同步取消中
 *
 * @note 允许在中断上下文调用。
 */
int ek_os_workq_cancel(ek_os_work_t *w);

/**
 * @brief 同步取消工作项（若正在执行回调则阻塞等待其完成）
 * @param w       工作项句柄（延时工作项可传入 &dwork.work）
 * @param timeout 相对超时时长（单位为内核时间基计数，推荐通过 EK_OS_MSEC()/EK_OS_SEC() 构造）；
 *                传 EK_OS_FOREVER 表示永久等待，传 0 表示非阻塞试探
 * @return 0 成功取消或已执行完成
 * @retval -ENOENT    w 为空或未关联有效工作队列
 * @retval -EPERM     在中断上下文或空闲线程调用
 * @retval -EBUSY     已有其它线程正在同步取消该工作项
 * @retval -EDEADLK   在该工作队列自身工作线程内调用
 * @retval -EAGAIN    timeout == 0 且工作项正在执行回调
 * @retval -ETIMEDOUT 等待回调完成超时
 * @retval -EINTR     等待期间被挂起打断
 * @retval -ECANCELED 等待期间工作队列被销毁
 *
 * @note 等待期间会置位 EK_OS_WORK_CANCELING 阻止该工作项被再次提交，退出前自动清除。
 * @note 不允许在中断上下文及所属工作队列自身线程内调用。
 */
int ek_os_workq_cancel_sync(ek_os_work_t *w, int64_t timeout);

/**
 * @brief 排空工作队列中在调用时刻前已就绪的工作项
 * @param wq      工作队列句柄
 * @param timeout 相对超时时长（单位为内核时间基计数，推荐通过 EK_OS_MSEC()/EK_OS_SEC() 构造）；
 *                传 EK_OS_FOREVER 表示永久等待，传 0 表示仅非阻塞探测当前是否为空
 * @return 0 已排空
 * @retval -ENOENT    wq 无效/已销毁
 * @retval -EPERM     在中断上下文、空闲线程，或工作线程已退出/挂起时调用
 * @retval -EDEADLK   在该工作队列自身工作线程内调用
 * @retval -EAGAIN    timeout == 0 且队列当前非空
 * @retval -ENOMEM    临时同步信号量创建失败
 * @retval -ETIMEDOUT 等待排空超时
 * @retval -EINTR     等待期间被挂起打断
 * @retval -ECANCELED 工作队列正在停机或在等待期间被销毁
 *
 * @note 通过向 ready_list 队尾投递栈上栅栏工作项实现，仅等待调用时刻前已入队的任务，
 *       不含未到期的延时任务或调用之后新提交的任务。
 * @note 不允许在中断上下文及该工作队列自身线程内调用。
 */
int ek_os_workq_flush(ek_os_workq_t *wq, int64_t timeout);

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

#    if EKCFG_OS_ISR_WORKQ == 1
/**
 * @brief ISR 工作项上半部回调原型（运行在中断上下文）
 * @return true 投递下半部工作项到内置 ISR 工作队列，false 仅在中断内完成处理、无需投递
 */
typedef bool (*ek_os_work_isr_top_fn_t)(void);

/**
 * @brief 获取内核内置的 ISR 工作队列句柄
 * @return 成功返回内置 ISR 工作队列句柄；调度器尚未启动时返回 NULL
 *
 * @note 由 ek_os_kernel_start() 自动静态创建，线程优先级为 EKCFG_OS_ISR_WORKQ_PRIO。
 * @note 禁止调用 ek_os_workq_destroy() 销毁该队列。
 */
ek_os_workq_t *ek_os_workq_isr(void);

#        if (defined(__arm__) || defined(__thumb__)) && (EKCFG_EXPORT == 1 || EKCFG_STATIC_ALLOC == 1)
#            define _EK_DEFINE_OS_WORKQ_ISR_IMPL2(id, irqn, top_fn, bottom_fn)              \
                static ek_os_work_t _ek_wq_isr_work_##id = {                                \
                    EK_LIST_HEAD_INIT(_ek_wq_isr_work_##id.node),                           \
                    (bottom_fn),                                                            \
                    NULL,                                                                   \
                    0U,                                                                     \
                };                                                                          \
                static void _ek_wq_isr_entry_##id(void)                                     \
                {                                                                           \
                    ek_os_work_isr_top_fn_t top = (top_fn);                                 \
                    if (!top || top())                                                      \
                    {                                                                       \
                        (void)ek_os_workq_submit(ek_os_workq_isr(), &_ek_wq_isr_work_##id); \
                    }                                                                       \
                }                                                                           \
                EK_DEFINE_ARCH_IRQ((irqn), _ek_wq_isr_entry_##id)
#            define _EK_DEFINE_OS_WORKQ_ISR_IMPL(id, irqn, top_fn, bottom_fn) \
                _EK_DEFINE_OS_WORKQ_ISR_IMPL2(id, irqn, top_fn, bottom_fn)

/**
 * @brief 静态声明中断工作项并自动安装硬件中断向量（中断线程化）
 * @param irqn      外部 IRQ 编号，或负的核心异常编号
 * @param top_fn    中断上半部回调（运行在 ISR 中清硬件标志，返回 true 触发下半部；可传 NULL 直接触发）
 * @param bottom_fn 中断下半部回调（在 ek_os_workq_isr() 线程上下文执行）
 *
 * @note 底层通过 EK_DEFINE_ARCH_IRQ() 自动安装中断向量；外设中断优先级与使能仍需在启动调度器后配置。
 * @note 多次中断未及处理时会合并为一次下半部回调，bottom_fn 内应排空当前全部待处理状态。
 * @note 必须在 .c 文件作用域使用，依赖 EKCFG_OS_ISR_WORKQ == 1。
 */
#            define EK_DEFINE_OS_WORKQ_ISR(irqn, top_fn, bottom_fn) \
                _EK_DEFINE_OS_WORKQ_ISR_IMPL(_EK_ARCH_UNIQUE_ID, irqn, top_fn, bottom_fn)
#        endif /* ARM && (EKCFG_EXPORT || EKCFG_STATIC_ALLOC) */
#    endif /* EKCFG_OS_ISR_WORKQ == 1 */

#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_RTOS == 1 */

#endif // EK_OS_WORKQ_H
