/**
 * @file vfs_bus.c
 * @brief VFS 总线注册、设备注册与通用总线操作实现
 * @author N1netyNine99
 */

#include "ek/vfs/vfs_bus.h"

#if EKCFG_VFS == 1

#    include "ek/core/io.h"
#    include "ek/vfs/vfs_core.h"
#    include "vfs_priv.h"

#    define PREFIX "/dev/bus/"

static int _bus_path(const char *name, char *path, size_t size)
{
    if (!name || name[0] == '\0') return -EINVAL;
    if (strchr(name, '/')) return -EINVAL;
    if (strlen(name) >= EKCFG_VFS_NAME_LEN) return -ENAMETOOLONG;

    int len = ek_snprintf(path, size, "%s%s", PREFIX, name);
    if (len < 0 || (size_t)len >= size) return -ENAMETOOLONG;

    return 0;
}

static int _bus_lookup(const char *name, vfs_node_t **out)
{
    char path[sizeof(PREFIX) + EKCFG_VFS_NAME_LEN];
    int ret = _bus_path(name, path, sizeof(path));
    if (ret < 0) return ret;

    const char *remain_path = NULL;
    vfs_node_t *node = look_up(path, &remain_path);
    if (!node || (remain_path && *remain_path != '\0')) return -ENODEV;
    if (node->type != EK_VFS_BUS) return -ENODEV;

    *out = node;
    return 0;
}

static int _bus_dev_path(const char *bus_name, const char *dev_name, char *path, size_t size)
{
    if (!dev_name || dev_name[0] == '\0') return -EINVAL;
    if (strchr(dev_name, '/')) return -EINVAL;
    if (strlen(dev_name) >= EKCFG_VFS_NAME_LEN) return -ENAMETOOLONG;

    char bus_path[sizeof(PREFIX) + EKCFG_VFS_NAME_LEN];
    int ret = _bus_path(bus_name, bus_path, sizeof(bus_path));
    if (ret < 0) return ret;

    int len = ek_snprintf(path, size, "%s/%s", bus_path, dev_name);
    if (len < 0 || (size_t)len >= size) return -ENAMETOOLONG;

    return 0;
}

int ek_vfs_bus_register(ek_vfs_bus_t *bus, const char *name, const ek_vfs_bus_ops_t *ops, void *priv)
{
    if (!bus || !ops) return -EINVAL;

    char path[sizeof(PREFIX) + EKCFG_VFS_NAME_LEN];
    int ret = _bus_path(name, path, sizeof(path));
    if (ret < 0) return ret;

    ret = ek_vfs_register(path, EK_VFS_BUS, NULL, bus);
    if (ret < 0) return ret;

    bus->ops = ops;
    bus->priv = priv;

    return 0;
}

int ek_vfs_bus_unregister(const char *name)
{
    char path[sizeof(PREFIX) + EKCFG_VFS_NAME_LEN];
    int ret = _bus_path(name, path, sizeof(path));
    if (ret < 0) return ret;

    const char *remain_path = NULL;
    vfs_node_t *node = look_up(path, &remain_path);
    if (!node || (remain_path && *remain_path != '\0')) return -ENOENT;
    if (node->type != EK_VFS_BUS) return -ENODEV;

    return ek_vfs_unregister(path);
}

int ek_vfs_bus_dev_register(const char *bus_name, const char *dev_name, const ek_vfs_ops_t *ops, void *priv)
{
    if (!ops) return -EINVAL;

    vfs_node_t *bus_node = NULL;
    int ret = _bus_lookup(bus_name, &bus_node);
    if (ret < 0) return ret;
    __EK_UNUSED(bus_node);

    char path[sizeof(PREFIX) + 2 * EKCFG_VFS_NAME_LEN + 1];
    ret = _bus_dev_path(bus_name, dev_name, path, sizeof(path));
    if (ret < 0) return ret;

    return ek_vfs_register(path, EK_VFS_DEV, ops, priv);
}

int ek_vfs_bus_dev_unregister(const char *bus_name, const char *dev_name)
{
    vfs_node_t *bus_node = NULL;
    int ret = _bus_lookup(bus_name, &bus_node);
    if (ret < 0) return ret;
    __EK_UNUSED(bus_node);

    char path[sizeof(PREFIX) + 2 * EKCFG_VFS_NAME_LEN + 1];
    ret = _bus_dev_path(bus_name, dev_name, path, sizeof(path));
    if (ret < 0) return ret;

    const char *remain_path = NULL;
    vfs_node_t *node = look_up(path, &remain_path);
    if (!node || (remain_path && *remain_path != '\0')) return -ENOENT;
    if (node->type != EK_VFS_DEV) return -ENODEV;

    return ek_vfs_unregister(path);
}

int ek_vfs_bus_xfer(const char *name, uint16_t addr, const char *tx, size_t tx_len, uint8_t *rx, size_t rx_len)
{
    vfs_node_t *node = NULL;
    int ret = _bus_lookup(name, &node);
    if (ret < 0) return ret;

    ek_vfs_bus_t *bus = (ek_vfs_bus_t *)node->priv;
    if (!bus || !bus->ops || !bus->ops->xfer) return -ENOTSUP;

    return bus->ops->xfer(bus, addr, tx, tx_len, rx, rx_len);
}

int ek_vfs_bus_probe(const char *name, uint16_t addr)
{
    vfs_node_t *node = NULL;
    int ret = _bus_lookup(name, &node);
    if (ret < 0) return ret;

    ek_vfs_bus_t *bus = (ek_vfs_bus_t *)node->priv;
    if (!bus || !bus->ops || !bus->ops->probe) return -ENOTSUP;

    return bus->ops->probe(bus, addr);
}

int ek_vfs_bus_reset(const char *name)
{
    vfs_node_t *node = NULL;
    int ret = _bus_lookup(name, &node);
    if (ret < 0) return ret;

    ek_vfs_bus_t *bus = (ek_vfs_bus_t *)node->priv;
    if (!bus || !bus->ops || !bus->ops->reset) return -ENOTSUP;

    return bus->ops->reset(bus);
}

#endif // EKCFG_VFS == 1
