/**
 * @file ek_kfifo.h
 * @brief 侵入式零拷贝 FIFO
 *
 * 元素由用户自己的结构体承载，结构体内嵌一个 ek_kfifo_node_t：
 * - 入队/出队只链接指针，不拷贝数据、不分配内存，O(1)；
 * - 出队后用 ek_kfifo_entry()/EK_CONTAINER_OF 取回外层对象；
 * - 所有操作都在一个很短的临界区内完成，ISR 与主循环/线程之间可以任意组合
 *   读写（多生产者、多消费者均可）；
 * - ek_kfifo_get_all() 一次取走整条链，适合 ISR 批量投递、主循环批量处理。
 *
 * 典型用法：用两个 FIFO 组成零拷贝收发流水线——空闲池 + 就绪队列。
 * @code
 * typedef struct {
 *     ek_kfifo_node_t node;   // 内嵌节点，位置不限
 *     uint16_t len;
 *     uint8_t  buf[256];
 * } frame_t;
 *
 * static frame_t frames[8];
 * EK_DEFINE_KFIFO(free_q);
 * EK_DEFINE_KFIFO(rx_q);
 *
 * void app_init(void) {
 *     EK_KFIFO_FILL(&free_q, frames, node);     // 8 个空帧放进空闲池
 * }
 *
 * void UART_IRQHandler(void) {                 // 生产者：ISR
 *     frame_t *f = ek_kfifo_get_entry(&free_q, frame_t, node);
 *     if (f == NULL) return;                   // 池空，丢帧
 *     f->len = uart_read_dma(f->buf, sizeof(f->buf));
 *     ek_kfifo_put(&rx_q, &f->node);           // 只挂指针，不拷贝 buf
 * }
 *
 * void app_poll(void) {                        // 消费者：主循环
 *     frame_t *f;
 *     while ((f = ek_kfifo_get_entry(&rx_q, frame_t, node)) != NULL) {
 *         handle(f->buf, f->len);
 *         ek_kfifo_put(&free_q, &f->node);     // 处理完归还空闲池
 *     }
 * }
 * @endcode
 *
 * @warning 一个节点同一时刻只能挂在一个 FIFO 中；节点出队之前不要释放或复用其外层对象。
 */

#ifndef EK_KFIFO_H
#define EK_KFIFO_H

#include "ek/ek_conf_internal.h"

#if EKCFG_KFIFO == 1

#    include "ek/core/ek_def.h"
#    include "ek/core/ek_critical.h"

/**
 * @brief 临界区钩子
 *
 * 默认使用 ek_critical_enter()/ek_critical_exit()（关中断）。
 * 所有 FIFO 只在同一个执行上下文中使用时，可以在 ek_conf.h 中定义为空以省掉开关中断：
 * @code
 * #define EK_KFIFO_CRITICAL_ENTER() ((void)0)
 * #define EK_KFIFO_CRITICAL_EXIT()  ((void)0)
 * @endcode
 */
#    ifndef EK_KFIFO_CRITICAL_ENTER
#        define EK_KFIFO_CRITICAL_ENTER() ek_critical_enter()
#    endif
#    ifndef EK_KFIFO_CRITICAL_EXIT
#        define EK_KFIFO_CRITICAL_EXIT() ek_critical_exit()
#    endif

/**
 * @brief FIFO 节点，内嵌到用户结构体中
 */
typedef struct ek_kfifo_node ek_kfifo_node_t;

struct ek_kfifo_node
{
    ek_kfifo_node_t *next; /**< 下一个节点；仅供 FIFO 内部使用 */
};

/**
 * @brief FIFO 控制块
 */
typedef struct
{
    ek_kfifo_node_t *head; /**< 队首（下一个出队的节点） */
    ek_kfifo_node_t *tail; /**< 队尾 */
    volatile uint32_t count; /**< 当前节点数 */
} ek_kfifo_t;

/**
 * @brief 静态初始化值，等价于 ek_kfifo_init()
 */
#    define EK_KFIFO_INITIALIZER { NULL, NULL, 0U }

/**
 * @brief 定义一个已初始化的静态 FIFO（不需要再调用 ek_kfifo_init）
 */
#    define EK_DEFINE_KFIFO(handle) static ek_kfifo_t handle = EK_KFIFO_INITIALIZER

/**
 * @brief 由节点取回外层对象（node 必须非 NULL）
 */
#    define ek_kfifo_entry(node, type, member) EK_CONTAINER_OF(node, type, member)

/**
 * @brief 出队并取回外层对象，FIFO 为空时返回 NULL
 */
#    define ek_kfifo_get_entry(fifo, type, member) \
        ((type *)_ek_kfifo_entry_or_null(ek_kfifo_get(fifo), offsetof(type, member)))

/**
 * @brief 查看队首对象但不出队，FIFO 为空时返回 NULL
 */
#    define ek_kfifo_peek_entry(fifo, type, member) \
        ((type *)_ek_kfifo_entry_or_null(ek_kfifo_peek_head(fifo), offsetof(type, member)))

/**
 * @brief 遍历 ek_kfifo_get_all() 返回的链（允许在遍历中重新入队/释放当前节点）
 * @param pos   当前节点（迭代变量，ek_kfifo_node_t *）
 * @param n     临时变量（ek_kfifo_node_t *）
 * @param chain 链首
 */
#    define ek_kfifo_chain_foreach_safe(pos, n, chain)                      \
        for ((pos) = (chain), (n) = ((pos) != NULL) ? (pos)->next : NULL; \
             (pos) != NULL;                                                 \
             (pos) = (n), (n) = ((pos) != NULL) ? (pos)->next : NULL)

/**
 * @brief 把一个对象数组全部放进 FIFO（常用于初始化空闲池）
 * @param fifo   目标 FIFO
 * @param array  对象数组（不是指针）
 * @param member 数组元素中 ek_kfifo_node_t 成员的名字
 */
#    define EK_KFIFO_FILL(fifo, array, member)                                    \
        ek_kfifo_fill((fifo), (void *)(array), sizeof((array)[0]), EK_ARRAY_LEN(array), \
                      (size_t)((const char *)&(array)[0].member - (const char *)&(array)[0]))

#    ifdef __cplusplus
extern "C"
{
#    endif

/**
 * @brief 初始化（清空）FIFO
 * @return 0 成功，-EINVAL fifo 为 NULL
 * @note 只是丢弃链表，不会访问原先挂着的节点
 */
int ek_kfifo_init(ek_kfifo_t *fifo);

/**
 * @brief 节点入队尾（可在 ISR 中调用）
 * @return 0 成功，-EINVAL 参数为 NULL 或节点已是队尾（重复入队）
 */
int ek_kfifo_put(ek_kfifo_t *fifo, ek_kfifo_node_t *node);

/**
 * @brief 节点插到队首，下一次 get 就会取到它（可在 ISR 中调用）
 * @return 0 成功，-EINVAL 参数为 NULL 或节点已是队首
 */
int ek_kfifo_put_front(ek_kfifo_t *fifo, ek_kfifo_node_t *node);

/**
 * @brief 把一条已经串好的链整体入队尾（可在 ISR 中调用）
 *
 * 链从 first 开始沿 next 走到 last，节点数在临界区外统计，临界区内只做 O(1) 的拼接。
 *
 * @param first 链首
 * @param last  链尾（last->next 会被置为 NULL）
 * @return 0 成功，-EINVAL 参数为 NULL 或从 first 走不到 last
 */
int ek_kfifo_put_list(ek_kfifo_t *fifo, ek_kfifo_node_t *first, ek_kfifo_node_t *last);

/**
 * @brief 把一个对象数组逐个入队（见 EK_KFIFO_FILL）
 * @return 0 成功，-EINVAL 参数非法
 */
int ek_kfifo_fill(ek_kfifo_t *fifo, void *array, size_t elem_size, size_t count, size_t node_offset);

/**
 * @brief 队首节点出队（可在 ISR 中调用）
 * @return 节点指针，FIFO 为空时返回 NULL
 */
ek_kfifo_node_t *ek_kfifo_get(ek_kfifo_t *fifo);

/**
 * @brief 一次取走全部节点（可在 ISR 中调用）
 *
 * 只在临界区内摘下整条链，后续遍历不需要关中断。
 *
 * @param count 输出取走的节点数，可为 NULL
 * @return 以 NULL 结尾的链首，FIFO 为空时返回 NULL；用 ek_kfifo_chain_foreach_safe 遍历
 */
ek_kfifo_node_t *ek_kfifo_get_all(ek_kfifo_t *fifo, uint32_t *count);

/**
 * @brief 查看队首节点但不出队
 * @warning 多消费者场景下返回后节点可能已被其他上下文取走
 */
ek_kfifo_node_t *ek_kfifo_peek_head(ek_kfifo_t *fifo);

/**
 * @brief 查看队尾节点但不出队
 */
ek_kfifo_node_t *ek_kfifo_peek_tail(ek_kfifo_t *fifo);

/**
 * @brief 从 FIFO 中撤回指定节点（O(n)，用于取消尚未处理的请求）
 * @return true 找到并移除，false 不在该 FIFO 中
 */
bool ek_kfifo_remove(ek_kfifo_t *fifo, ek_kfifo_node_t *node);

/**
 * @brief 当前节点数
 */
uint32_t ek_kfifo_count(const ek_kfifo_t *fifo);

/**
 * @brief FIFO 是否为空（fifo 为 NULL 视为空）
 */
bool ek_kfifo_is_empty(const ek_kfifo_t *fifo);

/**
 * @brief 内部使用：节点转外层对象，NULL 保持 NULL
 */
__EK_STATIC_INLINE void *_ek_kfifo_entry_or_null(ek_kfifo_node_t *node, size_t offset)
{
    return (node != NULL) ? (void *)((char *)node - offset) : NULL;
}

#    ifdef __cplusplus
}
#    endif

#endif /* EKCFG_KFIFO */

#endif /* EK_KFIFO_H */
