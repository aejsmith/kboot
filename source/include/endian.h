/*
 * Copyright (C) 2009-2021 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief               Byte order conversion functions.
 */

#ifndef __ENDIAN_H
#define __ENDIAN_H

#include <types.h>

/** Swap byte order in a 16-bit value.
 * @param val           Value to swap order of.
 * @return              Converted value. */
static inline uint16_t byte_order_swap16(uint16_t val) {
    uint16_t out = 0;

    out |= (val & 0x00ff) << 8;
    out |= (val & 0xff00) >> 8;
    return out;
}

/** Swap byte order in a 32-bit value.
 * @param val           Value to swap order of.
 * @return              Converted value. */
static inline uint32_t byte_order_swap32(uint32_t val) {
    uint32_t out = 0;

    out |= (val & 0x000000ff) << 24;
    out |= (val & 0x0000ff00) << 8;
    out |= (val & 0x00ff0000) >> 8;
    out |= (val & 0xff000000) >> 24;
    return out;
}

/** Swap byte order in a 64-bit value.
 * @param val           Value to swap order of.
 * @return              Converted value. */
static inline uint64_t byte_order_swap64(uint64_t val) {
    uint64_t out = 0;

    out |= (val & 0x00000000000000ff) << 56;
    out |= (val & 0x000000000000ff00) << 40;
    out |= (val & 0x0000000000ff0000) << 24;
    out |= (val & 0x00000000ff000000) << 8;
    out |= (val & 0x000000ff00000000) >> 8;
    out |= (val & 0x0000ff0000000000) >> 24;
    out |= (val & 0x00ff000000000000) >> 40;
    out |= (val & 0xff00000000000000) >> 56;
    return out;
}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#   define be16_to_cpu(v)   byte_order_swap16((v))
#   define be32_to_cpu(v)   byte_order_swap32((v))
#   define be64_to_cpu(v)   byte_order_swap64((v))
#   define le16_to_cpu(v)   (v)
#   define le32_to_cpu(v)   (v)
#   define le64_to_cpu(v)   (v)
#   define cpu_to_be16(v)   byte_order_swap16((v))
#   define cpu_to_be32(v)   byte_order_swap32((v))
#   define cpu_to_be64(v)   byte_order_swap64((v))
#   define cpu_to_le16(v)   (v)
#   define cpu_to_le32(v)   (v)
#   define cpu_to_le64(v)   (v)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#   define be16_to_cpu(v)   (v)
#   define be32_to_cpu(v)   (v)
#   define be64_to_cpu(v)   (v)
#   define le16_to_cpu(v)   byte_order_swap16((v))
#   define le32_to_cpu(v)   byte_order_swap32((v))
#   define le64_to_cpu(v)   byte_order_swap64((v))
#   define cpu_to_be16(v)   (v)
#   define cpu_to_be32(v)   (v)
#   define cpu_to_be64(v)   (v)
#   define cpu_to_le16(v)   byte_order_swap16((v))
#   define cpu_to_le32(v)   byte_order_swap32((v))
#   define cpu_to_le64(v)   byte_order_swap64((v))
#else
#   error "__BYTE_ORDER__ is not defined"
#endif

#endif /* __ENDIAN_H */
