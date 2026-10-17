#ifndef EK_PICOTHREAD_H
#define EK_PICOTHREAD_H

#include "ek_conf_internal.h"

#if EKCFG_PICOTHREAD == 1

#    include "ek_err.h"
#    include "ek_list.h"

#    if EKCFG_PICOTHREAD_SEM == 1
/**
 * @brief 信号量结构体前向声明
 */
typedef struct ek_pt_sem ek_pt_sem_t;

/**
 * @brief 信号量句柄
 */
typedef ek_pt_sem_t *ek_pt_sem_handle_t;

/**
 * @brief 信号量结构体
 */
struct ek_pt_sem
{
    uint8_t count; // 可用计数
    ek_list_node_t wait_list; // 等待队列
};
#    endif /* EKCFG_PICOTHREAD_SEM */

#    if EKCFG_PICOTHREAD_MSG == 1
#        include "ek_ringbuf.h"

/**
 * @brief 消息队列结构体前向声明
 */
typedef struct ek_pt_msg ek_pt_msg_t;

/**
 * @brief 消息队列句柄
 */
typedef ek_pt_msg_t *ek_pt_msg_handle_t;

/**
 * @brief 消息队列结构体
 *
 * 内部复用 ek_ringbuf_t 存储消息数据（rb 置尾，其 data[] 即消息数据区），recv_wait/send_wait 负责任务同步。
 */
struct ek_pt_msg
{
    ek_list_node_t recv_wait;
    ek_list_node_t send_wait;
    ek_ringbuf_t rb; /**< 置尾成员：rb.data 即消息数据区（动态=malloc 尾部，静态=union 尾部） */
};
#    endif /* EKCFG_PICOTHREAD_MSG */

/**
 * @brief 协作式微线程（protothread）结构体前向声明
 */
typedef struct ek_pt ek_pt_t;

/**
 * @brief 微线程句柄，指向 ek_pt_t 的指针
 */
typedef ek_pt_t *ek_pt_handle_t;

/**
 * @brief 微线程回调函数类型
 *
 * 每次调度时调用此回调，回调内通过 EK_PT_BEGIN/YEILD/END 宏实现协程式逻辑。
 *
 * @param pt  当前微线程句柄
 * @param arg 创建时传入的用户参数
 */
typedef void (*ek_pt_cb_t)(ek_pt_handle_t pt, void *arg);

/**
 * @brief 微线程状态枚举
 */
typedef enum
{
    EK_PT_STATE_READY = 0, // 就绪，等待调度
    EK_PT_STATE_SUSPEND, // 挂起，不参与调度，需手动 resume
    EK_PT_STATE_BLOCK, // 阻塞，等待定时唤醒
    EK_PT_STATE_RUNNING, // 正在执行回调

    EK_PT_STATE_MAX
} ek_pt_state_t;

/**
 * @brief 微线程结构体
 */
struct ek_pt
{
    uint8_t prio; // 优先级（数值越小优先级越高）
    ek_pt_state_t state; // 当前状态
    uint32_t tick; // 阻塞唤醒的绝对 tick
    uint32_t line; // protothread 行号，用于 YEIELD 恢复执行位置
    ek_list_node_t state_node; // 就绪/阻塞链表的侵入式节点
    ek_list_node_t event_node; // 事件链表的侵入式节点（信号量/消息队列等待）
    void *arg; // 用户参数
    ek_pt_cb_t cb; // 回调函数
#    if EKCFG_PICOTHREAD_SEM || EKCFG_PICOTHREAD_MSG
    ek_err_t wait_result; // 事件等待结果：EK_OK=成功，EK_ERR_TIMEOUT=超时
#    endif
#    if EKCFG_PICOTHREAD_SEM
    ek_pt_sem_t *wait_sem;
#    endif /* EKCFG_PICOTHREAD_SEM */
#    if EKCFG_PICOTHREAD_MSG
    ek_pt_msg_t *wait_msg;
#    endif /* EKCFG_PICOTHREAD_MSG */
};

/**
 * @brief protothread 入口宏，标记协程起始位置
 *
 * 必须在回调函数体最开头使用，与 EK_PT_END 配对。
 * 内部通过 switch-case 跳转实现协程恢复。
 *
 * @param pt  微线程句柄
 *
 * @code
 * static void my_task(ek_pt_handle_t pt, void *arg) {
 *     EK_PT_BEGIN(pt);
 *     // ... 协程逻辑 ...
 *     EK_PT_END(pt);
 * }
 * @endcode
 */
#    define EK_PT_BEGIN(pt) \
        switch ((pt)->line) \
        {                   \
            case 0:

/**
 * @brief protothread 让出执行权宏
 *
 * 保存当前行号后返回，下次调度时从该行号处恢复执行。
 * 调用后任务回到就绪链表，主循环下次调度会再次执行。
 *
 * @warning 此宏仅能在 EK_PT_BEGIN 和 EK_PT_END 之间使用
 *
 * @param pt  微线程句柄
 */
#    define EK_PT_YEILD(pt)        \
        do                         \
        {                          \
            (pt)->line = __LINE__; \
            return;                \
            case __LINE__:         \
        } while (0)

/**
 * @brief protothread 结束宏，标记协程终止位置
 *
 * 重置行号为 0，下次调度时从头执行（重新进入 EK_PT_BEGIN）。
 * 与 EK_PT_BEGIN 配对使用。
 *
 * @param pt  微线程句柄
 */
#    define EK_PT_END(pt) \
        (pt)->line = 0;   \
        }

#    ifdef __cplusplus
extern "C"
{
#    endif /* __cplusplus */

/**
 * @brief 初始化 picothread 调度器
 *
 * 初始化就绪链表和阻塞链表，设置初始化标志。
 * 重复调用安全（幂等）。
 */
void ek_pt_init(void);

/**
 * @brief 创建微线程
 *
 * @param cb   回调函数
 * @param prio 优先级（数值越小优先级越高，范围为 0..EKCFG_PT_PRIO_LOWEST）
 * @param arg  用户参数
 * @return 微线程句柄，失败返回 NULL（通过断言捕获）
 */
ek_pt_handle_t ek_pt_create(ek_pt_cb_t cb, uint8_t prio, void *arg);

/**
 * @brief 销毁微线程
 *
 * @param pt 微线程句柄
 */
void ek_pt_destroy(ek_pt_handle_t pt);

#    if EKCFG_STATIC_ALLOC == 1
#        include "ek_static_alloc.h"

ek_err_t ek_pt_init_static(ek_pt_t *pt, ek_pt_cb_t cb, uint8_t prio, void *arg);
void ek_pt_deinit_static(ek_pt_t *pt);

#        define EK_DEFINE_PT(handle, cb, prio, arg)                       \
            static ek_pt_t handle;                                        \
            static ek_err_t _static_alloc_init_##handle(void)             \
            {                                                             \
                return ek_pt_init_static(&(handle), (cb), (prio), (arg)); \
            }                                                             \
            EK_STATIC_ALLOC_REGISTER(handle, 20, _static_alloc_init_##handle)

#        if EKCFG_PICOTHREAD_SEM == 1
ek_err_t ek_pt_sem_init_static(ek_pt_sem_t *sem, uint8_t count);
void ek_pt_sem_deinit_static(ek_pt_sem_t *sem);

#            define EK_DEFINE_PT_SEM(handle, count)                   \
                static ek_pt_sem_t handle;                            \
                static ek_err_t _static_alloc_init_##handle(void)     \
                {                                                     \
                    return ek_pt_sem_init_static(&(handle), (count)); \
                }                                                     \
                EK_STATIC_ALLOC_REGISTER(handle, 20, _static_alloc_init_##handle)
#        endif /* EKCFG_PICOTHREAD_SEM */

#        if EKCFG_PICOTHREAD_MSG == 1
ek_err_t ek_pt_msg_init_static(ek_pt_msg_t *msg, size_t item_size, uint32_t item_amount);
void ek_pt_msg_deinit_static(ek_pt_msg_t *msg);

#            define EK_DEFINE_PT_MSG(handle, type, amount)                          \
                static union                                                        \
                {                                                                   \
                    ek_pt_msg_t msg;                                                \
                    type align_; /* 把 union 对齐抬到元素对齐 */           \
                    uint8_t storage[sizeof(ek_pt_msg_t) + sizeof(type) * (amount)]; \
                } handle##_mem;                                                     \
                static ek_pt_msg_t *const handle = &handle##_mem.msg;               \
                static ek_err_t _static_alloc_init_##handle(void)                   \
                {                                                                   \
                    return ek_pt_msg_init_static(handle, sizeof(type), (amount));   \
                }                                                                   \
                EK_STATIC_ALLOC_REGISTER(handle, 20, _static_alloc_init_##handle)
#        endif /* EKCFG_PICOTHREAD_MSG */
#    endif /* EKCFG_STATIC_ALLOC */

/**
 * @brief 执行一次调度
 *
 * 将到期阻塞任务移回就绪链表，取出最高优先级就绪任务执行回调，
 * 回调返回后根据状态重新分流。
 *
 * @param now 当前绝对 tick
 * @return == now → 就绪任务存在，不应睡眠；
 *         &gt;  now → 最近阻塞任务的唤醒 tick；
 *         == 0   → 无任务，可无限睡眠
 */
uint32_t ek_pt_schedule(uint32_t now);

/**
 * @brief 阻塞当前或指定任务
 *
 * @param pt    微线程句柄（传入 ek_pt_active() 阻塞当前任务）
 * @param xtick 阻塞时长（相对 tick），0=不等待，~0=永远等待
 *
 * @warning 不应直接调用，请使用 EK_PT_DELAY 宏。
 */
void ek_pt_block(ek_pt_handle_t pt, uint32_t xtick);

/**
 * @brief 挂起当前或指定任务
 *
 * @param pt 微线程句柄（NULL 表示挂起自己）
 * @return 传入 NULL 时返回被挂起的任务句柄，否则返回 NULL
 *
 * @warning 不应直接调用，请使用 EK_PT_SUSPEND 宏。
 */
ek_pt_t *ek_pt_suspend(ek_pt_handle_t pt);

/**
 * @brief 恢复挂起的任务
 *
 * @param pt 微线程句柄
 */
void ek_pt_resume(ek_pt_handle_t pt);

/**
 * @brief 获取当前正在运行的微线程句柄
 *
 * @return 当前微线程句柄，无任务运行时返回 NULL
 */
ek_pt_handle_t ek_pt_active(void);

/**
 * @brief 延迟指定 tick 数后恢复执行
 *
 * 内部组合 ek_pt_block + EK_PT_YEILD，使当前任务阻塞 xtick 后自动唤醒。
 *
 * @param xtick 延迟 tick 数
 */
#    define EK_PT_DELAY(xtick)                      \
        do                                          \
        {                                           \
            ek_pt_block((ek_pt_active()), (xtick)); \
            EK_PT_YEILD((ek_pt_active()));          \
        } while (0)

/**
 * @brief 挂起当前或指定任务，让出执行权
 *
 * 若挂起自身，则后续通过 EK_PT_RESUME 恢复。
 *
 * @param pt 微线程句柄（NULL=挂起自己，非 NULL=挂起指定任务）
 */
#    define EK_PT_SUSPEND(pt)         \
        do                            \
        {                             \
            ek_pt_t *_spt;            \
            _spt = ek_pt_suspend(pt); \
            if (_spt != NULL)         \
            {                         \
                EK_PT_YEILD(_spt);    \
            }                         \
        } while (0)

/**
 * @brief 恢复挂起的任务
 *
 * @param pt 微线程句柄
 */
#    define EK_PT_RESUME(pt) ek_pt_resume(pt)

#    if EKCFG_PICOTHREAD_SEM == 1
/**
 * @brief 创建信号量
 *
 * @param count 初始计数
 * @return 信号量句柄，失败返回 NULL（通过断言捕获）
 */
ek_pt_sem_handle_t ek_pt_sem_create(uint8_t count);

/**
 * @brief 销毁信号量
 *
 * @param sem 信号量句柄
 */
void ek_pt_sem_destroy(ek_pt_sem_handle_t sem);

/**
 * @brief 获取信号量（底层函数）
 *
 * 若计数 > 0 则立即返回 true；否则将当前任务加入等待队列并返回 false。
 *
 * @param sem 信号量句柄
 * @return true=成功获取，false=需等待
 *
 * @warning 不应直接调用，请使用 EK_PT_SEM_TAKE 宏。
 */
bool ek_pt_sem_take(ek_pt_sem_handle_t sem);

/**
 * @brief 释放信号量
 *
 * 若有等待任务则唤醒优先级最高的，否则增加计数。
 *
 * @param sem 信号量句柄
 */
void ek_pt_sem_give(ek_pt_sem_handle_t sem);

/**
 * @brief 获取信号量，带超时支持
 *
 * 若信号量可用则立即返回；否则阻塞当前任务并让出执行权，
 * 由 EK_PT_SEM_GIVE 唤醒或超时自动唤醒。
 * 恢复后通过 err 判断结果：EK_OK=成功，EK_ERR_TIMEOUT=超时。
 *
 * @param sem  信号量句柄
 * @param tick 超时 tick 数
 * @param err  输出参数，接收等待结果
 */
#        define EK_PT_SEM_TAKE(sem, tick, err)                             \
            do                                                             \
            {                                                              \
                while (!ek_pt_sem_take((sem)))                             \
                {                                                          \
                    ek_pt_block(ek_pt_active(), (tick));                   \
                    EK_PT_YEILD(ek_pt_active());                           \
                    if (ek_pt_active()->wait_result != EK_OK) break; \
                }                                                          \
                (err) = ek_pt_active()->wait_result;                       \
            } while (0)

/**
 * @brief 释放信号量
 *
 * @param sem 信号量句柄
 */
#        define EK_PT_SEM_GIVE(sem) ek_pt_sem_give((sem))

#    endif /* EKCFG_PICOTHREAD_SEM */

#    if EKCFG_PICOTHREAD_MSG == 1
/**
 * @brief 创建消息队列
 *
 * @param item_size   单条消息大小（字节）
 * @param item_amount 消息容量
 * @return 消息队列句柄，失败返回 NULL
 */
ek_pt_msg_handle_t ek_pt_msg_create(size_t item_size, uint32_t item_amount);

/**
 * @brief 销毁消息队列
 *
 * @param msg 消息队列句柄
 */
void ek_pt_msg_destroy(ek_pt_msg_handle_t msg);

/**
 * @brief 发送消息到队列（底层函数）
 *
 * 写入成功则返回 true；缓冲区满则将当前任务加入发送等待队列并返回 false。
 *
 * @param msg  消息队列句柄
 * @param data 指向消息数据的指针
 * @return true=发送成功，false=需等待
 *
 * @warning 不应直接调用，请使用 EK_PT_MSG_SEND 宏。
 */
bool ek_pt_msg_send(ek_pt_msg_handle_t msg, const void *data);

/**
 * @brief 从队列接收消息（底层函数）
 *
 * 读取成功则返回 true；缓冲区空则将当前任务加入接收等待队列并返回 false。
 *
 * @param msg  消息队列句柄
 * @param data 接收消息数据的缓冲区指针
 * @return true=接收成功，false=需等待
 *
 * @warning 不应直接调用，请使用 EK_PT_MSG_RECV 宏。
 */
bool ek_pt_msg_recv(ek_pt_msg_handle_t msg, void *data);

/**
 * @brief 发送消息，带超时支持
 *
 * 若队列有空位则立即返回；否则阻塞当前任务并让出执行权，
 * 由 EK_PT_MSG_RECV 消费后唤醒或超时自动唤醒。
 * 恢复后通过 err 判断结果：EK_OK=成功，EK_ERR_TIMEOUT=超时。
 *
 * @param msg  消息队列句柄
 * @param data 指向消息数据的指针
 * @param tick 超时 tick 数
 * @param err  输出参数，接收等待结果
 */
#        define EK_PT_MSG_SEND(msg, data, tick, err)                       \
            do                                                             \
            {                                                              \
                while (!ek_pt_msg_send((msg), (data)))                     \
                {                                                          \
                    ek_pt_block(ek_pt_active(), (tick));                   \
                    EK_PT_YEILD(ek_pt_active());                           \
                    if (ek_pt_active()->wait_result != EK_OK) break; \
                }                                                          \
                (err) = ek_pt_active()->wait_result;                       \
            } while (0)

/**
 * @brief 接收消息，带超时支持
 *
 * 若队列有数据则立即返回；否则阻塞当前任务并让出执行权，
 * 由 EK_PT_MSG_SEND 写入后唤醒或超时自动唤醒。
 * 恢复后通过 err 判断结果：EK_OK=成功，EK_ERR_TIMEOUT=超时。
 *
 * @param msg  消息队列句柄
 * @param data 接收消息数据的缓冲区指针
 * @param tick 超时 tick 数
 * @param err  输出参数，接收等待结果
 */
#        define EK_PT_MSG_RECV(msg, data, tick, err)                       \
            do                                                             \
            {                                                              \
                while (!ek_pt_msg_recv((msg), (data)))                     \
                {                                                          \
                    ek_pt_block(ek_pt_active(), (tick));                   \
                    EK_PT_YEILD(ek_pt_active());                           \
                    if (ek_pt_active()->wait_result != EK_OK) break; \
                }                                                          \
                (err) = ek_pt_active()->wait_result;                       \
            } while (0)

#    endif /* EKCFG_PICOTHREAD_MSG */

#    ifdef __cplusplus
}
#    endif /* __cplusplus */

#endif /* EKCFG_PICOTHREAD */

#endif /* EK_PICOTHREAD_H */
