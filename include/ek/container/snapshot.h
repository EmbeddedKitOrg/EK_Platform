/**
 * @file snapshot.h
 * @brief 快照数据管理
 * @author N1netyNine99
 *
 * 提供单槽快照（snapshot）数据结构：生产者以唯一 id 覆盖写入，
 * 消费者随时读取最新一份完整数据。
 * - 唯一 id（unique）为 0 表示无数据，写入时 id 不得与当前值相同
 * - 支持动态创建（控制块与数据单次分配）与静态初始化（EKCFG_STATIC_ALLOC）
 *
 * @note 动态使用前需调用 ek_snapshot_create() 创建，使用完毕后需调用 ek_snapshot_destroy() 释放
 */

#ifndef EK_SNAPSHOT_H
#define EK_SNAPSHOT_H

#include "ek/conf_internal.h"

#if EKCFG_SNAPSHOT == 1

#    include "ek/core/def.h"

/**
 * @brief 快照结构体前向声明
 */
typedef struct ek_snapshot ek_snapshot_t;

/**
 * @brief 快照结构体
 *
 * 保存最新一份完整数据副本，写入与读取按 data_size 整块拷贝。
 * 唯一 id（unique）用于区分快照新旧，0 表示无有效数据。
 */
struct ek_snapshot
{
    bool mem_owned; /**< true 表示由堆分配，destroy 时释放；false 表示位于调用者缓冲区 */
    uint32_t unique; /**< 当前快照的唯一 id，0 表示无数据 */
    size_t data_size; /**< 快照数据大小（字节） */
    uint8_t data[]; /**< 柔性数据区（动态分配位于堆块尾部，静态分配位于调用者缓冲区尾部） */
};

/**
 * @brief 静态创建快照所需的最小缓冲区字节数
 *
 * 缓冲区前部预留按 alignof(ek_snapshot_t) 对齐的生命周期标记头，
 * 后接控制块与快照数据区。缓冲区起始地址必须按 alignof(ek_snapshot_t) 对齐。
 */
#    define EK_SNAPSHOT_STATIC_BUF_SIZE(data_size) \
        (EK_ALIGN_UP(sizeof(uint32_t), alignof(ek_snapshot_t)) + sizeof(ek_snapshot_t) + (size_t)(data_size))

#    ifdef __cplusplus
extern "C"
{
#    endif /* __cplusplus */

/**
 * @brief 在调用者提供的缓冲区上静态创建快照
 * @param buf       缓冲区起始地址，必须按 alignof(ek_snapshot_t) 对齐
 * @param size      缓冲区字节数，不得小于 EK_SNAPSHOT_STATIC_BUF_SIZE(data_size)
 * @param data_size 快照数据大小（字节，须大于 0）
 * @return 成功返回快照对象指针，失败返回 NULL
 *
 * @note 缓冲区由调用者持有；destroy 只清除生命周期标记，不会释放缓冲区
 */
ek_snapshot_t *ek_snapshot_create_static(void *buf, size_t size, size_t data_size);

#    if EKCFG_STATIC_ALLOC == 1
#        include "ek/mem/static_alloc.h"

/**
 * @brief 定义静态初始化的快照对象
 * @param handle 快照对象名（static ek_snapshot_t *）
 * @param size 数据存储大小（字节）
 *
 * 展开 union 容器（快照对象与数据连续）与自动初始化函数，并注册到静态初始化机制
 */
#        define EK_SNAPSHOT_DEFINE(handle, size)                                                                \
            static union                                                                                        \
            {                                                                                                   \
                ek_snapshot_t snapshot;                                                                         \
                uint8_t storage[EK_SNAPSHOT_STATIC_BUF_SIZE(size)];                                             \
            } handle##_mem;                                                                                     \
            static ek_snapshot_t *handle;                                                                       \
            static int _static_alloc_init_##handle(void)                                                        \
            {                                                                                                   \
                handle = ek_snapshot_create_static(handle##_mem.storage, sizeof(handle##_mem.storage), (size)); \
                return handle ? 0 : -EINVAL;                                                                    \
            }                                                                                                   \
            EK_STATIC_ALLOC_REGISTER(handle, EK_STATIC_ALLOC_ORDER_SNAPSHOT, _static_alloc_init_##handle)

#    endif // EKCFG_STATIC_ALLOC == 1

/**
 * @brief 创建快照
 * @param data_size 快照数据大小（字节）
 * @return 成功返回快照对象指针，失败返回 NULL
 * @note 控制块与数据区单次分配，unique 初始为 0（无数据）
 * @note 使用完毕后需调用 ek_snapshot_destroy() 释放
 */
ek_snapshot_t *ek_snapshot_create(size_t data_size);

/**
 * @brief 销毁快照
 * @param snapshot 快照对象指针
 *
 * @note 动态对象释放堆内存，静态对象清除生命周期标记
 */
void ek_snapshot_destroy(ek_snapshot_t *snapshot);

/**
 * @brief 销毁快照并把 snapshot_ptr 设置为 NULL
 * @param snapshot_ptr 要销毁的快照指针
 */
#    define ek_snapshot_destroy_safely(snapshot_ptr) \
        do                                           \
        {                                            \
            ek_snapshot_destroy((snapshot_ptr));     \
            snapshot_ptr = NULL;                     \
        } while (0)

/**
 * @brief 写入快照
 * @param snapshot 快照对象指针
 * @param data 数据源指针，按 data_size 整块拷贝
 * @param unique 本次快照的唯一 id（不得与当前 id 相同；传入 0 会使快照重新变为无数据状态）
 * @return 0 成功
 * @return -ENOENT snapshot 无效或已销毁
 * @return -EINVAL data 为 NULL，或 unique 与当前 id 相同
 */
int ek_snapshot_set(ek_snapshot_t *snapshot, void *data, uint32_t unique);

/**
 * @brief 读取快照
 * @param snapshot 快照对象指针
 * @param data 接收缓冲区指针，拷出 data_size 字节
 * @return 0 成功
 * @return -ENOENT snapshot 无效或已销毁
 * @return -EINVAL data 为 NULL
 * @return -ENODATA 快照为空（尚未写入或已被清空）
 */
int ek_snapshot_get(ek_snapshot_t *snapshot, void *data);

/**
 * @brief 获取当前快照的唯一 id
 * @param snapshot 快照对象指针
 * @return 当前唯一 id，0 表示无数据或句柄无效/已销毁
 */
uint32_t ek_snapshot_get_unique(ek_snapshot_t *snapshot);

#    ifdef __cplusplus
}
#    endif /* __cplusplus */

#endif // EKCFG_SNAPSHOT == 1

#endif // EK_SNAPSHOT_H
