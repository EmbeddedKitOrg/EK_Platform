/**
 * @file ek_err.h
 * @brief 嵌入式通用错误码定义
 * @author N1netyNine99
 *
 * 错误码从 200 起步，避免与标准 errno.h (1-148) 冲突。
 * ek_err_t 为 uint8_t，范围 0-255，0 表示成功。
 */

#ifndef EK_ERR_H
#define EK_ERR_H

#include "ek_def.h"

/** 错误码类型 */
typedef int ek_err_t;

/** 成功 */
#define EK_OK ((ek_err_t)0)

/* ========== 通用错误 (200-209) ========== */
#define EK_ERR_FAILED   ((ek_err_t)200) /**< 通用失败 */
#define EK_ERR_TIMEOUT  ((ek_err_t)201) /**< 操作超时 */
#define EK_ERR_BUSY     ((ek_err_t)202) /**< 资源忙 */
#define EK_ERR_NOMEM    ((ek_err_t)203) /**< 内存不足 */
#define EK_ERR_INVAL    ((ek_err_t)204) /**< 无效参数 */
#define EK_ERR_NOTSUP   ((ek_err_t)205) /**< 不支持的操作 */
#define EK_ERR_NOTFOUND ((ek_err_t)206) /**< 未找到 */
#define EK_ERR_FULL     ((ek_err_t)207) /**< 缓冲区/资源已满 */
#define EK_ERR_EMPTY    ((ek_err_t)208) /**< 缓冲区/资源为空 */
#define EK_ERR_PERM     ((ek_err_t)209) /**< 权限不足 */

/* ========== 状态与初始化 (210-219) ========== */
#define EK_ERR_NOTINIT  ((ek_err_t)210) /**< 模块未初始化 */
#define EK_ERR_INITED   ((ek_err_t)211) /**< 模块已初始化 */
#define EK_ERR_NOTREADY ((ek_err_t)212) /**< 设备/模块未就绪 */
#define EK_ERR_DISABLED ((ek_err_t)213) /**< 功能已禁用 */
#define EK_ERR_ABORTED  ((ek_err_t)214) /**< 操作被中止（资源销毁等） */

/* ========== 数据与范围 (220-229) ========== */
#define EK_ERR_OVERFLOW   ((ek_err_t)220) /**< 数据溢出 */
#define EK_ERR_UNDERFLOW  ((ek_err_t)221) /**< 数据下溢 */
#define EK_ERR_OUTOFRANGE ((ek_err_t)222) /**< 数值超出范围 */
#define EK_ERR_NODATA     ((ek_err_t)223) /**< 无数据可用 */
#define EK_ERR_PARSE      ((ek_err_t)224) /**< 数据解析错误 */
#define EK_ERR_CHECKSUM   ((ek_err_t)225) /**< 校验和/CRC 错误 */

/* ========== 设备与硬件 (230-239) ========== */
#define EK_ERR_NODEV ((ek_err_t)230) /**< 设备不存在 */
#define EK_ERR_IO    ((ek_err_t)231) /**< I/O 错误 */
#define EK_ERR_FAULT ((ek_err_t)232) /**< 硬件故障 */
#define EK_ERR_NXIO  ((ek_err_t)233) /**< 设备未配置 */
#define EK_ERR_PROTO ((ek_err_t)234) /**< 协议错误 */
#define EK_ERR_LINK  ((ek_err_t)235) /**< 链路/连接错误 */
#define EK_ERR_RESET ((ek_err_t)236) /**< 设备复位 */

/* ========== 通信 (240-249) ========== */
#define EK_ERR_COMM    ((ek_err_t)240) /**< 通信错误 */
#define EK_ERR_NAK     ((ek_err_t)241) /**< 未确认/应答 */
#define EK_ERR_NOTACK  ((ek_err_t)242) /**< 未收到应答 */
#define EK_ERR_FRAMING ((ek_err_t)243) /**< 帧错误 */
#define EK_ERR_PARITY  ((ek_err_t)244) /**< 奇偶校验错误 */
#define EK_ERR_NOISE   ((ek_err_t)245) /**< 线路噪声 */

/* ========== 错误处理辅助宏 ========== */

/** 如果表达式返回非 EK_OK，直接 return 该错误码 */
#define EK_ERR_RETURN(expr)               \
    do                                    \
    {                                     \
        ek_err_t _err_ = (expr);          \
        if (_err_ != EK_OK) return _err_; \
    } while (0)

/** 如果表达式返回非 EK_OK，goto 到指定标签（用于 cleanup 模式） */
#define EK_ERR_GOTO(expr, label)        \
    do                                  \
    {                                   \
        ek_err_t _err_ = (expr);        \
        if (_err_ != EK_OK) goto label; \
    } while (0)

/**
 * @brief 将错误码转为可读字符串
 * @param err 错误码
 * @return 错误描述字符串
 */
__EK_STATIC_INLINE const char *ek_strerror(ek_err_t err)
{
    switch (err)
    {
        case EK_OK:
            return "OK";
        case EK_ERR_FAILED:
            return "Failed";
        case EK_ERR_TIMEOUT:
            return "Timeout";
        case EK_ERR_BUSY:
            return "Busy";
        case EK_ERR_NOMEM:
            return "No memory";
        case EK_ERR_INVAL:
            return "Invalid argument";
        case EK_ERR_NOTSUP:
            return "Not supported";
        case EK_ERR_NOTFOUND:
            return "Not found";
        case EK_ERR_FULL:
            return "Full";
        case EK_ERR_EMPTY:
            return "Empty";
        case EK_ERR_PERM:
            return "Permission denied";
        case EK_ERR_NOTINIT:
            return "Not initialized";
        case EK_ERR_INITED:
            return "Already initialized";
        case EK_ERR_NOTREADY:
            return "Not ready";
        case EK_ERR_DISABLED:
            return "Disabled";
        case EK_ERR_ABORTED:
            return "Aborted";
        case EK_ERR_OVERFLOW:
            return "Overflow";
        case EK_ERR_UNDERFLOW:
            return "Underflow";
        case EK_ERR_OUTOFRANGE:
            return "Out of range";
        case EK_ERR_NODATA:
            return "No data";
        case EK_ERR_PARSE:
            return "Parse error";
        case EK_ERR_CHECKSUM:
            return "Checksum error";
        case EK_ERR_NODEV:
            return "No such device";
        case EK_ERR_IO:
            return "I/O error";
        case EK_ERR_FAULT:
            return "Hardware fault";
        case EK_ERR_NXIO:
            return "Device not configured";
        case EK_ERR_PROTO:
            return "Protocol error";
        case EK_ERR_LINK:
            return "Link error";
        case EK_ERR_RESET:
            return "Device reset";
        case EK_ERR_COMM:
            return "Communication error";
        case EK_ERR_NAK:
            return "NAK received";
        case EK_ERR_NOTACK:
            return "No acknowledgment";
        case EK_ERR_FRAMING:
            return "Framing error";
        case EK_ERR_PARITY:
            return "Parity error";
        case EK_ERR_NOISE:
            return "Line noise";
        default:
            return "Unknown error";
    }
}

#endif /* EK_ERR_H */
