#ifndef EK_VFS_PRIV_H
#define EK_VFS_PRIV_H

#include "ek/vfs/ek_vfs_type.h"

ek_vfs_node_t *alloc_node(void);
void free_node(ek_vfs_node_t *node);

int get_alloced_node(void);

void attach_child(ek_vfs_node_t *parent, ek_vfs_node_t *node);
void detach_child(ek_vfs_node_t *node);

ek_vfs_node_t *find_child(ek_vfs_node_t *node, const char *token, uint16_t token_len);
const char *next_token(const char **cursor, uint16_t *token_len, bool *is_leaf);

#endif // EK_VFS_PRIV_H
