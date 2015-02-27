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
 * @brief               Shell interface.
 */

#include <lib/ctype.h>
#include <lib/string.h>

#include <assert.h>
#include <config.h>
#include <console.h>
#include <loader.h>

/** Length of the line buffer. */
#define LINE_BUF_LEN 256

/** Buffer to collect input in. */
static char line_buf[LINE_BUF_LEN];
static size_t line_read_pos;
static size_t line_write_pos;
static size_t line_length;

/** Whether the shell is currently enabled. */
bool shell_enabled;

/** Whether currently in the shell. */
bool shell_running;

/** Insert a character to the buffer at the current position.
 * @param ch            Character to insert. */
static void insert_char(char ch) {
    if (line_length < LINE_BUF_LEN - 1) {
        console_putc(config_console, ch);

        if (line_write_pos == line_length) {
            line_buf[line_length++] = ch;
            line_write_pos++;
        } else {
            size_t i;

            memmove(&line_buf[line_write_pos + 1], &line_buf[line_write_pos], line_length - line_write_pos);
            line_buf[line_write_pos++] = ch;
            line_length++;

            /* Reprint the character plus everything after, maintaining the
             * current cursor position. */
            for (i = 0; i < line_length - line_write_pos; i++)
                console_putc(config_console, line_buf[line_write_pos + i]);
            while (i--)
                console_putc(config_console, '\b');
        }
    }
}

/** Erase a character from the current position.
 * @param forward       If true, will erase the character at the current cursor
 *                      position, else will erase the previous one. */
static void erase_char(bool forward) {
    size_t i;

    if (forward) {
        if (line_write_pos == line_length)
            return;
    } else {
        if (!line_write_pos) {
            return;
        } else {
            /* Decrement position and fall through. */
            line_write_pos--;
            console_putc(config_console, '\b');
        }
    }

    line_length--;
    memmove(&line_buf[line_write_pos], &line_buf[line_write_pos + 1], line_length - line_write_pos);

    /* Reprint everything, maintaining cursor position. */
    for (i = 0; i < line_length - line_write_pos; i++)
        console_putc(config_console, line_buf[line_write_pos + i]);
    console_putc(config_console, ' ');
    i++;
    while (i--)
        console_putc(config_console, '\b');
}

/** Input helper for the shell.
 * @param nest          Nesting count (unused).
 * @return              Character read, or EOF on end of file. */
static int shell_input_helper(unsigned nest) {
    if (line_read_pos) {
        if (line_read_pos < line_length) {
            return line_buf[line_read_pos++];
        } else if (!nest) {
            /* Not expecting more input, return end. */
            return EOF;
        } else {
            /* Expecting more input, get another line. */
            line_read_pos = 0;
            line_write_pos = 0;
            line_length = 0;
            console_set_colour(config_console, COLOUR_WHITE, CONSOLE_COLOUR_BG);
            config_printf("> ");
            console_set_colour(config_console, CONSOLE_COLOUR_FG, CONSOLE_COLOUR_BG);
        }
    }

    /* Accumulate another line. */
    while (true) {
        uint16_t ch = main_console.in->getc(main_console.in_private);

        switch (ch) {
        case '\b':
            erase_char(false);
            break;
        case 0x7f:
            erase_char(true);
            break;
        case CONSOLE_KEY_LEFT:
            if (line_write_pos) {
                console_putc(config_console, '\b');
                line_write_pos--;
            }

            break;
        case CONSOLE_KEY_RIGHT:
            if (line_write_pos != line_length) {
                console_putc(config_console, line_buf[line_write_pos]);
                line_write_pos++;
            }

            break;
        case CONSOLE_KEY_HOME:
            while (line_write_pos) {
                console_putc(config_console, '\b');
                line_write_pos--;
            }

            break;
        case CONSOLE_KEY_END:
            while (line_write_pos < line_length) {
                console_putc(config_console, line_buf[line_write_pos]);
                line_write_pos++;
            }

            break;
        case '\n':
            /* Parser requires the newline at the end of the buffer to work
             * properly, so add it on. Buffer is guaranteed to have space for
             * the newline by insert_char(). */
            console_putc(config_console, '\n');
            line_buf[line_length++] = '\n';

            /* Start returning the line. */
            return line_buf[line_read_pos++];
        default:
            if (isprint(ch))
                insert_char(ch);

            break;
        }
    }
}

/** Main function of the shell. */
void shell_main(void) {
    assert(shell_enabled);

    shell_running = true;

    if (main_console.out && main_console.in) {
        config_console = &main_console;
    } else if (debug_console.out && debug_console.in) {
        config_console = &debug_console;
    } else {
        return;
    }

    current_environ = root_environ;

    while (true) {
        command_list_t *list;

        console_set_colour(config_console, COLOUR_WHITE, CONSOLE_COLOUR_BG);
        config_printf("KBoot> ");
        console_set_colour(config_console, CONSOLE_COLOUR_FG, CONSOLE_COLOUR_BG);

        line_read_pos = 0;
        line_write_pos = 0;
        line_length = 0;

        list = config_parse("<shell>", shell_input_helper);
        if (list) {
            command_list_exec(list, current_environ);
            command_list_destroy(list);

            /* If we now have a loader, load it. */
            if (current_environ->loader) {
                shell_running = false;
                environ_boot(current_environ);
            }
        }
    }

    shell_running = false;
}
