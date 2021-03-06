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
}
