/**
 * @file os_priv.h
 * @brief RTOS 内核内部数据结构与调度原语声明
 * @author N1netyNine99
 */

#ifndef EK_OS_PRIV_H
#define EK_OS_PRIV_H

#include "ek/conf_internal.h"

#if EKCFG_RTOS == 1

#    include "ek/os/thread.h"
#    include "ek/os/mutex.h"
#    include "ek/container/list.h"
#    include "ek/core/arch.h"

#    ifdef __cplusplus
extern "C"
{
#    endif

typedef struct os_thread os_thread_t;

/**
 * @brief 内核线程控制块（TCB）
 *
 * @note arch.sp 必须是首成员：上下文切换汇编依赖它位于偏移 0，
 *       kernel.c 中用 static_assert 固定该布局。
 */
struct os_thread
{
    struct
    {
        uintptr_t sp; /**< 保存的栈指针，线程切换时由 PendSV 汇编读写 */
    } arch;

    struct
    {
        ek_list_node_t state_node; /**< 挂在就绪/阻塞/挂起队列上的侵入式节点 */
        uint8_t state; /**< 当前状态，取值见 enum ek_thread_state */
        uint8_t prio; /**< 当前有效调度优先级，数值越小优先级越高 */
        uint8_t base_prio; /**< 线程固有基础优先级 */
        uint8_t sched_lock; /**< 调度器锁嵌套层数；> 0 时禁止被更高优先级线程抢占 */
    } sched;

    struct
    {
        ek_list_node_t node; /**< 挂在同步对象等待队列上的侵入式节点 */
        int64_t deadline; /**< 阻塞到期时刻（绝对时间）；EK_OS_FOREVER 表示无限等待 */
        int err; /**< 阻塞唤醒结果：0 成功、-ETIMEDOUT 超时、-EINTR 被打断、-ECANCELED 对象销毁 */
        void *data; /**< 唤醒直达交接槽（供 fifo/queue 等原语直接传递数据指针） */
    } wait;

    struct
    {
        void *waiting_lock; /**< 若阻塞在某把 mutex 上，指向该 mutex（用于链式传播） */
        ek_list_node_t owned_list; /**< 当前持有的所有 mutex 链表 */
    } lock;

    struct
    {
        void *base; /**< 栈内存起始地址；动态栈为 ek_malloc 返回值，供 GC 释放 */
        size_t size; /**< 可用栈字节数（不含 TCB） */
        bool owned; /**< true 表示栈由内核堆分配，销毁时需 ek_free；false 表示外部静态栈 */
    } stack;

    int tid; /**< 线程 ID；销毁后槽位被回收，旧 tid 不再有效 */
    ek_os_thread_fn_t fn; /**< 线程入口函数 */
    void *arg; /**< 传给入口函数的参数 */
    void *retval; /**< 线程退出返回值，供 join 取回 */
    ek_list_node_t join_wait; /**< 等待本线程结束的 joiner 队列头 */
    char name[EKCFG_OS_THREAD_NAME_LEN]; /**< 线程名，以 '\0' 结尾 */
};

extern os_thread_t *g_next_thrd;
extern os_thread_t *g_cur_thrd;
/**
 * @brief 线程入口函数返回后的内核收尾函数
 */
void kernel_thread_exit(void);

/**
 * @brief 为线程分配 TID 槽位并登记到全局线程表
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param thrd 线程控制块指针
 * @return 正整数 tid，或 -ENOSPC/-EINVAL
 */
int kernel_alloc_thread_locked(os_thread_t *thrd);

/**
 * @brief 释放指定 TID 占用的线程表槽位
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param tid 目标线程 ID
 */
void kernel_free_thread_locked(int tid);

/**
 * @brief 根据 TID 查找对应的线程控制块
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param tid 目标线程 ID
 * @return 线程控制块指针，无效或已释放返回 NULL
 */
os_thread_t *kernel_tid_to_thread_locked(int tid);

/**
 * @brief 获取线程控制块对应的 TID
 * @param thrd 线程控制块指针
 * @return tid，指针为空返回 -EINVAL
 */
int kernel_thread_to_tid(os_thread_t *thrd);

/**
 * @brief 将线程置为 READY 并插入对应优先级就绪链表
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param thrd 线程控制块指针
 */
void kernel_thread_ready_locked(os_thread_t *thrd);

/**
 * @brief 将线程从就绪链表摘除并标记为 RUNNING
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param thrd 线程控制块指针
 */
void kernel_thread_run_locked(os_thread_t *thrd);

/**
 * @brief 挂起线程（从当前队列及等待队列摘除并移入挂起链表）
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param thrd 线程控制块指针
 */
void kernel_thread_suspend_locked(os_thread_t *thrd);

/**
 * @brief 恢复处于 SUSPEND 状态的线程为 READY
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param thrd 线程控制块指针
 */
void kernel_thread_resume_locked(os_thread_t *thrd);

/**
 * @brief 终止线程（置为 DEAD、保存返回值、摘出队列并唤醒所有 joiner）
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param thrd   线程控制块指针
 * @param retval 线程退出返回值指针
 */
void kernel_thread_dead_locked(os_thread_t *thrd, void *retval);

/**
 * @brief 修改线程有效调度优先级并同步调整所在就绪链表（供调度与优先级继承使用）
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param thrd 线程控制块指针
 * @param prio 新有效优先级（数值越低优先级越高）
 */
void kernel_thread_set_prio_locked(os_thread_t *thrd, uint8_t prio);

/**
 * @brief 按 base_prio 与当前持有的 mutex 队头重算线程有效优先级，并向 waiting_lock 链式传播
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param thrd 线程控制块指针
 */
void kernel_thread_recalc_prio_locked(os_thread_t *thrd);

/**
 * @brief 将线程置为 BLOCK，若 timeout >= 0 则按到期时间升序加入超时链表
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param thrd    线程控制块指针
 * @param timeout 相对超时时长（< 0 表示永久阻塞）
 */
void kernel_thread_block_locked(os_thread_t *thrd, int64_t timeout);

/**
 * @brief 唤醒处于 BLOCK 状态的线程并写入等待结果
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param thrd 线程控制块指针
 * @param err  唤醒结果错误码（0 表示成功，负数表示超时或取消等原因）
 */
void kernel_thread_unblock_locked(os_thread_t *thrd, int err);

/**
 * @brief 判断指定线程是否为空闲线程
 * @param thrd 线程控制块指针
 * @return true 是空闲线程，false 不是
 */
bool kernel_is_idle_thread(const os_thread_t *thrd);

/**
 * @brief 触发一次调度判决并在需要时置位上下文切换
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 */
void kernel_schedule_locked(void);

/**
 * @brief 当前线程让出 CPU（仅当同优先级就绪队列存在其它线程时才真正轮转）
 * @note 要求调用时当前线程处于 RUNNING 状态；调用前须已持 ek_os_kernel_spin_lock() 临界区
 */
void kernel_thread_yield_locked(void);

/**
 * @brief 将当前线程按当前有效优先级升序插入指定等待队列
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param wait_list 同步对象的等待链表头
 */
void kernel_wait_enqueue_locked(ek_list_node_t *wait_list);

/**
 * @brief 将当前线程挂入等待队列并阻塞，唤醒后返回结果
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区；本函数内部负责恢复 irq_flags 并切出
 * @param wait_list 同步对象的等待链表头
 * @param timeout   相对超时时长（< 0 表示永久阻塞）
 * @param irq_flags 调用方保存的中断状态标志
 * @return 0 等待成功被正常唤醒，-ETIMEDOUT 超时，或其它负数错误码
 */
int kernel_wait_on_locked(ek_list_node_t *wait_list, int64_t timeout, uint32_t irq_flags);

/**
 * @brief 从等待队列头部唤醒一个等待中的线程
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param wait_list 同步对象的等待链表头
 * @param err       传递给被唤醒线程的结果错误码
 * @return true 成功唤醒了一个线程，false 等待队列为空
 */
bool kernel_wake_one_locked(ek_list_node_t *wait_list, int err);

/**
 * @brief 唤醒等待队列中的全部线程
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区
 * @param wait_list 同步对象的等待链表头
 * @param err       传递给被唤醒线程的结果错误码
 * @return true 至少唤醒了一个线程，false 等待队列为空
 */
bool kernel_wake_all_locked(ek_list_node_t *wait_list, int err);

/**
 * @brief 检查互斥量句柄是否有效
 * @param mutex 互斥量句柄
 * @return true 有效，false 无效或已销毁
 */
bool mutex_valid(const ek_os_mutex_t *mutex);

/**
 * @brief 在已持有内核自旋锁时彻底释放互斥量并移交所有权（不触发调度）
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区，且确认 mutex->owner == g_cur_thrd
 * @param mutex 互斥量句柄
 */
void kernel_mutex_release_locked(ek_os_mutex_t *mutex);

/**
 * @brief 将处于阻塞态的条件变量等待者原地转移到指定互斥量的等待队列（Wait Morphing）
 * @note 调用前必须已持 ek_os_kernel_spin_lock() 临界区，且确认 mutex 有效并被持锁
 * @param thrd  待转移的等待线程
 * @param mutex 目标互斥量句柄
 */
void kernel_wait_morph_to_mutex_locked(os_thread_t *thrd, ek_os_mutex_t *mutex);

#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_RTOS == 1 */

#endif // EK_OS_PRIV_H
