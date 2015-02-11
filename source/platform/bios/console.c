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
 * @brief               BIOS platform console functions.
 */

#include <arch/io.h>

#include <lib/utility.h>

#include <drivers/serial/ns16550.h>
#include <drivers/video/vga.h>

#include <bios/bios.h>
#include <bios/vbe.h>

#include <console.h>
#include <loader.h>
#include <memory.h>
#include <video.h>

/** BIOS video mode structure. */
typedef struct bios_video_mode {
    video_mode_t mode;                  /**< Video mode structure. */
    uint16_t num;                       /**< Mode number. */
} bios_video_mode_t;

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

/** Set a BIOS video mode.
 * @param _mode         Mode to set.
 * @return              Status code describing the result of the operation. */
static status_t bios_video_set_mode(video_mode_t *_mode) {
    bios_video_mode_t *mode = (bios_video_mode_t *)_mode;
    bios_regs_t regs;

    bios_regs_init(&regs);
    regs.eax = VBE_FUNCTION_SET_MODE;
    regs.ebx = mode->num;
    bios_call(0x10, &regs);
    if ((regs.ax & 0xff00) != 0) {
        dprintf("bios: failed to set VBE mode 0x%" PRIx16 " (0x%" PRIx16 ")\n", mode->num, regs.ax);
        return STATUS_SYSTEM_ERROR;
    }

    return STATUS_SUCCESS;
}

/** BIOS VBE video operations. */
static video_ops_t bios_vbe_video_ops = {
    .set_mode = &bios_video_set_mode,
};

/** BIOS VGA video operations. */
static video_ops_t bios_vga_video_ops = {
    .console = &vga_console_out_ops,
    .set_mode = &bios_video_set_mode,
};

/** Detect VBE video modes. */
static void vbe_init(void) {
    vbe_info_t *info = (vbe_info_t *)BIOS_MEM_BASE;
    vbe_mode_info_t *mode_info = (vbe_mode_info_t *)(BIOS_MEM_BASE + sizeof(vbe_info_t));
    bios_regs_t regs;
    uint16_t *location;

    /* Try to get controller information. */
    strncpy(info->vbe_signature, VBE_SIGNATURE, 4);
    bios_regs_init(&regs);
    regs.eax = VBE_FUNCTION_CONTROLLER_INFO;
    regs.edi = (ptr_t)info;
    bios_call(0x10, &regs);
    if ((regs.eax & 0xff) != 0x4f) {
        dprintf("bios: VBE is not supported\n");
        return;
    } else if((regs.ax & 0xff00) != 0) {
        dprintf("bios: failed to obtain VBE information (0x%" PRIx16 ")\n", regs.ax);
        return;
    }

    /* Iterate through the modes. 0xffff indicates the end of the list. */
    location = (uint16_t *)segoff_to_linear(info->video_mode_ptr);
    for (size_t i = 0; location[i] != 0xffff; i++) {
        bios_video_mode_t *mode;

        bios_regs_init(&regs);
        regs.eax = VBE_FUNCTION_MODE_INFO;
        regs.ecx = location[i];
        regs.edi = (ptr_t)mode_info;
        bios_call(0x10, &regs);
        if ((regs.eax & 0xFF00) != 0) {
            dprintf("bios: failed to obtain VBE mode information (0x%" PRIx16 ")\n", regs.ax);
            continue;
        }

        /* Check if the mode is suitable. TODO: Indexed modes. */
        if (mode_info->memory_model != 6) {
            continue;
        } else if ((mode_info->mode_attributes & (1<<0)) == 0) {
            /* Not supported. */
            continue;
        } else if ((mode_info->mode_attributes & (1<<3)) == 0) {
            /* Not colour. */
            continue;
        } else if ((mode_info->mode_attributes & (1<<4)) == 0) {
            /* Not a graphics mode. */
            continue;
        } else if ((mode_info->mode_attributes & (1<<7)) == 0) {
            /* Not usable in linear mode. */
            continue;
        } else if (mode_info->bits_per_pixel < 8) {
            continue;
        } else if (mode_info->phys_base_ptr == 0) {
            continue;
        }

        /* Add the mode to the list. */
        mode = malloc(sizeof(*mode));
        mode->num = location[i] | VBE_MODE_LFB;
        mode->mode.type = VIDEO_MODE_LFB;
        mode->mode.ops = &bios_vbe_video_ops;
        mode->mode.width = mode_info->x_resolution;
        mode->mode.height = mode_info->y_resolution;
        mode->mode.bpp = mode_info->bits_per_pixel;
        mode->mode.pitch = (info->vbe_version_major >= 3)
            ? mode_info->lin_bytes_per_scan_line
            : mode_info->bytes_per_scan_line;
        mode->mode.red_size = mode_info->red_mask_size;
        mode->mode.red_pos = mode_info->red_field_position;
        mode->mode.green_size = mode_info->green_mask_size;
        mode->mode.green_pos = mode_info->green_field_position;
        mode->mode.blue_size = mode_info->blue_mask_size;
        mode->mode.blue_pos = mode_info->blue_field_position;
        mode->mode.mem_phys = mode->mode.mem_virt = mode_info->phys_base_ptr;
        mode->mode.mem_size = round_up(mode->mode.height * mode->mode.pitch, PAGE_SIZE);

        video_mode_register(&mode->mode, false);
    }
}

/** Initialize the console. */
void bios_console_init(void) {
    uint8_t status;
    bios_video_mode_t *mode;

    /* Initialize the serial port as the debug console. TODO: Disable for
     * non-debug builds? */
    status = in8(SERIAL_PORT + 6);
    if ((status & ((1<<4) | (1<<5))) && status != 0xff) {
        ns16550_init(SERIAL_PORT);
        ns16550_config(SERIAL_CLOCK, SERIAL_BAUD_RATE);
    }

    /* Register the VGA video mode, which will set the main console. */
    mode = malloc(sizeof(*mode));
    mode->num = 3;
    mode->mode.type = VIDEO_MODE_VGA;
    mode->mode.ops = &bios_vga_video_ops;
    mode->mode.width = VGA_COLS;
    mode->mode.height = VGA_ROWS;
    mode->mode.mem_phys = mode->mode.mem_virt = VGA_MEM_BASE;
    mode->mode.mem_size = round_up(VGA_COLS * VGA_ROWS * sizeof(uint16_t), PAGE_SIZE);
    video_mode_register(&mode->mode, true);

    /* Use BIOS for input. */
    main_console.in = &bios_console_in_ops;

    /* Detect VBE video modes. */
    vbe_init();
}
