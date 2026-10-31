/**
 * @file ek_bitsteam.h
 * @brief 比特流读写
 * @author N1netyNine99
 *
 * 提供比特流的读写
 */

#ifndef EK_BITSTREAM_H
#define EK_BITSTREAM_H

#include "ek/ek_conf_internal.h"

#if EKCFG_BITSTREAM == 1

#    include "ek/core/ek_def.h"

#    ifdef __cplusplus
extern "C"
{
#    endif

#    define EK_BIT(x) (1UL << (x))

typedef struct ek_bitstream ek_bitwriter_t;
typedef struct ek_bitstream ek_bitreader_t;

struct ek_bitstream
{
    uint8_t *buf; // bit 流对应的缓冲区
    size_t bit_pos; // 当前 bit 的位置
    size_t bit_cap; // 最大比特数
};

/**
 * @brief 初始化位流写入器上下文并清零底层缓冲区。
 *
 * @param[out] bw    指向待初始化的位流写入器上下文结构体指针。不可为 NULL。
 * @param[in]  buf   用于承载紧凑二进制位流的外部字节数组缓冲区指针。不可为 NULL。
 * @param[in]  bytes 外部缓冲区的大小（单位：字节）。该值将决定允许写入的最大比特容量（bytes * 8）。
 *
 * @return 0 成功；-EINVAL 表示 bw 或 buf 为 NULL。
 *
 * @note 内部会调用 memset 清空传入的缓冲区，以确保未覆盖的位全为 0。
 */
int ek_stream_writer_init(ek_bitwriter_t *bw, uint8_t *buf, size_t bytes);

/**
 * @brief 初始化位流读取器上下文。
 *
 * @param[out] br    指向待初始化的位流读取器上下文结构体指针。不可为 NULL。
 * @param[in]  buf   指向待解析的空口紧凑数据流的字节缓冲区指针。不可为 NULL。
 * @param[in]  bytes 待读取数据缓冲区的有效字节大小。
 *
 * @return 0 成功；-EINVAL 表示 br 或 buf 为 NULL。
 *
 * @note 该函数仅复位读取指针游标并绑定输入数据源，不会修改输入缓冲区中的数据。
 */
int ek_bitstream_reader_init(ek_bitreader_t *br, const uint8_t *buf, size_t bytes);

/**
 * @brief 按大端位序（MSB-First）向位流缓冲区连续写入指定位宽的数据。
 *
 * @param[in,out] bw   指向已初始化的位流写入器结构体指针。
 * @param[in]     val  待写入的数值数据。仅有效位参与写入，超出 bits 宽度的更高位会被忽略。
 * @param[in]     bits 计划写入的比特位数。有效范围通常为 1 ~ 32。
 *
 * @return 0 成功；-EINVAL 表示 bw 为 NULL、bits 为 0 或 bits 大于 32。
 *
 * @note 1. 数据按照最高有效位（MSB）优先的顺序填入字节的高位（bit 7 -> bit 0）。
 * @note 2. 若写入的比特总数超出缓冲区的容量 bit_cap，写入过程将自动截断，防止内存越界。
 * @note 3. 容量不足时仍返回 0，不会报告写入被截断。
 */
int ek_bitstream_write(ek_bitwriter_t *bw, uint32_t val, uint8_t bits);

/**
 * @brief 从位流缓冲区按大端位序（MSB-First）提取指定位宽的数据并拼接为 32 位整数。
 *
 * @param[in,out] br   指向已初始化的位流读取器结构体指针。
 * @param[in]     bits 计划读取的比特位数。有效范围通常为 1 ~ 32。
 *
 * @return 成功时返回读取到的无符号位值（0 ~ 2^bits - 1）；参数非法返回 -EINVAL。
 *         到达缓冲区末尾时返回已读取的部分值，不额外报错。
 *
 * @warning 当前返回类型为 int：当 bits 为 32 且最高位为 1 时，结果可能变成负数。
 *          需要完整 32 位无符号值时，建议将返回类型改为 uint32_t。
 *
 * @note 读取规则与 ek_bitstream_write() 完全对称；若解析有符号数（如经纬度增量、加速度），
 *       外部调用者需对读取出的低位结果进行符号扩展（Sign Extension）。
 */
int ek_bitstream_read(ek_bitreader_t *br, uint8_t bits);

#    ifdef __cplusplus
}
#    endif

#endif // EKCFG_BITSTREAM

#endif // EK_BITSTREAM_H
