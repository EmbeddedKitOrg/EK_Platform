#ifndef EK_VFS_TYPE_H
#define EK_VFS_TYPE_H

#include "ek/ek_conf_internal.h"
#include "ek/core/ek_def.h"

#if defined(__has_include)
#    if __has_include(<fcntl.h>)
#        include <fcntl.h>
#    endif
#endif

/* ---------------- 基础读写权限（互斥） ---------------- */
#ifndef O_RDONLY
#    define O_RDONLY 0x0000
#endif
#ifndef O_WRONLY
#    define O_WRONLY 0x0001
#endif
#ifndef O_RDWR
#    define O_RDWR 0x0002
#endif
#ifndef O_ACCMODE
#    define O_ACCMODE 0x0003
#endif

/* ---------------- 运行状态标志（按位或） ---------------- */
#ifndef O_NONBLOCK
#    define O_NONBLOCK 0x0004 /* 裸机外设高频使用：非阻塞模式 */
#endif
#ifndef O_APPEND
#    define O_APPEND 0x0008 /* 尾部追加模式 */
#endif
#ifndef O_SYNC
#    define O_SYNC 0x0010 /* 同步 I/O */
#endif

/* ---------------- 存储文件生命周期标志 ---------------- */
#ifndef O_CREAT
#    define O_CREAT 0x0100 /* 不存在则新建 */
#endif
#ifndef O_TRUNC
#    define O_TRUNC 0x0200 /* 打开时清空内容 */
#endif
#ifndef O_EXCL
#    define O_EXCL 0x0400 /* 排他性检测 */
#endif

typedef struct ek_vfs_ops ek_vfs_ops_t;
typedef struct ek_vfs_node ek_vfs_node_t;

enum ek_vfs_type
{
    EK_VFS_DIR,
    EK_VFS_DEV,
    EK_VFS_MOUNT,

    EK_VFS_MAX
};

struct ek_vfs_ops
{
    int (*open)(void *node_priv, int flags, void **file_priv);
    ssize_t (*read)(void *file_priv, void *buf, size_t count);
    ssize_t (*write)(void *file_priv, const void *buf, size_t count);
    off_t (*lseek)(void *file_priv, off_t offset, int whence);
    int (*ioctl)(void *file_priv, uint32_t request, void *arg);
    int (*poll)(void *file_priv, uint32_t events);
    int (*close)(void *file_priv);
};

struct ek_vfs_node
{
    struct ek_vfs_node *parent;
    struct ek_vfs_node *child;
    struct ek_vfs_node *sibling;
    const ek_vfs_ops_t *ops;
    void *priv;
    union
    {
        uint32_t attr;
        struct
        {
            uint8_t type;
            uint8_t flags;
            uint8_t ref_count;
            uint8_t name_len; // 不包含结尾的 '\0'
        };
    };
    char name[EKCFG_VFS_NODE_NAME_LEN]; // 以 '\0' 结尾
};

#endif // EK_VFS_TYPE_H
