/**
 * @file ek_heap.h
 * @brief 内存管理接口
 * @author N1netyNine99
 *
 * 提供基于 TLSF 算法的动态内存分配功能
 */

#ifndef EK_HEAP_H
#define EK_HEAP_H

#include "ek_def.h"
#include "ek_conf_internal.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief  从默认内存堆分配内存
 * @param  size: 要分配的内存大小（字节）
 * @retval 分配的内存指针，失败返回 NULL
 */
void *ek_malloc(size_t size);

/**
 * @brief  重新分配内存大小
 * @param  ptr: 原内存指针
 * @param  size: 新的内存大小（字节）
 * @retval 重新分配后的内存指针，失败返回 NULL
 */
void *ek_realloc(void *ptr, size_t size);

/**
 * @brief  释放内存到默认内存堆
 * @param  ptr: 要释放的内存指针
 */
void ek_free(void *ptr);

/**
 * @brief  释放内存并将指针置为 NULL，避免悬空指针
 * @param  ptr: 要释放的内存指针
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

void ek_heap_init(void);
void ek_heap_destory(void);
size_t ek_heap_total_size(void);
pool_t ek_heap_add_pool(void *ptr, size_t size);
void ek_heap_remove_pool(pool_t pool);
size_t ek_heap_unused(void);
size_t ek_heap_used(void);

/* ========== 用户堆信息查询 ========== */

size_t ek_pool_total_size(pool_t pool);
size_t ek_pool_unused(pool_t pool);
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