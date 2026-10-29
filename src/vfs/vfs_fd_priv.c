/**
 * @file vfs_fd_priv.c
 * @brief VFS 文件描述符池实现
 * @author N1netyNine99
 */

#include "vfs_priv.h"
#include "ek_conf_internal.h"

#define FD_VALID_MASK ((EKCFG_VFS_MAX_FDS == 32) ? 0xFFFFFFFFUL : ((1UL << EKCFG_VFS_MAX_FDS) - 1UL))

static vfs_file_t s_fd_pool[EKCFG_VFS_MAX_FDS];
static uint32_t s_bitmap;

int fd_alloc(void)
{
    uint32_t free_mask = (~s_bitmap) & FD_VALID_MASK;

    if (free_mask == 0) return -ENFILE;

    int fd = __builtin_ctz(free_mask);
    s_bitmap |= (1UL << fd);
    memset(&s_fd_pool[fd], 0, sizeof(vfs_file_t));

    return fd;
}

void fd_free(int fd)
{
    if (fd < 3 || fd >= EKCFG_VFS_MAX_FDS) return;

    s_bitmap &= ~(1UL << fd);
    memset(&s_fd_pool[fd], 0, sizeof(vfs_file_t));
}

vfs_file_t *fd_to_file(int fd)
{
    if (fd < 0 || fd >= EKCFG_VFS_MAX_FDS) return NULL;
    if ((s_bitmap & (1UL << fd)) == 0) return NULL;

    return &s_fd_pool[fd];
}
