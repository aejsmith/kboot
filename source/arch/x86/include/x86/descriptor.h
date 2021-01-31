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
 * @brief               x86 descriptor table definitions.
 */

#ifndef __X86_DESCRIPTOR_H
#define __X86_DESCRIPTOR_H

/** Segment defintions. */
#define SEGMENT_CS32        0x08    /**< 32-bit code segment. */
#define SEGMENT_DS32        0x10    /**< 32-bit data segment. */
#define SEGMENT_CS64        0x18    /**< 64-bit code segment. */
#define SEGMENT_DS64        0x20    /**< 64-bit data segment. */
#define SEGMENT_CS16        0x28    /**< 16-bit code segment. */
#define SEGMENT_DS16        0x30    /**< 16-bit code segment. */

#ifdef __LP64__
#   define SEGMENT_CS       SEGMENT_CS64
#   define SEGMENT_DS       SEGMENT_DS64
#else
#   define SEGMENT_CS       SEGMENT_CS32
#   define SEGMENT_DS       SEGMENT_DS32
#endif

/** Number of IDT/GDT entries. */
#define IDT_ENTRY_COUNT     32
#define GDT_ENTRY_COUNT     7

#ifndef __ASM__

#include <types.h>

/** GDT pointer loaded into the GDTR register. */
typedef struct gdt_pointer {
    uint16_t limit;                 /**< Total size of GDT. */
    ptr_t base;                     /**< Virtual address of GDT. */
} __packed gdt_pointer_t;

/** IDT pointer loaded into the IDTR register. */
typedef struct idt_pointer {
    uint16_t limit;                 /**< Total size of IDT. */
    ptr_t base;                     /**< Virtual address of IDT. */
} __packed idt_pointer_t;

/** Structure of a GDT descriptor. */
typedef struct gdt_entry {
    unsigned limit0 : 16;           /**< Low part of limit. */
    unsigned base0 : 24;            /**< Low part of base. */
    unsigned type : 4;              /**< Type flag. */
    unsigned s : 1;                 /**< S (descriptor type) flag. */
    unsigned dpl : 2;               /**< Descriptor privilege level. */
    unsigned present : 1;           /**< Present. */
    unsigned limit1 : 4;            /**< High part of limit. */
    unsigned : 1;                   /**< Spare bit. */
    unsigned longmode : 1;          /**< 64-bit code segment. */
    unsigned db : 1;                /**< Default operation size. */
    unsigned granularity : 1;       /**< Granularity. */
    unsigned base1 : 8;             /**< High part of base. */
} __packed __aligned(8) gdt_entry_t;

#ifdef __LP64__

/** Structure of an IDT entry. */
typedef struct idt_entry {
    unsigned base0 : 16;            /**< Low part of handler address. */
    unsigned sel : 16;              /**< Code segment selector. */
    unsigned ist : 3;               /**< Interrupt Stack Table number. */
    unsigned : 5;                   /**< Unused - always zero. */
    unsigned flags : 8;             /**< Flags. */
    unsigned base1 : 16;            /**< Middle part of handler address. */
    unsigned base2 : 32;            /**< High part of handler address. */
    unsigned : 32;                  /**< Reserved. */
} __packed __aligned(8) idt_entry_t;

#else /* __LP64__ */

/** Structure of an IDT entry. */
typedef struct idt_entry {
    unsigned base0 : 16;            /**< Low part of handler address. */
    unsigned sel : 16;              /**< Code segment selector. */
    unsigned : 8;                   /**< Unused - always zero. */
    unsigned flags : 8;             /**< Flags. */
    unsigned base1 : 16;            /**< High part of handler address. */
} __packed __aligned(8) idt_entry_t;

#endif /* __LP64__ */

/** Set the GDTR register.
 * @param base          Virtual address of GDT.
 * @param limit         Size of GDT. */
static inline void x86_lgdt(ptr_t base, uint16_t limit) {
    gdt_pointer_t gdtp = { limit, base };

    __asm__ __volatile__("lgdt %0" :: "m"(gdtp));
}

/** Set the IDTR register.
 * @param base          Base address of IDT.
 * @param limit         Size of IDT. */
static inline void x86_lidt(ptr_t base, uint16_t limit) {
    idt_pointer_t idtp = { limit, base };

    __asm__ __volatile__("lidt %0" :: "m"(idtp));
}

extern void x86_descriptor_init(void);

#endif /* __ASM__ */
#endif /* __X86_DESCRIPTOR_H */
