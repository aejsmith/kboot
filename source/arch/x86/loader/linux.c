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
 * @brief               x86 Linux kernel loader.
 *
 * Currently we only support the 32-bit boot protocol, all 2.6 series and later
 * kernels support this as far as I know.
 *
 * Reference:
 *  - The Linux/x86 Boot Protocol
 *    http://lxr.linux.no/linux/Documentation/x86/boot.txt
 */

#include <x86/linux.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <loader/linux.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>

/** Check whether a Linux kernel image is valid.
 * @param loader        Loader internal data.
 * @return              Whether the kernel image is valid. */
bool linux_arch_check(linux_loader_t *loader) {
    linux_header_t header;
    status_t ret;

    /* Read in the kernel header. */
    ret = fs_read(loader->kernel, &header, sizeof(header), offsetof(linux_params_t, hdr));
    if (ret != STATUS_SUCCESS) {
        config_error("Error reading '%s': %pS", loader->path, ret);
        return false;
    }

    /* Check that this is a valid kernel image and that the version is
     * sufficient. We require at least protocol 2.03, earlier kernels don't
     * support the 32-bit boot protocol. */
    if (header.boot_flag != 0xaa55 || header.header != LINUX_MAGIC_SIGNATURE) {
        config_error("'%s' is not a Linux kernel image", loader->path);
        return false;
    } else if (header.version < 0x0203) {
        config_error("'%s' is too old (boot protocol 2.03 required)", loader->path);
        return false;
    } else if (!(header.loadflags & LINUX_LOAD_LOADED_HIGH)) {
        config_error("'%s' is not a bzImage kernel", loader->path);
        return false;
    }

    /* Check platform requirements. */
    return linux_platform_check(loader, &header);
}

/** Allocate memory to load the kernel to.
 * @param params        Kernel parameters structure.
 * @param size          Total load size required.
 * @param _phys         Where to store the allocated physical address.
 * @return              Pointer to virtual mapping of kernel, NULL on failure. */
static void *allocate_kernel(linux_params_t *params, size_t size, phys_ptr_t *_phys) {
    bool relocatable;
    size_t align, min_align;
    phys_ptr_t pref_addr;
    void *virt;

    if (params->hdr.version >= 0x0205 && params->hdr.relocatable_kernel) {
        relocatable = true;
        align = params->hdr.kernel_alignment;
        if (params->hdr.version >= 0x020a) {
            min_align = 1 << params->hdr.min_alignment;
            pref_addr = params->hdr.pref_address;
        } else {
            min_align = align;
            pref_addr = round_up(LINUX_BZIMAGE_ADDR, align);
        }
    } else {
        relocatable = false;
        align = min_align = 0;
        pref_addr = (params->hdr.version >= 0x020a)
            ? params->hdr.pref_address
            : LINUX_BZIMAGE_ADDR;
    }

    if (params->hdr.version >= 0x020a) {
        /* Protocol 2.10+ has a hint in the header which contains the amount of
         * memory the kernel requires to decompress itself. */
        assert(params->hdr.init_size >= size);
        size = round_up(params->hdr.init_size, PAGE_SIZE);
    } else {
        /* For earlier protocols, multiply the file size by 3 to account for
         * space required to decompress. This is the value that other boot
         * loaders use here. */
        size = round_up(size, PAGE_SIZE) * 3;
    }

    /* First try the preferred address. */
    virt = memory_alloc(
        size, 0, pref_addr, pref_addr + size, MEMORY_TYPE_ALLOCATED,
        MEMORY_ALLOC_CAN_FAIL, _phys);
    if (virt) {
        dprintf("linux: loading to preferred address 0x%" PRIxPHYS " (size: 0x%zx)\n", pref_addr, size);
        return virt;
    }

    /* If we're not relocatable we're now out of luck. */
    if (!relocatable)
        return NULL;

    /* Iterate down in powers of 2 until we reach the minimum alignment. */
    while (align >= min_align && align >= PAGE_SIZE) {
        virt = memory_alloc(size, align, 0x100000, 0, MEMORY_TYPE_ALLOCATED, MEMORY_ALLOC_CAN_FAIL, _phys);
        if (virt) {
            /* This is modifiable in 2.10+. Here, align will only be different
             * to the original value if 2.10+ (see above). */
            params->hdr.kernel_alignment = align;

            dprintf(
                "linux: loading to 0x%" PRIxPHYS " (size: 0x%zx, align: 0x%zx, min_align: 0x%zx)\n",
                *_phys, size, align, min_align);
            return virt;
        }

        align >>= 1;
    }

    return NULL;
}

/** Load an x86 Linux kernel.
 * @param loader        Loader internal data. */
__noreturn void linux_arch_load(linux_loader_t *loader) {
    linux_params_t *params;
    size_t cmdline_size, setup_size, load_size;
    char *cmdline;
    void *virt;
    phys_ptr_t phys, initrd_max;
    list_t memory_map;
    status_t ret;

    static_assert(sizeof(linux_params_t) == PAGE_SIZE);
    static_assert(offsetof(linux_params_t, hdr) == LINUX_HEADER_OFFSET);

    /* Allocate memory for the parameters data (the "zero page"). */
    params = memory_alloc(sizeof(linux_params_t), 0, 0x10000, 0x90000, MEMORY_TYPE_RECLAIMABLE, 0, NULL);
    memset(params, 0, sizeof(*params));

    /* Read in the kernel header. */
    ret = fs_read(loader->kernel, &params->hdr, sizeof(params->hdr), offsetof(linux_params_t, hdr));
    if (ret != STATUS_SUCCESS)
        boot_error("Error reading kernel header: %pS", ret);

    /* Start populating required fields in the header. Don't set heap_end_ptr or
     * the CAN_USE_HEAP flag, as these appear to only be required by the 16-bit
     * entry point which we do not use. */
    params->hdr.type_of_loader = 0xff;

    /* Calculate the maximum command line size. */
    cmdline_size = (params->hdr.version >= 0x0206) ? params->hdr.cmdline_size : 255;
    if (strlen(loader->cmdline) > cmdline_size)
        boot_error("Kernel command line is too long");

    /* Allocate memory for command line. */
    cmdline_size = round_up(cmdline_size + 1, PAGE_SIZE);
    cmdline = memory_alloc(cmdline_size, 0, 0x10000, 0x90000, MEMORY_TYPE_RECLAIMABLE, 0, NULL);
    strncpy(cmdline, loader->cmdline, cmdline_size);
    cmdline[cmdline_size] = 0;
    params->hdr.cmd_line_ptr = (uint32_t)cmdline;

    /* Determine the setup code size. */
    if (params->hdr.setup_sects) {
        setup_size = (params->hdr.setup_sects + 1) * 512;
    } else {
        setup_size = 5 * 512;
    }

    /* Load size is total file size minus setup size. */
    load_size = loader->kernel->size - setup_size;

    /* Allocate memory for the kernel image. */
    virt = allocate_kernel(params, load_size, &phys);
    if (!virt)
        boot_error("Insufficient memory available for kernel image");

    params->hdr.code32_start = phys + params->hdr.code32_start - LINUX_BZIMAGE_ADDR;

    /* Read in the kernel image. */
    ret = fs_read(loader->kernel, virt, load_size, setup_size);
    if (ret != STATUS_SUCCESS)
        boot_error("Error reading kernel image: %pS", ret);

    /* Load in the initrd(s). */
    if (loader->initrd_size) {
        initrd_max = (params->hdr.version >= 0x0203)
            ? params->hdr.initrd_addr_max
            : 0x37ffffff;

        /* It is recommended that the initrd be loaded as high as possible. */
        virt = memory_alloc(
            round_up(loader->initrd_size, PAGE_SIZE), 0,
            0x100000, initrd_max + 1, MEMORY_TYPE_MODULES, MEMORY_ALLOC_HIGH, &phys);

        dprintf(
            "linux: loading initrd to 0x%" PRIxPHYS " (size: 0x%zx, max: 0x%" PRIxPHYS ")\n",
            phys, loader->initrd_size, initrd_max);

        linux_initrd_load(loader, virt);

        params->hdr.ramdisk_image = phys;
        params->hdr.ramdisk_size = loader->initrd_size;
    }

    /* Set the video mode. */
    linux_video_set(loader);

    /* Perform pre-boot tasks. */
    loader_preboot();

    /* Get the final memory map and print it out for informational purposes.
     * Note that the memory_finalize() is necessary on EFI in order to free up
     * any internal allocations in the EFI memory map so that they will be free
     * to the kernel. */
    dprintf("linux: final physical memory map:\n");
    memory_finalize(&memory_map);
    memory_map_dump(&memory_map);

    /* Get platform code to do any setup it needs and enter the kernel.
     * For BIOS, this will obtain information usually obtained by the real-mode
     * bootstrap when using the 16-bit boot protocol, then jump to the 32-bit
     * entry point. For EFI, this will enter the kernel using the handover
     * protocol. */
    linux_platform_load(loader, params);
}
