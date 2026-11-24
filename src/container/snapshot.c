/**
 * @file snapshot.c
 * @brief 快照数据管理实现
 * @author N1netyNine99
 */

#include "ek/container/snapshot.h"

#if EKCFG_SNAPSHOT == 1

#    include "ek/mem/heap.h"

#    define SNAPSHOT_MAGIC              0x534E4150UL
#    define SNAPSHOT_HDR                ((size_t)EK_ALIGN_UP(sizeof(uint32_t), alignof(ek_snapshot_t)))
#    define SNAPSHOT_FROM_BUF(buf)      ((ek_snapshot_t *)(uintptr_t)((uintptr_t)(buf) + SNAPSHOT_HDR))
#    define SNAPSHOT_GET_MAGIC(snap)    (*(const uint32_t *)(uintptr_t)((uintptr_t)(snap) - SNAPSHOT_HDR))
#    define SNAPSHOT_SET_MAGIC(snap, v) (*(uint32_t *)(uintptr_t)((uintptr_t)(snap) - SNAPSHOT_HDR) = (uint32_t)(v))

static inline bool _snapshot_valid(const ek_snapshot_t *snapshot)
{
    if (!snapshot) return false;
    if (!snapshot->mem_owned) return SNAPSHOT_GET_MAGIC(snapshot) == SNAPSHOT_MAGIC;
    return true;
}

ek_snapshot_t *ek_snapshot_create_static(void *buf, size_t size, size_t data_size)
{
    if (!buf || data_size == 0U) return NULL;
    if (((uintptr_t)buf % alignof(ek_snapshot_t)) != 0U) return NULL;
    if (data_size > SIZE_MAX - SNAPSHOT_HDR - sizeof(ek_snapshot_t)) return NULL;
    if (size < EK_SNAPSHOT_STATIC_BUF_SIZE(data_size)) return NULL;

    ek_snapshot_t *snapshot = SNAPSHOT_FROM_BUF(buf);
    snapshot->mem_owned = false;
    snapshot->data_size = data_size;
    snapshot->unique = 0U;
    memset(snapshot->data, 0, data_size);
    SNAPSHOT_SET_MAGIC(snapshot, SNAPSHOT_MAGIC);

    return snapshot;
}

ek_snapshot_t *ek_snapshot_create(size_t data_size)
{
    if (data_size == 0U || data_size > SIZE_MAX - sizeof(ek_snapshot_t)) return NULL;

    ek_snapshot_t *snapshot = ek_malloc(sizeof(*snapshot) + data_size);
    if (!snapshot) return NULL;

    snapshot->mem_owned = true;
    snapshot->unique = 0U;
    snapshot->data_size = data_size;
    memset(snapshot->data, 0, data_size);

    return snapshot;
}

void ek_snapshot_destroy(ek_snapshot_t *snapshot)
{
    if (!_snapshot_valid(snapshot)) return;

    if (snapshot->mem_owned)
    {
        ek_free(snapshot);
        return;
    }

    SNAPSHOT_SET_MAGIC(snapshot, 0U);
    memset(snapshot->data, 0, snapshot->data_size);
    snapshot->data_size = 0U;
    snapshot->unique = 0U;
}

int ek_snapshot_set(ek_snapshot_t *snapshot, void *data, uint32_t unique)
{
    if (!_snapshot_valid(snapshot)) return -ENOENT;
    if (data == NULL) return -EINVAL;
    if (unique == snapshot->unique) return -EINVAL;

    memcpy(snapshot->data, data, snapshot->data_size);

    snapshot->unique = unique;

    return 0;
}

int ek_snapshot_get(ek_snapshot_t *snapshot, void *data)
{
    if (!_snapshot_valid(snapshot)) return -ENOENT;
    if (data == NULL) return -EINVAL;

    if (!snapshot->unique)
    {
        return -ENODATA;
    }

    memcpy(data, snapshot->data, snapshot->data_size);

    return 0;
}

uint32_t ek_snapshot_get_unique(ek_snapshot_t *snapshot)
{
    if (!_snapshot_valid(snapshot)) return 0U;
    return snapshot->unique;
}

#endif // EKCFG_SNAPSHOT == 1
