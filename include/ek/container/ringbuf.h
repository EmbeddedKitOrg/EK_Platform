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
    bool mem_owned; /**< true 表示由堆分配，destroy 时释放；false 表示位于调用者缓冲区 */
    uint32_t write_idx; /**< 写入位置索引 */
    uint32_t read_idx; /**< 读取位置索引 */
    uint32_t item_amount; /**< 当前元素个数 */
    size_t cap; /**< 缓冲区容量（元素个数） */
    size_t item_size; /**< 单个元素大小（字节） */
    uint8_t data[]; /**< 柔性数据区（动态分配位于堆块尾部，静态分配位于调用者缓冲区尾部） */
};
#    endif /* EKCFG_RINGBUF */

#    if EKCFG_RINGBUF_SPSC == 1
struct ek_ringbuf_spsc
{
    bool mem_owned; /**< true 表示由堆分配，destroy 时释放；false 表示位于调用者缓冲区 */
    uint32_t write_idx; /**< 写入位置索引 */
    uint32_t read_idx; /**< 读取位置索引 */
    size_t cap; /**< 底层槽位数量，实际最大可存元素数为 cap - 1 */
    size_t item_size; /**< 单个元素大小（字节） */
    uint8_t data[]; /**< 柔性数据区（动态分配位于堆块尾部，静态分配位于调用者缓冲区尾部） */
};
#    endif /* EKCFG_RINGBUF_SPSC */

#    ifdef __cplusplus
extern "C"
{
#    endif /* __cplusplus */

#    if EKCFG_RINGBUF == 1
/**
 * @brief 静态创建环形缓冲区所需的最小缓冲区字节数
 *
 * 缓冲区前部预留按 alignof(ek_ringbuf_t) 对齐的生命周期标记头，
 * 后接控制块与连续元素存储区。缓冲区起始地址必须按 alignof(ek_ringbuf_t) 对齐。
 */
#        define EK_RINGBUF_STATIC_BUF_SIZE(item_size, item_amount)                         \
            (EK_ALIGN_UP(sizeof(uint32_t), alignof(ek_ringbuf_t)) + sizeof(ek_ringbuf_t) + \
             (size_t)(item_size) * (item_amount))

/**
 * @brief 判断环形缓冲区是否已满
 * @param rb 环形缓冲区指针
 * @return true 已满
 * @return false 未满或句柄无效/已销毁
 */
bool ek_ringbuf_full(const ek_ringbuf_t *rb);

/**
 * @brief 判断环形缓冲区是否为空
 * @param rb 环形缓冲区指针
 * @return true 为空
 * @return false 不为空或句柄无效/已销毁
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
 * @brief 在调用者提供的缓冲区上静态创建环形缓冲区
 * @param buf         缓冲区起始地址，必须按 alignof(ek_ringbuf_t) 对齐
 * @param size        缓冲区字节数，不得小于 EK_RINGBUF_STATIC_BUF_SIZE(item_size, item_amount)
 * @param item_size   单个元素大小（字节，须大于 0）
 * @param item_amount 缓冲区容量（元素个数，须大于 0）
 * @return 成功返回缓冲区指针，失败返回 NULL
 *
 * @note 缓冲区由调用者持有；destroy 只清除生命周期标记，不会释放缓冲区
 */
ek_ringbuf_t *ek_ringbuf_create_static(void *buf, size_t size, size_t item_size, uint32_t item_amount);

/**
 * @brief 销毁环形缓冲区
 * @param rb 要销毁的环形缓冲区
 *
 * @note 动态对象释放堆内存，静态对象清除生命周期标记；配合 ek_ringbuf_destroy_safely() 使用
 */
void ek_ringbuf_destroy(ek_ringbuf_t *rb);

#        if EKCFG_STATIC_ALLOC == 1
#            include "ek/mem/static_alloc.h"

/**
 * @brief 静态声明一个环形缓冲区并自动注册初始化
 * @param handle 缓冲区句柄变量名（static ek_ringbuf_t *）
 * @param type   元素数据类型
 * @param amount 缓冲区容量（元素个数）
 *
 * @note 必须在 .c 文件作用域使用，且同一翻译单元内重复使用 handle 会重定义
 * @note 句柄在 ek_static_alloc_init() 执行前不可使用
 */
#            define EK_DEFINE_RINGBUF(handle, type, amount)                                          \
                static union                                                                         \
                {                                                                                    \
                    ek_ringbuf_t rb;                                                                 \
                    type align_; /* 把 union 对齐抬到元素对齐 */                                     \
                    uint8_t storage[EK_RINGBUF_STATIC_BUF_SIZE(sizeof(type), (amount))];             \
                } handle##_mem;                                                                      \
                static ek_ringbuf_t *handle;                                                         \
                static int _static_alloc_init_##handle(void)                                         \
                {                                                                                    \
                    handle = ek_ringbuf_create_static(                                               \
                        handle##_mem.storage, sizeof(handle##_mem.storage), sizeof(type), (amount)); \
                    return handle ? 0 : -EINVAL;                                                     \
                }                                                                                    \
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
 * @return -ENOENT rb 无效或已销毁
 * @return -EINVAL item 为 NULL
 * @return -ENOSPC 缓冲区已满
 */
int ek_ringbuf_write(ek_ringbuf_t *rb, const void *item);

/**
 * @brief 从环形缓冲区读取一个元素
 * @param rb 环形缓冲区指针
 * @param item 存储读取结果的缓冲区指针，传入 NULL 则直接丢弃数据
 * @return 0 读取成功
 * @return -ENOENT rb 无效或已销毁
 * @return -ENODATA 缓冲区为空
 */
int ek_ringbuf_read(ek_ringbuf_t *rb, void *item);

/**
 * @brief 查看环形缓冲区首个元素（不移动读指针）
 * @param rb 环形缓冲区指针
 * @param item 存储查看结果的缓冲区指针
 * @return 0 查看成功
 * @return -ENOENT rb 无效或已销毁
 * @return -EINVAL item 为 NULL
 * @return -ENODATA 缓冲区为空
 */
int ek_ringbuf_peek(ek_ringbuf_t *rb, void *item);
#    endif /* EKCFG_RINGBUF */

#    if EKCFG_RINGBUF_SPSC == 1
/**
 * @brief 静态创建 SPSC 环形缓冲区所需的最小缓冲区字节数
 *
 * 缓冲区前部预留按 alignof(ek_ringbuf_spsc_t) 对齐的生命周期标记头，
 * 后接控制块与连续元素存储区。缓冲区起始地址必须按 alignof(ek_ringbuf_spsc_t) 对齐。
 */
#        define EK_RINGBUF_SPSC_STATIC_BUF_SIZE(item_size, amount)                                   \
            (EK_ALIGN_UP(sizeof(uint32_t), alignof(ek_ringbuf_spsc_t)) + sizeof(ek_ringbuf_spsc_t) + \
             (size_t)(item_size) * (amount))

/**
 * @brief 判断 SPSC 环形缓冲区是否已满
 * @param rb 环形缓冲区指针
 * @return true 已满
 * @return false 未满或句柄无效/已销毁
 */
bool ek_ringbuf_full_spsc(const ek_ringbuf_spsc_t *rb);

/**
 * @brief 判断 SPSC 环形缓冲区是否为空
 * @param rb 环形缓冲区指针
 * @return true 为空
 * @return false 不为空或句柄无效/已销毁
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
 * @brief 在调用者提供的缓冲区上静态创建 SPSC 环形缓冲区
 * @param buf       缓冲区起始地址，必须按 alignof(ek_ringbuf_spsc_t) 对齐
 * @param size      缓冲区字节数，不得小于 EK_RINGBUF_SPSC_STATIC_BUF_SIZE(item_size, amount)
 * @param item_size 单个元素大小（字节，须大于 0）
 * @param amount    底层槽位数量（须大于 1，实际最大可存元素数为 amount - 1）
 * @return 成功返回缓冲区指针，失败返回 NULL
 *
 * @note 缓冲区由调用者持有；destroy 只清除生命周期标记，不会释放缓冲区
 */
ek_ringbuf_spsc_t *ek_ringbuf_create_spsc_static(void *buf, size_t size, size_t item_size, uint32_t amount);

/**
 * @brief 销毁 SPSC 环形缓冲区
 * @param rb 要销毁的环形缓冲区
 *
 * @note 动态对象释放堆内存，静态对象清除生命周期标记
 */
void ek_ringbuf_destroy_spsc(ek_ringbuf_spsc_t *rb);

#        if EKCFG_STATIC_ALLOC == 1
#            include "ek/mem/static_alloc.h"

/**
 * @brief 静态声明一个 SPSC 环形缓冲区并自动注册初始化
 * @param handle 缓冲区句柄变量名（static ek_ringbuf_spsc_t *）
 * @param type   元素数据类型
 * @param amount 底层槽位数量（须大于 1，实际最大可存元素数为 amount - 1）
 *
 * @note 必须在 .c 文件作用域使用，且同一翻译单元内重复使用 handle 会重定义
 * @note 句柄在 ek_static_alloc_init() 执行前不可使用
 */
#            define EK_DEFINE_RINGBUF_SPSC(handle, type, amount)                                     \
                static union                                                                         \
                {                                                                                    \
                    ek_ringbuf_spsc_t rb;                                                            \
                    type align_; /* 把 union 对齐抬到元素对齐 */                                     \
                    uint8_t storage[EK_RINGBUF_SPSC_STATIC_BUF_SIZE(sizeof(type), (amount))];        \
                } handle##_mem;                                                                      \
                static ek_ringbuf_spsc_t *handle;                                                    \
                static int _static_alloc_init_##handle(void)                                         \
                {                                                                                    \
                    handle = ek_ringbuf_create_spsc_static(                                          \
                        handle##_mem.storage, sizeof(handle##_mem.storage), sizeof(type), (amount)); \
                    return handle ? 0 : -EINVAL;                                                     \
                }                                                                                    \
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
 * @return -ENOENT rb 无效或已销毁
 * @return -EINVAL item 为 NULL
 * @return -ENOSPC 缓冲区已满
 */
int ek_ringbuf_write_spsc(ek_ringbuf_spsc_t *rb, const void *item);

/**
 * @brief 从 SPSC 环形缓冲区读取一个元素
 * @param rb 环形缓冲区指针
 * @param item 存储读取结果的缓冲区指针，传入 NULL 则直接丢弃数据
 * @return 0 读取成功
 * @return -ENOENT rb 无效或已销毁
 * @return -ENODATA 缓冲区为空
 */
int ek_ringbuf_read_spsc(ek_ringbuf_spsc_t *rb, void *item);

/**
 * @brief 查看 SPSC 环形缓冲区首个元素（不移动读指针）
 * @param rb 环形缓冲区指针
 * @param item 存储查看结果的缓冲区指针
 * @return 0 查看成功
 * @return -ENOENT rb 无效或已销毁
 * @return -EINVAL item 为 NULL
 * @return -ENODATA 缓冲区为空
 */
int ek_ringbuf_peek_spsc(ek_ringbuf_spsc_t *rb, void *item);
#    endif /* EKCFG_RINGBUF_SPSC */

#    ifdef __cplusplus
}
#    endif /* __cplusplus */

#endif /* EKCFG_RINGBUF || EKCFG_RINGBUF_SPSC */

#endif /* EK_RINGBUF_H */
