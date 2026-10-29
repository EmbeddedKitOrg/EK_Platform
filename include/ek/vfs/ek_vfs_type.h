/**
 * @file ek_vfs_type.h
 * @brief VFS 公共类型、打开标志与节点操作表
 * @author N1netyNine99
 */

#ifndef EK_VFS_TYPE_H
#define EK_VFS_TYPE_H

#include "ek/ek_conf_internal.h"

#if EKCFG_VFS == 1

#    ifdef __cplusplus
extern "C"
{
#    endif

#    include "ek/core/ek_def.h"

#    if defined(__has_include)
#        if __has_include(<fcntl.h>)
#            include <fcntl.h>
#        endif
#    endif

#    if defined(__has_include)
#        if __has_include(<sys/types.h>)
#            include <sys/types.h>
#        endif
#    endif

/* 没有 <sys/types.h> 的裸机环境使用的最小 mode_t 兜底。 */
#    ifndef _MODE_T_DECLARED
typedef unsigned int mode_t;
#        define _MODE_T_DECLARED
#    endif

/* ---------------- 基础读写权限（互斥） ---------------- */
#    ifndef O_RDONLY
#        define O_RDONLY 0x0000 /**< 只读打开 */
#    endif
#    ifndef O_WRONLY
#        define O_WRONLY 0x0001 /**< 只写打开 */
#    endif
#    ifndef O_RDWR
#        define O_RDWR 0x0002 /**< 读写打开 */
#    endif
#    ifndef O_ACCMODE
#        define O_ACCMODE 0x0003 /**< 访问模式掩码：flags & O_ACCMODE */
#    endif

/* ---------------- 运行状态标志（按位或） ---------------- */
#    ifndef O_NONBLOCK
#        define O_NONBLOCK 0x0004 /**< 非阻塞模式：裸机外设高频使用 */
#    endif
#    ifndef O_APPEND
#        define O_APPEND 0x0008 /**< 尾部追加模式 */
#    endif
#    ifndef O_SYNC
#        define O_SYNC 0x0010 /**< 同步 I/O：write 返回前数据已落盘/落总线 */
#    endif

/* ---------------- 存储文件生命周期标志 ---------------- */
#    ifndef O_CREAT
#        define O_CREAT 0x0100 /**< 不存在则新建（需要 mode 参数） */
#    endif
#    ifndef O_TRUNC
#        define O_TRUNC 0x0200 /**< 打开时清空内容 */
#    endif
#    ifndef O_EXCL
#        define O_EXCL 0x0400 /**< 与 O_CREAT 配合：已存在则失败 */
#    endif

typedef struct ek_vfs_ops ek_vfs_ops_t;

/**
 * @brief VFS 节点类型
 */
enum ek_vfs_type
{
    EK_VFS_DIR, /**< 目录节点，可继续向下遍历 */
    EK_VFS_DEV, /**< 设备节点，注册时必须提供 ops */
    EK_VFS_MOUNT, /**< 挂载点，look_up 到此停止，剩余路径交给外部文件系统 */
    EK_VFS_MAX /**< 类型数量哨兵 */
};

/**
 * @brief VFS 节点操作表
 * @note 每个回调的第一个参数都是 open() 产生的 file_priv；
 *       没有 open 回调时，VFS 会直接使用 node->priv。
 */
struct ek_vfs_ops
{
    /**
     * @brief 打开节点
     * @param node_priv 节点私有数据（注册时传入的 priv）
     * @param flags O_RDONLY / O_WRONLY / O_RDWR 与 O_* 标志
     * @param file_priv 输出：本次打开实例的私有数据
     * @return 0 成功；负数 errno 失败
     */
    int (*open)(void *node_priv, int flags, void **file_priv);

    /**
     * @brief 读取数据
     * @param file_priv open 返回的打开实例数据
     * @param buf 接收缓冲区
     * @param count 期望读取的字节数
     * @return 读取字节数；0 表示 EOF；负数 errno 失败
     */
    ssize_t (*read)(void *file_priv, void *buf, size_t count);

    /**
     * @brief 写入数据
     * @param file_priv open 返回的打开实例数据
     * @param buf 待写入缓冲区
     * @param count 期望写入的字节数
     * @return 写入字节数；负数 errno 失败
     */
    ssize_t (*write)(void *file_priv, const void *buf, size_t count);

    /**
     * @brief 调整读写位置
     * @param file_priv open 返回的打开实例数据
     * @param offset 偏移量
     * @param whence SEEK_SET / SEEK_CUR / SEEK_END
     * @return 新的绝对位置；负数 errno 失败
     */
    off_t (*lseek)(void *file_priv, off_t offset, int whence);

    /**
     * @brief 设备控制
     * @param file_priv open 返回的打开实例数据
     * @param request 设备相关请求码
     * @param arg 请求参数
     * @return 0 或驱动定义值；负数 errno 失败
     */
    int (*ioctl)(void *file_priv, uint32_t request, void *arg);

    /**
     * @brief 查询事件
     * @param file_priv open 返回的打开实例数据
     * @param events 请求的事件位
     * @return revents 事件位；负数 errno 失败
     */
    int (*poll)(void *file_priv, uint32_t events);

    /**
     * @brief 关闭打开实例
     * @param file_priv open 返回的打开实例数据
     * @return 0 成功；负数 errno 失败
     * @note 只释放 open 分配的资源，不要释放 node_priv
     */
    int (*close)(void *file_priv);
};

#    ifdef __cplusplus
}
#    endif

#endif // EKCFG_VFS == 1

#endif // EK_VFS_TYPE_H
