/*
 * Copyright (C) 2014 Alex Smith
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
 * @brief               Core definitions.
 */

#ifndef __LOADER_H
#define __LOADER_H

#include <arch/loader.h>

#include <platform/loader.h>

#include <console.h>
#include <status.h>
#include <types.h>

extern char __start[], __end[];

/**
 * Offset to apply to a physical address to get a virtual address.
 *
 * To handle platforms where the loader runs from the virtual address space
 * and physical memory is not identity mapped, this value is added on to any
 * physical address used to obtain a virtual address that maps it. If it is
 * not specified by the architecture, it is assumed that physical addresses
 * can be used directly without modification.
 */
#ifndef TARGET_VIRT_OFFSET
#   define TARGET_VIRT_OFFSET   0
#endif

/**
 * Highest physical address accessible to the loader.
 *
 * Specifies the highest physical address which the loader can access. If this
 * is not specified by the architecture, it is assumed that the loader can
 * access the low 4GB of the physical address space.
 */
#ifndef TARGET_PHYS_MAX
#   define TARGET_PHYS_MAX      0xffffffff
#endif

/** Convert a virtual address to a physical address.
 * @param addr          Address to convert.
 * @return              Converted physical address. */
static inline phys_ptr_t virt_to_phys(ptr_t addr) {
    return (addr - TARGET_VIRT_OFFSET);
}

/** Convert a physical address to a virtual address.
 * @param addr          Address to convert.
 * @return              Converted virtual address. */
static inline ptr_t phys_to_virt(phys_ptr_t addr) {
    return (addr + TARGET_VIRT_OFFSET);
}

/** Builtin object definition structure. */
typedef struct builtin {
    /** Type of the builtin. */
    enum {
        BUILTIN_TYPE_PARTITION,
        BUILTIN_TYPE_FS,
        BUILTIN_TYPE_COMMAND,
    } type;

    /** Pointer to object. */
    void *object;
} builtin_t;

extern builtin_t __builtins_start[], __builtins_end[];

/** Define a builtin object. */
#define DEFINE_BUILTIN(type, object) \
    static builtin_t __builtin_##object __section(".builtins") __used = { \
        type, \
        &object \
    }

/** Iterate over builtin objects. */
#define builtin_foreach(builtin_type, object_type, var) \
    int __iter_##var = 0; \
    for (object_type *var = (object_type *)__builtins_start[0].object; \
            __iter_##var < (__builtins_end - __builtins_start); \
            var = (object_type *)__builtins_start[++__iter_##var].object) \
        if(__builtins_start[__iter_##var].type == builtin_type)

#define vprintf(fmt, args) console_vprintf(&main_console, fmt, args)
#define printf(fmt...) console_printf(&main_console, fmt)
#define dvprintf(fmt, args) console_vprintf(&debug_console, fmt, args)
#define dprintf(fmt...) console_printf(&debug_console, fmt)

extern void boot_error(const char *fmt, ...) __printf(1, 2) __noreturn;
extern void internal_error(const char *fmt, ...) __printf(1, 2) __noreturn;

extern void backtrace(int (*print)(const char *fmt, ...));

extern void loader_main(void);

#endif /* __LOADER_H */
