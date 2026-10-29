/**
 * @file ek_vfs_core.h
 * @brief VFS 核心接口：节点注册、路径查找与文件操作
 * @author N1netyNine99
 */

#ifndef EK_VFS_CORE_H
#define EK_VFS_CORE_H

#include "ek/ek_conf_internal.h"

#if EKCFG_VFS == 1

#    include "ek_vfs_type.h"

/**
 * @brief 初始化 VFS 根节点
 * @note 由 EK_EXPORT_COMPONENTS 在 ek_export_init() 中自动调用
 */
void ek_vfs_init(void);

/**
 * @brief 查询剩余可用的 VFS 节点数量
 * @return 剩余节点数；VFS 未初始化时返回 -EINVAL
 */
int ek_vfs_node_avail(void);

/**
 * @brief 注册一个 VFS 节点
 * @param path 绝对路径，必须以 '/' 开头
 * @param type 节点类型：EK_VFS_DIR / EK_VFS_DEV / EK_VFS_MOUNT
 * @param ops 节点操作表；type 为 EK_VFS_DEV 时不能为 NULL
 * @param priv 节点私有数据，传给 ops 回调
 * @return 0 成功；负数 errno 失败
 * @retval -EINVAL 参数非法
 * @retval -ENOMEM 节点池已满
 * @retval -EEXIST 叶子节点已存在
 * @retval -ENOTDIR 中间路径不是目录
 * @note 注册多级路径时，缺失的中间目录会自动创建
 */
int ek_vfs_register(const char *path, uint8_t type, ek_vfs_ops_t *ops, void *priv);

/**
 * @brief 注销一个 VFS 节点
 * @param path 绝对路径
 * @return 0 成功；负数 errno 失败
 * @retval -EPERM 不能注销根节点
 * @retval -ENOENT 节点不存在
 * @retval -EBUSY 节点仍被打开
 * @retval -ENOTEMPTY 目录非空
 */
int ek_vfs_unregister(const char *path);

/**
 * @brief 以树形文本输出 VFS 结构
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 * @return 写入的字符数；参数非法返回 -EINVAL
 * @note 输出格式受 EKCFG_VFS_TREE_DUMP_UTF8 控制
 */
int ek_vfs_tree_dump(char *buf, size_t size);

/**
 * @brief 打开路径并返回文件描述符
 * @param path 绝对路径
 * @param flags O_RDONLY / O_WRONLY / O_RDWR，可与 O_* 标志按位或
 * @param ... O_CREAT 时传入 mode_t
 * @return 非负 fd；负数 errno 失败
 * @retval -EINVAL 参数非法
 * @retval -ENOENT 路径不存在且没有 O_CREAT
 * @retval -EISDIR 以写模式打开目录
 * @retval -EEXIST O_CREAT | O_EXCL 且节点已存在
 * @retval -EACCES O_TRUNC 与只读模式冲突
 * @retval -ENOTSUP O_CREAT 或挂载点操作尚未实现
 * @note 当前 O_CREAT / O_TRUNC 尚未完整实现
 */
int ek_vfs_open(const char *path, int flags, ...);

/**
 * @brief 从 fd 读取数据
 * @param fd 文件描述符
 * @param buf 接收缓冲区
 * @param size 期望读取的字节数
 * @return 读取的字节数；0 表示 EOF；负数 errno 失败
 * @retval -EBADF fd 无效，或不是以可读模式打开
 * @retval -EISDIR fd 指向目录
 * @retval -ENOTSUP 节点没有 read 回调
 */
ssize_t ek_vfs_read(int fd, void *buf, size_t size);

/**
 * @brief 向 fd 写入数据
 * @param fd 文件描述符
 * @param buf 待写入缓冲区
 * @param size 期望写入的字节数
 * @return 写入的字节数；负数 errno 失败
 * @retval -EBADF fd 无效，或不是以可写模式打开
 * @retval -EISDIR fd 指向目录
 * @retval -ENOTSUP 节点没有 write 回调
 */
ssize_t ek_vfs_write(int fd, const void *buf, size_t size);

/**
 * @brief 调整 fd 的读写位置
 * @param fd 文件描述符
 * @param offset 偏移量，可正可负
 * @param whence SEEK_SET / SEEK_CUR / SEEK_END
 * @return 新的绝对位置；负数 errno 失败
 * @retval -EBADF fd 无效
 * @retval -ESPIPE 节点不支持 seek
 * @note 成功后会同步更新 vfs_file_t::offset
 */
off_t ek_vfs_lseek(int fd, off_t offset, int whence);

/**
 * @brief 设备控制
 * @param fd 文件描述符
 * @param request 设备相关请求码
 * @param arg 请求参数
 * @return 0 或驱动定义的非负值；负数 errno 失败
 * @retval -EBADF fd 无效
 * @retval -ENOTTY 节点没有 ioctl 回调
 * @note 当前实现检查的是 lseek 回调，后续应改为 ioctl 回调
 */
int ek_vfs_ioctl(int fd, uint32_t request, void *arg);

/**
 * @brief 查询 fd 上的事件
 * @param fd 文件描述符
 * @param events 请求的事件位
 * @return revents 事件位；负数 errno 失败
 * @retval -EBADF fd 无效
 * @retval -EINVAL events 为 0
 * @retval -ENOTSUP 节点没有 poll 回调
 * @note 当前函数名为 poll()，与 libc poll() 同名
 */
int poll(int fd, uint32_t events);

/**
 * @brief 关闭 fd
 * @param fd 文件描述符
 * @return 0 成功；驱动 close 返回的错误码；-EBADF fd 无效
 * @note 无论驱动 close 是否失败，fd 都会被释放
 */
int close(int fd);

#    ifdef __cplusplus
}
#    endif

#endif // EKCFG_VFS == 1

#endif // EK_VFS_CORE_H
