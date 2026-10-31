/**
 * @file ek_bitsteam.c
 * @brief 比特流读写
 * @author N1netyNine99
 *
 * 提供比特流的读写
 */
#include "ek/container/ek_bitstream.h"

#if EKCFG_BITSTREAM == 1

int ek_stream_writer_init(ek_bitwriter_t *bw, uint8_t *buf, size_t bytes)
{
    if (bw == NULL || buf == NULL) return -EINVAL;
    bw->buf = buf;
    bw->bit_pos = 0;
    bw->bit_cap = bytes * 8;
    memset(buf, 0, bytes);
    return 0;
}

int ek_bitstream_reader_init(ek_bitreader_t *br, const uint8_t *buf, size_t bytes)
{
    if (br == NULL || buf == NULL) return -EINVAL;

    br->buf = (uint8_t *)buf;
    br->bit_pos = 0;
    br->bit_cap = bytes * 8;
    return 0;
}

int ek_bitstream_write(ek_bitwriter_t *bw, uint32_t val, uint8_t bits)
{
    if (bw == NULL || bits == 0 || bits > 32) return -EINVAL;

    for (int i = (int)bits - 1; i >= 0; --i)
    {
        if (bw->bit_pos >= bw->bit_cap)
        {
            break;
        }

        if ((val >> i) & 1U)
        {
            bw->buf[bw->bit_pos / 8] |= (uint8_t)(1U << (7 - (bw->bit_pos % 8)));
        }
        bw->bit_pos++;
    }

    return 0;
}

int ek_bitstream_read(ek_bitreader_t *br, uint8_t bits)
{
    if (br == NULL || bits == 0 || bits > 32) return -EINVAL;

    uint32_t val = 0;

    for (uint8_t i = 0; i < bits; ++i)
    {
        if (br->bit_pos >= br->bit_cap)
        {
            break;
        }

        val <<= 1U;
        if (br->buf[br->bit_pos / 8] & (uint8_t)(1U << (7 - (br->bit_pos % 8))))
        {
            val |= 1U;
        }
        br->bit_pos++;
    }

    return val;
}

#endif // EK_BITSTREAM_H
