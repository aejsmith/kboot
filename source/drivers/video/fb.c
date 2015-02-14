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
 *  - The framebuffer console must be initialized after memory detection is
 *    done as it uses the physical memory manager to allocate a backbuffer.
 */

#include <drivers/video/fb.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>
#include <video.h>

/** Framebuffer console state. */
typedef struct fb_console {
    video_mode_t *mode;                 /**< Video mode in use. */

    uint8_t *mapping;                   /**< Mapping of the framebuffer. */
    uint8_t *backbuffer;                /**< Back buffer (to speed up copying). */
    char *glyphs;                       /**< Cache of glyphs on the console. */

    uint16_t cols;                      /**< Number of columns on the console. */
    uint16_t rows;                      /**< Number of rows on the console. */
    uint16_t x;                         /**< X position of the cursor. */
    uint16_t y;                         /**< Y position of the cursor. */
} fb_console_t;

/** Dimensions and colours of the console font. */
#define FONT_WIDTH              8
#define FONT_HEIGHT             16
#define FONT_FG                 0xaaaaaa
#define FONT_BG                 0x000000

extern unsigned char console_font[];

/** Get the byte offset of a pixel.
 * @param fb            Framebuffer console.
 * @param x             X position of pixel.
 * @param y             Y position of pixel.
 * @return              Byte offset of the pixel. */
static inline size_t fb_offset(fb_console_t *fb, uint32_t x, uint32_t y) {
    return (y * fb->mode->pitch) + (x * (fb->mode->bpp >> 3));
}

/** Convert an R8G8B8 value to the framebuffer format.
 * @param fb            Framebuffer console.
 * @param rgb           32-bit RGB value.
 * @return              Calculated pixel value. */
static inline uint32_t rgb888_to_fb(fb_console_t *fb, uint32_t rgb) {
    uint32_t red = ((rgb >> (24 - fb->mode->red_size)) & ((1 << fb->mode->red_size) - 1)) << fb->mode->red_pos;
    uint32_t green = ((rgb >> (16 - fb->mode->green_size)) & ((1 << fb->mode->green_size) - 1)) << fb->mode->green_pos;
    uint32_t blue = ((rgb >> (8 - fb->mode->blue_size)) & ((1 << fb->mode->blue_size) - 1)) << fb->mode->blue_pos;

    return red | green | blue;
}

/** Put a pixel on the framebuffer.
 * @param fb            Framebuffer console.
 * @param x             X position.
 * @param y             Y position.
 * @param rgb           RGB colour to draw. */
static void fb_putpixel(fb_console_t *fb, uint16_t x, uint16_t y, uint32_t rgb) {
    size_t offset = fb_offset(fb, x, y);
    void *main = fb->mapping + offset;
    void *back = fb->backbuffer + offset;
    uint32_t value = rgb888_to_fb(fb, rgb);

    switch (fb->mode->bpp >> 3) {
    case 2:
        *(uint16_t *)main = (uint16_t)value;
        *(uint16_t *)back = (uint16_t)value;
        break;
    case 3:
        ((uint16_t *)main)[0] = value & 0xffff;
        ((uint8_t *)main)[2] = (value >> 16) & 0xff;
        ((uint16_t *)back)[0] = value & 0xffff;
        ((uint8_t *)back)[2] = (value >> 16) & 0xff;
        break;
    case 4:
        *(uint32_t *)main = value;
        *(uint32_t *)back = value;
        break;
    }
}

/** Draw a rectangle in a solid colour.
 * @param fb            Framebuffer console.
 * @param x             X position of rectangle.
 * @param y             Y position of rectangle.
 * @param width         Width of rectangle.
 * @param height        Height of rectangle.
 * @param rgb           Colour to draw in. */
static void fb_fillrect(fb_console_t *fb, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t rgb) {
    if (x == 0 && width == fb->mode->width && (rgb == 0 || rgb == 0xffffff)) {
        /* Fast path where we can fill a block quickly. */
        memset(fb->mapping + (y * fb->mode->pitch), (uint8_t)rgb, height * fb->mode->pitch);
        memset(fb->backbuffer + (y * fb->mode->pitch), (uint8_t)rgb, height * fb->mode->pitch);
    } else {
        for (uint32_t i = 0; i < height; i++) {
            for (uint32_t j = 0; j < width; j++)
                fb_putpixel(fb, x + j, y + i, rgb);
        }
    }
}

/** Copy part of the framebuffer to another location.
 * @param fb            Framebuffer console.
 * @param dest_x        X position of destination.
 * @param dest_y        Y position of destination.
 * @param src_x         X position of source area.
 * @param src_y         Y position of source area.
 * @param width         Width of area to copy.
 * @param height        Height of area to copy. */
static void fb_copyrect(
    fb_console_t *fb, uint32_t dest_x, uint32_t dest_y, uint32_t src_x, uint32_t src_y,
    uint32_t width, uint32_t height)
{
    size_t dest_offset, src_offset;

    if (dest_x == 0 && src_x == 0 && width == fb->mode->width) {
        /* Fast path where we can copy everything in one go. */
        dest_offset = dest_y * fb->mode->pitch;
        src_offset = src_y * fb->mode->pitch;

        /* Copy everything on the backbuffer, then copy the affected section to
         * the main framebuffer. */
        memmove(fb->backbuffer + dest_offset, fb->backbuffer + src_offset, height * fb->mode->pitch);
        memcpy(fb->mapping + dest_offset, fb->backbuffer + dest_offset, height * fb->mode->pitch);
    } else {
        /* Copy line by line. */
        for (uint32_t i = 0; i < height; i++) {
            dest_offset = fb_offset(fb, dest_x, dest_y + i);
            src_offset = fb_offset(fb, src_x, src_y + i);

            memmove(fb->backbuffer + dest_offset, fb->backbuffer + src_offset, width * (fb->mode->bpp >> 3));
            memcpy(fb->mapping + dest_offset, fb->backbuffer + dest_offset, width * (fb->mode->bpp >> 3));
        }
    }
}

/** (Re)draw the glyph at the specified position the console.
 * @param fb            Framebuffer console.
 * @param x             X position (characters).
 * @param y             Y position (characters).
 * @param fg            Foreground colour.
 * @param bg            Background colour. */
static void draw_glyph(fb_console_t *fb, uint16_t x, uint16_t y, uint32_t fg, uint32_t bg) {
    char ch = fb->glyphs[(y * fb->cols) + x];

    /* Convert to a pixel position. */
    x *= FONT_WIDTH;
    y *= FONT_HEIGHT;

    /* Draw the glyph. */
    for (uint16_t i = 0; i < FONT_HEIGHT; i++) {
        for (uint16_t j = 0; j < FONT_WIDTH; j++) {
            if (console_font[(ch * FONT_HEIGHT) + i] & (1 << (7 - j))) {
                fb_putpixel(fb, x + j, y + i, fg);
            } else {
                fb_putpixel(fb, x + j, y + i, bg);
            }
        }
    }
}

/** Enable the cursor.
 * @param fb            Framebuffer console. */
static void enable_cursor(fb_console_t *fb) {
    /* Draw in inverted colours. */
    draw_glyph(fb, fb->x, fb->y, FONT_BG, FONT_FG);
}

/** Disable the cursor.
 * @param fb            Framebuffer console. */
static void disable_cursor(fb_console_t *fb) {
    /* Draw back in the correct colours. */
    draw_glyph(fb, fb->x, fb->y, FONT_FG, FONT_BG);
}

/** Write a character to the console.
 * @param _fb           Pointer to framebuffer console.
 * @param ch            Character to write. */
static void fb_console_putc(void *_fb, char ch) {
    fb_console_t *fb = _fb;

    disable_cursor(fb);

    switch (ch) {
    case '\b':
        /* Backspace, move back one character if we can. */
        if (fb->x) {
            fb->x--;
        } else if (fb->y) {
            fb->x = fb->cols - 1;
            fb->y--;
        }

        break;
    case '\r':
        /* Carriage return, move to the start of the line. */
        fb->x = 0;
        break;
    case '\n':
        /* Newline, treat it as if a carriage return was there (will be handled
         * below). */
        fb->x = fb->cols;
        break;
    case '\t':
        fb->x += 8 - (fb->x % 8);
        break;
    default:
        /* If it is a non-printing character, ignore it. */
        if (ch < ' ')
            break;

        fb->glyphs[(fb->y * fb->cols) + fb->x] = ch;
        draw_glyph(fb, fb->x, fb->y, FONT_FG, FONT_BG);
        fb->x++;
        break;
    }

    /* If we have reached the edge of the screen insert a new line. */
    if (fb->x >= fb->cols) {
        fb->x = 0;
        if (++fb->y < fb->rows)
            fb_fillrect(fb, 0, FONT_HEIGHT * fb->y, fb->mode->width, FONT_HEIGHT, FONT_BG);
    }

    /* If we have reached the bottom of the screen, scroll. */
    if (fb->y >= fb->rows) {
        /* Move everything up and fill the last row with blanks. */
        memmove(fb->glyphs, fb->glyphs + fb->cols, (fb->rows - 1) * fb->cols);
        memset(fb->glyphs + ((fb->rows - 1) * fb->cols), ' ', fb->cols);
        fb_copyrect(fb, 0, 0, 0, FONT_HEIGHT, fb->mode->width, (fb->rows - 1) * FONT_HEIGHT);
        fb_fillrect(fb, 0, FONT_HEIGHT * (fb->rows - 1), fb->mode->width, FONT_HEIGHT, FONT_BG);

        /* Update the cursor position. */
        fb->y = fb->rows - 1;
    }

    enable_cursor(fb);
}

/** Reset the console to a default state.
 * @param _fb           Pointer to framebuffer console. */
static void fb_console_reset(void *_fb) {
    fb_console_t *fb = _fb;

    fb->x = fb->y = 0;

    fb_fillrect(fb, 0, 0, fb->mode->width, fb->mode->height, FONT_BG);
    memset(fb->glyphs, ' ', fb->cols * fb->rows * sizeof(*fb->glyphs));
    enable_cursor(fb);
}

/** Initialize the console.
 * @param mode          Video mode being used.
 * @return              Pointer to console private data. */
static void *fb_console_init(video_mode_t *mode) {
    fb_console_t *fb;
    size_t size;

    assert(mode->type == VIDEO_MODE_LFB);

    fb = malloc(sizeof(*fb));
    fb->mode = mode;
    fb->mapping = (uint8_t *)mode->mem_virt;
    fb->cols = mode->width / FONT_WIDTH;
    fb->rows = mode->height / FONT_HEIGHT;

    /* Allocate a backbuffer and glyph cache. */
    size = round_up(mode->pitch * mode->height, PAGE_SIZE);
    fb->backbuffer = memory_alloc(size, 0, 0, 0, MEMORY_TYPE_INTERNAL, MEMORY_ALLOC_HIGH, NULL);
    if (!fb->backbuffer)
        internal_error("Failed to allocate console backbuffer");

    size = round_up(fb->cols * fb->rows * sizeof(*fb->glyphs), PAGE_SIZE);
    fb->glyphs = memory_alloc(size, 0, 0, 0, MEMORY_TYPE_INTERNAL, MEMORY_ALLOC_HIGH, NULL);
    if (!fb->glyphs)
        internal_error("Failed to allocate console glyph cache");

    fb_console_reset(fb);

    return fb;
}

/** Deinitialize the console.
 * @param _fb           Pointer to framebuffer console. */
static void fb_console_deinit(void *_fb) {
    fb_console_t *fb = _fb;
    size_t size;

    size = round_up(fb->mode->pitch * fb->mode->height, PAGE_SIZE);
    memory_free(fb->backbuffer, size);

    size = round_up(fb->cols * fb->rows * sizeof(*fb->glyphs), PAGE_SIZE);
    memory_free(fb->glyphs, size);

    free(fb);
}

/** Framebuffer console output operations. */
console_out_ops_t fb_console_out_ops = {
    .putc = fb_console_putc,
    .reset = fb_console_reset,
    .init = fb_console_init,
    .deinit = fb_console_deinit,
};
