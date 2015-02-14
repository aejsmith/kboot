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
 * @brief               VGA console implementation.
 */

#include <arch/io.h>

#include <drivers/video/vga.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <console.h>
#include <loader.h>
#include <memory.h>
#include <video.h>

/** VGA console state. */
typedef struct vga_console {
    video_mode_t *mode;                 /**< Video mode in use. */
    uint16_t *mapping;                  /**< Mapping of the VGA console. */
    bool cursor_visible;                /**< Whether the cursor is currently visible. */
} vga_console_t;

/** Default attributes to use. */
#define VGA_ATTRIB      0x0700

/** Write a cell in VGA memory (character + attributes).
 * @param vga           VGA console.
 * @param x             X position.
 * @param y             Y position.
 * @param val           Cell value. */
static inline void write_cell(vga_console_t *vga, uint16_t x, uint16_t y, uint16_t val) {
    write16(&vga->mapping[(y * vga->mode->width) + x], val);
}

/** Write a character at a position, preserving attributes.
 * @param vga           VGA console.
 * @param x             X position.
 * @param y             Y position.
 * @param ch            Character to write. */
static inline void write_char(vga_console_t *vga, uint16_t x, uint16_t y, char ch) {
    uint16_t attrib;

    attrib = read16(&vga->mapping[(y * vga->mode->width) + x]) & 0xff00;
    write_cell(vga, x, y, attrib | ch);
}

/** Update the hardware cursor.
 * @param vga           VGA console. */
static void update_hw_cursor(vga_console_t *vga) {
    uint16_t x = (vga->cursor_visible) ? vga->mode->x : 0;
    uint16_t y = (vga->cursor_visible) ? vga->mode->y : (vga->mode->height + 1);
    uint16_t pos = (y * vga->mode->width) + x;

    out8(VGA_CRTC_INDEX, 14);
    out8(VGA_CRTC_DATA, pos >> 8);
    out8(VGA_CRTC_INDEX, 15);
    out8(VGA_CRTC_DATA, pos & 0xff);
}

/** Write a character to the console.
 * @param _vga          Pointer to VGA console.
 * @param ch            Character to write. */
static void vga_console_putc(void *_vga, char ch) {
    vga_console_t *vga = _vga;
    uint16_t i;

    switch (ch) {
    case '\b':
        /* Backspace, move back one character if we can. */
        if (vga->mode->x) {
            vga->mode->x--;
        } else if (vga->mode->y) {
            vga->mode->x = vga->mode->width - 1;
            vga->mode->y--;
        }

        break;
    case '\r':
        /* Carriage return, move to the start of the line. */
        vga->mode->x = 0;
        break;
    case '\n':
        /* Newline, treat it as if a carriage return was also there. */
        vga->mode->x = 0;
        vga->mode->y++;
        break;
    case '\t':
        vga->mode->x += 8 - (vga->mode->x % 8);
        break;
    default:
        /* If it is a non-printing character, ignore it. */
        if (ch < ' ')
            break;

        write_char(vga, vga->mode->x, vga->mode->y, ch);
        vga->mode->x++;
        break;
    }

    /* If we have reached the edge of the screen insert a new line. */
    if (vga->mode->x >= vga->mode->width) {
        vga->mode->x = 0;
        vga->mode->y++;
    }

    /* Scroll if we've reached the end of the draw region. */
    if (vga->mode->y >= vga->mode->height) {
        /* Shift up the content of the VGA memory. */
        memmove(vga->mapping, vga->mapping + vga->mode->width, (vga->mode->height - 1) * vga->mode->width * 2);

        /* Fill the last line with blanks. */
        for (i = 0; i < vga->mode->width; i++)
            write_cell(vga, i, vga->mode->height - 1, ' ' | VGA_ATTRIB);

        vga->mode->y = vga->mode->height - 1;
    }

    update_hw_cursor(vga);
}

/** Reset the console to a default state.
 * @param _vga          Pointer to VGA console. */
static void vga_console_reset(void *_vga) {
    vga_console_t *vga = _vga;
    uint16_t i, j;

    vga->mode->x = vga->mode->y = 0;
    vga->cursor_visible = true;

    update_hw_cursor(vga);

    for (i = 0; i < vga->mode->height; i++) {
        for (j = 0; j < vga->mode->width; j++)
            write_cell(vga, j, i, ' ' | VGA_ATTRIB);
    }

    update_hw_cursor(vga);
}

/** Initialize the VGA console.
 * @param mode          Video mode being used.
 * @return              Private data for the console. */
static void *vga_console_init(video_mode_t *mode) {
    vga_console_t *vga;

    assert(mode->type == VIDEO_MODE_VGA);

    vga = malloc(sizeof(*vga));
    vga->mode = mode;
    vga->mapping = (uint16_t *)mode->mem_virt;

    vga_console_reset(vga);

    return vga;
}

/** Deinitialize the console.
 * @param vga           Pointer to VGA console. */
static void vga_console_deinit(void *vga) {
    free(vga);
}

/** VGA main console output operations. */
console_out_ops_t vga_console_out_ops = {
    .putc = vga_console_putc,
    .reset = vga_console_reset,
    .init = vga_console_init,
    .deinit = vga_console_deinit,
};
