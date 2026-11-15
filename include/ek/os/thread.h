/**
 * @file thread.h
 * @brief RTOS 线程创建与生命周期管理接口
 * @author N1netyNine99
 */

#ifndef EK_OS_THREAD_H
#define EK_OS_THREAD_H

#include "ek/conf_internal.h"

#include "ek/core/def.h"

/**
 * @brief 线程入口函数类型
 * @param arg 创建线程时传入的用户参数
 */
typedef void (*ek_os_thread_fn_t)(void *arg);

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
    EK_OS_THREAD_ABORT,
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
 * @brief 修改线程优先级
 * @param tid  目标线程 ID
 * @param prio 新优先级（数值越低优先级越高）
 * @return 0 成功，负数错误码失败
 */
int ek_os_thread_set_prio(int tid, uint8_t prio);

/**
 * @brief 终止指定线程
 * @param tid 目标线程 ID
 * @return 0 成功（若终止当前线程则不返回），负数错误码失败
 */
int ek_os_thread_abort(int tid);

/**
 * @brief 阻塞当前线程
 * @param timeout 阻塞时间
 * @return 0 成功，负数错误码失败
 */
int ek_os_thread_sleep(int64_t timeout);

#if EKCFG_STATIC_ALLOC == 1
#    include "ek/mem/static_alloc.h"

/** @brief 静态线程缓冲区中为内核 TCB 预留的字节数 */
#    define EK_OS_THREAD_TCB_SIZE EK_ALIGN_UP(sizeof(void *) * 12U + 24U + (EKCFG_OS_THREAD_NAME_LEN), 8)

/**
 * @brief 静态声明一个线程并自动注册初始化
 * @param handle     线程句柄变量名（static int，保存分配到的 tid；同时作为线程名）
 * @param fn         线程入口函数
 * @param arg        传入入口函数的参数
 * @param prio       线程优先级（数值越低优先级越高）
 * @param stack_size 线程栈大小（字节，不得小于 EKCFG_OS_THREAD_MIN_STACK）
 */
#    define EK_DEFINE_OS_THREAD(handle, fn, arg, prio, stack_size)                                                  \
        static int handle = -1;                                                                                     \
        static uint8_t _ek_os_thrd_stk_##handle[_EK_OS_ALIGN8(stack_size) + EK_OS_THREAD_TCB_SIZE] __EK_ALIGNED(8); \
        static int _static_alloc_init_##handle(void)                                                                \
        {                                                                                                           \
            int tid = ek_os_thread_create_static(                                                                   \
                #handle, (fn), (arg), (prio), _ek_os_thrd_stk_##handle, sizeof(_ek_os_thrd_stk_##handle));          \
            if (tid < 0) return tid;                                                                                \
            handle = tid;                                                                                           \
            return 0;                                                                                               \
        }                                                                                                           \
        EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_OS_THREAD, _static_alloc_init_##handle)
#endif /* EKCFG_STATIC_ALLOC == 1 */

#endif // EK_OS_THREAD_H
