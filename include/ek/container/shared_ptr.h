/**
 * @file shared_ptr.h
 * @brief 共享指针接口
 * @author N1netyNine99
 *
 * 控制头隐藏在数据区之前，调用者只接触数据指针。
 * 引用计数使用 32 位原子操作，分配和释放固定使用 ek_malloc/ek_free。
 *
 * @note 不支持 weak_ptr，循环引用会导致内存泄漏
 * @note 返回的数据指针必须交给 ek_shared_ptr_unref() 释放
 */

#ifndef SHARED_PTR_H
#define SHARED_PTR_H

#include "ek/conf_internal.h"

#if EKCFG_SHARED_PTR == 1

#    include "ek/core/def.h"

#    ifdef __cplusplus
extern "C"
{
#    endif /* __cplusplus */

/**
 * @brief 分配共享数据区
 * @param size 数据区大小（字节）
 * @return 数据区指针，失败返回 NULL
 * @note 实际分配大小为隐藏头部 + size，返回指针指向数据区
 */
void *ek_shared_ptr_alloc(size_t size);

/**
 * @brief 增加一个共享引用
 * @param ptr ek_shared_ptr_alloc() 返回的数据指针
 * @return 成功返回同一个数据指针；参数无效或引用计数饱和时返回 NULL
 * @note 调用者必须已经持有一个有效引用
 */
void *ek_shared_ptr_ref(void *ptr);

/**
 * @brief 减少一个共享引用
 * @param ptr ek_shared_ptr_alloc() 返回的数据指针
 * @note 最后一个引用释放整块内存，调用后不能再访问 ptr
 */
void ek_shared_ptr_unref(void *ptr);

/**
 * @brief 获取共享数据区大小
 * @param ptr 数据指针
 * @return 数据区大小；参数无效时返回 0
 */
size_t ek_shared_ptr_size(const void *ptr);

#    ifdef __cplusplus
}
#    endif /* __cplusplus */

#endif

#endif // SHARED_PTR_H
