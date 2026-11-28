/**
 * @file evoke.h
 * @brief 协作式事件驱动任务调度器
 * @author N1netyNine99
 *
 * 提供轻量级的事件驱动机制，用于非 RTOS 环境下的任务调度和事件处理
 * 支持任务等待事件、延迟发布、ISR 请求队列等功能
 *
 * @note 仅在 EKCFG_RTOS == 0 时可用
 * @note 睡眠与定时器回调通过 ek_evoke_ops_t 函数表传入，不再使用弱函数覆盖
 */

#ifndef EK_EVOKE_H
#define EK_EVOKE_H

#include "ek/conf_internal.h"

#if EKCFG_RTOS == 0 && EKCFG_EVOKE == 1

#    include "ek/core/def.h"
#    include "ek/container/list.h"

#    ifdef __cplusplus
extern "C"
{
#    endif

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
 * @brief 处理函数表（前置声明）
 */
typedef struct ek_evoke_ops ek_evoke_ops_t;

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
    EK_EVOKE_STATE_WAITTING, /**< 等待状态 */
    EK_EVOKE_STATE_READY, /**< 就绪状态 */
    EK_EVOKE_STATE_RUNNING, /**< 运行状态 */

    EK_EVOKE_STATE_MAX, /**< 状态最大值 */
} ek_evoke_state_e;

struct ek_evoke_ops
{
    /**
     * @brief 设置定时器
     * @param xtick 距离唤醒的 tick 数（相对当前 tick 基准）
     *
     * @note 到期后需在中断中调用 ek_evoke_delay_timer_callback()
     */
    void (*set_timer)(uint32_t xtick);

    /**
     * @brief 浅睡眠
     *
     * @note 由事件主循环调用，通常使用 WFI 指令
     */
    void (*light_sleep)(void);

    /**
     * @brief 深度睡眠
     *
     * @note 由事件主循环调用，通常进入低功耗模式
     */
    void (*deep_sleep)(void);
};

/**
 * @brief 任务结构体
 */
struct ek_evoke_task
{
    ek_evoke_state_e state; /**< 任务状态 */
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
 * @warning 销毁事件时会将所有等待该事件的任务状态设为 IDLE
 */
void ek_evoke_event_destroy(ek_evoke_event_t *evt);

/**
 * @brief 初始化静态任务对象
 * @param tsk 任务对象指针（由用户分配）
 * @param cb 任务回调函数，不可为 NULL
 * @param arg 用户参数
 * @return 0 成功；-EINVAL 参数无效
 *
 * @note EK_DEFINE_EVOKE_TASK 会自动调用，无需手动调用
 */
int ek_evoke_task_init_static(ek_evoke_task_t *tsk, ek_evoke_cb_t cb, void *arg);

/**
 * @brief 反初始化静态任务对象
 * @param tsk 任务对象指针，NULL 时无操作
 *
 * @note 若任务不在 IDLE 状态，会先从所在链表移除
 */
void ek_evoke_task_deinit_static(ek_evoke_task_t *tsk);

/**
 * @brief 初始化静态事件对象
 * @param evt 事件对象指针（由用户分配）
 * @param init 初始计数值
 * @return 0 成功；-EINVAL 参数无效
 *
 * @note EK_DEFINE_EVOKE_EVENT 会自动调用，无需手动调用
 */
int ek_evoke_event_init_static(ek_evoke_event_t *evt, uint32_t init);

/**
 * @brief 反初始化静态事件对象
 * @param evt 事件对象指针，NULL 时无操作
 *
 * @warning 所有等待任务会被移出等待链表并置为 IDLE（不会被唤醒）
 */
void ek_evoke_event_deinit_static(ek_evoke_event_t *evt);

#    if EKCFG_STATIC_ALLOC == 1
#        include "ek/mem/static_alloc.h"

/**
 * @brief 静态声明一个 evoke 任务对象并自动注册初始化
 * @param handle 任务句柄变量名（static ek_evoke_task_t）
 * @param cb     任务回调函数（不可为 NULL）
 * @param arg    传递给回调的用户参数
 *
 * @note 必须在 .c 文件作用域使用，且同一翻译单元内重复使用 handle 会重定义
 * @note 在 ek_static_alloc_init() 执行前任务未初始化，不可提前使用
 */
#        define EK_DEFINE_EVOKE_TASK(handle, cb, arg)                     \
            static ek_evoke_task_t handle;                                \
            static int _static_alloc_init_##handle(void)                  \
            {                                                             \
                return ek_evoke_task_init_static(&(handle), (cb), (arg)); \
            }                                                             \
            EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_EVOKE, _static_alloc_init_##handle)

/**
 * @brief 静态声明一个 evoke 事件对象并自动注册初始化
 * @param handle     事件句柄变量名（static ek_evoke_event_t）
 * @param init_count 事件初始计数值
 *
 * @note 必须在 .c 文件作用域使用，且同一翻译单元内重复使用 handle 会重定义
 * @note 在 ek_static_alloc_init() 执行前事件未初始化，不可提前使用
 */
#        define EK_DEFINE_EVOKE_EVENT(handle, init_count)                   \
            static ek_evoke_event_t handle;                                 \
            static int _static_alloc_init_##handle(void)                    \
            {                                                               \
                return ek_evoke_event_init_static(&(handle), (init_count)); \
            }                                                               \
            EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_EVOKE, _static_alloc_init_##handle)
#    endif /* EKCFG_STATIC_ALLOC */

/* ========== 事件订阅/分发 ========== */

/**
 * @brief 任务订阅事件
 * @param tsk 任务句柄
 * @param evt 事件句柄
 * @retval true 任务立即就绪（事件已有计数且无其他等待任务）
 * @retval false 任务进入等待状态，或参数无效（tsk/evt 为 NULL）
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
 * @return 0 成功；负数 errno 失败
 * @retval -EINVAL evt 为 NULL
 * @retval -ENOSPC ISR 请求队列已满
 * @note 请求会被放入 ISR 请求队列，在主循环中处理
 */
int ek_evoke_event_broadcast_from_isr(ek_evoke_event_t *evt, void *payload);

/**
 * @brief ISR 中发布事件
 * @param evt 事件句柄
 * @param payload 事件携带的数据
 * @return 0 成功；负数 errno 失败
 * @retval -EINVAL evt 为 NULL
 * @retval -ENOSPC ISR 请求队列已满
 * @note 请求会被放入 ISR 请求队列，在主循环中处理
 */
int ek_evoke_event_publish_from_isr(ek_evoke_event_t *evt, void *payload);

/**
 * @brief ISR 中延迟发布事件
 * @param evt 事件句柄
 * @param payload 事件携带的数据
 * @param delay 延迟时间（tick）
 * @param broadcast true 广播模式，false 发布模式
 * @return 0 成功；负数 errno 失败
 * @retval -EINVAL evt 为 NULL
 * @retval -ENOSPC ISR 请求队列已满
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
 * @param ops 平台回调表，三个函数指针都必须非空
 * @return 参数无效返回 -EINVAL；正常情况下不会返回
 * @note 应在 main() 中调用
 */
int ek_evoke_event_loop(ek_evoke_ops_t *ops);

/* ========== 回调函数 ========== */

/**
 * @brief 延迟定时器到期回调
 *
 * @note 在 set_timer() 设定的定时器到期中断中调用，用于唤醒延迟事件
 */
void ek_evoke_delay_timer_callback(void);

#    ifdef __cplusplus
}
#    endif

#endif // EKCFG_RTOS == 0 && EKCFG_EVOKE == 1

#endif // EK_EVOKE_H
