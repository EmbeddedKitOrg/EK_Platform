/**
 * @file ek_ringbuf.h
 * @brief 环形缓冲区（Ring Buffer）
 * @author N1netyNine99
 *
 * 通用的环形缓冲区实现，支持任意数据类型的存储
 */

#ifndef EK_RINGBUF_H
#define EK_RINGBUF_H

#include "ek/ek_conf_internal.h"
#include "ek/core/ek_def.h"

#if (EKCFG_RINGBUF == 1) || (EKCFG_RINGBUF_SPSC == 1)

/**
 * @brief 环形缓冲区结构
 */
typedef struct ek_ringbuf ek_ringbuf_t;
typedef struct ek_ringbuf_spsc ek_ringbuf_spsc_t;

#    if EKCFG_RINGBUF == 1
struct ek_ringbuf
{
    uint32_t write_idx; /**< 写入位置索引 */
    uint32_t read_idx; /**< 读取位置索引 */
    uint32_t item_amount; /**< 当前元素个数 */
    size_t cap; /**< 缓冲区容量（元素个数） */
    size_t item_size; /**< 单个元素大小（字节） */
#        if EKCFG_RTOS == 1
    EK_LOCK_TYPE lock;
#        endif /* EKCFG_RTOS */
    uint8_t data[]; /**< 唯一数据位置：动态=malloc 尾部，静态=union 尾部 */
};
#    endif /* EKCFG_RINGBUF */

#    if EKCFG_RINGBUF_SPSC == 1
struct ek_ringbuf_spsc
{
    uint32_t write_idx; /**< 写入位置索引 */
    uint32_t read_idx; /**< 读取位置索引 */
    size_t cap; /**< 底层槽位数量，实际最大可存元素数为 cap - 1 */
    size_t item_size; /**< 单个元素大小（字节） */
    uint8_t data[]; /**< 唯一数据位置：动态=malloc 尾部，静态=union 尾部 */
};
#    endif /* EKCFG_RINGBUF_SPSC */

#    ifdef __cplusplus
extern "C"
{
#    endif /* __cplusplus */

#    if EKCFG_RINGBUF == 1
/**
 * @brief 判断环形缓冲区是否已满
 * @param rb 环形缓冲区指针
 * @return true 已满
 * @return false 未满
 */
bool ek_ringbuf_full(const ek_ringbuf_t *rb);

/**
 * @brief 判断环形缓冲区是否为空
 * @param rb 环形缓冲区指针
 * @return true 为空
 * @return false 不为空
 */
bool ek_ringbuf_empty(const ek_ringbuf_t *rb);

/**
 * @brief 创建环形缓冲区
 * @param item_size 单个元素大小（字节）
 * @param item_amount 缓冲区容量（元素个数）
 * @return 成功返回缓冲区指针，失败返回 NULL
 */
ek_ringbuf_t *ek_ringbuf_create(size_t item_size, uint32_t item_amount);

/**
 * @brief 销毁环形缓冲区
 * @param rb 要销毁的环形缓冲区
 *
 * @note 缓冲区与控制块同块分配，本函数仅 ek_free(rb) 一次；配合 ek_ringbuf_destroy_safely() 使用
 */
void ek_ringbuf_destroy(ek_ringbuf_t *rb);

#        if EKCFG_STATIC_ALLOC == 1
#            include "ek/mem/ek_static_alloc.h"

int ek_ringbuf_init_static(ek_ringbuf_t *rb, size_t item_size, uint32_t item_amount);
void ek_ringbuf_deinit_static(ek_ringbuf_t *rb);

#            define EK_DEFINE_RINGBUF(handle, type, amount)                          \
                static union                                                         \
                {                                                                    \
                    ek_ringbuf_t rb;                                                 \
                    type align_; /* 把 union 对齐抬到元素对齐 */                     \
                    uint8_t storage[sizeof(ek_ringbuf_t) + sizeof(type) * (amount)]; \
                } handle##_mem;                                                      \
                static ek_ringbuf_t *const handle = &handle##_mem.rb;                \
                static int _static_alloc_init_##handle(void)                         \
                {                                                                    \
                    return ek_ringbuf_init_static(handle, sizeof(type), (amount));   \
                }                                                                    \
                EK_STATIC_ALLOC_REGISTER(handle, 10, _static_alloc_init_##handle)
#        endif

/**
 * @brief 销毁环形缓冲区并把rb_ptr设置为NULL
 * @param rb_ptr 要销毁的环形缓冲区
 *
 */
#        define ek_ringbuf_destroy_safely(rb_ptr) \
            do                                    \
            {                                     \
                ek_ringbuf_destroy(rb_ptr);       \
                rb_ptr = NULL;                    \
            } while (0)

/**
 * @brief 向环形缓冲区写入一个元素
 * @param rb 环形缓冲区指针
 * @param item 要写入的元素指针
 * @return 0 写入成功
 * @return -ENOSPC 缓冲区已满
 * @return -EBUSY 资源忙（RTOS 模式）
 */
int ek_ringbuf_write(ek_ringbuf_t *rb, const void *item);

/**
 * @brief 从环形缓冲区读取一个元素
 * @param rb 环形缓冲区指针
 * @param item 存储读取结果的缓冲区指针，传入 NULL 则直接丢弃数据
 * @return 0 读取成功
 * @return -ENODATA 缓冲区为空
 * @return -EBUSY 资源忙（RTOS 模式）
 */
int ek_ringbuf_read(ek_ringbuf_t *rb, void *item);

/**
 * @brief 查看环形缓冲区首个元素（不移动读指针）
 * @param rb 环形缓冲区指针
 * @param item 存储查看结果的缓冲区指针
 * @return 0 查看成功
 * @return -ENODATA 缓冲区为空
 * @return -EBUSY 资源忙（RTOS 模式）
 */
int ek_ringbuf_peek(ek_ringbuf_t *rb, void *item);
#    endif /* EKCFG_RINGBUF */

#    if EKCFG_RINGBUF_SPSC == 1
/**
 * @brief 判断 SPSC 环形缓冲区是否已满
 * @param rb 环形缓冲区指针
 * @return true 已满
 * @return false 未满
 */
bool ek_ringbuf_full_spsc(const ek_ringbuf_spsc_t *rb);

/**
 * @brief 判断 SPSC 环形缓冲区是否为空
 * @param rb 环形缓冲区指针
 * @return true 为空
 * @return false 不为空
 */
bool ek_ringbuf_empty_spsc(const ek_ringbuf_spsc_t *rb);

/**
 * @brief 创建 SPSC 环形缓冲区
 * @param item_size 单个元素大小（字节）
 * @param item_amount 底层槽位数量，实际最大可存元素数为 item_amount - 1
 * @return 成功返回缓冲区指针，失败返回 NULL
 */
ek_ringbuf_spsc_t *ek_ringbuf_create_spsc(size_t item_size, uint32_t item_amount);

/**
 * @brief 销毁 SPSC 环形缓冲区
 * @param rb 要销毁的环形缓冲区
 */
void ek_ringbuf_destroy_spsc(ek_ringbuf_spsc_t *rb);

#        if EKCFG_STATIC_ALLOC == 1
int ek_ringbuf_init_spsc_static(ek_ringbuf_spsc_t *rb, size_t item_size, uint32_t amount);
void ek_ringbuf_deinit_spsc_static(ek_ringbuf_spsc_t *rb);

#            define EK_DEFINE_RINGBUF_SPSC(handle, type, amount)                          \
                static union                                                              \
                {                                                                         \
                    ek_ringbuf_spsc_t rb;                                                 \
                    type align_; /* 把 union 对齐抬到元素对齐 */                          \
                    uint8_t storage[sizeof(ek_ringbuf_spsc_t) + sizeof(type) * (amount)]; \
                } handle##_mem;                                                           \
                static ek_ringbuf_spsc_t *const handle = &handle##_mem.rb;                \
                static int _static_alloc_init_##handle(void)                              \
                {                                                                         \
                    return ek_ringbuf_init_spsc_static(handle, sizeof(type), (amount));   \
                }                                                                         \
                EK_STATIC_ALLOC_REGISTER(handle, 10, _static_alloc_init_##handle)
#        endif

/**
 * @brief 销毁 SPSC 环形缓冲区并把 rb_ptr 设置为 NULL
 * @param rb_ptr 要销毁的环形缓冲区
 */
#        define ek_ringbuf_destroy_safely_spsc(rb_ptr) \
            do                                         \
            {                                          \
                ek_ringbuf_destroy_spsc(rb_ptr);       \
                rb_ptr = NULL;                         \
            } while (0)

/**
 * @brief 向 SPSC 环形缓冲区写入一个元素
 * @param rb 环形缓冲区指针
 * @param item 要写入的元素指针
 * @return 0 写入成功
 * @return -ENOSPC 缓冲区已满
 */
int ek_ringbuf_write_spsc(ek_ringbuf_spsc_t *rb, const void *item);

/**
 * @brief 从 SPSC 环形缓冲区读取一个元素
 * @param rb 环形缓冲区指针
 * @param item 存储读取结果的缓冲区指针，传入 NULL 则直接丢弃数据
 * @return 0 读取成功
 * @return -ENODATA 缓冲区为空
 */
int ek_ringbuf_read_spsc(ek_ringbuf_spsc_t *rb, void *item);

/**
 * @brief 查看 SPSC 环形缓冲区首个元素（不移动读指针）
 * @param rb 环形缓冲区指针
 * @param item 存储查看结果的缓冲区指针
 * @return 0 查看成功
 * @return -ENODATA 缓冲区为空
 */
int ek_ringbuf_peek_spsc(ek_ringbuf_spsc_t *rb, void *item);
#    endif /* EKCFG_RINGBUF_SPSC */

#    ifdef __cplusplus
}
#    endif /* __cplusplus */

#endif /* EKCFG_RINGBUF || EKCFG_RINGBUF_SPSC */

#endif /* EK_RINGBUF_H */
