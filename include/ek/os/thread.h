/**
 * @file thread.h
 * @brief RTOS 线程创建与生命周期管理接口
 * @author N1netyNine99
 */

#ifndef EK_OS_THREAD_H
#define EK_OS_THREAD_H

#include "ek/conf_internal.h"

#if EKCFG_RTOS == 1

#    include "ek/core/def.h"

#    ifdef __cplusplus
extern "C"
{
#    endif

/**
 * @brief 线程入口函数类型
 * @param arg 创建线程时传入的用户参数
 * @return 线程返回值指针（通过 ek_os_thread_join 的 retval 出参获取）
 */
typedef void *(*ek_os_thread_fn_t)(void *arg);

/**
 * @brief 线程状态枚举
 */
enum ek_thread_state
{
    EK_OS_THREAD_UNUSED,
    EK_OS_THREAD_READY,
    EK_OS_THREAD_BLOCK,
    EK_OS_THREAD_SUSPEND,
    EK_OS_THREAD_RUNNING,
    EK_OS_THREAD_DEAD,
};

/**
 * @brief 动态创建线程并加入调度（从内核堆分配栈与 TCB）
 * @param name 线程名称
 * @param fn   线程入口函数
 * @param arg  传入入口函数的参数
 * @param prio 线程优先级（数值越低优先级越高）
 * @param size 栈大小（字节，不得小于 EKCFG_OS_THREAD_MIN_STACK）
 * @return 正整数线程 ID（tid），负数错误码失败
 */
int ek_os_thread_create(const char *name, ek_os_thread_fn_t fn, void *arg, uint8_t prio, size_t size);

/**
 * @brief 静态创建线程并加入调度（使用用户提供的栈缓冲区）
 * @param name  线程名称
 * @param fn    线程入口函数
 * @param arg   传入入口函数的参数
 * @param prio  线程优先级（数值越低优先级越高）
 * @param stack 用户提供的内存缓冲区
 * @param size  缓冲区总字节数（包含 TCB 与栈空间）
 * @return 正整数线程 ID（tid），负数错误码失败
 */
int ek_os_thread_create_static(
    const char *name, ek_os_thread_fn_t fn, void *arg, uint8_t prio, void *stack, size_t size);
/**
 * @brief 挂起指定线程
 * @param tid 目标线程 ID
 * @return 0 成功，负数错误码失败
 */
int ek_os_thread_suspend(int tid);

/**
 * @brief 恢复已挂起的线程
 * @param tid 目标线程 ID
 * @return 0 成功，负数错误码失败
 */
int ek_os_thread_resume(int tid);

/**
 * @brief 修改线程的基础优先级（并重新计算有效优先级）
 * @param tid  目标线程 ID
 * @param prio 新优先级（数值越低优先级越高）
 * @return 0 成功，负数错误码失败
 */
int ek_os_thread_set_prio(int tid, uint8_t prio);

/**
 * @brief 查询线程当前的有效优先级
 * @param tid  目标线程 ID，可传 ek_os_kernel_current_tid() 查询自身
 * @param prio 输出当前有效优先级（数值越低优先级越高）
 * @return 0 成功，负数错误码失败（-ESRCH 线程不存在，-EINVAL 出参为空）
 *
 * @note 有效优先级受优先级继承影响：持有互斥量期间可能高于基础优先级，
 *       释锁或锁被销毁后回落到基础优先级。
 */
int ek_os_thread_get_prio(int tid, uint8_t *prio);

/**
 * @brief 等待目标线程结束并回收其栈与 TCB 资源
 * @param tid     目标线程 ID
 * @param timeout 相对超时时长（单位为内核时间基计数，推荐通过 EK_OS_MSEC()/EK_OS_SEC() 构造；传 EK_OS_FOREVER 或 < 0 表示永久等待，0 表示立即试探）
 * @param retval  用于接收子线程返回值的出参指针，不关心可传 NULL
 * @return 0 成功，负数错误码失败（-EDEADLK/-ESRCH/-EAGAIN/-ETIMEDOUT/-EINTR/-EBUSY/-EPERM）
 */
int ek_os_thread_join(int tid, int64_t timeout, void **retval);

/**
 * @brief 主动结束当前线程并唤醒正在 join 它的线程
 * @param retval 传递给 joiner 的返回值指针
 */
__EK_NO_RETURN void ek_os_thread_exit(void *retval);

/**
 * @brief 主动让出 CPU 给同优先级的其它就绪线程
 * @return 0 成功（含无同级竞争者、直接继续运行的情况），负数错误码失败
 * @retval -EPERM 在中断上下文、空闲线程或持有调度器锁时调用
 *
 * @note 只有同优先级就绪队列中还有其它线程时才真正切换，否则立即返回。
 * @note 更高优先级线程的就绪抢占由调度器自动完成，不依赖本接口。
 */
int ek_os_thread_yield(void);

/**
 * @brief 使当前线程休眠指定时长
 * @param timeout 休眠时长（单位为内核时间基计数，推荐通过 EK_OS_MSEC()/EK_OS_SEC() 构造；须 >= 0，传 0 直接返回）
 * @return 0 成功，负数错误码失败（-EINVAL 参数小于 0，-EPERM 在中断上下文、空闲线程或内核未启动时调用）
 */
int ek_os_thread_sleep(int64_t timeout);

#    if EKCFG_STATIC_ALLOC == 1
#        include "ek/mem/static_alloc.h"

/** @brief 静态线程缓冲区中为内核 TCB 预留的字节数 */
#        define EK_OS_THREAD_TCB_SIZE EK_ALIGN_UP(sizeof(void *) * 15U + 24U + (EKCFG_OS_THREAD_NAME_LEN), 8)

/**
 * @brief 静态声明一个线程并自动注册初始化
 * @param handle     线程句柄变量名（static int，保存分配到的 tid；同时作为线程名）
 * @param fn         线程入口函数
 * @param arg        传入入口函数的参数
 * @param prio       线程优先级（数值越低优先级越高）
 * @param stack_size 线程栈大小（字节，不得小于 EKCFG_OS_THREAD_MIN_STACK）
 */
#        define EK_DEFINE_OS_THREAD(handle, fn, arg, prio, stack_size)                                                \
            static int handle = -1;                                                                                   \
            static uint8_t _ek_os_thrd_stk_##handle[EK_ALIGN_UP(stack_size, 8) + EK_OS_THREAD_TCB_SIZE] __EK_ALIGNED( \
                8);                                                                                                   \
            static int _static_alloc_init_##handle(void)                                                              \
            {                                                                                                         \
                int tid = ek_os_thread_create_static(                                                                 \
                    #handle, (fn), (arg), (prio), _ek_os_thrd_stk_##handle, sizeof(_ek_os_thrd_stk_##handle));        \
                if (tid < 0) return tid;                                                                              \
                handle = tid;                                                                                         \
                return 0;                                                                                             \
            }                                                                                                         \
            EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_OS_THREAD, _static_alloc_init_##handle)
#    endif /* EKCFG_STATIC_ALLOC == 1 */

#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_RTOS == 1 */

#endif // EK_OS_THREAD_H
