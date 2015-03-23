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
 * @brief               BIOS platform main functions.
 */

#include <arch/io.h>

#include <x86/cpu.h>
#include <x86/descriptor.h>

#include <bios/bios.h>
#include <bios/disk.h>
#include <bios/multiboot.h>
#include <bios/pxe.h>

#include <console.h>
#include <device.h>
#include <loader.h>
#include <memory.h>
#include <time.h>

/** Main function of the BIOS loader. */
__noreturn void bios_main(void) {
    console_init();
    bios_video_init();

    arch_init();

    loader_main();
}

/** Detect and register all devices. */
void target_device_probe(void) {
    bios_disk_init();
    multiboot_init();
    pxe_init();
}

/** Reboot the system. */
void target_reboot(void) {
    uint8_t val;

    /* Try the keyboard controller. */
    do {
        val = in8(0x64);
        if (val & (1 << 0))
            in8(0x60);
    } while (val & (1 << 1));
    out8(0x64, 0xfe);
    delay(100);

    /* Fall back on a triple fault. */
    x86_lidt(0, 0);
    __asm__ volatile("ud2");

    while (true)
        ;
}
