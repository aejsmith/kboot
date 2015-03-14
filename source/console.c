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

#include <lib/printf.h>

#include <console.h>
#include <loader.h>
#include <ui.h>

/** Debug log size. */
#define DEBUG_LOG_SIZE      8192

/** Debug output log. */
static char debug_log[DEBUG_LOG_SIZE];
static size_t debug_log_start;
static size_t debug_log_length;

/** Main console. */
console_t main_console;

/** Debug console. */
console_t debug_console;

/** Helper for console_vprintf().
 * @param ch            Character to display.
 * @param data          Console to use.
 * @param total         Pointer to total character count. */
void console_vprintf_helper(char ch, void *data, int *total) {
    console_t *console = data;

    console_putc(console, ch);

    if (console == &debug_console) {
        /* Store in the log buffer. */
        debug_log[(debug_log_start + debug_log_length) % DEBUG_LOG_SIZE] = ch;
        if (debug_log_length < DEBUG_LOG_SIZE) {
            debug_log_length++;
        } else {
            debug_log_start = (debug_log_start + 1) % DEBUG_LOG_SIZE;
        }
    }

    *total = *total + 1;
}

/** Output a formatted message to a console.
 * @param console       Console to print to.
 * @param fmt           Format string used to create the message.
 * @param args          Arguments to substitute into format.
 * @return              Number of characters printed. */
int console_vprintf(console_t *console, const char *fmt, va_list args) {
    return do_vprintf(console_vprintf_helper, console, fmt, args);
}

/** Output a formatted message to a console.
 * @param console       Console to print to.
 * @param fmt           Format string used to create the message.
 * @param ...           Arguments to substitute into format.
 * @return              Number of characters printed. */
int console_printf(console_t *console, const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = console_vprintf(console, fmt, args);
    va_end(args);

    return ret;
}

#ifdef CONFIG_TARGET_HAS_UI

/** Display the debug log. */
void debug_log_display(void) {
    ui_window_t *textview = ui_textview_create(
        "Debug Log", debug_log, DEBUG_LOG_SIZE, debug_log_start,
        debug_log_length);

    ui_display(textview, ui_console, 0);
    ui_window_destroy(textview);
}

#endif /* CONFIG_TARGET_HAS_UI */
