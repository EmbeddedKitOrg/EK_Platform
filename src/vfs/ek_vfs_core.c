#include "ek/ek_conf_internal.h"
#include "ek/vfs/ek_vfs_core.h"
#include "ek/core/ek_export.h"
#include "ek/core/ek_io.h"
#include "ek_vfs_priv.h"

#if EKCFG_VFS_TREE_DUMP_UTF8
#    define TREE_BRANCH "├── "
#    define TREE_CORNER "└── "
#    define TREE_TRUNK  "│   "
#    define TREE_SPACE  "    "
#else
#    define TREE_BRANCH "|-- "
#    define TREE_CORNER "`-- "
#    define TREE_TRUNK  "|   "
#    define TREE_SPACE  "    "
#endif

static inline void _safe_advance(size_t *offset, size_t buf_size, int written);

static ek_vfs_node_t *s_root;

void ek_vfs_init(void)
{
    s_root = alloc_node();

    s_root->name[0] = '/';
    s_root->name[1] = '\0';

    s_root->attr = 0;
    s_root->type = EK_VFS_DIR;
    s_root->name_len = 1;

    s_root->parent = NULL;
    s_root->child = NULL;
    s_root->sibling = NULL;
    s_root->ops = NULL;
    s_root->priv = NULL;
}

EK_EXPORT_COMPONENTS(ek_vfs_init, 5);

int ek_vfs_node_avail(void)
{
    if (!s_root) return -EINVAL;
    return EKCFG_VFS_MAX_NODES - get_alloced_node();
}

int ek_vfs_register(const char *path, uint8_t type, ek_vfs_ops_t *ops, void *priv)
{
    if (!s_root) return -EINVAL;
    if (get_alloced_node() >= EKCFG_VFS_MAX_NODES) return -ENOMEM;
    if (!path || path[0] != '/') return -EINVAL;
    if (type >= EK_VFS_MAX) return -EINVAL;
    if (type == EK_VFS_DEV && !ops) return -EINVAL;

    ek_vfs_node_t *parent_node = s_root;
    const char *cursor = path;
    const char *token = NULL;
    uint16_t token_len = 0;
    bool is_leaf = false;

    while ((token = next_token(&cursor, &token_len, &is_leaf)))
    {
        if (token_len >= EKCFG_VFS_NODE_NAME_LEN) token_len = EKCFG_VFS_NODE_NAME_LEN - 1;
        ek_vfs_node_t *target = find_child(parent_node, token, token_len);

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
    if (!s_root) return -EINVAL;
    if (!path || path[0] != '/') return -EINVAL;
    if (!strcmp(path, "/")) return -EPERM;

    const char *remain_path = NULL;
    ek_vfs_node_t *target = ek_vfs_look_up(path, &remain_path);

    if (!target || (remain_path && *remain_path != '\0')) return -ENOENT;
    if (target->ref_count) return -EBUSY;
    if (target->type == EK_VFS_DIR && target->child) return -ENOTEMPTY;

    detach_child(target);
    if (target->ops && target->ops->close) target->ops->close(target->priv);

    free_node(target);

    return 0;
}

ek_vfs_node_t *ek_vfs_look_up(const char *path, const char **remain_path)
{
    const char *cursor = path;
    const char *token = NULL;
    uint16_t token_len = 0;

    ek_vfs_node_t *curr = s_root;

    while ((token = next_token(&cursor, &token_len, NULL)))
    {
        // 遇到外部的文件系统的挂载点不归 VFS 处理
        if (curr->type == EK_VFS_MOUNT)
        {
            if (remain_path) *remain_path = token;
            return curr;
        }

        // 只有当前节点的类型为 EK_VFS_DIR 时
        // 才可以继续向下遍历
        if (curr->type != EK_VFS_DIR) return NULL;

        // 在当前的层级里找到了对应的 token
        // 才继续向下遍历
        ek_vfs_node_t *child = find_child(curr, token, token_len);
        if (!child) return NULL;

        curr = child;
    }

    // 剩下路径都被消耗殆尽
    if (remain_path) *remain_path = "";

    return curr;
}

int ek_vfs_tree_dump(char *buf, size_t size)
{
    if (!buf || !size) return -EINVAL;
    if (!s_root)
    {
        int len = ek_snprintf(buf, size, "<vfs is not initialized yet.>" CRLF);
        return (len && (size_t)len < size) ? len : (int)(size - 1);
    }

    size_t offset = 0;
    int depth = 0;
    ek_vfs_node_t *curr = s_root;
    uint32_t active_mask = 0;

    while (curr)
    {
        if (offset >= size - 1) break;

        if (curr == s_root)
        {
            int n = ek_snprintf(buf + offset, size - offset, "/" CRLF);
            _safe_advance(&offset, size, n);
        }
        else
        {
            for (uint8_t i = 0; i < depth; i++)
            {
                if (offset >= size - 1) break;
                const char *trunk = (active_mask & (1UL << i)) ? TREE_TRUNK : TREE_SPACE;
                int n = ek_snprintf(buf + offset, size - offset, "%s", trunk);
                _safe_advance(&offset, size, n);
            }
        }

        if (offset < size - 1)
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

        while (curr && curr->sibling)
        {
            curr = curr->parent;
            if (depth > 0) depth--;
        }
        if (curr) curr = curr->sibling;
    }

    buf[offset] = '\0';
    return (int)offset;
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
