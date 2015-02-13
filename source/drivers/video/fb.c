/*
 * Copyright (C) 2015 Alex Smith
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
 * @brief               Framebuffer console implementation.
 *
 * Notes:
 *  - The framebuffer console must be initialized after memory detection is done
 *    as it uses the physical memory manager to allocate a backbuffer.
 */

#include <drivers/video/fb.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>
#include <video.h>

extern unsigned char console_font[];

/** Dimensions and colours of the console font. */
#define FONT_WIDTH              8
#define FONT_HEIGHT             16
#define FONT_FG                 0xaaaaaa
#define FONT_BG                 0x000000

/** Framebuffer console mode. */
static video_mode_t *fb_mode;

/** Framebuffer mapping and backbuffer. */
static uint8_t *fb_mapping;
static uint8_t *fb_backbuffer;

/** Framebuffer console state. */
static uint16_t fb_console_cols;
static uint16_t fb_console_rows;
static uint16_t fb_console_x;
static uint16_t fb_console_y;

/** Cache of the glyphs on the console. */
static char *fb_console_glyphs;

/** Get the byte offset of a pixel.
 * @param x             X position of pixel.
 * @param y             Y position of pixel.
 * @return              Byte offset of the pixel. */
static inline size_t fb_offset(uint32_t x, uint32_t y) {
    return (y * fb_mode->pitch) + (x * (fb_mode->bpp >> 3));
}

/** Convert an R8G8B8 value to the framebuffer format.
 * @param rgb           32-bit RGB value.
 * @return              Calculated pixel value. */
static inline uint32_t rgb888_to_fb(uint32_t rgb) {
    uint32_t red = ((rgb >> (24 - fb_mode->red_size)) & ((1 << fb_mode->red_size) - 1)) << fb_mode->red_pos;
    uint32_t green = ((rgb >> (16 - fb_mode->green_size)) & ((1 << fb_mode->green_size) - 1)) << fb_mode->green_pos;
    uint32_t blue = ((rgb >> (8 - fb_mode->blue_size)) & ((1 << fb_mode->blue_size) - 1)) << fb_mode->blue_pos;

    return red | green | blue;
}

/** Put a pixel on the framebuffer.
 * @param x             X position.
 * @param y             Y position.
 * @param rgb           RGB colour to draw. */
static void fb_putpixel(uint16_t x, uint16_t y, uint32_t rgb) {
    size_t offset = fb_offset(x, y);
    void *fb = fb_mapping + offset;
    void *bb = fb_backbuffer + offset;
    uint32_t value = rgb888_to_fb(rgb);

    switch (fb_mode->bpp >> 3) {
    case 2:
        *(uint16_t *)fb = (uint16_t)value;
        *(uint16_t *)bb = (uint16_t)value;
        break;
    case 3:
        ((uint16_t *)fb)[0] = value & 0xffff;
        ((uint8_t *)fb)[2] = (value >> 16) & 0xff;
        ((uint16_t *)bb)[0] = value & 0xffff;
        ((uint8_t *)bb)[2] = (value >> 16) & 0xff;
        break;
    case 4:
        *(uint32_t *)fb = value;
        *(uint32_t *)bb = value;
        break;
    }
}

/** Draw a rectangle in a solid colour.
 * @param x             X position of rectangle.
 * @param y             Y position of rectangle.
 * @param width         Width of rectangle.
 * @param height        Height of rectangle.
 * @param rgb           Colour to draw in. */
static void fb_fillrect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t rgb) {
    if (x == 0 && width == fb_mode->width && (rgb == 0 || rgb == 0xffffff)) {
        /* Fast path where we can fill a block quickly. */
        memset(fb_mapping + (y * fb_mode->pitch), (uint8_t)rgb, height * fb_mode->pitch);
        memset(fb_backbuffer + (y * fb_mode->pitch), (uint8_t)rgb, height * fb_mode->pitch);
    } else {
        for (uint32_t i = 0; i < height; i++) {
            for (uint32_t j = 0; j < width; j++)
                fb_putpixel(x + j, y + i, rgb);
        }
    }
}

/** Copy part of the framebuffer to another location.
 * @param dest_x        X position of destination.
 * @param dest_y        Y position of destination.
 * @param src_x         X position of source area.
 * @param src_y         Y position of source area.
 * @param width         Width of area to copy.
 * @param height        Height of area to copy. */
static void fb_copyrect(
    uint32_t dest_x, uint32_t dest_y, uint32_t src_x, uint32_t src_y,
    uint32_t width, uint32_t height)
{
    size_t dest_offset, src_offset;

    if (dest_x == 0 && src_x == 0 && width == fb_mode->width) {
        /* Fast path where we can copy everything in one go. */
        dest_offset = dest_y * fb_mode->pitch;
        src_offset = src_y * fb_mode->pitch;

        /* Copy everything on the backbuffer, then copy the affected section to
         * the main framebuffer. */
        memmove(fb_backbuffer + dest_offset, fb_backbuffer + src_offset, height * fb_mode->pitch);
        memcpy(fb_mapping + dest_offset, fb_backbuffer + dest_offset, height * fb_mode->pitch);
    } else {
        /* Copy line by line. */
        for (uint32_t i = 0; i < height; i++) {
            dest_offset = fb_offset(dest_x, dest_y + i);
            src_offset = fb_offset(src_x, src_y + i);

            memmove(fb_backbuffer + dest_offset, fb_backbuffer + src_offset, width * (fb_mode->bpp >> 3));
            memcpy(fb_mapping + dest_offset, fb_backbuffer + dest_offset, width * (fb_mode->bpp >> 3));
        }
    }
}

/** (Re)draw the glyph at the specified position the console.
 * @param x             X position (characters).
 * @param y             Y position (characters).
 * @param fg            Foreground colour.
 * @param bg            Background colour. */
static void draw_glyph(uint16_t x, uint16_t y, uint32_t fg, uint32_t bg) {
    char ch = fb_console_glyphs[(y * fb_console_cols) + x];

    /* Convert to a pixel position. */
    x *= FONT_WIDTH;
    y *= FONT_HEIGHT;

    /* Draw the glyph. */
    for (uint16_t i = 0; i < FONT_HEIGHT; i++) {
        for (uint16_t j = 0; j < FONT_WIDTH; j++) {
            if (console_font[(ch * FONT_HEIGHT) + i] & (1 << (7 - j))) {
                fb_putpixel(x + j, y + i, fg);
            } else {
                fb_putpixel(x + j, y + i, bg);
            }
        }
    }
}

/** Enable the cursor. */
static void enable_cursor(void) {
    /* Draw in inverted colours. */
    draw_glyph(fb_console_x, fb_console_y, FONT_BG, FONT_FG);
}

/** Disable the cursor. */
static void disable_cursor(void) {
    /* Draw back in the correct colours. */
    draw_glyph(fb_console_x, fb_console_y, FONT_FG, FONT_BG);
}

/** Write a character to the console.
 * @param ch            Character to write. */
static void fb_console_putc(char ch) {
    disable_cursor();

    switch (ch) {
    case '\b':
        /* Backspace, move back one character if we can. */
        if (fb_console_x) {
            fb_console_x--;
        } else if (fb_console_y) {
            fb_console_x = fb_console_cols - 1;
            fb_console_y--;
        }

        break;
    case '\r':
        /* Carriage return, move to the start of the line. */
        fb_console_x = 0;
        break;
    case '\n':
        /* Newline, treat it as if a carriage return was there (will be handled
         * below). */
        fb_console_x = fb_console_cols;
        break;
    case '\t':
        fb_console_x += 8 - (fb_console_x % 8);
        break;
    default:
        /* If it is a non-printing character, ignore it. */
        if (ch < ' ')
            break;

        fb_console_glyphs[(fb_console_y * fb_console_cols) + fb_console_x] = ch;
        draw_glyph(fb_console_x, fb_console_y, FONT_FG, FONT_BG);
        fb_console_x++;
        break;
    }

    /* If we have reached the edge of the screen insert a new line. */
    if (fb_console_x >= fb_console_cols) {
        fb_console_x = 0;
        if (++fb_console_y < fb_console_rows)
            fb_fillrect(0, FONT_HEIGHT * fb_console_y, fb_mode->width, FONT_HEIGHT, FONT_BG);
    }

    /* If we have reached the bottom of the screen, scroll. */
    if (fb_console_y >= fb_console_rows) {
        /* Move everything up and fill the last row with blanks. */
        memmove(fb_console_glyphs, fb_console_glyphs + fb_console_cols, (fb_console_rows - 1) * fb_console_cols);
        memset(fb_console_glyphs + ((fb_console_rows - 1) * fb_console_cols), ' ', fb_console_cols);
        fb_copyrect(0, 0, 0, FONT_HEIGHT, fb_mode->width, (fb_console_rows - 1) * FONT_HEIGHT);
        fb_fillrect(0, FONT_HEIGHT * (fb_console_rows - 1), fb_mode->width, FONT_HEIGHT, FONT_BG);

        /* Update the cursor position. */
        fb_console_y = fb_console_rows - 1;
    }

    enable_cursor();
}

/** Reset the console to a default state. */
static void fb_console_reset(void) {
    fb_console_x = fb_console_y = 0;

    fb_fillrect(0, 0, fb_mode->width, fb_mode->height, FONT_BG);
    enable_cursor();
}

/** Initialize the console.
 * @param mode          Video mode being used. */
static void fb_console_init(video_mode_t *mode) {
    size_t size;

    assert(mode->type == VIDEO_MODE_LFB);

    fb_mode = mode;
    fb_mapping = (uint8_t *)mode->mem_virt;
    fb_console_cols = mode->width / FONT_WIDTH;
    fb_console_rows = mode->height / FONT_HEIGHT;
    fb_console_x = fb_console_y = 0;

    /* Allocate a backbuffer and glyph cache. */
    size = round_up(mode->pitch * mode->height, PAGE_SIZE);
    fb_backbuffer = memory_alloc(size, 0, 0, 0, MEMORY_TYPE_INTERNAL, MEMORY_ALLOC_HIGH, NULL);
    if (!fb_backbuffer)
        internal_error("Failed to allocate console backbuffer");
    memset(fb_backbuffer, 0, size);

    size = round_up(fb_console_cols * fb_console_rows * sizeof(*fb_console_glyphs), PAGE_SIZE);
    fb_console_glyphs = memory_alloc(size, 0, 0, 0, MEMORY_TYPE_INTERNAL, MEMORY_ALLOC_HIGH, NULL);
    if (!fb_console_glyphs)
        internal_error("Failed to allocate console glyph cache");
    memset(fb_console_glyphs, 0, size);
}

/** Deinitialize the console. */
static void fb_console_deinit(void) {
    size_t size;

    size = round_up(fb_mode->pitch * fb_mode->height, PAGE_SIZE);
    memory_free(fb_backbuffer, size);

    size = round_up(fb_console_cols * fb_console_rows * sizeof(*fb_console_glyphs), PAGE_SIZE);
    memory_free(fb_console_glyphs, size);
}

/** Framebuffer console output operations. */
console_out_ops_t fb_console_out_ops = {
    .putc = fb_console_putc,
    .reset = fb_console_reset,
    .init = fb_console_init,
    .deinit = fb_console_deinit,
};
