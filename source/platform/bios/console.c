/*
 * Copyright (C) 2014 Alex Smith
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
 * @brief               BIOS platform console functions.
 */

#include <arch/io.h>

#include <lib/utility.h>

#include <drivers/serial/ns16550.h>
#include <drivers/video/vga.h>

#include <bios/bios.h>

#include <console.h>
#include <memory.h>
#include <video.h>

/** Serial port definitions. */
#define SERIAL_PORT         0x3f8
#define SERIAL_CLOCK        1843200
#define SERIAL_BAUD_RATE    115200

/** VGA console definitions. */
#define VGA_MEM_BASE        0xb8000
#define VGA_COLS            80
#define VGA_ROWS            25

/** Check for a character from the console.
 * @return              Whether a character is available. */
static bool bios_console_poll(void) {
    bios_regs_t regs;

    bios_regs_init(&regs);
    regs.eax = 0x0100;
    bios_call(0x16, &regs);
    return !(regs.eflags & X86_FLAGS_ZF);
}

/** Read a character from the console.
 * @return              Character read. */
static uint16_t bios_console_getc(void) {
    bios_regs_t regs;
    uint8_t ascii, scan;

    bios_regs_init(&regs);

    /* INT16 AH=00h on Apple's BIOS emulation will hang forever if there is no
     * key available, so loop trying to poll for one first. */
    do {
        regs.eax = 0x0100;
        bios_call(0x16, &regs);
    } while (regs.eflags & X86_FLAGS_ZF);

    /* Get the key code. */
    regs.eax = 0x0000;
    bios_call(0x16, &regs);

    /* Convert certain scan codes to special keys. */
    ascii = regs.eax & 0xff;
    scan = (regs.eax >> 8) & 0xff;
    switch (scan) {
    case 0x48:
        return CONSOLE_KEY_UP;
    case 0x50:
        return CONSOLE_KEY_DOWN;
    case 0x4b:
        return CONSOLE_KEY_LEFT;
    case 0x4d:
        return CONSOLE_KEY_RIGHT;
    case 0x47:
        return CONSOLE_KEY_HOME;
    case 0x4f:
        return CONSOLE_KEY_END;
    case 0x53:
        return 0x7f;
    case 0x3b ... 0x44:
        return CONSOLE_KEY_F1 + (scan - 0x3b);
    default:
        /* Convert CR to LF. */
        return (ascii == '\r') ? '\n' : ascii;
    }
}

/** BIOS console input operations. */
static console_in_ops_t bios_console_in_ops = {
    .poll = bios_console_poll,
    .getc = bios_console_getc,
};

/** BIOS VGA video operations. */
static video_ops_t bios_vga_video_ops = {
    .console = &vga_console_out_ops,
};

/** Initialize the console. */
void bios_console_init(void) {
    uint8_t status;
    video_mode_t *mode;

    /* Initialize the serial port as the debug console. TODO: Disable for
     * non-debug builds? */
    status = in8(SERIAL_PORT + 6);
    if ((status & ((1<<4) | (1<<5))) && status != 0xff) {
        ns16550_init(SERIAL_PORT);
        ns16550_config(SERIAL_CLOCK, SERIAL_BAUD_RATE);
    }

    /* Register the VGA video mode, which will set the main console. TODO:
     * integrate with VBE code so we get a set_mode function which restores to
     * VGA mode. */
    mode = malloc(sizeof(*mode));
    mode->type = VIDEO_MODE_VGA;
    mode->ops = &bios_vga_video_ops;
    mode->width = VGA_COLS;
    mode->height = VGA_ROWS;
    mode->mem_phys = mode->mem_virt = VGA_MEM_BASE;
    mode->mem_size = round_up(VGA_COLS * VGA_ROWS * sizeof(uint16_t), PAGE_SIZE);
    video_mode_register(mode, true);

    /* Use BIOS calls for input. */
    main_console.in = &bios_console_in_ops;
}
