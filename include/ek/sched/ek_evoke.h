/**
 * @file ek_evoke.h
 * @brief 协作式事件驱动任务调度器
 * @author N1netyNine99
 *
 * 提供轻量级的事件驱动机制，用于非 RTOS 环境下的任务调度和事件处理
 * 支持任务等待事件、延迟发布、ISR 请求队列等功能
 *
 * @note 仅在 EKCFG_RTOS == 0 时可用
 * @note 需要用户实现睡眠和定时器回调的弱函数
 */

#ifndef EK_EVOKE_H
#define EK_EVOKE_H

#include "ek/ek_conf_internal.h"

#if EKCFG_RTOS == 0 && EKCFG_EVOKE == 1

#    include "ek/core/ek_def.h"
#    include "ek/container/ek_list.h"

/**
 * @brief ISR 请求队列最大容量
 */
#    ifndef EK_EVOKE_MAX_ISR_REQ
#        define EK_EVOKE_MAX_ISR_REQ (10)
#    endif /* EK_EVOKE_MAX_ISR_REQ */

/**
 * @brief 延迟请求池最大容量
 */
#    ifndef EK_EVOKE_MAX_DEFER_REQ
#        define EK_EVOKE_MAX_DEFER_REQ (10)
#    endif /* EK_EVOKE_MAX_DEFER_REQ */

/**
 * @brief 任务结构体（前置声明）
 */
typedef struct ek_evoke_task ek_evoke_task_t;

/**
 * @brief 事件结构体（前置声明）
 */
typedef struct ek_evoke_event ek_evoke_event_t;

/**
 * @brief 任务回调函数类型
 * @param evt 触发的事件
 * @param arg 用户传入的参数
 */
typedef void (*ek_evoke_cb_t)(ek_evoke_event_t *, void *);

/**
 * @brief 任务状态枚举
 */
typedef enum
{
    EK_EVOKE_STATE_IDLE = 0, /**< 空闲状态 */
    EK_EVOKE_STATE_DELAY, /**< 延迟状态 */
    EK_EVOKE_STATE_WAITING, /**< 等待状态 */
    EK_EVOKE_STATE_READY, /**< 就绪状态 */
    EK_EVOKE_STATE_RUNNING, /**< 运行状态 */

    EK_EVOKE_STATE_MAX, /**< 状态最大值 */
} ek_evoke_state_t;

/** @deprecated 拼写错误的旧名，请改用 EK_EVOKE_STATE_WAITING */
#    define EK_EVOKE_STATE_WAITTING EK_EVOKE_STATE_WAITING

/**
 * @brief 任务结构体
 */
struct ek_evoke_task
{
    ek_evoke_state_t state; /**< 任务状态 */
    ek_list_node_t node; /**< 链表节点 */
    ek_evoke_event_t *wait_event; /**< 等待的事件 */
    void *arg; /**< 用户参数 */
    ek_evoke_cb_t cb; /**< 回调函数 */
};

/**
 * @brief 事件结构体
 */
struct ek_evoke_event
{
    ek_list_node_t wait_list; /**< 等待该事件的任务链表 */
    uint32_t count; /**< 事件计数（用于信号量机制） */
    void *data; /**< 事件携带的数据 */
};

/* ========== 初始化 ========== */

/**
 * @brief 初始化事件调度器
 *
 * @note 使用前必须调用此函数进行初始化
 */
void ek_evoke_init(void);

/* ========== 任务/事件 创建/销毁 ========== */

/**
 * @brief 创建任务
 * @param cb 任务回调函数
 * @param arg 用户参数
 * @return 任务句柄
 */
ek_evoke_task_t *ek_evoke_task_create(ek_evoke_cb_t cb, void *arg);

/**
 * @brief 销毁任务
 * @param tsk 任务句柄
 * @note 可以在任务自己的回调中调用，释放会推迟到回调返回之后
 */
void ek_evoke_task_destroy(ek_evoke_task_t *tsk);

/**
 * @brief 创建事件
 * @param init 初始计数值
 * @return 事件句柄
 */
ek_evoke_event_t *ek_evoke_event_create(uint32_t init);

/**
 * @brief 销毁事件
 * @param evt 事件句柄
 *
 * @warning 销毁事件时会将所有等待/就绪于该事件的任务状态设为 IDLE，
 *          并丢弃尚未到期的、指向该事件的延时请求。
 *          已进入 ISR 请求队列但尚未处理的请求无法撤回，销毁前请确保中断不会再投递该事件。
 */
void ek_evoke_event_destroy(ek_evoke_event_t *evt);

int ek_evoke_task_init_static(ek_evoke_task_t *tsk, ek_evoke_cb_t cb, void *arg);
void ek_evoke_task_deinit_static(ek_evoke_task_t *tsk);
int ek_evoke_event_init_static(ek_evoke_event_t *evt, uint32_t init);
void ek_evoke_event_deinit_static(ek_evoke_event_t *evt);

#    if EKCFG_STATIC_ALLOC == 1
#        include "ek/mem/ek_static_alloc.h"

#        define EK_DEFINE_EVOKE_TASK(handle, cb, arg)                     \
            static ek_evoke_task_t handle;                                \
            static int _static_alloc_init_##handle(void)                  \
            {                                                             \
                return ek_evoke_task_init_static(&(handle), (cb), (arg)); \
            }                                                             \
            EK_STATIC_ALLOC_REGISTER(handle, 20, _static_alloc_init_##handle)

#        define EK_DEFINE_EVOKE_EVENT(handle, init_count)                   \
            static ek_evoke_event_t handle;                                 \
            static int _static_alloc_init_##handle(void)                    \
            {                                                               \
                return ek_evoke_event_init_static(&(handle), (init_count)); \
            }                                                               \
            EK_STATIC_ALLOC_REGISTER(handle, 20, _static_alloc_init_##handle)
#    endif /* EKCFG_STATIC_ALLOC */

/* ========== 事件订阅/分发 ========== */

/**
 * @brief 任务订阅事件
 * @param tsk 任务句柄
 * @param evt 事件句柄
 * @return true 任务立即就绪（事件已有计数）
 * @return false 任务进入等待状态
 */
bool ek_evoke_event_subscribe(ek_evoke_task_t *tsk, ek_evoke_event_t *evt);

/**
 * @brief 广播事件（唤醒所有等待任务）
 * @param evt 事件句柄
 * @param payload 事件携带的数据
 *
 * @note 如果没有任务等待，则增加事件计数
 */
void ek_evoke_event_broadcast(ek_evoke_event_t *evt, void *payload);

/**
 * @brief 发布事件（唤醒单个等待任务）
 * @param evt 事件句柄
 * @param payload 事件携带的数据
 *
 * @note 如果没有任务等待，则增加事件计数
 */
void ek_evoke_event_publish(ek_evoke_event_t *evt, void *payload);

/**
 * @brief 延迟发布事件
 * @param evt 事件句柄
 * @param payload 事件携带的数据
 * @param delay 延迟时间（tick）
 * @param broadcast true 广播模式，false 发布模式
 */
void ek_evoke_event_defer(ek_evoke_event_t *evt, void *payload, uint32_t delay, bool broadcast);

/**
 * @brief ISR 中广播事件
 * @param evt 事件句柄
 * @param payload 事件携带的数据
 * @return 执行情况
 * @note 请求会被放入 ISR 请求队列，在主循环中处理
 */
int ek_evoke_event_broadcast_from_isr(ek_evoke_event_t *evt, void *payload);

/**
 * @brief ISR 中发布事件
 * @param evt 事件句柄
 * @param payload 事件携带的数据
 * @return 执行情况
 * @note 请求会被放入 ISR 请求队列，在主循环中处理
 */
int ek_evoke_event_publish_from_isr(ek_evoke_event_t *evt, void *payload);

/**
 * @brief ISR 中延迟发布事件
 * @param evt 事件句柄
 * @param payload 事件携带的数据
 * @param delay 延迟时间（tick）
 * @param broadcast true 广播模式，false 发布模式
 * @return 执行情况
 * @note 请求会被放入 ISR 请求队列，在主循环中处理
 */
int ek_evoke_event_defer_from_isr(ek_evoke_event_t *evt, void *payload, uint32_t delay, bool broadcast);

/* ========== 睡眠锁 ========== */

/**
 * @brief 睡眠锁（禁止深度睡眠）
 *
 * @note 每次调用增加锁计数，需要与 unlock 配对使用
 */
void ek_evoke_sleep_lock(void);

/**
 * @brief 解锁睡眠
 *
 * @note 每次调用减少锁计数，锁计数为 0 时允许深度睡眠
 */
void ek_evoke_sleep_unlock(void);

/* ========== 事件主循环 ========== */

/**
 * @brief 事件主循环
 *
 * @note 此函数永远不会返回，等价于 for (;;) ek_evoke_run_once();
 */
void ek_evoke_event_loop(void);

/**
 * @brief 执行一轮调度
 *
 * 依次处理 ISR 请求、到期的延时事件、就绪任务；无事可做时装载延时定时器并睡眠。
 * 需要把 evoke 嵌入自有主循环（或在主机上做单元测试）时使用。
 */
void ek_evoke_run_once(void);

/**
 * @brief 获取调度器当前时刻（tick）
 *
 * 时间只由延时定时器推进（见 ek_evoke_get_timer_elapsed）。
 */
uint32_t ek_evoke_get_tick(void);

/* ========== 回调函数 ========== */

/**
 * @brief 延迟定时器回调函数
 *
 * @note 在 ek_evoke_set_timer() 装载的定时器到期中断中调用，用于唤醒延迟事件
 */
void ek_evoke_delay_timer_callback(void);

/* ========== 弱函数（用户实现） ========== */

/**
 * @brief 进入临界区（弱函数）
 *
 * @note 默认实现调用 ek_critical_enter()（Cortex-M 上为保存并置位 PRIMASK，可嵌套）。
 *       自行实现时必须屏蔽所有会调用 *_from_isr / ek_evoke_delay_timer_callback 的中断，
 *       并且不能用 BASEPRI 屏蔽唤醒中断——睡眠函数会在临界区内调用（见下）。
 */
void ek_evoke_enter_critical(void);

/**
 * @brief 退出临界区（弱函数）
 *
 * @note 默认实现调用 ek_critical_exit()
 */
void ek_evoke_exit_critical(void);

/**
 * @brief 装载一次性延时定时器（弱函数）
 * @param xtick 从现在起多少 tick 后触发（总是大于 0）
 *
 * @note 在临界区内调用。重新装载时需同时清除上一次尚未处理的到期中断标志，
 *       否则旧的到期中断会被当成新定时的到期。
 */
void ek_evoke_set_timer(uint32_t xtick);

/**
 * @brief 读取延时定时器自最近一次 ek_evoke_set_timer() 以来走过的 tick（弱函数）
 *
 * @note 在临界区内调用。通常直接读低功耗定时器（如 LPTIM）的计数值。
 *       默认实现返回 0：此时调度器只在定时器到期时推进时间，
 *       在定时器运行途中新增的延时请求会以上一次装载时刻为起点计算，可能提前触发。
 *       需要精确延时时请实现本函数。
 */
uint32_t ek_evoke_get_timer_elapsed(void);

/**
 * @brief 浅睡眠（弱函数）
 *
 * @note 在临界区内调用（中断已被屏蔽）。通常直接执行 WFI：
 *       Cortex-M 上 PRIMASK=1 时挂起的中断仍能唤醒 WFI，返回后由调度器退出临界区，
 *       中断服务函数随即执行。函数内不要自行打开中断。
 */
void ek_evoke_light_sleep(void);

/**
 * @brief 深度睡眠（弱函数）
 *
 * @note 与 ek_evoke_light_sleep 相同，在临界区内调用；
 *       醒来后可以在这里恢复时钟，再返回给调度器开中断。
 */
void ek_evoke_deep_sleep(void);

#endif // EKCFG_RTOS == 0 && EKCFG_EVOKE == 1

#endif // EK_EVOKE_H
