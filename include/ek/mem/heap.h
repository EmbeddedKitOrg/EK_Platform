/**
 * @file heap.h
 * @brief 内存管理接口
 * @author N1netyNine99
 *
 * 提供基于 TLSF 算法的动态内存分配功能
 */

#ifndef EK_HEAP_H
#define EK_HEAP_H

#include "ek/core/def.h"
#include "ek/conf_internal.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 从默认内存堆分配内存（弱函数）
 * @param size 要分配的内存大小（字节）
 * @return 分配的内存指针
 * @retval NULL 分配失败或在 RTOS 中断上下文中调用
 */
void *ek_malloc(size_t size);

/**
 * @brief 重新分配内存大小（弱函数）
 * @param ptr 原内存指针
 * @param size 新的内存大小（字节）
 * @return 重新分配后的内存指针
 * @retval NULL 分配失败或在 RTOS 中断上下文中调用
 */
void *ek_realloc(void *ptr, size_t size);

/**
 * @brief 释放内存到默认内存堆（弱函数）
 * @param ptr 要释放的内存指针（传入 NULL 或在 RTOS 中断上下文中调用时直接返回）
 */
void ek_free(void *ptr);

/**
 * @brief 释放内存并将指针变量置为 NULL，避免悬空指针
 * @param ptr 指向待释放内存的可修改指针变量（左值，不能是带副作用的表达式或二级指针地址）
 */
#ifndef ek_free_safely
#    define ek_free_safely(ptr) \
        do                      \
        {                       \
            ek_free((ptr));     \
            ptr = NULL;         \
        } while (0)
#endif

#if EKCFG_HEAP_TLSF == 1

#    include "../../third_party/tlsf/tlsf.h"

/* ========== 默认堆操作 ========== */

/**
 * @brief 初始化默认 TLSF 内存堆
 */
void ek_heap_init(void);

/**
 * @brief 销毁默认 TLSF 内存堆
 * @note RTOS 下不允许在中断上下文中调用
 */
void ek_heap_destory(void);

/**
 * @brief 获取默认内存堆的可用总字节数（不含 TLSF 控制块开销）
 * @return 默认内存堆总字节数
 */
size_t ek_heap_total_size(void);

/**
 * @brief 向默认 TLSF 堆追加一块内存池
 * @param ptr 内存池起始地址
 * @param size 内存池字节数
 * @return 成功返回内存池句柄，失败或在 RTOS 中断上下文中调用返回 NULL
 */
pool_t ek_heap_add_pool(void *ptr, size_t size);

/**
 * @brief 从默认 TLSF 堆移除指定内存池
 * @param pool 要移除的内存池句柄
 * @note RTOS 下不允许在中断上下文中调用
 */
void ek_heap_remove_pool(pool_t pool);

/**
 * @brief 查询默认堆主池中的空闲字节数
 * @return 空闲字节数；在 RTOS 中断上下文中调用返回 0
 */
size_t ek_heap_unused(void);

/**
 * @brief 查询默认堆主池中的已用字节数
 * @return 已用字节数；在 RTOS 中断上下文中调用返回 0
 */
size_t ek_heap_used(void);

/* ========== 内存池信息查询 ========== */

/**
 * @brief 查询指定内存池的总可用字节数
 * @param pool 内存池句柄
 * @return 内存池总字节数；在 RTOS 中断上下文中调用返回 0
 */
size_t ek_pool_total_size(pool_t pool);

/**
 * @brief 查询指定内存池中的空闲字节数
 * @param pool 内存池句柄
 * @return 空闲字节数；在 RTOS 中断上下文中调用返回 0
 */
size_t ek_pool_unused(pool_t pool);

/**
 * @brief 查询指定内存池中的已用字节数
 * @param pool 内存池句柄
 * @return 已用字节数；在 RTOS 中断上下文中调用返回 0
 */
size_t ek_pool_used(pool_t pool);

#else

/* ========================================================================
 * EKCFG_HEAP_TLSF == 0: 用户自定义内存分配器
 * 用户需要:
 *   定义 ek_malloc, ek_free, ek_realloc 函数的强版本
 * ======================================================================== */

#endif /* EKCFG_HEAP_TLSF */

#ifdef __cplusplus
}
#endif

#endif /* EK_HEAP_H */