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
 */

#include <drivers/video/fb.h>

#include <lib/string.h>

#include <assert.h>
#include <loader.h>
#include <video.h>

extern unsigned char console_font[];

/** Dimensions and colours of the console font. */
#define FONT_WIDTH              8
#define FONT_HEIGHT             16
#define FONT_FG                 0xaaaaaa
#define FONT_BG                 0x000000

/** Framebuffer console mode. */
static video_mode_t *fb_mode;

/** Framebuffer mapping. */
static uint8_t *fb_mapping;

/** Framebuffer console state. */
static uint16_t fb_console_cols;
static uint16_t fb_console_rows;
static uint16_t fb_console_x;
static uint16_t fb_console_y;

/** Get the byte offset of a pixel.
 * @param x             X position of pixel.
 * @param y             Y position of pixel.
 * @return              Byte offset of the pixel. */
static inline size_t fb_offset(uint32_t x, uint32_t y) {
    return (((y * fb_mode->width) + x) * (fb_mode->bpp >> 3));
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
 * @param x     X position.
 * @param y     Y position.
 * @param rgb       RGB colour to draw. */
static void fb_putpixel(uint16_t x, uint16_t y, uint32_t rgb) {
    size_t offset = fb_offset(x, y);
    void *dest = fb_mapping + offset;
    uint32_t value = rgb888_to_fb(rgb);

    switch (fb_mode->bpp >> 3) {
    case 2:
        *(uint16_t *)dest = (uint16_t)value;
        break;
    case 3:
        ((uint16_t *)dest)[0] = value & 0xffff;
        ((uint8_t *)dest)[2] = (value >> 16) & 0xff;
        break;
    case 4:
        *(uint32_t *)dest = value;
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
        memset(fb_mapping + fb_offset(0, y), (uint8_t)rgb, width * height * (fb_mode->bpp >> 3));
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
        dest_offset = fb_offset(0, dest_y);
        src_offset = fb_offset(0, src_y);

        /* Copy everything on the backbuffer. */
        memmove(fb_mapping + dest_offset, fb_mapping + src_offset, fb_mode->width * height * (fb_mode->bpp >> 3));
    } else {
        /* Copy line by line. */
        for (uint32_t i = 0; i < height; i++) {
            dest_offset = fb_offset(dest_x, dest_y + i);
            src_offset = fb_offset(src_x, src_y + i);

            /* Copy everything on the backbuffer. */
            memmove(fb_mapping + dest_offset, fb_mapping + src_offset, width * (fb_mode->bpp >> 3));
        }
    }
}

/** Draw a glyph at the specified position the console.
 * @param ch            Character to draw.
 * @param x             X position (characters).
 * @param y             Y position (characters).
 * @param fg            Foreground colour.
 * @param bg            Background colour. */
static void draw_glyph(char ch, uint16_t x, uint16_t y, uint32_t fg, uint32_t bg) {
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
    //if(fb_console_glyphs) {
    //    fb_console_draw_glyph(
    //        fb_console_glyphs[(fb_console_y * fb_console_cols) + fb_console_x],
    //        fb_console_x, fb_console_y, FONT_BG, FONT_FG);
    //}
}

/** Disable the cursor. */
static void disable_cursor(void) {
    ///* Draw back in the correct colours. */
    //if(fb_console_glyphs) {
    //    fb_console_draw_glyph(
    //        fb_console_glyphs[(fb_console_y * fb_console_cols) + fb_console_x],
    //        fb_console_x, fb_console_y, FONT_FG, FONT_BG);
    //}
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

        draw_glyph(ch, fb_console_x, fb_console_y, FONT_FG, FONT_BG);
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
    assert(mode->type == VIDEO_MODE_LFB);

    fb_mode = mode;
    fb_mapping = (uint8_t *)mode->mem_virt;
    fb_console_cols = mode->width / FONT_WIDTH;
    fb_console_rows = mode->height / FONT_HEIGHT;
    fb_console_x = fb_console_y = 0;
}

/** Framebuffer console output operations. */
console_out_ops_t fb_console_out_ops = {
    .putc = fb_console_putc,
    .reset = fb_console_reset,
    .init = fb_console_init,
};
