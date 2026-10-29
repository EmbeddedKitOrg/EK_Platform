/**
 * @file vfs_priv.h
 * @brief VFS 内部节点、打开实例与私有函数声明
 * @author N1netyNine99
 */

#ifndef VFS_PRIV_H
#define VFS_PRIV_H

#include "ek/vfs/ek_vfs_type.h"

typedef struct vfs_node vfs_node_t;
typedef struct vfs_file vfs_file_t;

/**
 * @brief VFS 节点
 * @note 节点名最长 EKCFG_VFS_NODE_NAME_LEN - 1 个字符
 */
struct vfs_node
{
    struct vfs_node *parent; /**< 父节点；根节点为 NULL */
    struct vfs_node *child; /**< 第一个子节点 */
    struct vfs_node *sibling; /**< 下一个兄弟节点 */
    const ek_vfs_ops_t *ops; /**< 节点操作表；目录节点可为 NULL */
    void *priv; /**< 节点私有数据，注册时传入 */

    union
    {
        uint32_t attr; /**< 节点属性原始值 */
        struct
        {
            uint8_t type; /**< 节点类型：EK_VFS_DIR / EK_VFS_DEV / EK_VFS_MOUNT */
            uint8_t flags; /**< 节点标志位，当前保留 */
            uint8_t ref_count; /**< 当前打开实例数量；非 0 时 unregister 返回 -EBUSY */
            uint8_t name_len; /**< 节点名长度，不包含结尾 '\0' */
        };
    };

    char name[EKCFG_VFS_NODE_NAME_LEN]; /**< 节点名，以 '\0' 结尾 */
};

/**
 * @brief 一次 open() 产生的打开实例
 * @note 由 fd_to_file() 返回；close() 后对应槽位会被清空
 */
struct vfs_file
{
    int flags; /**< open 时传入的 O_* 标志 */
    vfs_node_t *node; /**< 对应的 VFS 节点 */
    const ek_vfs_ops_t *ops; /**< 节点操作表快照，只读 */
    void *file_priv; /**< 驱动 open 返回的打开实例数据 */
    off_t offset; /**< 当前读写位置缓存，由 lseek/read/write 维护 */
};

/**
 * @brief 初始化 VFS 根节点
 * @note 创建名为 "/" 的 EK_VFS_DIR 根节点
 */
void root_init(void);

/**
 * @brief 从节点池分配一个节点
 * @return 节点指针；节点池耗尽返回 NULL
 * @note 首次调用时把节点池串成空闲链表
 */
vfs_node_t *alloc_node(void);

/**
 * @brief 把节点归还到节点池
 * @param node 要释放的节点；不是节点池内的指针时忽略
 */
void free_node(vfs_node_t *node);

/**
 * @brief 返回当前已分配的节点数量
 * @return 已分配节点数
 */
int get_alloced_node(void);

/**
 * @brief 返回 VFS 根节点
 * @return 根节点指针；未初始化时为 NULL
 */
vfs_node_t *get_root(void);

/**
 * @brief 把 node 追加为 parent 的最后一个子节点
 * @param parent 父节点
 * @param node 要挂载的子节点
 * @note 会设置 node->parent 和 node->sibling
 */
void attach_child(vfs_node_t *parent, vfs_node_t *node);

/**
 * @brief 从父节点的子链表中摘除 node
 * @param node 要摘除的节点
 * @note 会清空 node->parent 和 node->sibling
 */
void detach_child(vfs_node_t *node);

/**
 * @brief 在节点的直接子节点中按名字查找
 * @param node 父节点，必须是目录
 * @param token 名字
 * @param token_len 名字长度
 * @return 匹配的子节点；未找到返回 NULL
 * @note 只查找直接子节点，不会递归
 */
vfs_node_t *find_child(vfs_node_t *node, const char *token, uint16_t token_len);

/**
 * @brief 从路径中解析下一个 token
 * @param cursor 输入输出：当前解析位置
 * @param token_len 输出：token 长度
 * @param is_leaf 输出：当前 token 是否为路径最后一段
 * @return token 起始地址；路径结束返回 NULL
 * @note 跳过连续的 '/'，不会修改原路径字符串
 */
const char *next_token(const char **cursor, uint16_t *token_len, bool *is_leaf);

/**
 * @brief 按绝对路径查找节点
 * @param path 绝对路径
 * @param remain_path 输出：挂载点之后的剩余路径；可为 NULL
 * @return 找到的节点；路径不存在或中间节点不是目录时返回 NULL
 * @note 遇到 EK_VFS_MOUNT 会停止遍历，把剩余路径交给外部文件系统
 */
vfs_node_t *look_up(const char *path, const char **remain_path);

/**
 * @brief 分配一个文件描述符
 * @return 非负 fd；-ENFILE 表示 fd 池已满
 * @note 使用 s_bitmap 位图记录占用状态
 */
int fd_alloc(void);

/**
 * @brief 释放文件描述符
 * @param fd 要释放的 fd
 * @note 当前只释放 fd >= 3 的槽位
 */
void fd_free(int fd);

/**
 * @brief 把 fd 转换为打开实例
 * @param fd 文件描述符
 * @return 打开实例指针；fd 越界或未分配时返回 NULL
 */
vfs_file_t *fd_to_file(int fd);

#endif // VFS_PRIV_H
