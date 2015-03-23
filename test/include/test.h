/*
 * Copyright (C) 2012-2015 Alex Smith
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
 * @brief               KBoot test kernel definitions.
 */

#ifndef __TEST_H
#define __TEST_H

#include <arch/page.h>

#include <lib/utility.h>

#include <assert.h>
#include <elf.h>
#include <kboot.h>
#include <loader.h>

/** Address space definitions. */
#ifdef __LP64__
#   define PHYS_MAP_BASE    0xfffffffe00000000
#   define PHYS_MAP_SIZE    0x100000000
#   define VIRT_MAP_BASE    0xffffffff00000000
#   define VIRT_MAP_SIZE    0x80000000
#   define PHYS_MAX         0xffffffff
#else
#   define VIRT_MAP_BASE    0xc0000000
#   define VIRT_MAP_SIZE    0x40000000
#   define PHYS_MAX         0xffffffff
#endif

extern void mmu_map(ptr_t virt, phys_ptr_t phys, size_t size);

extern ptr_t virt_alloc(size_t size);

extern void *phys_map(phys_ptr_t addr, size_t size);
extern phys_ptr_t phys_alloc(phys_size_t size);

extern void debug_console_init(void);
extern void primary_console_init(kboot_tag_t *tags);
extern void mm_init(kboot_tag_t *tags);
extern void mmu_init(kboot_tag_t *tags);

extern void kmain(uint32_t magic, kboot_tag_t *tags);

#endif /* __TEST_H */
