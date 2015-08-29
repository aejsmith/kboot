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

#include <drivers/console/fb.h>
#include <drivers/console/font.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <fb.h>
#include <loader.h>
#include <memory.h>
#include <video.h>

/** Framebuffer character information. */
typedef struct fb_char {
    char ch;                            /**< Character to display (0 == space). */
    uint8_t fg;                         /**< Foreground colour. */
    uint8_t bg;                         /**< Background colour. */
} fb_char_t;

/** Framebuffer console state. */
typedef struct fb_console_out {
    console_out_t console;              /**< Console output device header. */

    fb_char_t *chars;                   /**< Cache of characters on the console. */

    uint16_t cols;                      /**< Number of columns on the console. */
    uint16_t rows;                      /**< Number of rows on the console. */

    draw_region_t region;               /**< Current draw region. */
    colour_t fg_colour;                 /**< Current foreground colour. */
    colour_t bg_colour;                 /**< Current background colour. */
    uint16_t cursor_x;                  /**< X position of the cursor. */
    uint16_t cursor_y;                  /**< Y position of the cursor. */
    bool cursor_visible;                /**< Whether the cursor is enabled. */
} fb_console_out_t;

/** Framebuffer console colour table. */
static uint32_t fb_colour_table[] = {
    [COLOUR_BLACK]         = 0xff000000,
    [COLOUR_BLUE]          = 0xff0000aa,
    [COLOUR_GREEN]         = 0xff00aa00,
    [COLOUR_CYAN]          = 0xff00aaaa,
    [COLOUR_RED]           = 0xffaa0000,
    [COLOUR_MAGENTA]       = 0xffaa00aa,
    [COLOUR_BROWN]         = 0xffaa5500,
    [COLOUR_LIGHT_GREY]    = 0xffaaaaaa,
    [COLOUR_GREY]          = 0xff555555,
    [COLOUR_LIGHT_BLUE]    = 0xff5555ff,
    [COLOUR_LIGHT_GREEN]   = 0xff55ff55,
    [COLOUR_LIGHT_CYAN]    = 0xff55ffff,
    [COLOUR_LIGHT_RED]     = 0xffff5555,
    [COLOUR_LIGHT_MAGENTA] = 0xffff55ff,
    [COLOUR_YELLOW]        = 0xffffff55,
    [COLOUR_WHITE]         = 0xffffffff,
};

/** Draw the glyph at the specified position the console.
 * @param fb            Framebuffer console.
 * @param x             X position (characters).
 * @param y             Y position (characters). */
static void draw_glyph(fb_console_out_t *fb, uint16_t x, uint16_t y) {
    size_t idx = (y * fb->cols) + x;
    char ch = fb->chars[idx].ch;
    uint32_t fg, bg;

    if (ch) {
        fg = fb_colour_table[fb->chars[idx].fg];
        bg = fb_colour_table[fb->chars[idx].bg];
    } else {
        /* Character is 0, this indicates that the character has not been
         * written yet, so draw space with default colours. */
        ch = ' ';
        fg = fb_colour_table[CONSOLE_COLOUR_FG];
        bg = fb_colour_table[CONSOLE_COLOUR_BG];
    }

    /* Convert to a pixel position. */
    x *= CONSOLE_FONT_WIDTH;
    y *= CONSOLE_FONT_HEIGHT;

    /* Draw the glyph. */
    for (uint16_t i = 0; i < CONSOLE_FONT_HEIGHT; i++) {
        for (uint16_t j = 0; j < CONSOLE_FONT_WIDTH; j++) {
            if (console_font[(ch * CONSOLE_FONT_HEIGHT) + i] & (1 << (7 - j))) {
                fb_put_pixel(x + j, y + i, fg);
            } else {
                fb_put_pixel(x + j, y + i, bg);
            }
        }
    }
}

/** Toggle the cursor if enabled.
 * @param fb            Framebuffer console. */
static void toggle_cursor(fb_console_out_t *fb) {
    if (fb->cursor_visible) {
        size_t idx = (fb->cursor_y * fb->cols) + fb->cursor_x;

        if (fb->chars[idx].ch) {
            /* Invert the colours. */
            swap(fb->chars[idx].fg, fb->chars[idx].bg);
        } else {
            /* Nothing has yet been writen, initialize the character. We must
             * be enabling the cursor if this is the case, so invert colours. */
            fb->chars[idx].ch = ' ';
            fb->chars[idx].fg = CONSOLE_COLOUR_BG;
            fb->chars[idx].bg = CONSOLE_COLOUR_FG;
        }

        /* Redraw in new colours. */
        draw_glyph(fb, fb->cursor_x, fb->cursor_y);
    }
}

/** Set the draw region of the console.
 * @param console       Console output device.
 * @param region        New draw region, or NULL to restore to whole console. */
static void fb_console_set_region(console_out_t *console, const draw_region_t *region) {
    fb_console_out_t *fb = (fb_console_out_t *)console;

    if (region) {
        assert(region->width && region->height);
        assert(region->x + region->width <= fb->cols);
        assert(region->y + region->height <= fb->rows);

        memcpy(&fb->region, region, sizeof(fb->region));
    } else {
        fb->region.x = fb->region.y = 0;
        fb->region.width = fb->cols;
        fb->region.height = fb->rows;
        fb->region.scrollable = true;
    }

    /* Move cursor to top of region. */
    toggle_cursor(fb);
    fb->cursor_x = fb->region.x;
    fb->cursor_y = fb->region.y;
    toggle_cursor(fb);
}

/** Get the current draw region.
 * @param console       Console output device.
 * @param region        Where to store details of the current draw region. */
static void fb_console_get_region(console_out_t *console, draw_region_t *region) {
    fb_console_out_t *fb = (fb_console_out_t *)console;

    memcpy(region, &fb->region, sizeof(*region));
}

/** Set the current colours.
 * @param console       Console output device.
 * @param fg            Foreground colour.
 * @param bg            Background colour. */
static void fb_console_set_colour(console_out_t *console, colour_t fg, colour_t bg) {
    fb_console_out_t *fb = (fb_console_out_t *)console;

    if (fg == COLOUR_DEFAULT)
        fg = CONSOLE_COLOUR_FG;
    if (bg == COLOUR_DEFAULT)
        bg = CONSOLE_COLOUR_BG;

    fb->fg_colour = fg;
    fb->bg_colour = bg;
}

/** Set whether the cursor is visible.
 * @param console       Console output device.
 * @param visible       Whether the cursor should be visible. */
static void fb_console_set_cursor_visible(console_out_t *console, bool visible) {
    fb_console_out_t *fb = (fb_console_out_t *)console;

    toggle_cursor(fb);
    fb->cursor_visible = visible;
    toggle_cursor(fb);
}

/** Set the cursor position.
 * @param console       Console output device.
 * @param x             New X position (relative to draw region). Negative
 *                      values will move the cursor back from the right edge of
 *                      the draw region.
 * @param y             New Y position (relative to draw region). Negative
 *                      values will move the cursor up from the bottom edge of
 *                      the draw region. */
static void fb_console_set_cursor_pos(console_out_t *console, int16_t x, int16_t y) {
    fb_console_out_t *fb = (fb_console_out_t *)console;

    assert(abs(x) < fb->region.width);
    assert(abs(y) < fb->region.height);

    toggle_cursor(fb);
    fb->cursor_x = (x < 0) ? fb->region.x + fb->region.width + x : fb->region.x + x;
    fb->cursor_y = (y < 0) ? fb->region.y + fb->region.height + y : fb->region.y + y;
    toggle_cursor(fb);
}

/** Get the cursor position.
 * @param console       Console output device.
 * @param _x            Where to store X position (relative to draw region).
 * @param _y            Where to store Y position (relative to draw region). */
static void fb_console_get_cursor_pos(console_out_t *console, uint16_t *_x, uint16_t *_y) {
    fb_console_out_t *fb = (fb_console_out_t *)console;

    if (_x)
        *_x = fb->cursor_x - fb->region.x;
    if (_y)
        *_y = fb->cursor_y - fb->region.y;
}

/** Clear an area to the current background colour.
 * @param console       Console output device.
 * @param x             Start X position (relative to draw region).
 * @param y             Start Y position (relative to draw region).
 * @param width         Width of the area (if 0, whole width is cleared).
 * @param height        Height of the area (if 0, whole height is cleared). */
static void fb_console_clear(console_out_t *console, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    fb_console_out_t *fb = (fb_console_out_t *)console;

    assert(x + width <= fb->region.width);
    assert(y + height <= fb->region.height);

    if (!width)
        width = fb->region.width - x;
    if (!height)
        height = fb->region.height - y;

    for (uint16_t i = 0; i < height; i++) {
        for (uint16_t j = 0; j < width; j++) {
            uint16_t abs_x = fb->region.x + j;
            uint16_t abs_y = fb->region.y + i;
            size_t idx = (abs_y * fb->cols) + abs_x;

            fb->chars[idx].ch = ' ';
            fb->chars[idx].fg = fb->fg_colour;
            fb->chars[idx].bg = fb->bg_colour;

            if (fb->cursor_visible && abs_x == fb->cursor_x && abs_y == fb->cursor_y) {
                /* Avoid redrawing the glyph twice. */
                toggle_cursor(fb);
            } else {
                draw_glyph(fb, abs_x, abs_y);
            }
        }
    }
}

/** Scroll the draw region up (move contents down).
 * @param console       Console output device. */
static void fb_console_scroll_up(console_out_t *console) {
    fb_console_out_t *fb = (fb_console_out_t *)console;

    toggle_cursor(fb);

    /* Move everything down. */
    for (uint16_t i = fb->region.height - 1; i > 0; i--) {
        memmove(
            &fb->chars[((fb->region.y + i) * fb->cols) + fb->region.x],
            &fb->chars[((fb->region.y + i - 1) * fb->cols) + fb->region.x],
            fb->region.width * sizeof(*fb->chars));

        fb_copy_rect(
            fb->region.x * CONSOLE_FONT_WIDTH, (fb->region.y + i) * CONSOLE_FONT_HEIGHT,
            fb->region.x * CONSOLE_FONT_WIDTH, (fb->region.y + i - 1) * CONSOLE_FONT_HEIGHT,
            fb->region.width * CONSOLE_FONT_WIDTH, CONSOLE_FONT_HEIGHT);
    }

    /* Fill the first row with blanks. */
    memset(&fb->chars[(fb->region.y * fb->cols) + fb->region.x], 0, fb->region.width * sizeof(*fb->chars));
    fb_fill_rect(
        fb->region.x * CONSOLE_FONT_WIDTH, fb->region.y * CONSOLE_FONT_HEIGHT,
        fb->region.width * CONSOLE_FONT_WIDTH, CONSOLE_FONT_HEIGHT,
        fb_colour_table[CONSOLE_COLOUR_BG]);

    toggle_cursor(fb);
}

/** Scroll the draw region down (does not change cursor).
 * @param fb            Framebuffer console. */
static void scroll_down(fb_console_out_t *fb) {
    /* Move everything up. */
    for (uint16_t i = 0; i < fb->region.height - 1; i++) {
        memmove(
            &fb->chars[((fb->region.y + i) * fb->cols) + fb->region.x],
            &fb->chars[((fb->region.y + i + 1) * fb->cols) + fb->region.x],
            fb->region.width * sizeof(*fb->chars));

        fb_copy_rect(
            fb->region.x * CONSOLE_FONT_WIDTH, (fb->region.y + i) * CONSOLE_FONT_HEIGHT,
            fb->region.x * CONSOLE_FONT_WIDTH, (fb->region.y + i + 1) * CONSOLE_FONT_HEIGHT,
            fb->region.width * CONSOLE_FONT_WIDTH, CONSOLE_FONT_HEIGHT);
    }

    /* Fill the last row with blanks. */
    memset(
        &fb->chars[((fb->region.y + fb->region.height - 1) * fb->cols) + fb->region.x],
        0, fb->region.width * sizeof(*fb->chars));
    fb_fill_rect(
        fb->region.x * CONSOLE_FONT_WIDTH, (fb->region.y + fb->region.height - 1) * CONSOLE_FONT_HEIGHT,
        fb->region.width * CONSOLE_FONT_WIDTH, CONSOLE_FONT_HEIGHT,
        fb_colour_table[CONSOLE_COLOUR_BG]);
}

/** Scroll the draw region down (move contents up).
 * @param console       Console output device. */
static void fb_console_scroll_down(console_out_t *console) {
    fb_console_out_t *fb = (fb_console_out_t *)console;

    toggle_cursor(fb);
    scroll_down(fb);
    toggle_cursor(fb);
}

/** Write a character to the console.
 * @param console       Console output device.
 * @param ch            Character to write. */
static void fb_console_putc(console_out_t *console, char ch) {
    fb_console_out_t *fb = (fb_console_out_t *)console;
    size_t idx;

    toggle_cursor(fb);

    switch (ch) {
    case '\b':
        /* Backspace, move back one character if we can. */
        if (fb->cursor_x > fb->region.x) {
            fb->cursor_x--;
        } else if (fb->cursor_y > fb->region.y) {
            fb->cursor_x = fb->region.x + fb->region.width - 1;
            fb->cursor_y--;
        }

        break;
    case '\r':
        /* Carriage return, move to the start of the line. */
        fb->cursor_x = fb->region.x;
        break;
    case '\n':
        /* Newline, treat it as if a carriage return was there. */
        fb->cursor_x = fb->region.x;
        fb->cursor_y++;
        break;
    case '\t':
        fb->cursor_x += 8 - (fb->cursor_x % 8);
        break;
    default:
        /* If it is a non-printing character, ignore it. */
        if (ch < ' ')
            break;

        idx = (fb->cursor_y * fb->cols) + fb->cursor_x;
        fb->chars[idx].ch = ch;
        fb->chars[idx].fg = fb->fg_colour;
        fb->chars[idx].bg = fb->bg_colour;
        draw_glyph(fb, fb->cursor_x, fb->cursor_y);

        fb->cursor_x++;
        break;
    }

    /* If we have reached the edge of the draw region insert a new line. */
    if (fb->cursor_x >= fb->region.x + fb->region.width) {
        fb->cursor_x = fb->region.x;
        fb->cursor_y++;
    }

    /* If we have reached the bottom of the draw region, scroll. */
    if (fb->cursor_y >= fb->region.y + fb->region.height) {
        if (fb->region.scrollable)
            scroll_down(fb);

        /* Update the cursor position. */
        fb->cursor_y = fb->region.y + fb->region.height - 1;
    }

    toggle_cursor(fb);
}

/** Initialize the console.
 * @param console       Console output device. */
static void fb_console_init(console_out_t *console) {
    fb_console_out_t *fb = (fb_console_out_t *)console;

    assert(current_video_mode->type == VIDEO_MODE_LFB);

    fb->cols = current_video_mode->width / CONSOLE_FONT_WIDTH;
    fb->rows = current_video_mode->height / CONSOLE_FONT_HEIGHT;

    /* Allocate a character cache. */
    fb->chars = malloc_large(fb->cols * fb->rows * sizeof(*fb->chars));

    fb->fg_colour = CONSOLE_COLOUR_FG;
    fb->bg_colour = CONSOLE_COLOUR_BG;
    fb->cursor_visible = false;
    fb_console_set_region(console, NULL);

    /* Clear the console. */
    fb_fill_rect(0, 0, 0, 0, fb_colour_table[CONSOLE_COLOUR_BG]);
    memset(fb->chars, 0, fb->cols * fb->rows * sizeof(*fb->chars));
    toggle_cursor(fb);
}

/** Deinitialize the console.
 * @param console       Console output device. */
static void fb_console_deinit(console_out_t *console) {
    fb_console_out_t *fb = (fb_console_out_t *)console;

    free_large(fb->chars);
}

/** Framebuffer console output operations. */
static console_out_ops_t fb_console_out_ops = {
    .set_region = fb_console_set_region,
    .get_region = fb_console_get_region,
    .set_colour = fb_console_set_colour,
    .set_cursor_visible = fb_console_set_cursor_visible,
    .set_cursor_pos = fb_console_set_cursor_pos,
    .get_cursor_pos = fb_console_get_cursor_pos,
    .clear = fb_console_clear,
    .scroll_up = fb_console_scroll_up,
    .scroll_down = fb_console_scroll_down,
    .putc = fb_console_putc,
    .init = fb_console_init,
    .deinit = fb_console_deinit,
};

/** Create a framebuffer console.
 * @return              Framebuffer console output device. */
console_out_t *fb_console_create(void) {
    fb_console_out_t *fb = malloc(sizeof(*fb));

    memset(fb, 0, sizeof(*fb));
    fb->console.ops = &fb_console_out_ops;
    return &fb->console;
}
