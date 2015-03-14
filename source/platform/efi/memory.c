/*
 * Copyright (C) 2014-2015 Alex Smith
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
 * @brief               EFI memory allocation functions.
 *
 * On EFI, we don't use the generic memory management code. This is because
 * while we're still in boot services mode, the firmware is in control of the
 * memory map and we should use the memory allocation services to allocate
 * memory. Since it is possible for the memory map to change underneath us, we
 * cannot just get the memory map once during init and use it with the generic
 * MM code.
 *
 * The AllocatePages boot service cannot provide all the functionality of
 * memory_alloc() (no alignment or minimum address constraints). Therefore,
 * we implement memory_alloc() by getting the current memory map each time it
 * is called and scanning it for a suitable range, and then allocating an exact
 * range with AllocatePages.
 *
 * There is a widespread bug which prevents the use of user-defined memory type
 * values, which causes the firmware to crash if a value outside of the pre-
 * defined value range is used. To avoid this we keep track of range types
 * ourself rather than storing it as a user-defined memory type.
 */

#include <lib/list.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <efi/efi.h>
#include <efi/memory.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>

/** EFI specifies page size as 4KB regardless of the system. */
#define EFI_PAGE_SIZE       0x1000

/** List of allocated memory ranges. */
static LIST_DECLARE(efi_memory_ranges);

/**
 * Get the current memory map.
 *
 * Gets a copy of the current memory map. This function is a wrapper for the
 * EFI GetMemoryMap boot service which handles allocation of an appropriately
 * sized buffer, and ensures that the array entries are contiguous (the
 * descriptor size returned by the firmware can change in future).
 *
 * @param _memory_map   Where to store pointer to memory map.
 * @param _num_entries  Where to store number of entries in memory map.
 * @param _map_key      Where to store the key for the current memory map.
 *
 * @return              EFI status code.
 */
static efi_status_t efi_get_memory_map(
    efi_memory_descriptor_t **_memory_map, efi_uintn_t *_num_entries,
    efi_uintn_t *_map_key)
{
    efi_memory_descriptor_t *memory_map = NULL, *orig;
    efi_uintn_t size = 0, descriptor_size, num_entries, i;
    efi_uint32_t descriptor_version;
    efi_status_t ret;

    /* Call a first time to get the needed buffer size. */
    ret = efi_call(
        efi_boot_services->get_memory_map,
        &size, memory_map, _map_key, &descriptor_size, &descriptor_version);
    if (ret != EFI_SUCCESS && ret != EFI_BUFFER_TOO_SMALL)
        return ret;

    num_entries = size / descriptor_size;

    if (ret == EFI_BUFFER_TOO_SMALL) {
        memory_map = malloc(size);

        ret = efi_call(
            efi_boot_services->get_memory_map,
            &size, memory_map, _map_key, &descriptor_size, &descriptor_version);
        if (ret != EFI_SUCCESS) {
            free(memory_map);
            return ret;
        }

        if (descriptor_size != sizeof(*memory_map)) {
            orig = memory_map;
            memory_map = malloc(num_entries * sizeof(*memory_map));

            for (i = 0; i < num_entries; i++) {
                memcpy(&memory_map[i],
                    (void *)orig + (descriptor_size * i),
                    min(descriptor_size, sizeof(*memory_map)));
            }

            free(orig);
        }
    }

    *_memory_map = memory_map;
    *_num_entries = num_entries;
    return ret;
}

/** Check whether a range can satisfy an allocation.
 * @param range         Memory descriptor for range to check.
 * @param size          Size of the allocation.
 * @param align         Alignment of the allocation.
 * @param min_addr      Minimum address for the start of the allocated range.
 * @param max_addr      Maximum address of the end of the allocated range.
 * @param flags         Behaviour flags.
 * @param _phys         Where to store address for allocation.
 * @return              Whether the range can satisfy the allocation. */
static bool is_suitable_range(
    efi_memory_descriptor_t *range, phys_size_t size, phys_size_t align,
    phys_ptr_t min_addr, phys_ptr_t max_addr, unsigned flags,
    efi_physical_address_t *_phys)
{
    phys_ptr_t start, range_end, match_start, match_end;

    if (range->type != EFI_CONVENTIONAL_MEMORY)
        return false;

    range_end = range->physical_start + (range->num_pages * EFI_PAGE_SIZE) - 1;

    /* Check if this range contains addresses in the requested range. */
    match_start = max(min_addr, range->physical_start);
    match_end = min(max_addr, range_end);
    if (match_end <= match_start)
        return false;

    /* Align the base address and check that the range fits. */
    if (flags & MEMORY_ALLOC_HIGH) {
        start = round_down((match_end - size) + 1, align);
        if (start < match_start)
            return false;
    } else {
        start = round_up(match_start, align);
        if ((start + size - 1) > match_end)
            return false;
    }

    *_phys = start;
    return true;
}

/** Sort comparison function for the EFI memory map. */
static int forward_sort_compare(const void *a, const void *b) {
    const efi_memory_descriptor_t *first = a;
    const efi_memory_descriptor_t *second = b;

    if (first->physical_start > second->physical_start) {
        return 1;
    } else if (first->physical_start < second->physical_start) {
        return -1;
    } else {
        return 0;
    }
}

/** Reverse sort comparison function for the EFI memory map. */
static int reverse_sort_compare(const void *a, const void *b) {
    const efi_memory_descriptor_t *first = a;
    const efi_memory_descriptor_t *second = b;

    if (second->physical_start > first->physical_start) {
        return 1;
    } else if (second->physical_start < first->physical_start) {
        return -1;
    } else {
        return 0;
    }
}

/** Allocate a range of physical memory.
 * @param size          Size of the range (multiple of PAGE_SIZE).
 * @param align         Alignment of the range (power of 2, at least PAGE_SIZE).
 * @param min_addr      Minimum address for the start of the allocated range.
 * @param max_addr      Maximum address of the last byte of the allocated range.
 * @param type          Type to give the allocated range.
 * @param flags         Behaviour flags.
 * @param _phys         Where to store physical address of allocation.
 * @return              Virtual address of allocation on success, NULL on failure. */
void *memory_alloc(
    phys_size_t size, phys_size_t align, phys_ptr_t min_addr, phys_ptr_t max_addr,
    uint8_t type, unsigned flags, phys_ptr_t *_phys)
{
    efi_memory_descriptor_t *memory_map __cleanup_free = NULL;
    efi_uintn_t num_entries, map_key;
    efi_status_t ret;

    assert(!(size % PAGE_SIZE));
    assert(!(align % PAGE_SIZE));
    assert(type != MEMORY_TYPE_FREE);

    if (!align)
        align = PAGE_SIZE;
    if (!min_addr)
        min_addr = TARGET_PHYS_MIN;
    if (!max_addr || max_addr > TARGET_PHYS_MAX)
        max_addr = TARGET_PHYS_MAX;

    assert(!(size % PAGE_SIZE));
    assert((max_addr - min_addr) >= (size - 1));
    assert(type != MEMORY_TYPE_FREE);

    /* Get the current memory map. */
    ret = efi_get_memory_map(&memory_map, &num_entries, &map_key);
    if (ret != EFI_SUCCESS)
        internal_error("Failed to get memory map (0x%zx)", ret);

    /* EFI does not specify that the memory map is sorted, so make sure it is.
     * Sort in forward or reverse order depending on whether we want to allocate
     * highest possible address first. */
    qsort(memory_map, num_entries, sizeof(*memory_map),
        (flags & MEMORY_ALLOC_HIGH) ? reverse_sort_compare : forward_sort_compare);

    /* Find a free range that is large enough to hold the new range. */
    for (efi_uintn_t i = 0; i < num_entries; i++) {
        efi_physical_address_t start;
        memory_range_t *range;

        if (is_suitable_range(&memory_map[i], size, align, min_addr, max_addr, flags, &start)) {
            /* Ask the firmware to allocate this exact address. Should succeed
             * as it is marked in the memory map as free, so raise an error if
             * this fails. */
            ret = efi_call(
                efi_boot_services->allocate_pages,
                EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA, size / EFI_PAGE_SIZE, &start);
            if (ret != STATUS_SUCCESS)
                internal_error("Failed to allocate memory (0x%zx)", ret);

            /* Add a structure to track the allocation type (see comment at top). */
            range = malloc(sizeof(*range));
            range->start = start;
            range->size = size;
            range->type = type;
            list_init(&range->header);
            list_append(&efi_memory_ranges, &range->header);

            dprintf(
                "memory: allocated 0x%" PRIxPHYS "-0x%" PRIxPHYS " (align: 0x%" PRIxPHYS ", type: %u)\n",
                start, start + size, align, type);

            if (_phys)
                *_phys = start;

            return (void *)phys_to_virt(start);
        }
    }

    if (flags & MEMORY_ALLOC_CAN_FAIL) {
        return NULL;
    } else {
        boot_error("Insufficient memory available (allocating %" PRIuPHYS " bytes)", size);
    }
}

/** Free a range of physical memory.
 * @param addr          Virtual address of allocation.
 * @param size          Size of range to free. */
void memory_free(void *addr, phys_size_t size) {
    phys_ptr_t phys = virt_to_phys((ptr_t)addr);

    assert(!(phys % PAGE_SIZE));
    assert(!(size % PAGE_SIZE));

    list_foreach(&efi_memory_ranges, iter) {
        memory_range_t *range = list_entry(iter, memory_range_t, header);

        if (range->start == phys) {
            efi_status_t ret;

            if (range->size != size) {
                internal_error(
                    "Bad memory_free size 0x%" PRIxPHYS " (expected 0x%" PRIxPHYS ")",
                    size, range->size);
            }

            ret = efi_call(efi_boot_services->free_pages, phys, size / EFI_PAGE_SIZE);
            if (ret != EFI_SUCCESS)
                internal_error("Failed to free EFI memory (0x%zx)", ret);

            list_remove(&range->header);
            free(range);
            return;
        }
    }

    internal_error("Bad memory_free address 0x%" PRIxPHYS, phys);
}

/**
 * Finalize the memory map.
 *
 * This should be called once all memory allocations have been performed. It
 * marks all internal memory ranges as free and returns the final memory map
 * to be passed to the OS.
 *
 * @param map           Head of list to place the memory map into.
 */
void memory_finalize(list_t *map) {
    efi_memory_descriptor_t *efi_map __cleanup_free = NULL;
    efi_uintn_t num_entries, map_key;
    efi_status_t ret;

    list_init(map);

    /* Get the current memory map. */
    ret = efi_get_memory_map(&efi_map, &num_entries, &map_key);
    if (ret != EFI_SUCCESS)
        internal_error("Failed to get memory map (0x%zx)", ret);

    /* Add all free ranges to the memory map. */
    for (efi_uintn_t i = 0; i < num_entries; i++) {
        switch (efi_map[i].type) {
        case EFI_CONVENTIONAL_MEMORY:
        case EFI_BOOT_SERVICES_CODE:
        case EFI_BOOT_SERVICES_DATA:
        case EFI_LOADER_CODE:
        case EFI_LOADER_DATA:
            memory_map_insert(map,
                efi_map[i].physical_start,
                efi_map[i].num_pages * EFI_PAGE_SIZE,
                MEMORY_TYPE_FREE);
            break;
        }
    }

    /* Mark all ranges allocated by memory_alloc() with the correct type. */
    list_foreach(&efi_memory_ranges, iter) {
        memory_range_t *range = list_entry(iter, memory_range_t, header);

        memory_map_insert(
            map, range->start, range->size,
            (range->type == MEMORY_TYPE_INTERNAL) ? MEMORY_TYPE_FREE : range->type);
    }
}

/** Initialize the EFI memory allocator. */
void efi_memory_init(void) {
    efi_memory_descriptor_t *memory_map;
    efi_uintn_t num_entries, map_key, i;
    efi_status_t ret;

    /* For informational purposes, we print out a list of all the usable memory
     * we see in the memory map. Don't print out everything, the memory map is
     * probably pretty big (e.g. OVMF under QEMU returns a map with nearly 50
     * entries here). */
    ret = efi_get_memory_map(&memory_map, &num_entries, &map_key);
    if (ret != EFI_SUCCESS)
        internal_error("Failed to get memory map: 0x%zx\n", ret);

    dprintf("efi: usable memory ranges (%zu total):\n", num_entries);
    for (i = 0; i < num_entries; i++) {
        if (memory_map[i].type != EFI_CONVENTIONAL_MEMORY)
            continue;

        dprintf(
            " 0x%016" PRIxPHYS "-0x%016" PRIxPHYS " (%" PRIu64 " KiB)\n",
            memory_map[i].physical_start,
            memory_map[i].physical_start + (memory_map[i].num_pages * EFI_PAGE_SIZE),
            (memory_map[i].num_pages * EFI_PAGE_SIZE) / 1024);
    }

    free(memory_map);
}

/** Release all allocated memory. */
void efi_memory_cleanup(void) {
    list_foreach_safe(&efi_memory_ranges, iter) {
        memory_range_t *range = list_entry(iter, memory_range_t, header);
        efi_status_t ret;

        ret = efi_call(efi_boot_services->free_pages, range->start, range->size / EFI_PAGE_SIZE);
        if (ret != EFI_SUCCESS)
            internal_error("Failed to free EFI memory (0x%zx)", ret);

        list_remove(&range->header);
        free(range);
    }
}
