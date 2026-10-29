/**
 * @file ek_vfs_core.c
 * @brief VFS 核心实现：节点注册、路径查找、文件操作与树输出
 * @author N1netyNine99
 */

#include "ek/ek_conf_internal.h"
#if EKCFG_VFS == 1

#    include "ek/vfs/ek_vfs_core.h"
#    include "ek/core/ek_io.h"
#    include "ek/core/ek_export.h"
#    include "vfs_priv.h"

#    if EKCFG_VFS_TREE_DUMP_UTF8
#        define TREE_BRANCH "├── "
#        define TREE_CORNER "└── "
#        define TREE_TRUNK  "│   "
#        define TREE_SPACE  "    "
#    else
#        define TREE_BRANCH "|-- "
#        define TREE_CORNER "`-- "
#        define TREE_TRUNK  "|   "
#        define TREE_SPACE  "    "
#    endif

void ek_vfs_init(void)
{
    return root_init();
}

EK_EXPORT_COMPONENTS(ek_vfs_init, 5);

int ek_vfs_node_avail(void)
{
    if (!get_root()) return -EINVAL;
    return EKCFG_VFS_MAX_NODES - get_alloced_node();
}

int ek_vfs_register(const char *path, uint8_t type, ek_vfs_ops_t *ops, void *priv)
{
    if (!get_root()) return -EINVAL;
    if (get_alloced_node() >= EKCFG_VFS_MAX_NODES) return -ENOMEM;
    if (!path || path[0] != '/') return -EINVAL;
    if (type >= EK_VFS_MAX) return -EINVAL;
    if (type == EK_VFS_DEV && !ops) return -EINVAL;

    vfs_node_t *parent_node = get_root();
    const char *cursor = path;
    const char *token = NULL;
    uint16_t token_len = 0;
    bool is_leaf = false;

    while ((token = next_token(&cursor, &token_len, &is_leaf)))
    {
        if (token_len >= EKCFG_VFS_NODE_NAME_LEN) token_len = EKCFG_VFS_NODE_NAME_LEN - 1;
        vfs_node_t *target = find_child(parent_node, token, token_len);

        if (target)
        {
            if (is_leaf) return -EEXIST;
            if (target->type != EK_VFS_DIR) return -ENOTDIR;
            parent_node = target;
            continue;
        }

        target = alloc_node();
        if (!target) return -ENOMEM;

        target->parent = parent_node;
        target->child = NULL;
        target->sibling = NULL;
        target->attr = 0;

        memcpy(target->name, token, token_len);
        target->name[token_len] = '\0';
        target->name_len = token_len;

        target->ops = is_leaf ? ops : NULL;
        target->priv = is_leaf ? priv : NULL;
        target->type = is_leaf ? type : EK_VFS_DIR;

        attach_child(parent_node, target);
        parent_node = target;
    }

    return 0;
}

int ek_vfs_unregister(const char *path)
{
    if (!get_root()) return -EINVAL;
    if (!path || path[0] != '/') return -EINVAL;
    if (!strcmp(path, "/")) return -EPERM;

    const char *remain_path = NULL;
    vfs_node_t *target = look_up(path, &remain_path);

    if (!target || (remain_path && *remain_path != '\0')) return -ENOENT;
    if (target->ref_count) return -EBUSY;
    if (target->type == EK_VFS_DIR && target->child) return -ENOTEMPTY;

    detach_child(target);
    if (target->ops && target->ops->close) target->ops->close(target->priv);

    free_node(target);

    return 0;
}

static inline void _safe_advance(size_t *offset, size_t buf_size, int written)
{
    if (written > 0)
    {
        *offset += (size_t)written;
        if (*offset >= buf_size)
        {
            *offset = buf_size - 1;
        }
    }
}

int ek_vfs_tree_dump(char *buf, size_t size)
{
    if (!buf || !size) return -EINVAL;
    if (!get_root())
    {
        int len = ek_snprintf(buf, size, "<vfs is not initialized yet.>" CRLF);
        return (len && (size_t)len < size) ? len : (int)(size - 1);
    }

    size_t offset = 0;
    int depth = 0;
    vfs_node_t *curr = get_root();
    uint32_t active_mask = 0;

    while (curr)
    {
        if (offset >= size - 1) break;

        if (curr == get_root())
        {
            int n = ek_snprintf(buf + offset, size - offset, "/" CRLF);
            _safe_advance(&offset, size, n);
        }
        else
        {
            for (uint8_t i = 1; i < depth; i++)
            {
                if (offset >= size - 1) break;
                const char *trunk = (active_mask & (1UL << i)) ? TREE_TRUNK : TREE_SPACE;
                int n = ek_snprintf(buf + offset, size - offset, "%s", trunk);
                _safe_advance(&offset, size, n);
            }
        }

        if (curr != get_root() && offset < size - 1)
        {
            bool is_last = (curr->sibling == NULL);
            if (is_last)
            {
                if (depth < 32) active_mask &= ~(1UL << depth);
            }
            else
            {
                if (depth < 32) active_mask |= (1UL << depth);
            }

            const char *branch = is_last ? TREE_CORNER : TREE_BRANCH;
            const char *slash = (curr->type == EK_VFS_DIR) ? "/" : "";

            int n = ek_snprintf(buf + offset, size - offset, "%s%s%s" CRLF, branch, curr->name, slash);
            _safe_advance(&offset, size, n);
        }

        // 深度优先搜索
        if (curr->child)
        {
            curr = curr->child;
            depth++;
            continue;
        }
        else if (curr->sibling)
        {
            curr = curr->sibling;
            continue;
        }

        while (curr && curr != get_root() && curr->sibling == NULL)
        {
            curr = curr->parent;
            if (depth > 0) depth--;
        }
        if (!curr || curr == get_root()) break;
        curr = curr->sibling;
    }

    buf[offset] = '\0';
    return (int)offset;
}

int ek_vfs_open(const char *path, int flags, ...)
{
    mode_t mode = 0;

    if (!path || path[0] != '/') return -EINVAL;

    int acc = flags & O_ACCMODE;
    if (acc == O_ACCMODE) return -EINVAL;
    if ((flags & O_EXCL) && !(flags & O_CREAT)) return -EINVAL;

    if (flags & O_CREAT)
    {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, mode_t);
        va_end(ap);
        __EK_UNUSED(mode);
    }

    const char *remain_path = NULL;
    vfs_node_t *node = look_up(path, &remain_path);

    if (!node)
    {
        if (!(flags & O_CREAT)) return -ENOENT;
        // TODO: 调用文件系统 create；当前没有 create 回调，只能 -ENOTSUP
        return -ENOTSUP;
    }

    if (remain_path && *remain_path != '\0')
    {
        // TODO: 挂载点，把 remain_path 交给挂载的文件系统
        return -ENOTSUP;
    }

    if (node->type == EK_VFS_DIR && acc != O_RDONLY) return -EISDIR;
    if ((flags & O_CREAT) && (flags & O_EXCL)) return -EEXIST;
    if ((flags & O_TRUNC) && acc == O_RDONLY) return -EACCES;

    int fd = fd_alloc();
    if (fd < 0) return fd;

    void *file_priv = node->priv;
    if (node->ops && node->ops->open)
    {
        int ret = node->ops->open(node->priv, flags, &file_priv);
        if (ret < 0)
        {
            fd_free(fd);
            return ret;
        }
    }

    vfs_file_t *file = fd_to_file(fd);
    if (!file)
    {
        if (node->ops && node->ops->close) node->ops->close(file_priv);
        fd_free(fd);
        return -EBADF;
    }

    file->node = node;
    file->ops = node->ops;
    file->flags = flags;
    file->file_priv = file_priv;
    file->offset = 0;
    node->ref_count++;

    return fd;
}

ssize_t ek_vfs_read(int fd, void *buf, size_t size)
{
    vfs_file_t *file = fd_to_file(fd);
    if (file == NULL) return -EBADF;

    if ((file->flags & O_ACCMODE) == O_WRONLY) return -EBADF;
    if (file->node->type == EK_VFS_DIR) return -EISDIR;
    if (!file->ops || !file->ops->read) return -ENOTSUP;

    return file->ops->read(file->file_priv, buf, size);
}

ssize_t ek_vfs_write(int fd, const void *buf, size_t size)
{
    vfs_file_t *file = fd_to_file(fd);
    if (file == NULL) return -EBADF;

    if ((file->flags & O_ACCMODE) == O_RDONLY) return -EBADF;
    if (file->node->type == EK_VFS_DIR) return -EISDIR;
    if (!file->ops || !file->ops->write) return -ENOTSUP;

    return file->ops->write(file->file_priv, buf, size);
}

off_t ek_vfs_lseek(int fd, off_t offset, int whence)
{
    vfs_file_t *file = fd_to_file(fd);
    if (file == NULL) return -EBADF;
    if (!file->ops || !file->ops->lseek) return -ESPIPE;

    off_t ret = file->ops->lseek(file->file_priv, offset, whence);
    if (ret >= 0) file->offset = ret;
    return ret;
}

int ek_vfs_ioctl(int fd, uint32_t request, void *arg)
{
    vfs_file_t *file = fd_to_file(fd);
    if (file == NULL) return -EBADF;
    if (!file->ops || !file->ops->lseek) return -ENOTTY;

    return file->ops->ioctl(file->file_priv, request, arg);
}

int poll(int fd, uint32_t events)
{
    vfs_file_t *file = fd_to_file(fd);
    if (file == NULL) return -EBADF;
    if (events == 0) return -EINVAL;
    if (file->ops == NULL || file->ops->poll == NULL) return -ENOTSUP;

    return file->ops->poll(file->file_priv, events);
}

int close(int fd)
{
    vfs_file_t *file = fd_to_file(fd);
    if (file == NULL) return -EBADF;

    vfs_node_t *node = file->node;
    int ret = 0;

    if (file->ops && file->ops->close) ret = file->ops->close(file->file_priv);
    if (node && node->ref_count > 0) node->ref_count--;

    fd_free(fd);

    return ret;
}
#endif // EKCFG_VFS == 1
