/*
 * Copyright (C) 2015 Alex Smith
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
 * @brief               MMU functions.
 */

#ifndef __MMU_H
#define __MMU_H

#include <loader.h>

/** Type of an MMU context. */
typedef struct mmu_context mmu_context_t;

extern bool mmu_map(mmu_context_t *ctx, load_ptr_t virt, phys_ptr_t phys, load_size_t size);

extern bool mmu_memset(mmu_context_t *ctx, load_ptr_t addr, uint8_t value, load_size_t size);
extern bool mmu_memcpy_to(mmu_context_t *ctx, load_ptr_t dest, const void *src, load_size_t size);
extern bool mmu_memcpy_from(mmu_context_t *ctx, void *dest, load_ptr_t src, load_size_t size);

extern mmu_context_t *mmu_context_create(load_mode_t mode, unsigned phys_type);

#endif /* __MMU_H */
