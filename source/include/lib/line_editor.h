/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
