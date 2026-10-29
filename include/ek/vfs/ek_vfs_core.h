#ifndef EK_VFS_CORE_H
#define EK_VFS_CORE_H

#include "ek/ek_conf_internal.h"
#include "ek_vfs_type.h"

void ek_vfs_init(void);
int ek_vfs_node_avail(void);

int ek_vfs_register(const char *path, uint8_t type, ek_vfs_ops_t *ops, void *priv);
int ek_vfs_unregister(const char *path);

ek_vfs_node_t *ek_vfs_look_up(const char *path, const char **remain_path);

#endif // EK_VFS_CORE_H
