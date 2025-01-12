/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Compiler-specific macros/definitions.
 */

#ifndef __COMPILER_H
#define __COMPILER_H

#ifdef __GNUC__
#   define __unused         __attribute__((unused))
#   define __used           __attribute__((used))
#   define __packed         __attribute__((packed))
#   define __aligned(a)     __attribute__((aligned(a)))
#   define __noreturn       __attribute__((noreturn))
#   define __malloc         __attribute__((malloc))
#   define __printf(a, b)   __attribute__((format(printf, a, b)))
#   define __deprecated     __attribute__((deprecated))
#   define __section(s)     __attribute__((section(s)))
#   define __cleanup(f)     __attribute__((cleanup(f)))
#   define __weak           __attribute__((weak))
#   define likely(x)        __builtin_expect(!!(x), 1)
#   define unlikely(x)      __builtin_expect(!!(x), 0)
#   define unreachable()    __builtin_unreachable()
#else
#   error "KBoot does not currently support non-GCC compatible compilers"
#endif

#define STRINGIFY(val)      #val
#define XSTRINGIFY(val)     STRINGIFY(val)

#define static_assert(cond) \
    do { \
        struct __static_assert { \
            char static_assert_failed[(cond) ? 1 : -1]; \
        }; \
    } while (0)

#endif /* __COMPILER_H */
