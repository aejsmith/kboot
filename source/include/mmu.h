/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               MMU functions.
 */

#ifndef __MMU_H
#define __MMU_H

#include <loader.h>

/** Memory mapping flags. */
#define MMU_MAP_RO              (1<<0)  /**< Map as read-only. */
#define MMU_MAP_CACHE_SHIFT     1
#define MMU_MAP_CACHE_MASK      (3<<1)
#define MMU_MAP_CACHE_DEFAULT   (0<<1)  /**< Default caching behaviour. */
#define MMU_MAP_CACHE_WT        (1<<1)  /**< Map as write-through. */
#define MMU_MAP_CACHE_UC        (2<<1)  /**< Map as uncached. */

/** Type of an MMU context. */
typedef struct mmu_context mmu_context_t;

extern bool mmu_map(mmu_context_t *ctx, load_ptr_t virt, phys_ptr_t phys, load_size_t size, uint32_t flags);

extern bool mmu_memset(mmu_context_t *ctx, load_ptr_t addr, uint8_t value, load_size_t size);
extern bool mmu_memcpy_to(mmu_context_t *ctx, load_ptr_t dest, const void *src, load_size_t size);
extern bool mmu_memcpy_from(mmu_context_t *ctx, void *dest, load_ptr_t src, load_size_t size);

extern mmu_context_t *mmu_context_create(load_mode_t mode, unsigned phys_type);

#endif /* __MMU_H */
