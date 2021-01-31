/*
 * Copyright (C) 2009-2021 Alex Smith
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

#ifndef __LIB_LINE_EDITOR_H
#define __LIB_LINE_EDITOR_H

#include <types.h>

struct console;

/** Line editor state. */
typedef struct line_editor {
    struct console *console;            /**< Console to output to. */
    char *buf;                          /**< String being edited. */
    size_t len;                         /**< Current string length. */
    size_t offset;                      /**< Current string offset. */
} line_editor_t;

extern void line_editor_init(line_editor_t *editor, struct console *console, const char *str);
extern void line_editor_output(line_editor_t *editor);
extern void line_editor_input(line_editor_t *editor, uint16_t key);
extern char *line_editor_finish(line_editor_t *editor);
extern void line_editor_destroy(line_editor_t *editor);

#endif /* __LIB_LINE_EDITOR_H */
