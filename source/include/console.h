/*
 * Copyright (C) 2010-2014 Alex Smith
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
 * @brief               Console functions.
 */

#ifndef __CONSOLE_H
#define __CONSOLE_H

#include <types.h>

struct video_mode;

/** Console draw region structure. */
typedef struct draw_region {
    uint16_t x;                         /**< X position. */
    uint16_t y;                         /**< Y position. */
    uint16_t width;                     /**< Width of region. */
    uint16_t height;                    /**< Height of region. */
    bool scrollable;                    /**< Whether to scroll when cursor reaches the end. */
} draw_region_t;

/** Console colour definitions (match VGA colours). */
typedef enum colour {
    COLOUR_BLACK,                       /**< Black. */
    COLOUR_BLUE,                        /**< Dark Blue. */
    COLOUR_GREEN,                       /**< Dark Green. */
    COLOUR_CYAN,                        /**< Dark Cyan. */
    COLOUR_RED,                         /**< Dark Red. */
    COLOUR_MAGENTA,                     /**< Dark Magenta. */
    COLOUR_BROWN,                       /**< Brown. */
    COLOUR_LIGHT_GREY,                  /**< Light Grey. */
    COLOUR_GREY,                        /**< Dark Grey. */
    COLOUR_LIGHT_BLUE,                  /**< Light Blue. */
    COLOUR_LIGHT_GREEN,                 /**< Light Green. */
    COLOUR_LIGHT_CYAN,                  /**< Light Cyan. */
    COLOUR_LIGHT_RED,                   /**< Light Red. */
    COLOUR_LIGHT_MAGENTA,               /**< Light Magenta. */
    COLOUR_YELLOW,                      /**< Yellow. */
    COLOUR_WHITE,                       /**< White. */
} colour_t;

/** Default console colours. */
#define CONSOLE_COLOUR_FG       COLOUR_LIGHT_GREY
#define CONSOLE_COLOUR_BG       COLOUR_BLACK

/** Console output operations structure. */
typedef struct console_out_ops {
    /** Initialize the console.
     * @param mode          Video mode being used.
     * @return              Private data for the console. */
    void *(*init)(struct video_mode *mode);

    /** Deinitialize the console before changing video modes.
     * @param private       Private data for the console. */
    void (*deinit)(void *private);

    /** Reset the console to a default state.
     * @param private       Private data for the console. */
    void (*reset)(void *private);

    /**
     * Set the draw region of the console.
     *
     * Sets the draw region of the console. All operations on the console (i.e.
     * writing, scrolling) will be constrained to this region. The cursor will
     * be moved to 0, 0 within this region.
     *
     * @param private       Private data for the console.
     * @param region        New draw region, or NULL to restore to whole console.
     */
    void (*set_region)(void *private, const draw_region_t *region);

    /** Get the current draw region.
     * @param private       Private data for the console.
     * @param region        Where to store details of the current draw region. */
    void (*get_region)(void *private, draw_region_t *region);

    /** Set the current colours.
     * @param private       Private data for the console.
     * @param fg            Foreground colour.
     * @param bg            Background colour. */
    void (*set_colour)(void *private, colour_t fg, colour_t bg);

    /** Set whether the cursor is enabled.
     * @param private       Private data for the console.
     * @param enable        Whether the cursor is enabled. */
    void (*enable_cursor)(void *private, bool enable);

    /** Move the cursor.
     * @param private       Private data for the console.
     * @param x             New X position (relative to draw region). Negative
     *                      values will move the cursor back from the right edge
     *                      of the draw region.
     * @param y             New Y position (relative to draw region). Negative
     *                      values will move the cursor up from the bottom edge
     *                      of the draw region. */
    void (*move_cursor)(void *private, int16_t x, int16_t y);

    /** Clear an area to the current background colour.
     * @param private       Private data for the console.
     * @param x             Start X position (relative to draw region).
     * @param y             Start Y position (relative to draw region).
     * @param width         Width of the area (if 0, whole width is cleared).
     * @param height        Height of the area (if 0, whole height is cleared). */
    void (*clear)(void *private, uint16_t x, uint16_t y, uint16_t width, uint16_t height);

    /** Scroll the draw region up (move contents down).
     * @param private       Private data for the console. */
    void (*scroll_up)(void *private);

    /** Scroll the draw region down (move contents up).
     * @param private       Private data for the console. */
    void (*scroll_down)(void *private);

    /** Write a character to the console.
     * @param private       Private data for the console.
     * @param ch            Character to write. */
    void (*putc)(void *private, char ch);
} console_out_ops_t;

/** Special key codes. */
#define CONSOLE_KEY_UP      0x100
#define CONSOLE_KEY_DOWN    0x101
#define CONSOLE_KEY_LEFT    0x102
#define CONSOLE_KEY_RIGHT   0x103
#define CONSOLE_KEY_HOME    0x104
#define CONSOLE_KEY_END     0x105
#define CONSOLE_KEY_F1      0x106
#define CONSOLE_KEY_F2      0x107
#define CONSOLE_KEY_F3      0x108
#define CONSOLE_KEY_F4      0x109
#define CONSOLE_KEY_F5      0x10a
#define CONSOLE_KEY_F6      0x10b
#define CONSOLE_KEY_F7      0x10c
#define CONSOLE_KEY_F8      0x10d
#define CONSOLE_KEY_F9      0x10e
#define CONSOLE_KEY_F10     0x10f

/** Console input operations structure. */
typedef struct console_in_ops {
    /** Check for a character from the console.
     * @param private       Private data for the console.
     * @return              Whether a character is available. */
    bool (*poll)(void *private);

    /** Read a character from the console.
     * @param private       Private data for the console.
     * @return              Character read. */
    uint16_t (*getc)(void *private);
} console_in_ops_t;

/**
 * Structure describing a console.
 *
 * A console is separated into output and input operations. The reason for this
 * is that we may have separate code for handling input and output. For example,
 * on the BIOS platform's main console we have output via VGA, but input via
 * BIOS calls.
 */
typedef struct console {
    const console_out_ops_t *out;       /**< Output operations. */
    void *out_private;                  /**< Private data for output handler. */
    const console_in_ops_t *in;         /**< Input operations. */
    void *in_private;                   /**< Private data for input handler. */
} console_t;

/** Debug log size. */
#define DEBUG_LOG_SIZE      8192

extern char debug_log[DEBUG_LOG_SIZE];
extern size_t debug_log_start;
extern size_t debug_log_length;

extern console_t main_console;
extern console_t debug_console;

/**
 * Write a character to a console.
 *
 * Writes a character to a console. If the console has no output operations
 * defined, this function will do nothing.
 *
 * @param console           Console to write to.
 * @param ch                Character to write.
 */
static inline void console_putc(console_t *console, char ch) {
    if (console->out)
        console->out->putc(console->out_private, ch);
}

extern void console_vprintf_helper(char ch, void *data, int *total);
extern int console_vprintf(console_t *console, const char *fmt, va_list args);
extern int console_printf(console_t *console, const char *fmt, ...) __printf(2, 3);

#endif /* __CONSOLE_H */
