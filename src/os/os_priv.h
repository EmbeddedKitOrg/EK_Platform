/**
 * @file os_priv.h
 * @brief RTOS 内核内部数据结构与调度原语声明
 * @author N1netyNine99
 */

#ifndef EK_OS_PRIV_H
#define EK_OS_PRIV_H

#include "ek/conf_internal.h"
#include "ek/os/thread.h"
#include "ek/container/list.h"
#include "ek/core/arch.h"

typedef struct os_thread os_thread_t;

struct os_thread
{
    uintptr_t sp;
    void *stack_base;
    size_t stack_size;

    ek_os_thread_fn_t fn;
    void *arg;
    ek_list_node_t state_node;
    ek_list_node_t wait_node;

    int tid;
    int wait_err;
    uint8_t state;
    uint8_t prio;
    bool stack_owned;
    int64_t deadline;
    char name[EKCFG_OS_THREAD_NAME_LEN];
};

extern os_thread_t *g_next_thrd;
extern os_thread_t *g_cur_thrd;
/**
 * @brief 线程入口函数返回后的内核收尾函数
 */
void kernel_thread_exit(void);

/**
 * @brief 为线程分配 TID 槽位并登记到全局线程表
 * @param thrd 线程控制块指针
 * @return 正整数 tid，或 -ENOSPC/-EINVAL
 */
int kernel_alloc_thread(os_thread_t *thrd);

/**
 * @brief 释放指定 TID 占用的线程表槽位
 * @param tid 目标线程 ID
 */
void kernel_free_thread(int tid);

/**
 * @brief 根据 TID 查找对应的线程控制块
 * @param tid 目标线程 ID
 * @return 线程控制块指针，无效或已释放返回 NULL
 */
os_thread_t *kernel_tid_to_thread(int tid);

/**
 * @brief 获取线程控制块对应的 TID
 * @param thrd 线程控制块指针
 * @return tid，指针为空返回 -EINVAL
 */
int kernel_thread_to_tid(os_thread_t *thrd);

/**
 * @brief 将线程置为 READY 并插入对应优先级就绪链表
 * @param thrd 线程控制块指针
 */
void kernel_thread_ready(os_thread_t *thrd);

/**
 * @brief 将线程从就绪链表摘除并标记为 RUNNING
 * @param thrd 线程控制块指针
 */
void kernel_thread_run(os_thread_t *thrd);

/**
 * @brief 挂起线程（从当前队列及等待队列摘除并移入挂起链表）
 * @param thrd 线程控制块指针
 */
void kernel_thread_suspend(os_thread_t *thrd);

/**
 * @brief 恢复处于 SUSPEND 状态的线程为 READY
 * @param thrd 线程控制块指针
 */
void kernel_thread_resume(os_thread_t *thrd);

/**
 * @brief 终止线程（置为 ABORT、释放槽位并在需要时放入 GC 回收队列）
 * @param thrd 线程控制块指针
 */
void kernel_thread_abort(os_thread_t *thrd);

/**
 * @brief 修改线程优先级并同步调整所在就绪链表
 * @param thrd 线程控制块指针
 * @param prio 新优先级（数值越低优先级越高）
 */
void kernel_thread_set_prio(os_thread_t *thrd, uint8_t prio);

/**
 * @brief 将线程置为 BLOCK，若 timeout >= 0 则按到期时间升序加入超时链表
 * @param thrd    线程控制块指针
 * @param timeout 相对超时时长（< 0 表示永久阻塞）
 */
void kernel_thread_block(os_thread_t *thrd, int64_t timeout);

/**
 * @brief 唤醒处于 BLOCK 状态的线程并写入等待结果
 * @param thrd 线程控制块指针
 * @param err  唤醒结果错误码（0 表示成功，负数表示超时或取消等原因）
 */
void kernel_thread_unblock(os_thread_t *thrd, int err);

/**
 * @brief 判断指定线程是否为空闲线程
 * @param thrd 线程控制块指针
 * @return true 是空闲线程，false 不是
 */
bool kernel_is_idle_thread(const os_thread_t *thrd);

/**
 * @brief 触发一次调度判决并在需要时置位上下文切换
 */
void kernel_schedule(void);

/**
 * @brief 将当前线程挂入等待队列并阻塞，唤醒后返回结果
 * @note 调用前必须已持 ek_arch_irq_lock() 临界区；本函数内部负责恢复 irq_flags 并切出
 * @param wait_list 同步对象的等待链表头
 * @param timeout   相对超时时长（< 0 表示永久阻塞）
 * @param irq_flags 调用方保存的中断状态标志
 * @return 0 等待成功被正常唤醒，-ETIMEDOUT 超时，或其它负数错误码
 */
int kernel_wait_on(ek_list_node_t *wait_list, int64_t timeout, uint32_t irq_flags);

/**
 * @brief 从等待队列头部唤醒一个等待中的线程
 * @param wait_list 同步对象的等待链表头
 * @param err       传递给被唤醒线程的结果错误码
 * @return true 成功唤醒了一个线程，false 等待队列为空
 */
bool kernel_wake_one(ek_list_node_t *wait_list, int err);

/**
 * @brief 唤醒等待队列中的全部线程
 * @param wait_list 同步对象的等待链表头
 * @param err       传递给被唤醒线程的结果错误码
 * @return true 至少唤醒了一个线程，false 等待队列为空
 */
bool kernel_wake_all(ek_list_node_t *wait_list, int err);

#endif // EK_OS_PRIV_H
