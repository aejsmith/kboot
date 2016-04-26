/*
 * Copyright (C) 2010-2015 Alex Smith
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

#include <lib/list.h>

struct console_out;
struct console_in;

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

    /**
     * Default foreground/background colour.
     *
     * This is defined separately to allow, for example, on serial consoles for
     * the colours to be set back to the defaults for the console. For other
     * console implementations, it simply sets the defaults defined below.
     */
    COLOUR_DEFAULT,
} colour_t;

/** Default console colours. */
#define CONSOLE_COLOUR_FG       COLOUR_LIGHT_GREY
#define CONSOLE_COLOUR_BG       COLOUR_BLACK

/** Console output operations structure. */
typedef struct console_out_ops {
    /**
     * Initialization operations.
     */

    /** Initialize the console when it is made active.
     * @param console       Console output device. */
    void (*init)(struct console_out *console);

    /** Deinitialize the console when it is being made inactive.
     * @param console       Console output device. */
    void (*deinit)(struct console_out *console);

    /**
     * Basic operations.
     */

    /** Write a character to the console.
     * @param console       Console output device.
     * @param ch            Character to write. */
    void (*putc)(struct console_out *console, char ch);

    /** Set the current colours (optional).
     * @param console       Console output device.
     * @param fg            Foreground colour.
     * @param bg            Background colour. */
    void (*set_colour)(struct console_out *console, colour_t fg, colour_t bg);

    /** Set whether the cursor is visible.
     * @param console       Console output device.
     * @param visible       Whether to make the cursor visible. */
    void (*set_cursor_visible)(struct console_out *console, bool visible);

    /**
     * UI operations.
     */

    /** Begin UI mode (optional).
     * @param console       Console output device. */
    void (*begin_ui)(struct console_out *console);

    /** End UI mode (optional).
     * @param console       Console output device. */
    void (*end_ui)(struct console_out *console);

    /**
     * Set the draw region of the console.
     *
     * Sets the draw region of the console. All operations on the console (i.e.
     * writing, scrolling) will be constrained to this region. The cursor will
     * be moved to 0, 0 within this region.
     *
     * @param console       Console output device.
     * @param region        New draw region, or NULL to restore to whole console.
     */
    void (*set_region)(struct console_out *console, const draw_region_t *region);

    /** Get the current draw region.
     * @param console       Console output device.
     * @param region        Where to store details of the current draw region. */
    void (*get_region)(struct console_out *console, draw_region_t *region);

    /** Set the cursor position.
     * @param console       Console output device.
     * @param x             New X position (relative to draw region). Negative
     *                      values will move the cursor back from the right edge
     *                      of the draw region.
     * @param y             New Y position (relative to draw region). Negative
     *                      values will move the cursor up from the bottom edge
     *                      of the draw region. */
    void (*set_cursor_pos)(struct console_out *console, int16_t x, int16_t y);

    /** Get the cursor position.
     * @param console       Console output device.
     * @param _x            Where to store X position (relative to draw region).
     * @param _y            Where to store Y position (relative to draw region). */
    void (*get_cursor_pos)(struct console_out *console, uint16_t *_x, uint16_t *_y);

    /** Clear an area to the current background colour.
     * @param console       Console output device.
     * @param x             Start X position (relative to draw region).
     * @param y             Start Y position (relative to draw region).
     * @param width         Width of the area (if 0, whole width is cleared).
     * @param height        Height of the area (if 0, whole height is cleared). */
    void (*clear)(struct console_out *console, uint16_t x, uint16_t y, uint16_t width, uint16_t height);

    /** Scroll the draw region up (move contents down).
     * @param console       Console output device. */
    void (*scroll_up)(struct console_out *console);

    /** Scroll the draw region down (move contents up).
     * @param console       Console output device. */
    void (*scroll_down)(struct console_out *console);
} console_out_ops_t;

/** Console output structure (embedded in implementation-specific structure). */
typedef struct console_out {
    const console_out_ops_t *ops;       /**< Output operations. */
    bool in_ui;                         /**< Whether in UI mode. */
} console_out_t;

/** Special key codes. */
#define CONSOLE_KEY_UP          0x100
#define CONSOLE_KEY_DOWN        0x101
#define CONSOLE_KEY_LEFT        0x102
#define CONSOLE_KEY_RIGHT       0x103
#define CONSOLE_KEY_HOME        0x104
#define CONSOLE_KEY_END         0x105
#define CONSOLE_KEY_F1          0x106
#define CONSOLE_KEY_F2          0x107
#define CONSOLE_KEY_F3          0x108
#define CONSOLE_KEY_F4          0x109
#define CONSOLE_KEY_F5          0x10a
#define CONSOLE_KEY_F6          0x10b
#define CONSOLE_KEY_F7          0x10c
#define CONSOLE_KEY_F8          0x10d
#define CONSOLE_KEY_F9          0x10e
#define CONSOLE_KEY_F10         0x10f
#define CONSOLE_KEY_PGUP        0x110
#define CONSOLE_KEY_PGDOWN      0x111

/** Console input operations structure. */
typedef struct console_in_ops {
    /** Initialize the console when it is made active.
     * @param console       Console input device. */
    void (*init)(struct console_in *console);

    /** Deinitialize the console when it is being made inactive.
     * @param console       Console input device. */
    void (*deinit)(struct console_in *console);

    /** Check for a character from the console.
     * @param console       Console input device.
     * @return              Whether a character is available. */
    bool (*poll)(struct console_in *console);

    /** Read a character from the console.
     * @param console       Console input device.
     * @return              Character read. */
    uint16_t (*getc)(struct console_in *console);
} console_in_ops_t;

/** Console input structure (embedded in implementation-specific structure). */
typedef struct console_in {
    const console_in_ops_t *ops;        /**< Input operations. */
} console_in_t;

/**
 * Structure describing a console.
 *
 * A console is a named combination of an output and input device. The reason
 * for splitting output and input is that we may have separate code for handling
 * input and output. For example, on EFI we have output via the framebuffer,
 * but input via the EFI console.
 */
typedef struct console {
    list_t header;

    const char *name;                   /**< Name of the console. */
    console_out_t *out;                 /**< Output device. */
    console_in_t *in;                   /**< Input device. */
    unsigned active;                    /**< Active count. */
} console_t;

/** Console capabilities. */
#define CONSOLE_CAP_OUT         (1<<0)  /**< Console supports basic output. */
#define CONSOLE_CAP_IN          (1<<1)  /**< Console supports input. */
#define CONSOLE_CAP_UI          (1<<2)  /**< Console supports the user interface. */

extern console_t primary_console;

extern console_t *current_console;
extern console_t *debug_console;

extern bool console_has_caps(console_t *console, unsigned caps);

extern void console_putc(console_t *console, char ch);
extern void console_set_colour(console_t *console, colour_t fg, colour_t bg);
extern void console_set_cursor_visible(console_t *console, bool visible);
extern void console_begin_ui(console_t *console);
extern void console_end_ui(console_t *console);
extern void console_set_region(console_t *console, const draw_region_t *region);
extern void console_get_region(console_t *console, draw_region_t *region);
extern void console_set_cursor_pos(console_t *console, int16_t x, int16_t y);
extern void console_get_cursor_pos(console_t *console, uint16_t *_x, uint16_t *_y);
extern void console_clear(console_t *console, uint16_t x, uint16_t y, uint16_t width, uint16_t height);
extern void console_scroll_up(console_t *console);
extern void console_scroll_down(console_t *console);
extern bool console_poll(console_t *console);
extern uint16_t console_getc(console_t *console);

extern void console_vprintf_helper(char ch, void *data, int *total);
extern int console_vprintf(console_t *console, const char *fmt, va_list args);
extern int console_printf(console_t *console, const char *fmt, ...) __printf(2, 3);

extern console_t *console_lookup(const char *name);
extern void console_register(console_t *console);
extern void console_set_current(console_t *console);
extern void console_set_debug(console_t *console);

extern void target_console_init(void);

extern void console_init(void);

#ifdef CONFIG_TARGET_HAS_UI
extern void debug_log_display(void);
#endif

#endif /* __CONSOLE_H */
