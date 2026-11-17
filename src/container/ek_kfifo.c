/**
 * @file ek_kfifo.c
 * @brief 侵入式零拷贝 FIFO 实现
 */

#include "ek/container/ek_kfifo.h"

#if EKCFG_KFIFO == 1

int ek_kfifo_init(ek_kfifo_t *fifo)
{
    if (fifo == NULL) return -EINVAL;

    EK_KFIFO_CRITICAL_ENTER();
    fifo->head = NULL;
    fifo->tail = NULL;
    fifo->count = 0U;
    EK_KFIFO_CRITICAL_EXIT();
    return 0;
}

int ek_kfifo_put(ek_kfifo_t *fifo, ek_kfifo_node_t *node)
{
    if (fifo == NULL || node == NULL) return -EINVAL;

    int err = 0;
    node->next = NULL;

    EK_KFIFO_CRITICAL_ENTER();
    if (node == fifo->tail)
    {
        err = -EINVAL; /* 同一节点连续入队两次会让链表成环 */
    }
    else
    {
        if (fifo->tail != NULL) fifo->tail->next = node;
        else fifo->head = node;
        fifo->tail = node;
        fifo->count++;
    }
    EK_KFIFO_CRITICAL_EXIT();
    return err;
}

int ek_kfifo_put_front(ek_kfifo_t *fifo, ek_kfifo_node_t *node)
{
    if (fifo == NULL || node == NULL) return -EINVAL;

    int err = 0;

    EK_KFIFO_CRITICAL_ENTER();
    if (node == fifo->head)
    {
        err = -EINVAL;
    }
    else
    {
        node->next = fifo->head;
        fifo->head = node;
        if (fifo->tail == NULL) fifo->tail = node;
        fifo->count++;
    }
    EK_KFIFO_CRITICAL_EXIT();
    return err;
}

int ek_kfifo_put_list(ek_kfifo_t *fifo, ek_kfifo_node_t *first, ek_kfifo_node_t *last)
{
    if (fifo == NULL || first == NULL || last == NULL) return -EINVAL;

    /* 链由调用方私有，统计节点数不需要关中断 */
    uint32_t n = 1U;
    for (ek_kfifo_node_t *pos = first; pos != last; pos = pos->next)
    {
        if (pos->next == NULL) return -EINVAL;
        n++;
    }
    last->next = NULL;

    EK_KFIFO_CRITICAL_ENTER();
    if (fifo->tail != NULL) fifo->tail->next = first;
    else fifo->head = first;
    fifo->tail = last;
    fifo->count += n;
    EK_KFIFO_CRITICAL_EXIT();
    return 0;
}

int ek_kfifo_fill(ek_kfifo_t *fifo, void *array, size_t elem_size, size_t count, size_t node_offset)
{
    if (fifo == NULL || array == NULL || count == 0U) return -EINVAL;
    if (elem_size < sizeof(ek_kfifo_node_t) || node_offset > elem_size - sizeof(ek_kfifo_node_t)) return -EINVAL;

    /* 先在临界区外把数组串成一条链，再整体拼接 */
    uint8_t *base = (uint8_t *)array;
    ek_kfifo_node_t *first = (ek_kfifo_node_t *)(void *)(base + node_offset);
    ek_kfifo_node_t *prev = first;
    for (size_t i = 1U; i < count; i++)
    {
        ek_kfifo_node_t *node = (ek_kfifo_node_t *)(void *)(base + i * elem_size + node_offset);
        prev->next = node;
        prev = node;
    }
    return ek_kfifo_put_list(fifo, first, prev);
}

ek_kfifo_node_t *ek_kfifo_get(ek_kfifo_t *fifo)
{
    if (fifo == NULL) return NULL;

    EK_KFIFO_CRITICAL_ENTER();
    ek_kfifo_node_t *node = fifo->head;
    if (node != NULL)
    {
        fifo->head = node->next;
        if (fifo->head == NULL) fifo->tail = NULL;
        fifo->count--;
    }
    EK_KFIFO_CRITICAL_EXIT();

    if (node != NULL) node->next = NULL;
    return node;
}

ek_kfifo_node_t *ek_kfifo_get_all(ek_kfifo_t *fifo, uint32_t *count)
{
    if (fifo == NULL)
    {
        if (count != NULL) *count = 0U;
        return NULL;
    }

    EK_KFIFO_CRITICAL_ENTER();
    ek_kfifo_node_t *chain = fifo->head;
    const uint32_t n = fifo->count;
    fifo->head = NULL;
    fifo->tail = NULL;
    fifo->count = 0U;
    EK_KFIFO_CRITICAL_EXIT();

    if (count != NULL) *count = n;
    return chain;
}

ek_kfifo_node_t *ek_kfifo_peek_head(ek_kfifo_t *fifo)
{
    if (fifo == NULL) return NULL;

    EK_KFIFO_CRITICAL_ENTER();
    ek_kfifo_node_t *node = fifo->head;
    EK_KFIFO_CRITICAL_EXIT();
    return node;
}

ek_kfifo_node_t *ek_kfifo_peek_tail(ek_kfifo_t *fifo)
{
    if (fifo == NULL) return NULL;

    EK_KFIFO_CRITICAL_ENTER();
    ek_kfifo_node_t *node = fifo->tail;
    EK_KFIFO_CRITICAL_EXIT();
    return node;
}

bool ek_kfifo_remove(ek_kfifo_t *fifo, ek_kfifo_node_t *node)
{
    if (fifo == NULL || node == NULL) return false;

    bool found = false;

    EK_KFIFO_CRITICAL_ENTER();
    ek_kfifo_node_t *prev = NULL;
    for (ek_kfifo_node_t *pos = fifo->head; pos != NULL; prev = pos, pos = pos->next)
    {
        if (pos != node) continue;

        if (prev != NULL) prev->next = pos->next;
        else fifo->head = pos->next;
        if (fifo->tail == pos) fifo->tail = prev;
        fifo->count--;
        found = true;
        break;
    }
    EK_KFIFO_CRITICAL_EXIT();

    if (found) node->next = NULL;
    return found;
}

uint32_t ek_kfifo_count(const ek_kfifo_t *fifo)
{
    if (fifo == NULL) return 0U;
    return fifo->count; /* 32 位对齐读，单次访问即原子 */
}

bool ek_kfifo_is_empty(const ek_kfifo_t *fifo)
{
    return ek_kfifo_count(fifo) == 0U;
}

#endif /* EKCFG_KFIFO */
