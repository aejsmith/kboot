/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Utility functions/macros.
 */

#ifndef __LIB_UTILITY_H
#define __LIB_UTILITY_H

#include <types.h>

/** Get the number of bits in a type. */
#define bits(t)         (sizeof(t) * 8)

/** Get the number of elements in an array. */
#define array_size(a)   (sizeof((a)) / sizeof((a)[0]))

/** Round a value up.
 * @param val           Value to round.
 * @param nearest       Boundary to round up to.
 * @return              Rounded value. */
#define round_up(val, nearest) \
    __extension__ \
    ({ \
        typeof(val) __n = val; \
        if (__n % (nearest)) { \
            __n -= __n % (nearest); \
            __n += nearest; \
        } \
        __n; \
    })

/** Round a value down.
 * @param val           Value to round.
 * @param nearest       Boundary to round up to.
 * @return              Rounded value. */
#define round_down(val, nearest)    \
    __extension__ \
    ({ \
        typeof(val) __n = val; \
        if (__n % (nearest)) \
            __n -= __n % (nearest); \
        __n; \
    })

/** Check if a value is a power of 2.
 * @param val           Value to check.
 * @return              Whether value is a power of 2. */
#define is_pow2(val) \
    ((val) && ((val) & ((val) - 1)) == 0)

/** Get the lowest value out of a pair of values. */
#define min(a, b) \
    ((a) < (b) ? (a) : (b))

/** Get the highest value out of a pair of values. */
#define max(a, b) \
    ((a) < (b) ? (b) : (a))

/** Swap two values. */
#define swap(a, b) \
    { \
        typeof(a) __tmp = a; \
        a = b; \
        b = __tmp; \
    }

/** Calculate the absolute value of the given value. */
#define abs(val) \
    ((val) < 0 ? -(val) : (val))

/** Get a pointer to the object containing a given object.
 * @param ptr           Pointer to child object.
 * @param type          Type of parent object.
 * @param member        Member in parent.
 * @return              Pointer to parent object. */
#define container_of(ptr, type, member) \
    __extension__ \
    ({ \
        const typeof(((type *)0)->member) *__mptr = ptr; \
        (type *)((char *)__mptr - offsetof(type, member)); \
    })

/** Find first set bit in a native-sized value.
 * @param value         Value to test.
 * @return              Position of first set bit plus 1, or 0 if value is 0. */
static inline unsigned long ffs(unsigned long value) {
    return __builtin_ffsl(value);
}

/** Find last set bit in a native-sized value.
 * @param value     Value to test.
 * @return          Position of last set bit plus 1, or 0 if value is 0. */
static inline unsigned long fls(unsigned long value) {
    if (!value)
        return 0;

    return bits(unsigned long) - __builtin_clzl(value);
}

/** Checksum a memory range.
 * @param start         Start of range to check.
 * @param size          Size of range to check.
 * @return              True if checksum is equal to 0, false if not. */
static inline bool checksum_range(void *start, size_t size) {
    uint8_t *range = (uint8_t *)start;
    uint8_t checksum = 0;
    size_t i;

    for (i = 0; i < size; i++)
        checksum += range[i];

    return (checksum == 0);
}

extern void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

#endif /* __LIB_UTILITY_H */
