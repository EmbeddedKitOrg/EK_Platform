/**
 * @file ek_snapshot.h
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

#include "ek/ek_conf_internal.h"

#if EKCFG_SNAPSHOT == 1

#    include "ek/core/ek_def.h"

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
#    if EKCFG_RTOS == 1
    EK_LOCK_TYPE lock; /**< 互斥锁（仅 RTOS 模式） */
#    endif /* EKCFG_RTOS */

    uint32_t unique; /**< 当前快照的唯一 id，0 表示无数据 */
    size_t data_size; /**< 快照数据大小（字节） */
    uint8_t data[]; /**< 唯一数据位置：动态=malloc 尾部，静态=union 尾部 */
};

#    ifdef __cplusplus
extern "C"
{
#    endif /* __cplusplus */

/**
 * @brief 静态初始化快照
 * @param snapshot 快照对象指针
 * @param data_size 快照数据大小（字节）
 * @return -EINVAL 参数为空或 data_size 为 0
 */
int ek_snapshot_init_static(ek_snapshot_t *snapshot, size_t data_size);

/**
 * @brief 反初始化静态快照，清空数据并复位配置
 * @param snapshot 快照对象指针
 */
void ek_snapshot_deinit_static(ek_snapshot_t *snapshot);

#    if EKCFG_STATIC_ALLOC == 1
#        include "ek/mem/ek_static_alloc.h"

/**
 * @brief 定义静态初始化的快照对象
 * @param handle 快照对象名
 * @param size 数据存储大小（字节）
 *
 * 展开 union 容器（快照对象与数据连续）与自动初始化函数，并注册到静态初始化机制
 */
#        define EK_SNAPSHOT_DEFINE(handle, size)                         \
            static union                                                 \
            {                                                            \
                ek_snapshot_t snapshot;                                  \
                uint8_t storage[sizeof(ek_snapshot_t) + (size)];         \
            } handle##_mem;                                              \
            static ek_snapshot_t *const handle = &handle##_mem.snapshot; \
            static int _static_alloc_init_##handle(void)                 \
            {                                                            \
                return ek_snapshot_init_static(handle, (size));          \
            }                                                            \
            EK_STATIC_ALLOC_REGISTER(handle, 10, _static_alloc_init_##handle)

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
 * @brief 销毁快照并释放内存
 * @param snapshot 快照对象指针
 */
void ek_snapshot_destroy(ek_snapshot_t *snapshot);

/**
 * @brief 销毁快照并把snapshot_ptr设置为NULL
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
 * @param unique 本次快照的唯一 id，不得为 0 或与当前 id 相同
 * @return 0 成功
 * @return -EINVAL unique 与当前 id 相同
 * @return -EBUSY 资源忙（RTOS 模式）
 */
int ek_snapshot_set(ek_snapshot_t *snapshot, void *data, uint32_t unique);

/**
 * @brief 读取快照
 * @param snapshot 快照对象指针
 * @param data 接收缓冲区指针，拷出 data_size 字节
 * @return 0 成功
 * @return -ENODATA 快照为空（尚未写入或已被清空）
 * @return -EBUSY 资源忙（RTOS 模式）
 */
int ek_snapshot_get(ek_snapshot_t *snapshot, void *data);

/**
 * @brief 获取当前快照的唯一 id
 * @param snapshot 快照对象指针
 * @return 当前唯一 id，0 表示无数据
 */
uint32_t ek_snapshot_get_unique(ek_snapshot_t *snapshot);

#    ifdef __cplusplus
}
#    endif /* __cplusplus */

#endif // EKCFG_SNAPSHOT == 1

#endif // EK_SNAPSHOT_H
