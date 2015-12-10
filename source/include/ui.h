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
 * @brief               User interface.
 */

#ifndef __UI_H
#define __UI_H

#include <config.h>
#include <console.h>

#ifdef CONFIG_TARGET_HAS_UI

struct ui_entry;
struct ui_window;

/** Return codes for input handling functions. */
typedef enum input_result {
    INPUT_HANDLED,                      /**< No special action needed. */
    INPUT_RENDER_ENTRY,                 /**< Re-render the list entry. */
    INPUT_RENDER_HELP,                  /**< Re-render help (in case possible actions change). */
    INPUT_RENDER_WINDOW,                /**< Re-render the whole window. */
    INPUT_CLOSE,                        /**< Close the window. */
} input_result_t;

/** Structure defining a window type. */
typedef struct ui_window_type {
    /** Destroy the window (optional).
     * @param window        Window to destroy. */
    void (*destroy)(struct ui_window *window);

    /** Render the window.
     * @note                The draw region will be set to the content area,
     *                      cursor will be positioned at (0, 0). If the cursor
     *                      should be visible, this function should position and
     *                      enable it.
     * @param window        Window to render. */
    void (*render)(struct ui_window *window);

    /** Write the help text for the window.
     * @note                The draw region will be set to the help area, cursor
     *                      will be positioned where to write.
     * @param window        Window to write for. */
    void (*help)(struct ui_window *window);

    /** Handle input on the window.
     * @note                Draw region and cursor state are maintained from the
     *                      state initially set by render() and across all calls
     *                      to this until one returns INPUT_RENDER_WINDOW.
     * @param window        Window input was performed on.
     * @param key           Key that was pressed.
     * @return              Input handling result. */
    input_result_t (*input)(struct ui_window *window, uint16_t key);
} ui_window_type_t;

/** Window header structure. */
typedef struct ui_window {
    ui_window_type_t *type;             /**< Type of the window. */
    const char *title;                  /**< Title of the window. */
} ui_window_t;

/** Structure defining a UI list entry type. */
typedef struct ui_entry_type {
    /** Destroy the entry (optional).
     * @param entry         Entry to destroy. */
    void (*destroy)(struct ui_entry *entry);

    /** Render the entry.
     * @note                The draw region will set to where to render, cursor
     *                      will be positioned at (0, 0).
     * @param entry         Entry to render. */
    void (*render)(struct ui_entry *entry);

    /** Write the help text for the entry.
     * @note                The draw region will be set to the help area, cursor
     *                      will be positioned where to write.
     * @param entry         Entry to write for. */
    void (*help)(struct ui_entry *entry);

    /** Handle input on the entry.
     * @param entry         Entry input was performed on.
     * @param key           Key that was pressed.
     * @return              Input handling result. */
    input_result_t (*input)(struct ui_entry *entry, uint16_t key);
} ui_entry_type_t;

/** List entry header structure. */
typedef struct ui_entry {
    ui_entry_type_t *type;              /**< Type of the entry. */
} ui_entry_t;

extern void ui_window_destroy(ui_window_t *window);
extern void ui_entry_destroy(ui_entry_t *entry);

extern void ui_print_action(uint16_t key, const char *name);

extern void ui_push_title(const char *title);
extern void ui_pop_title(void);

extern bool ui_display(ui_window_t *window, unsigned timeout);

extern ui_window_t *ui_list_create(const char *title, bool exitable);
extern void ui_list_insert(ui_window_t *window, ui_entry_t *entry, bool selected);
extern void ui_list_add_section(ui_window_t *window, const char *text);
extern bool ui_list_empty(ui_window_t *window);

extern ui_entry_t *ui_link_create(ui_window_t *window);
extern ui_entry_t *ui_entry_create(const char *label, value_t *value);
extern ui_entry_t *ui_checkbox_create(const char *label, value_t *value);
extern ui_entry_t *ui_textbox_create(const char *label, value_t *value);
extern ui_entry_t *ui_chooser_create(const char *label, value_t *value);
extern void ui_chooser_insert(ui_entry_t *entry, const value_t *value, char *label);

extern ui_window_t *ui_textview_create(const char *title, const char *buf, size_t size, size_t start, size_t len);

#endif /* CONFIG_TARGET_HAS_UI */
#endif /* __UI_H */
