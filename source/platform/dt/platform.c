/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               DT platform main functions.
 */

#include <drivers/disk/ramdisk.h>

#include <dt/console.h>
#include <dt/memory.h>

#include <console.h>
#include <device.h>
#include <dt.h>
#include <loader.h>

/** Main function of the DT loader.
 * @param fdt           Address of FDT. */
__noreturn void dt_main(void *fdt) {
    /* If we've built for a specific platform we can initialize an early debug
     * console. */
    dt_early_console_init();

    bool had_debug_console = debug_console != NULL;

    dprintf("\ndt: base @ %p, fdt @ %p\n", __start, fdt);
    dt_init(fdt);

    console_init();

    /* Print this now if we didn't have an early platform-specific console. */
    if (!had_debug_console)
        dprintf("\ndt: base @ %p, fdt @ %p\n", __start, fdt);

    arch_init();

    int len;
    const char *compatible = fdt_getprop(fdt, 0, "compatible", &len);
    if (compatible) {
        dprintf("dt: platform compatibility:");

        const char *end = compatible + len;
        while (compatible < end) {
            dprintf(" %s", compatible);
            compatible += strlen(compatible) + 1;
        }

        dprintf("\n");
    }

    const char *model = fdt_getprop(fdt, 0, "model", &len);
    if (model)
        dprintf("dt: platform model: %s\n", model);

    loader_main();
}

/** Detect and register all devices. */
void target_device_probe(void) {
    if (dt_initrd_size != 0)
        ramdisk_create("initrd", (void *)dt_initrd_address, dt_initrd_size, true);

    dt_device_probe();
}
