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
 * @brief               Line editor.
 */

#include <lib/ctype.h>
#include <lib/line_editor.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <console.h>
#include <memory.h>

/**
 * Size of an allocation chunk.
 *
 * We work in chunks to avoid reallocating the buffer on every modification to
 * the line. When the line length crosses a chunk boundary the buffer is
 * reallocated to the next chunk size up.
 */
#define LINE_EDITOR_CHUNK_SIZE   128

/**
 * Begin editing a line.
 *
 * Initializes the line editor state. If not provided with an initial string,
 * the line will initially be empty. The provided string is not modified,editing takes place on an internal buffer.
 *
 * @param editor        Line editor state.
 * @param console       Console to output to.
 * @param str           Initial string, or NULL for empty string.
 */
void line_editor_init(line_editor_t *editor, console_t *console, const char *str) {
    editor->console = console;
    editor->len = (str) ? strlen(str) : 0;
    editor->offset = editor->len;

    if (editor->len) {
        editor->buf = malloc(round_up(editor->len, LINE_EDITOR_CHUNK_SIZE));
        memcpy(editor->buf, str, editor->len);
    } else {
        editor->buf = NULL;
    }
}

/** Output the line and place the cursor at the current position.
 * @param editor        Line editor state. */
void line_editor_output(line_editor_t *editor) {
    uint16_t x, y;
    bool visible;

    for (size_t i = 0; i <= editor->len; i++) {
        if (i == editor->offset)
            console_get_cursor(editor->console, &x, &y, &visible);

        if (i < editor->len)
            console_putc(editor->console, editor->buf[i]);
    }

    console_set_cursor(editor->console, x, y, visible);
}

/** Reprint from the current offset, mainting cursor position.
 * @param editor        Line editor state.
 * @param space         Whether to print an additional space at the end (after
 *                      removing a character). */
static void reprint_from_current(line_editor_t *editor, bool space) {
    uint16_t x, y;
    bool visible;

    console_get_cursor(editor->console, &x, &y, &visible);

    for (size_t i = editor->offset; i < editor->len; i++)
        console_putc(editor->console, editor->buf[i]);

    if (space)
        console_putc(editor->console, ' ');

    console_set_cursor(editor->console, x, y, visible);
}

/** Insert a character to the buffer at the current position.
 * @param editor        Line editor state.
 * @param ch            Character to insert. */
static void insert_char(line_editor_t *editor, char ch) {
    /* Resize the buffer if this will go over a chunk boundary. */
    if (!(editor->len % LINE_EDITOR_CHUNK_SIZE))
        editor->buf = realloc(editor->buf, editor->len + LINE_EDITOR_CHUNK_SIZE);

    console_putc(editor->console, ch);

    if (editor->offset == editor->len) {
        editor->buf[editor->len++] = ch;
        editor->offset++;
    } else {
        memmove(&editor->buf[editor->offset + 1], &editor->buf[editor->offset], editor->len - editor->offset);
        editor->buf[editor->offset++] = ch;
        editor->len++;

        /* Reprint the character plus everything after. */
        reprint_from_current(editor, false);
    }
}

/** Erase a character from the current position.
 * @param editor        Line editor state.
 * @param forward       If true, will erase the character at the current cursor
 *                      position, else will erase the previous one. */
static void erase_char(line_editor_t *editor, bool forward) {
    if (forward) {
        if (editor->offset == editor->len)
            return;
    } else {
        if (!editor->offset) {
            return;
        } else {
            /* Decrement position and fall through. */
            editor->offset--;
            console_putc(editor->console, '\b');
        }
    }

    editor->len--;
    memmove(&editor->buf[editor->offset], &editor->buf[editor->offset + 1], editor->len - editor->offset);

    /* If we're now on a chunk boundary, we can resize the buffer down a chunk. */
    if (!(editor->len % LINE_EDITOR_CHUNK_SIZE))
        editor->buf = realloc(editor->buf, editor->len);

    /* Reprint everything. */
    reprint_from_current(editor, true);
}

/** Handle input on the line editor.
 * @param editor        Line editor state.
 * @param key           Key that was pressed. */
void line_editor_input(line_editor_t *editor, uint16_t key) {
    switch (key) {
    case CONSOLE_KEY_LEFT:
        if (editor->offset) {
            console_putc(editor->console, '\b');
            editor->offset--;
        }

        break;
    case CONSOLE_KEY_RIGHT:
        if (editor->offset != editor->len) {
            console_putc(editor->console, editor->buf[editor->offset]);
            editor->offset++;
        }

        break;
    case CONSOLE_KEY_HOME:
        while (editor->offset) {
            console_putc(editor->console, '\b');
            editor->offset--;
        }

        break;
    case CONSOLE_KEY_END:
        while (editor->offset < editor->len) {
            console_putc(editor->console, editor->buf[editor->offset]);
            editor->offset++;
        }

        break;
    case '\b':
        erase_char(editor, false);
        break;
    case 0x7f:
        erase_char(editor, true);
        break;
    case '\n':
        /* The shell code sends \n to place it at the end of the buffer. */
        editor->offset = editor->len;
        insert_char(editor, key);
        break;
    default:
        if (isprint(key))
            insert_char(editor, key);

        break;
    }
}

/**
 * Finish editing and return updated string.
 *
 * Returns a pointer to the new, null-terminated string. Since the editor works
 * with larger memory chunks internally, this function resizes the string down
 * to the correct size. This function will always return non-NULL, even if the
 * buffer is empty.
 *
 * @param editor        Line editor state.
 *
 * @return              Pointer to new string.
 */
char *line_editor_finish(line_editor_t *editor) {
    if (editor->len) {
        char *str;

        assert(editor->buf);

        str = realloc(editor->buf, editor->len + 1);
        str[editor->len] = 0;

        editor->buf = NULL;
        return str;
    } else {
        assert(!editor->buf);
        return strdup("");
    }
}

/** Discard editing state.
 * @param editor        Line editor state. */
void line_editor_destroy(line_editor_t *editor) {
    free(editor->buf);
}
