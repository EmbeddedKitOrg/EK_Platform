/**
 * @file ringbuf.h
 * @brief 环形缓冲区（Ring Buffer）
 * @author N1netyNine99
 *
 * 通用的环形缓冲区实现，支持任意数据类型的存储
 */

#ifndef EK_RINGBUF_H
#define EK_RINGBUF_H

#include "ek/conf_internal.h"
#include "ek/core/def.h"

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
    uint8_t data[]; /**< 柔性数据区（动态分配位于堆块尾部，静态分配位于 union 尾部） */
};
#    endif /* EKCFG_RINGBUF */

#    if EKCFG_RINGBUF_SPSC == 1
struct ek_ringbuf_spsc
{
    uint32_t write_idx; /**< 写入位置索引 */
    uint32_t read_idx; /**< 读取位置索引 */
    size_t cap; /**< 底层槽位数量，实际最大可存元素数为 cap - 1 */
    size_t item_size; /**< 单个元素大小（字节） */
    uint8_t data[]; /**< 柔性数据区（动态分配位于堆块尾部，静态分配位于 union 尾部） */
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

/**
 * @brief 在调用者提供的缓冲区上静态初始化环形缓冲区
 * @param rb          环形缓冲区对象指针（后随紧邻的数据缓冲区）
 * @param item_size   单个元素大小（字节，须大于 0）
 * @param item_amount 缓冲区容量（元素个数，须大于 0）
 * @return 0 成功
 * @return -EINVAL rb 为 NULL，或 item_size / item_amount 为 0
 */
int ek_ringbuf_init_static(ek_ringbuf_t *rb, size_t item_size, uint32_t item_amount);

/**
 * @brief 反初始化静态环形缓冲区，清空索引与配置
 * @param rb 环形缓冲区对象指针
 */
void ek_ringbuf_deinit_static(ek_ringbuf_t *rb);

#        if EKCFG_STATIC_ALLOC == 1
#            include "ek/mem/static_alloc.h"

/**
 * @brief 静态声明一个环形缓冲区并自动注册初始化
 * @param handle 缓冲区句柄变量名（static ek_ringbuf_t *const）
 * @param type   元素数据类型
 * @param amount 缓冲区容量（元素个数）
 *
 * @note 必须在 .c 文件作用域使用，且同一翻译单元内重复使用 handle 会重定义
 * @note 句柄在 ek_static_alloc_init() 执行前不可使用
 */
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
                EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_RINGBUF, _static_alloc_init_##handle)
#        endif

/**
 * @brief 销毁环形缓冲区并把 rb_ptr 设置为 NULL
 * @param rb_ptr 要销毁的环形缓冲区指针
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
 * @return -EINVAL rb 或 item 为 NULL
 * @return -ENOSPC 缓冲区已满
 */
int ek_ringbuf_write(ek_ringbuf_t *rb, const void *item);

/**
 * @brief 从环形缓冲区读取一个元素
 * @param rb 环形缓冲区指针
 * @param item 存储读取结果的缓冲区指针，传入 NULL 则直接丢弃数据
 * @return 0 读取成功
 * @return -EINVAL rb 为 NULL
 * @return -ENODATA 缓冲区为空
 */
int ek_ringbuf_read(ek_ringbuf_t *rb, void *item);

/**
 * @brief 查看环形缓冲区首个元素（不移动读指针）
 * @param rb 环形缓冲区指针
 * @param item 存储查看结果的缓冲区指针
 * @return 0 查看成功
 * @return -EINVAL rb 或 item 为 NULL
 * @return -ENODATA 缓冲区为空
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
 * @param item_amount 底层槽位数量（必须大于 1），实际最大可存元素数为 item_amount - 1
 * @return 成功返回缓冲区指针，失败返回 NULL
 */
ek_ringbuf_spsc_t *ek_ringbuf_create_spsc(size_t item_size, uint32_t item_amount);

/**
 * @brief 销毁 SPSC 环形缓冲区
 * @param rb 要销毁的环形缓冲区
 */
void ek_ringbuf_destroy_spsc(ek_ringbuf_spsc_t *rb);

/**
 * @brief 在调用者提供的缓冲区上静态初始化 SPSC 环形缓冲区
 * @param rb        环形缓冲区对象指针（后随紧邻的数据缓冲区）
 * @param item_size 单个元素大小（字节，须大于 0）
 * @param amount    底层槽位数量（须大于 1，实际最大可存元素数为 amount - 1）
 * @return 0 成功
 * @return -EINVAL rb 为 NULL、item_size 为 0 或 amount <= 1
 */
int ek_ringbuf_init_spsc_static(ek_ringbuf_spsc_t *rb, size_t item_size, uint32_t amount);

/**
 * @brief 反初始化静态 SPSC 环形缓冲区，清空索引与配置
 * @param rb 环形缓冲区对象指针
 */
void ek_ringbuf_deinit_spsc_static(ek_ringbuf_spsc_t *rb);

#        if EKCFG_STATIC_ALLOC == 1
#            include "ek/mem/static_alloc.h"

/**
 * @brief 静态声明一个 SPSC 环形缓冲区并自动注册初始化
 * @param handle 缓冲区句柄变量名（static ek_ringbuf_spsc_t *const）
 * @param type   元素数据类型
 * @param amount 底层槽位数量（须大于 1，实际最大可存元素数为 amount - 1）
 *
 * @note 必须在 .c 文件作用域使用，且同一翻译单元内重复使用 handle 会重定义
 * @note 句柄在 ek_static_alloc_init() 执行前不可使用
 */
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
                EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_RINGBUF, _static_alloc_init_##handle)
#        endif

/**
 * @brief 销毁 SPSC 环形缓冲区并把 rb_ptr 设置为 NULL
 * @param rb_ptr 要销毁的环形缓冲区指针
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
 * @return -EINVAL rb 或 item 为 NULL
 * @return -ENOSPC 缓冲区已满
 */
int ek_ringbuf_write_spsc(ek_ringbuf_spsc_t *rb, const void *item);

/**
 * @brief 从 SPSC 环形缓冲区读取一个元素
 * @param rb 环形缓冲区指针
 * @param item 存储读取结果的缓冲区指针，传入 NULL 则直接丢弃数据
 * @return 0 读取成功
 * @return -EINVAL rb 为 NULL
 * @return -ENODATA 缓冲区为空
 */
int ek_ringbuf_read_spsc(ek_ringbuf_spsc_t *rb, void *item);

/**
 * @brief 查看 SPSC 环形缓冲区首个元素（不移动读指针）
 * @param rb 环形缓冲区指针
 * @param item 存储查看结果的缓冲区指针
 * @return 0 查看成功
 * @return -EINVAL rb 或 item 为 NULL
 * @return -ENODATA 缓冲区为空
 */
int ek_ringbuf_peek_spsc(ek_ringbuf_spsc_t *rb, void *item);
#    endif /* EKCFG_RINGBUF_SPSC */

#    ifdef __cplusplus
}
#    endif /* __cplusplus */

#endif /* EKCFG_RINGBUF || EKCFG_RINGBUF_SPSC */

#endif /* EK_RINGBUF_H */
