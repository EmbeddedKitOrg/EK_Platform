/**
 * @file vfs_node_priv.c
 * @brief VFS 节点池、树结构与路径解析实现
 * @author N1netyNine99
 */

#include "vfs_priv.h"
#include "ek/ek_conf_internal.h"

static vfs_node_t s_node_pool[EKCFG_VFS_MAX_NODES];
static vfs_node_t *s_node_free_head;
static uint16_t s_node_alloced_count;
static vfs_node_t *s_node_root = NULL;
/* fd 池副本，当前未使用；fd 池由 vfs_fd_priv.c 管理 */
static vfs_file_t s_fd_pool[EKCFG_VFS_MAX_FDS];

void root_init(void)
{
    s_node_root = alloc_node();

    s_node_root->name[0] = '/';
    s_node_root->name[1] = '\0';

    s_node_root->attr = 0;
    s_node_root->type = EK_VFS_DIR;
    s_node_root->name_len = 1;

    s_node_root->parent = NULL;
    s_node_root->child = NULL;
    s_node_root->sibling = NULL;
    s_node_root->ops = NULL;
    s_node_root->priv = NULL;
}

vfs_node_t *alloc_node(void)
{
    if (s_node_free_head == NULL)
    {
        memset(s_node_pool, 0, sizeof(s_node_pool));
        for (int i = 0; i < EKCFG_VFS_MAX_NODES - 1; i++)
        {
            s_node_pool[i].sibling = &s_node_pool[i + 1];
        }
        s_node_pool[EKCFG_VFS_MAX_NODES - 1].sibling = NULL;
        s_node_free_head = s_node_pool;
        s_node_alloced_count = 0;
    }

    vfs_node_t *node = s_node_free_head;
    s_node_free_head = s_node_free_head->sibling;
    memset(node, 0, sizeof(*node));
    s_node_alloced_count++;
    return node;
}

void free_node(vfs_node_t *node)
{
    if (!node) return;
    if (node < &s_node_pool[0] || node >= &s_node_pool[EKCFG_VFS_MAX_NODES]) return;

    memset(node, 0, sizeof(*node));
    node->sibling = s_node_free_head;
    s_node_free_head = node;

    s_node_alloced_count--;
}

int get_alloced_node(void)
{
    return s_node_alloced_count;
}

void attach_child(vfs_node_t *parent, vfs_node_t *node)
{
    vfs_node_t **curr = &parent->child;
    while (*curr)
    {
        curr = &(*curr)->sibling;
    }

    *curr = node;
    node->parent = parent;
    node->sibling = NULL;
}

void detach_child(vfs_node_t *node)
{
    vfs_node_t **curr = &node->parent->child;
    while (*curr && *curr != node)
    {
        curr = &(*curr)->sibling;
    }
    if (*curr) *curr = node->sibling;
    node->parent = NULL;
    node->sibling = NULL;
}

const char *next_token(const char **cursor, uint16_t *token_len, bool *is_leaf)
{
    if (!cursor) return NULL;

    const char *p = *cursor;

    // 跳过 多个 `/`
    while (*p == '/') p++;
    if (*p == '\0') return 0;
    const char *start = p;

    const char *end = start;
    while (*end != '/' && *end != '\0') end++;
    *cursor = end;

    *token_len = (end - start);

    if (is_leaf)
    {
        const char *peek = end;
        while (*peek == '/') peek++;
        *is_leaf = (*peek == '\0');
    }
    return start;
}

vfs_node_t *find_child(vfs_node_t *node, const char *token, uint16_t token_len)
{
    if (node->type != EK_VFS_DIR) return NULL;

    vfs_node_t *curr = node->child;

    while (curr)
    {
        if (curr->name_len == token_len && !memcmp(curr->name, token, token_len))
        {
            return curr;
        }

        curr = curr->sibling;
    }

    return NULL;
}

vfs_node_t *get_root(void)
{
    return s_node_root;
}

vfs_node_t *look_up(const char *path, const char **remain_path)
{
    const char *cursor = path;
    const char *token = NULL;
    uint16_t token_len = 0;

    vfs_node_t *curr = s_node_root;

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
        vfs_node_t *child = find_child(curr, token, token_len);
        if (!child) return NULL;

        curr = child;
    }

    // 剩下路径都被消耗殆尽
    if (remain_path) *remain_path = "";

    return curr;
}
