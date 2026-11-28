/**
 * @file stack.h
 * @brief 通用栈数据结构
 * @author N1netyNine99
 *
 * 本模块实现了一个类型通用的栈（后进先出，LIFO）数据结构。
 * 支持任意数据类型的存储，通过指定元素大小实现类型无关的通用存储。
 *
 * @note 支持动态创建（ek_stack_create() / ek_stack_destroy()）
 *       与静态创建（ek_stack_create_static() / EK_DEFINE_STACK）。
 *
 * @warning 所有操作函数均会对传入参数做检查，
 *          传入 NULL 或已销毁句柄会返回对应错误码（或直接返回），不触发断言。
 */

#ifndef EK_STACK_H
#define EK_STACK_H

#include "ek/conf_internal.h"

#if EKCFG_STACK == 1

#    include "ek/core/def.h"

/**
 * @brief 栈结构体
 *
 * 封装了栈的基本属性，包括栈顶指针、元素大小和容量。
 */
typedef struct ek_stack ek_stack_t;

struct ek_stack
{
    bool mem_owned; /**< true 表示由堆分配，destroy 时释放；false 表示位于调用者缓冲区 */
    uint32_t sp; /**< 栈顶指针（stack pointer），指向下一个写入位置 */
    size_t item_size; /**< 单个元素的大小（字节） */
    uint32_t cap; /**< 栈的最大容量 */
    uint8_t data[]; /**< 柔性数据区（动态分配位于堆块尾部，静态分配位于调用者缓冲区尾部） */
};

/**
 * @brief 静态创建栈所需的最小缓冲区字节数
 *
 * 缓冲区前部预留按 alignof(ek_stack_t) 对齐的生命周期标记头，
 * 后接控制块与连续元素存储区。缓冲区起始地址必须按 alignof(ek_stack_t) 对齐。
 */
#    define EK_STACK_STATIC_BUF_SIZE(item_size, item_amount) \
        (EK_ALIGN_UP(sizeof(uint32_t), alignof(ek_stack_t)) + sizeof(ek_stack_t) + (size_t)(item_size) * (item_amount))

#    ifdef __cplusplus
extern "C"
{
#    endif /* __cplusplus */

/**
 * @brief 检查栈是否已满
 *
 * @param sk 栈指针
 * @return true 栈已满
 * @return false 栈未满或句柄无效/已销毁
 *
 * @note 复杂度：O(1)
 */
bool ek_stack_full(ek_stack_t *sk);

/**
 * @brief 检查栈是否为空
 *
 * @param sk 栈指针
 * @return true 栈为空
 * @return false 栈非空或句柄无效/已销毁
 *
 * @note 复杂度：O(1)
 */
bool ek_stack_empty(ek_stack_t *sk);

/**
 * @brief 创建一个新栈
 *
 * 分配内存并初始化一个栈实例。
 *
 * @param item_size 单个元素的大小（字节）
 * @param item_amount 栈的容量（最多可存储的元素数量）
 * @return 成功返回栈指针，失败返回 NULL
 *
 * @warning item_size 或 item_amount 为 0、总大小溢出或内存分配失败时返回 NULL
 */
ek_stack_t *ek_stack_create(size_t item_size, uint32_t item_amount);

/**
 * @brief 在调用者提供的缓冲区上静态创建栈
 * @param buf         缓冲区起始地址，必须按 alignof(ek_stack_t) 对齐
 * @param size        缓冲区字节数，不得小于 EK_STACK_STATIC_BUF_SIZE(item_size, item_amount)
 * @param item_size   单个元素的大小（字节，须大于 0）
 * @param item_amount 栈容量（元素个数，须大于 0）
 * @return 成功返回栈指针，失败返回 NULL
 *
 * @note 缓冲区由调用者持有；destroy 只清除生命周期标记，不会释放缓冲区
 */
ek_stack_t *ek_stack_create_static(void *buf, size_t size, size_t item_size, uint32_t item_amount);

/**
 * @brief 销毁栈
 *
 * @param sk 栈指针
 *
 * @note 动态对象释放堆内存，静态对象清除生命周期标记；配合 ek_stack_destroy_safely() 使用
 * @warning 传入 NULL 或已销毁句柄时直接返回
 */
void ek_stack_destroy(ek_stack_t *sk);

#    if EKCFG_STATIC_ALLOC == 1
#        include "ek/mem/static_alloc.h"

/**
 * @brief 静态声明一个栈并自动注册初始化
 * @param handle 栈句柄变量名（static ek_stack_t *）
 * @param type   元素数据类型
 * @param amount 栈容量（元素个数）
 *
 * @note 必须在 .c 文件作用域使用，且同一翻译单元内重复使用 handle 会重定义
 * @note 句柄在 ek_static_alloc_init() 执行前不可使用
 */
#        define EK_DEFINE_STACK(handle, type, amount)                                            \
            static union                                                                         \
            {                                                                                    \
                ek_stack_t sk;                                                                   \
                type align_; /* 把 union 对齐抬到元素对齐 */                                     \
                uint8_t storage[EK_STACK_STATIC_BUF_SIZE(sizeof(type), (amount))];               \
            } handle##_mem;                                                                      \
            static ek_stack_t *handle;                                                           \
            static int _static_alloc_init_##handle(void)                                         \
            {                                                                                    \
                handle = ek_stack_create_static(                                                 \
                    handle##_mem.storage, sizeof(handle##_mem.storage), sizeof(type), (amount)); \
                return handle ? 0 : -EINVAL;                                                     \
            }                                                                                    \
            EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_STACK, _static_alloc_init_##handle)
#    endif

/**
 * @brief 销毁栈并把 sk_ptr 设置为 NULL
 * @param sk_ptr 要销毁的栈指针
 */
#    define ek_stack_destroy_safely(sk_ptr) \
        do                                  \
        {                                   \
            ek_stack_destroy((sk_ptr));     \
            sk_ptr = NULL;                  \
        } while (0)

/**
 * @brief 将元素压入栈
 *
 * 将一个元素复制到栈顶。
 *
 * @param sk 栈指针
 * @param item 指向要压入的元素的指针
 * @return 0 成功压入
 * @return -ENOENT sk 无效或已销毁
 * @return -EINVAL item 为 NULL
 * @return -ENOSPC 栈已满
 *
 * @note 复杂度：O(1)
 * @note 会按 item_size 字节数复制数据
 */
int ek_stack_push(ek_stack_t *sk, const void *item);

/**
 * @brief 从栈顶弹出一个元素
 *
 * 将栈顶元素复制到指定位置，并从栈中移除。
 *
 * @param sk 栈指针
 * @param item 指向接收弹出元素的缓冲区的指针
 * @return 0 成功弹出
 * @return -ENOENT sk 无效或已销毁
 * @return -EINVAL item 为 NULL
 * @return -ENODATA 栈为空
 *
 * @note 复杂度：O(1)
 * @note 会按 item_size 字节数复制数据
 */
int ek_stack_pop(ek_stack_t *sk, void *item);

#    ifdef __cplusplus
}
#    endif /* __cplusplus */

#endif /* EKCFG_STACK */

#endif /* EK_STACK_H */
