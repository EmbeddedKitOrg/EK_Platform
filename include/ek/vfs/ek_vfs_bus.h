/**
 * @file ek_vfs_bus.h
 * @brief VFS 总线对象、设备注册与通用总线操作接口
 * @author N1netyNine99
 */

#ifndef EK_VFS_BUS_H
#define EK_VFS_BUS_H

#include "ek/ek_conf_internal.h"

#if EKCFG_VFS == 1

#    include "ek/vfs/ek_vfs_type.h"

#    ifdef __cplusplus
extern "C"
{
#    endif

/**
 * @brief 总线对象
 * @note 由调用方持有，生命周期必须覆盖挂载在该总线上的所有设备。
 *       注册成功后绑定到 /dev/bus/<name> 节点，节点 priv 指向本对象。
 */
struct ek_vfs_bus
{
    const ek_vfs_bus_ops_t *ops; /**< 总线操作表，由 BSP 实现 */
    void *priv; /**< BSP 私有上下文，如 I2C 句柄、锁和超时配置 */
};

/**
 * @brief 注册总线目录
 * @param bus 总线对象；成功后写入 ops 和 priv
 * @param name 总线名，单段，不能包含 '/'
 * @param ops 总线操作表，不能为 NULL
 * @param priv BSP 私有上下文
 * @return 0 成功；负数 errno 失败
 * @retval -EINVAL 参数非法
 * @retval -ENAMETOOLONG 名字过长或路径截断
 * @retval -EEXIST 总线目录已存在
 * @retval -ENOMEM 节点池已满
 * @note 注册路径为 /dev/bus/<name>，节点类型为 EK_VFS_BUS，节点 priv 指向 bus。
 */
int ek_vfs_bus_register(ek_vfs_bus_t *bus, const char *name, const ek_vfs_bus_ops_t *ops, void *priv);

/**
 * @brief 注销总线目录
 * @param name 总线名，单段，不能包含 '/'
 * @return 0 成功；负数 errno 失败
 * @retval -EINVAL 参数非法
 * @retval -ENAMETOOLONG 名字过长或路径截断
 * @retval -ENOENT 总线不存在
 * @retval -ENODEV 节点不是 BUS
 * @retval -EBUSY 总线目录仍被打开
 * @retval -ENOTEMPTY 总线下仍有设备
 * @note 必须先注销该总线下的所有设备。
 */
int ek_vfs_bus_unregister(const char *name);

/**
 * @brief 在已注册总线目录下注册设备节点
 * @param bus_name 总线名，单段，不能包含 '/'
 * @param dev_name 设备名，单段，不能包含 '/'
 * @param ops 设备文件操作表，不能为 NULL
 * @param priv 设备私有数据
 * @return 0 成功；负数 errno 失败
 * @retval -EINVAL 参数非法
 * @retval -ENAMETOOLONG 名字过长或路径截断
 * @retval -ENODEV 总线不存在或节点不是 BUS
 * @retval -EEXIST 设备节点已存在
 * @retval -ENOMEM 节点池已满
 * @note 设备路径为 /dev/bus/<bus_name>/<dev_name>，总线必须先注册。
 */
int ek_vfs_bus_dev_register(const char *bus_name, const char *dev_name, const ek_vfs_ops_t *ops, void *priv);

/**
 * @brief 注销总线下的设备节点
 * @param bus_name 总线名，单段，不能包含 '/'
 * @param dev_name 设备名，单段，不能包含 '/'
 * @return 0 成功；负数 errno 失败
 * @retval -EINVAL 参数非法
 * @retval -ENAMETOOLONG 名字过长或路径截断
 * @retval -ENODEV 总线不存在、节点不是 BUS，或设备节点不是 DEV
 * @retval -ENOENT 设备节点不存在
 * @retval -EBUSY 设备仍被打开
 * @note 设备节点注销时由 VFS core 调用设备 ops 的 destroy 回调。
 */
int ek_vfs_bus_dev_unregister(const char *bus_name, const char *dev_name);

/**
 * @brief 按总线名执行一次总线传输
 * @param name 总线名，单段，不能包含 '/'
 * @param addr 总线地址；I2C 为 7-bit 从机地址
 * @param tx 写缓冲区；tx_len 为 0 时忽略
 * @param tx_len 写长度
 * @param rx 读缓冲区；rx_len 为 0 时忽略
 * @param rx_len 读长度
 * @return 0 成功；负数 errno 失败
 * @retval -EINVAL 参数非法
 * @retval -ENAMETOOLONG 名字过长或路径截断
 * @retval -ENODEV 总线不存在或节点不是 BUS
 * @retval -ENOTSUP 总线未实现 transfer
 * @note 每次调用都会按名字查找总线，适合控制面、调试和 scan；
 *       设备数据面应缓存 ek_vfs_bus_t * 并直接调用 ops->transfer。
 */
int ek_vfs_bus_xfer(const char *name, uint16_t addr, const char *tx, size_t tx_len, uint8_t *rx, size_t rx_len);

/**
 * @brief 按总线名探测一个地址
 * @param name 总线名，单段，不能包含 '/'
 * @param addr 总线地址；I2C 为 7-bit 从机地址
 * @return 0 表示设备存在；负数 errno 失败
 * @retval -EINVAL 参数非法
 * @retval -ENAMETOOLONG 名字过长或路径截断
 * @retval -ENODEV 总线不存在、节点不是 BUS，或该地址无设备
 * @retval -ENOTSUP 总线未实现 probe
 * @note probe 是可选的，具体探测方式由 BSP 决定。
 */
int ek_vfs_bus_probe(const char *name, uint16_t addr);

/**
 * @brief 按总线名复位或恢复总线
 * @param name 总线名，单段，不能包含 '/'
 * @return 0 成功；负数 errno 失败
 * @retval -EINVAL 参数非法
 * @retval -ENAMETOOLONG 名字过长或路径截断
 * @retval -ENODEV 总线不存在或节点不是 BUS
 * @retval -ENOTSUP 总线未实现 reset
 * @note 这是总线级操作，不会注销总线上的设备。
 */
int ek_vfs_bus_reset(const char *name);

#    ifdef __cplusplus
}
#    endif

#endif // EKCFG_VFS == 1

#endif // EK_VFS_BUS_H
