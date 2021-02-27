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
 * @brief               ARM64 MMU functions.
 */

#include <arch/page.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>
#include <mmu.h>

/** Create a mapping in an MMU context.
 * @param ctx           Context to map in.
 * @param virt          Virtual address to map.
 * @param phys          Physical address to map to.
 * @param size          Size of the mapping to create.
 * @return              Whether the supplied addresses were valid. */
bool mmu_map(mmu_context_t *ctx, load_ptr_t virt, phys_ptr_t phys, load_size_t size) {
    internal_error("TODO: mmu_map");
}

/** Set bytes in an area of virtual memory.
 * @param ctx           Context to use.
 * @param addr          Virtual address to write to, must be mapped.
 * @param value         Value to write.
 * @param size          Number of bytes to write.
 * @return              Whether the range specified was valid. */
bool mmu_memset(mmu_context_t *ctx, load_ptr_t addr, uint8_t value, load_size_t size) {
    internal_error("TODO: mmu_memset");
}

/** Copy to an area of virtual memory.
 * @param ctx           Context to use.
 * @param dest          Virtual address to write to, must be mapped.
 * @param src           Memory to read from.
 * @param size          Number of bytes to copy.
 * @return              Whether the range specified was valid. */
bool mmu_memcpy_to(mmu_context_t *ctx, load_ptr_t dest, const void *src, load_size_t size) {
    internal_error("TODO: mmu_memcpy_to");
}

/** Copy from an area of virtual memory.
 * @param ctx           Context to use.
 * @param dest          Memory to write to.
 * @param src           Virtual address to read from, must be mapped.
 * @param size          Number of bytes to copy.
 * @return              Whether the range specified was valid. */
bool mmu_memcpy_from(mmu_context_t *ctx, void *dest, load_ptr_t src, load_size_t size) {
    internal_error("TODO: mmu_memcpy_from");
}

/** Create a new MMU context.
 * @param mode          Load mode for the OS.
 * @param phys_type     Physical memory type to use when allocating tables.
 * @return              Pointer to context. */
mmu_context_t *mmu_context_create(load_mode_t mode, unsigned phys_type) {
    assert(mode == LOAD_MODE_64BIT);

    internal_error("TODO: mmu_context_create");
}
