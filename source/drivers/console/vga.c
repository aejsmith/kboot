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

#include <drivers/console/vga.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <console.h>
#include <loader.h>
#include <memory.h>
#include <video.h>

/** VGA console state. */
typedef struct vga_console_out {
    console_out_t console;              /**< Console output device header. */

    uint16_t *mapping;                  /**< Mapping of the VGA console. */
    draw_region_t region;               /**< Current draw region. */
    uint16_t attrib;                    /**< Current attributes. */
    bool cursor_visible;                /**< Whether the cursor is currently enabled. */
} vga_console_out_t;

/** Default attributes to use. */
#define VGA_ATTRIB      0x0700

/** Write a cell in VGA memory (character + attributes).
 * @param vga           VGA console.
 * @param x             X position.
 * @param y             Y position.
 * @param val           Cell value. */
static inline void write_cell(vga_console_out_t *vga, uint16_t x, uint16_t y, uint16_t val) {
    write16(&vga->mapping[(y * current_video_mode->width) + x], val);
}

/** Update the hardware cursor.
 * @param vga           VGA console. */
static void update_hw_cursor(vga_console_out_t *vga) {
    uint16_t x = (vga->cursor_visible) ? current_video_mode->x : 0;
    uint16_t y = (vga->cursor_visible) ? current_video_mode->y : (current_video_mode->height + 1);
    uint16_t pos = (y * current_video_mode->width) + x;

    out8(VGA_CRTC_INDEX, 14);
    out8(VGA_CRTC_DATA, pos >> 8);
    out8(VGA_CRTC_INDEX, 15);
    out8(VGA_CRTC_DATA, pos & 0xff);
}

/** Set the draw region of the console.
 * @param console       Console output device.
 * @param region        New draw region, or NULL to restore to whole console. */
static void vga_console_set_region(console_out_t *console, const draw_region_t *region) {
    vga_console_out_t *vga = (vga_console_out_t *)console;

    if (region) {
        assert(region->width && region->height);
        assert(region->x + region->width <= current_video_mode->width);
        assert(region->y + region->height <= current_video_mode->height);

        memcpy(&vga->region, region, sizeof(vga->region));
    } else {
        vga->region.x = vga->region.y = 0;
        vga->region.width = current_video_mode->width;
        vga->region.height = current_video_mode->height;
        vga->region.scrollable = true;
    }

    /* Move cursor to top of region. */
    current_video_mode->x = vga->region.x;
    current_video_mode->y = vga->region.y;
    update_hw_cursor(vga);
}

/** Get the current draw region.
 * @param console       Console output device.
 * @param region        Where to store details of the current draw region. */
static void vga_console_get_region(console_out_t *console, draw_region_t *region) {
    vga_console_out_t *vga = (vga_console_out_t *)console;

    memcpy(region, &vga->region, sizeof(*region));
}

/** Set the current colours.
 * @param console       Console output device.
 * @param fg            Foreground colour.
 * @param bg            Background colour. */
static void vga_console_set_colour(console_out_t *console, colour_t fg, colour_t bg) {
    vga_console_out_t *vga = (vga_console_out_t *)console;

    if (fg == COLOUR_DEFAULT)
        fg = CONSOLE_COLOUR_FG;
    if (bg == COLOUR_DEFAULT)
        bg = CONSOLE_COLOUR_BG;

    /* Colour values are defined to be the same as VGA colours. */
    vga->attrib = (fg << 8) | (bg << 12);
}

/** Set the cursor properties.
 * @param console       Console output device.
 * @param x             New X position (relative to draw region). Negative
 *                      values will move the cursor back from the right edge of
 *                      the draw region.
 * @param y             New Y position (relative to draw region). Negative
 *                      values will move the cursor up from the bottom edge of
 *                      the draw region.
 * @param visible       Whether the cursor should be visible. */
static void vga_console_set_cursor(console_out_t *console, int16_t x, int16_t y, bool visible) {
    vga_console_out_t *vga = (vga_console_out_t *)console;

    assert(abs(x) < vga->region.width);
    assert(abs(y) < vga->region.height);

    current_video_mode->x = (x < 0) ? vga->region.x + vga->region.width + x : vga->region.x + x;
    current_video_mode->y = (y < 0) ? vga->region.y + vga->region.height + y : vga->region.y + y;
    vga->cursor_visible = visible;
    update_hw_cursor(vga);
}

/** Get the cursor properties.
 * @param console       Console output device.
 * @param _x            Where to store X position (relative to draw region).
 * @param _y            Where to store Y position (relative to draw region).
 * @param _visible      Where to store whether the cursor is visible */
static void vga_console_get_cursor(console_out_t *console, uint16_t *_x, uint16_t *_y, bool *_visible) {
    vga_console_out_t *vga = (vga_console_out_t *)console;

    if (_x)
        *_x = current_video_mode->x - vga->region.x;
    if (_y)
        *_y = current_video_mode->y - vga->region.y;
    if (_visible)
        *_visible = vga->cursor_visible;
}

/** Clear an area to the current background colour.
 * @param console       Console output device.
 * @param x             Start X position (relative to draw region).
 * @param y             Start Y position (relative to draw region).
 * @param width         Width of the area (if 0, whole width is cleared).
 * @param height        Height of the area (if 0, whole height is cleared). */
static void vga_console_clear(console_out_t *console, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    vga_console_out_t *vga = (vga_console_out_t *)console;

    assert(x + width <= vga->region.width);
    assert(y + height <= vga->region.height);

    if (!width)
        width = vga->region.width - x;
    if (!height)
        height = vga->region.height - y;

    for (uint16_t i = 0; i < height; i++) {
        for (uint16_t j = 0; j < width; j++)
            write_cell(vga, vga->region.x + j, vga->region.y + i, ' ' | vga->attrib);
    }
}

/** Scroll the draw region up (move contents down).
 * @param console       Console output device. */
static void vga_console_scroll_up(console_out_t *console) {
    vga_console_out_t *vga = (vga_console_out_t *)console;

    /* Move everything down. */
    for (uint16_t i = vga->region.height - 1; i > 0; i--) {
        memmove(
            &vga->mapping[((vga->region.y + i) * current_video_mode->width) + vga->region.x],
            &vga->mapping[((vga->region.y + i - 1) * current_video_mode->width) + vga->region.x],
            vga->region.width * sizeof(*vga->mapping));
    }

    /* Fill the first row with blanks. */
    for (uint16_t j = 0; j < vga->region.width; j++)
        write_cell(vga, vga->region.x + j, vga->region.y, ' ' | vga->attrib);
}

/** Scroll the draw region down (move contents up).
 * @param console       Console output device. */
static void vga_console_scroll_down(console_out_t *console) {
    vga_console_out_t *vga = (vga_console_out_t *)console;

    /* Move everything up. */
    for (uint16_t i = 0; i < vga->region.height - 1; i++) {
        memmove(
            &vga->mapping[((vga->region.y + i) * current_video_mode->width) + vga->region.x],
            &vga->mapping[((vga->region.y + i + 1) * current_video_mode->width) + vga->region.x],
            vga->region.width * sizeof(*vga->mapping));
    }

    /* Fill the last row with blanks. */
    for (uint16_t j = 0; j < vga->region.width; j++)
        write_cell(vga, vga->region.x + j, vga->region.y + vga->region.height - 1, ' ' | vga->attrib);
}

/** Write a character to the console.
 * @param console       Console output device.
 * @param ch            Character to write. */
static void vga_console_putc(console_out_t *console, char ch) {
    vga_console_out_t *vga = (vga_console_out_t *)console;

    switch (ch) {
    case '\b':
        /* Backspace, move back one character if we can. */
        if (current_video_mode->x > vga->region.x) {
            current_video_mode->x--;
        } else if (current_video_mode->y > vga->region.y) {
            current_video_mode->x = vga->region.x + vga->region.width - 1;
            current_video_mode->y--;
        }

        break;
    case '\r':
        /* Carriage return, move to the start of the line. */
        current_video_mode->x = vga->region.x;
        break;
    case '\n':
        /* Newline, treat it as if a carriage return was also there. */
        current_video_mode->x = vga->region.x;
        current_video_mode->y++;
        break;
    case '\t':
        current_video_mode->x += 8 - (current_video_mode->x % 8);
        break;
    default:
        /* If it is a non-printing character, ignore it. */
        if (ch < ' ')
            break;

        write_cell(vga, current_video_mode->x, current_video_mode->y, (uint16_t)ch | vga->attrib);
        current_video_mode->x++;
        break;
    }

    /* If we have reached the edge of the screen insert a new line. */
    if (current_video_mode->x >= vga->region.x + vga->region.width) {
        current_video_mode->x = vga->region.x;
        current_video_mode->y++;
    }

    /* Scroll if we've reached the end of the draw region. */
    if (current_video_mode->y >= vga->region.y + vga->region.height) {
        if (vga->region.scrollable)
            vga_console_scroll_down(console);

        current_video_mode->y = vga->region.y + vga->region.height - 1;
    }

    update_hw_cursor(vga);
}

/** Reset the console to a default state.
 * @param console       Console output device. */
static void vga_console_reset(console_out_t *console) {
    vga_console_out_t *vga = (vga_console_out_t *)console;

    vga->cursor_visible = true;
    vga->attrib = (CONSOLE_COLOUR_FG << 8) | (CONSOLE_COLOUR_BG << 12);
    vga_console_set_region(console, NULL);

    for (uint16_t i = 0; i < current_video_mode->height; i++) {
        for (uint16_t j = 0; j < current_video_mode->width; j++)
            write_cell(vga, j, i, ' ' | vga->attrib);
    }
}

/** Initialize the VGA console.
 * @param console       Console output device. */
static void vga_console_init(console_out_t *console) {
    vga_console_out_t *vga = (vga_console_out_t *)console;

    assert(current_video_mode->type == VIDEO_MODE_VGA);

    vga->mapping = (uint16_t *)current_video_mode->mem_virt;
    vga_console_reset(console);
}

/** VGA console output operations. */
static console_out_ops_t vga_console_out_ops = {
    .set_region = vga_console_set_region,
    .get_region = vga_console_get_region,
    .set_colour = vga_console_set_colour,
    .set_cursor = vga_console_set_cursor,
    .get_cursor = vga_console_get_cursor,
    .clear = vga_console_clear,
    .scroll_up = vga_console_scroll_up,
    .scroll_down = vga_console_scroll_down,
    .putc = vga_console_putc,
    .reset = vga_console_reset,
    .init = vga_console_init,
};

/** Create a VGA console.
 * @return              VGA console output device. */
console_out_t *vga_console_create(void) {
    vga_console_out_t *vga = malloc(sizeof(*vga));
    vga->console.ops = &vga_console_out_ops;
    return &vga->console;
}
