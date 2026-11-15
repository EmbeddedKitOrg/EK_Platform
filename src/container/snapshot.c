/**
 * @file snapshot.c
 * @brief 快照数据管理实现
 * @author N1netyNine99
 */

#include "ek/container/snapshot.h"

#if EKCFG_SNAPSHOT == 1

#    include "ek/mem/heap.h"

int ek_snapshot_init_static(ek_snapshot_t *snapshot, size_t data_size)
{
    if (!snapshot || !data_size) return -EINVAL;

    snapshot->data_size = data_size;
    snapshot->unique = 0;
    memset(snapshot->data, 0, data_size);

    return 0;
}

void ek_snapshot_deinit_static(ek_snapshot_t *snapshot)
{
    if (!snapshot) return;

    memset(snapshot->data, 0, snapshot->data_size);
    snapshot->data_size = 0;
    snapshot->unique = 0;
}

ek_snapshot_t *ek_snapshot_create(size_t data_size)
{
    if (data_size == 0U) return NULL;

    ek_snapshot_t *snapshot = ek_malloc(sizeof(*snapshot) + data_size);
    if (!snapshot) return NULL;

    snapshot->unique = 0;
    snapshot->data_size = data_size;
    memset(snapshot->data, 0, data_size);

    return snapshot;
}

void ek_snapshot_destroy(ek_snapshot_t *snapshot)
{
    if (snapshot == NULL) return;

    ek_free(snapshot);
}

int ek_snapshot_set(ek_snapshot_t *snapshot, void *data, uint32_t unique)
{
    if (snapshot == NULL || data == NULL) return -EINVAL;
    if (unique == snapshot->unique) return -EINVAL;

    memcpy(snapshot->data, data, snapshot->data_size);

    snapshot->unique = unique;

    return 0;
}

int ek_snapshot_get(ek_snapshot_t *snapshot, void *data)
{
    if (snapshot == NULL || data == NULL) return -EINVAL;

    if (!snapshot->unique)
    {
        return -ENODATA;
    }

    memcpy(data, snapshot->data, snapshot->data_size);

    return 0;
}

uint32_t ek_snapshot_get_unique(ek_snapshot_t *snapshot)
{
    if (snapshot == NULL) return 0U;
    return snapshot->unique;
}

#endif // EKCFG_SNAPSHOT == 1
