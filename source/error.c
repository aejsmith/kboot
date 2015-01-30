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
 * @brief               Boot error handling functions.
 */

#include <lib/printf.h>

#include <console.h>
#include <loader.h>
#include <shell.h>

/** Helper for printing error messages.
 * @param ch            Character to display.
 * @param data          Ignored.
 * @param total         Pointer to total character count. */
static void error_printf_helper(char ch, void *data, int *total) {
    if (debug_console.out)
        debug_console.out->putc(ch);
    if (main_console.out)
        main_console.out->putc(ch);

    *total = *total + 1;
}

/** Formatted print function for error functions. */
static int error_printf(const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = do_vprintf(error_printf_helper, NULL, fmt, args);
    va_end(args);

    return ret;
}

/** Raise an internal error.
 * @param fmt           Error format string.
 * @param ...           Values to substitute into format. */
void __noreturn internal_error(const char *fmt, ...) {
    va_list args;

    if (main_console.out)
        main_console.out->reset();

    error_printf("\nAn internal error has occurred:\n\n");

    va_start(args, fmt);
    do_vprintf(error_printf_helper, NULL, fmt, args);
    va_end(args);

    error_printf("\n\n");
    error_printf("Please report this error to http://kiwi.alex-smith.me.uk/\n");
    #ifdef __PIC__
        error_printf("Backtrace (base = %p):\n", __start);
    #else
        error_printf("Backtrace:\n");
    #endif
    backtrace(error_printf);

    system_halt();
}

/** Display details of a boot error.
 * @param fmt           Error format string.
 * @param ...           Values to substitute into format. */
void __noreturn boot_error(const char *fmt, ...) {
    va_list args;

    /* TODO: This eventually needs to go in a UI window and should let you
     * reboot the machine. */

    if (main_console.out)
        main_console.out->reset();

    error_printf("\nAn error has occurred during boot:\n\n");

    va_start(args, fmt);
    do_vprintf(error_printf_helper, NULL, fmt, args);
    va_end(args);

    error_printf("\n\n");
    error_printf("Ensure that you have enough memory available, that you do not have any\n");
    error_printf("malfunctioning hardware and that your computer meets the minimum system\n");
    error_printf("requirements for the operating system.\n");

    // temporary
    error_printf("\n");
    shell_main();

    system_halt();
}
