#include "ek_vfs_priv.h"

static ek_vfs_node_t s_node_pool[EKCFG_VFS_MAX_NODES];
static ek_vfs_node_t *s_free_head;
static uint16_t s_alloced_count;

ek_vfs_node_t *alloc_node(void)
{
    if (s_free_head == NULL)
    {
        memset(s_node_pool, 0, sizeof(s_node_pool));
        for (int i = 0; i < EKCFG_VFS_MAX_NODES - 1; i++)
        {
            s_node_pool[i].sibling = &s_node_pool[i];
        }
        s_node_pool[EKCFG_VFS_MAX_NODES - 1].sibling = NULL;
        s_free_head = s_node_pool;
        s_alloced_count = 0;
    }

    ek_vfs_node_t *node = s_free_head;
    s_free_head = s_free_head->sibling;
    memset(node, 0, sizeof(*node));
    s_alloced_count++;
    return node;
}

void free_node(ek_vfs_node_t *node)
{
    if (!node) return;
    if (node < &s_node_pool[0] || node >= &s_node_pool[EKCFG_VFS_MAX_NODES]) return;

    memset(node, 0, sizeof(*node));
    node->sibling = s_free_head;
    s_free_head = node;

    s_alloced_count--;
}

int get_alloced_node(void)
{
    return s_alloced_count;
}

void attach_child(ek_vfs_node_t *parent, ek_vfs_node_t *node)
{
    ek_vfs_node_t **curr = &parent->child;
    while (*curr)
    {
        curr = &(*curr)->sibling;
    }

    *curr = node;
    node->parent = parent;
    node->sibling = NULL;
}

void detach_child(ek_vfs_node_t *node)
{
    ek_vfs_node_t **curr = &node->parent->child;
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
    *cursor = p;

    *token_len = (end - start);

    if (is_leaf)
    {
        const char *peek = end;
        while (*peek == '/') peek++;
        *is_leaf = (*peek == '\0');
    }
    return start;
}

ek_vfs_node_t *find_child(ek_vfs_node_t *node, const char *token, uint16_t token_len)
{
    if (node->type != EK_VFS_DIR) return NULL;

    ek_vfs_node_t *curr = node->child;

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
