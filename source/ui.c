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

#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>
#include <ui.h>

/** Structure representing a list window. */
typedef struct ui_list {
    ui_window_t window;                 /**< Window header. */

    bool exitable;                      /**< Whether the menu can be exited. */
    ui_entry_t **entries;               /**< Array of entries. */
    size_t count;                       /**< Number of entries. */
    size_t offset;                      /**< Offset of first entry displayed. */
    size_t selected;                    /**< Index of selected entry. */
} ui_list_t;

/** Structure representing a link. */
typedef struct ui_link {
    ui_entry_t entry;                   /**< Entry header. */

    ui_window_t *window;                /**< Window that this links to. */
} ui_link_t;

/** Structure representing a checkbox. */
typedef struct ui_checkbox {
    ui_entry_t entry;                   /**< Entry header. */

    const char *label;                  /**< Label for the checkbox. */
    value_t *value;                     /**< Value modified by the checkbox. */
} ui_checkbox_t;

/** Structure representing a textbox. */
typedef struct ui_textbox {
    ui_entry_t entry;                   /**< Entry header. */

    const char *label;                  /**< Label for the textbox. */
    value_t *value;                     /**< Value modified by the textbox. */
} ui_textbox_t;

/** Structure for a textbox editor window. */
typedef struct ui_textbox_editor {
    ui_window_t window;

    char *buf;                          /**< String being edited. */
    size_t len;                         /**< Current string length. */
    size_t offset;                      /**< Current string offset. */
    bool update;                        /**< Whether to update textbox when the window closes. */
} ui_textbox_editor_t;

/** Properties of the UI console. */
static uint16_t ui_console_width;
static uint16_t ui_console_height;

/** Console that the UI is running on. */
console_t *ui_console;

/** Dimensions of the content area. */
#define CONTENT_WIDTH               (ui_console_width - 4)
#define CONTENT_HEIGHT              (ui_console_height - 6)

/** Size of a chunk to allocate in the textbox editor window. */
#define TEXTBOX_EDITOR_CHUNK_SIZE   128

/** Print an action (for help text).
 * @param key           Key for the action.
 * @param name          Name of the action. */
void ui_print_action(uint16_t key, const char *name) {
    switch (key) {
    case CONSOLE_KEY_UP:
        ui_printf("Up");
        break;
    case CONSOLE_KEY_DOWN:
        ui_printf("Down");
        break;
    case CONSOLE_KEY_LEFT:
        ui_printf("Left");
        break;
    case CONSOLE_KEY_RIGHT:
        ui_printf("Right");
        break;
    case CONSOLE_KEY_HOME:
        ui_printf("Home");
        break;
    case CONSOLE_KEY_END:
        ui_printf("End");
        break;
    case CONSOLE_KEY_F1 ... CONSOLE_KEY_F10:
        ui_printf("F%u", key + 1 - CONSOLE_KEY_F1);
        break;
    case '\n':
        ui_printf("Enter");
        break;
    case '\e':
        ui_printf("Esc");
        break;
    default:
        ui_printf("%c", key & 0xFF);
        break;
    }

    ui_printf(" = %s  ", name);
}

/** Set the draw region to the title region. */
static inline void set_title_region(void) {
    draw_region_t region;

    region.x = 2;
    region.y = 1;
    region.width = ui_console_width - 4;
    region.height = 1;
    region.scrollable = false;

    console_set_region(ui_console, &region);
    console_set_colour(ui_console, COLOUR_WHITE, COLOUR_BLACK);
}

/** Set the draw region to the help region. */
static inline void set_help_region(void) {
    draw_region_t region;

    region.x = 2;
    region.y = ui_console_height - 2;
    region.width = ui_console_width - 4;
    region.height = 1;
    region.scrollable = false;

    console_set_region(ui_console, &region);
    console_set_colour(ui_console, COLOUR_WHITE, COLOUR_BLACK);
}

/** Set the draw region to the content region. */
static inline void set_content_region(void) {
    draw_region_t region;

    region.x = 2;
    region.y = 3;
    region.width = CONTENT_WIDTH;
    region.height = CONTENT_HEIGHT;
    region.scrollable = false;

    console_set_region(ui_console, &region);
    console_set_colour(ui_console, COLOUR_LIGHT_GREY, COLOUR_BLACK);
}

/** Render help text for a window.
 * @param window        Window to render help text for.
 * @param timeout       Seconds remaining.
 * @param update        Whether this is an update. This will cause the current
 *                      draw region and cursor position to be preserved */
static void render_help(ui_window_t *window, unsigned timeout, bool update) {
    draw_region_t region;
    uint16_t x, y;
    bool visible;

    if (update) {
        console_get_region(ui_console, &region);
        console_get_cursor(ui_console, &x, &y, &visible);
    }

    set_help_region();

    /* Do not need to clear if this is not an update. */
    if (update)
        console_clear(ui_console, 0, 0, 0, 0);

    window->type->help(window);

    /* Only draw timeout if it is non-zero. */
    if (timeout) {
        console_set_cursor(ui_console, 0 - ((timeout >= 10) ? 12 : 11), 0, false);
        ui_printf("%u second(s)", timeout);
    }

    if (update) {
        console_set_region(ui_console, &region);
        console_set_colour(ui_console, COLOUR_LIGHT_GREY, COLOUR_BLACK);
        console_set_cursor(ui_console, x, y, visible);
    }
}

/** Render the contents of a window.
 * @param window        Window to render.
 * @param timeout       Seconds remaining. */
static void render_window(ui_window_t *window, unsigned timeout) {
    draw_region_t region;

    /* Clear the console and save its dimensions for convenient access. */
    console_reset(ui_console);
    console_get_region(ui_console, &region);
    ui_console_width = region.width;
    ui_console_height = region.height;

    /* Disable the cursor. */
    console_set_cursor(ui_console, 0, 0, false);

    /* Draw the title. */
    set_title_region();
    ui_printf("%s", window->title);

    /* Draw the help text. */
    render_help(window, timeout, false);

    /* Draw content last, so console state set by render() is preserved. */
    set_content_region();
    window->type->render(window);
}

/** Display a user interface.
 * @param window        Window to display.
 * @param console       Console to display on.
 * @param timeout       Seconds to wait before closing the window if no input.
 *                      If 0, the window will not time out. */
void ui_display(ui_window_t *window, console_t *console, unsigned timeout) {
    bool done;

    if (!console->out || !console->in)
        return;

    ui_console = console;
    render_window(window, timeout);

    /* Handle input until told to exit. */
    done = false;
    while (!done) {
        uint16_t key;
        input_result_t result;

        key = console_getc(ui_console);
        result = window->type->input(window, key);
        switch (result) {
        case INPUT_CLOSE:
            done = true;
            break;
        case INPUT_RENDER_HELP:
            /* Doing a partial update, should preserve the draw region and the
             * cursor state within it. */
            render_help(window, timeout, true);
            break;
        case INPUT_RENDER_WINDOW:
            render_window(window, timeout);
            break;
        default:
            /* INPUT_RENDER_ENTRY is handled by ui_list_input(). */
            break;
        }
    }

    console_reset(ui_console);
}

/** Destroy a list window.
 * @param window        Window to destroy. */
static void ui_list_destroy(ui_window_t *window) {
    ui_list_t *list = (ui_list_t *)window;

    for (size_t i = 0; i < list->count; i++)
        ui_entry_destroy(list->entries[i]);

    free(list->entries);
}

/** Render an entry from a list.
 * @param entry         Entry to render.
 * @param pos           Position to render at.
 * @param selected      Whether the entry is selected. */
static void render_entry(ui_entry_t *entry, size_t pos, bool selected) {
    draw_region_t region, content;

    /* Work out where to put the entry. */
    console_get_region(ui_console, &content);
    region.x = content.x;
    region.y = content.y + pos;
    region.width = content.width;
    region.height = 1;
    region.scrollable = false;
    console_set_region(ui_console, &region);

    /* Clear the area. If the entry is selected, it should be highlighted. */
    console_set_colour(ui_console,
        (selected) ? COLOUR_BLACK : COLOUR_LIGHT_GREY,
        (selected) ? COLOUR_LIGHT_GREY : COLOUR_BLACK);
    console_clear(ui_console, 0, 0, 0, 0);

    /* Render the entry. */
    entry->type->render(entry);

    /* Restore content region and colour. */
    console_set_region(ui_console, &content);
    console_set_colour(ui_console, COLOUR_LIGHT_GREY, COLOUR_BLACK);
}

/** Render a list window.
 * @param window        Window to render. */
static void ui_list_render(ui_window_t *window) {
    ui_list_t *list = (ui_list_t *)window;
    size_t end;

    /* Calculate the range of entries to display. */
    end = min(list->offset + CONTENT_HEIGHT, list->count);

    /* Render the entries. */
    for (size_t i = list->offset; i < end; i++) {
        size_t pos = i - list->offset;
        bool selected = i == list->selected;
        render_entry(list->entries[i], pos, selected);
    }
}

/** Write the help text for a list window.
 * @param window        Window to write for. */
static void ui_list_help(ui_window_t *window) {
    ui_list_t *list = (ui_list_t *)window;

    if (list->count) {
        ui_entry_t *selected = list->entries[list->selected];

        /* Print help for the selected entry. */
        if (selected->type->help)
            selected->type->help(selected);
    }

    if (list->exitable)
        ui_print_action('\e', "Back");
}

/** Handle input on a list window.
 * @param window        Window input was performed on.
 * @param key           Key that was pressed.
 * @return              Input handling result. */
static input_result_t ui_list_input(ui_window_t *window, uint16_t key) {
    ui_list_t *list = (ui_list_t *)window;
    ui_entry_t *entry;
    input_result_t ret;

    switch (key) {
    case CONSOLE_KEY_UP:
        if (list->selected == 0)
            return INPUT_HANDLED;

        /* Redraw current entry as not selected. */
        entry = list->entries[list->selected];
        render_entry(entry, list->selected - list->offset, false);

        /* If selected becomes less than the offset, must scroll up. */
        list->selected--;
        if (list->selected < list->offset) {
            list->offset--;
            console_scroll_up(ui_console);
        }

        /* Draw the new entry highlighted. */
        entry = list->entries[list->selected];
        render_entry(entry, list->selected - list->offset, true);

        /* Possible actions may have changed, re-render help. */
        return INPUT_RENDER_HELP;
    case CONSOLE_KEY_DOWN:
        if (list->selected >= list->count - 1)
            return INPUT_HANDLED;

        /* Redraw current entry as not selected. */
        entry = list->entries[list->selected];
        render_entry(entry, list->selected - list->offset, false);

        /* If selected is now off screen, must scroll down. */
        list->selected++;
        if (list->selected >= list->offset + CONTENT_HEIGHT) {
            list->offset++;
            console_scroll_down(ui_console);
        }

        /* Draw the new entry highlighted. */
        entry = list->entries[list->selected];
        render_entry(entry, list->selected - list->offset, true);

        /* Possible actions may have changed, re-render help. */
        return INPUT_RENDER_HELP;
    case '\e':
        return (list->exitable) ? INPUT_CLOSE : INPUT_HANDLED;
    default:
        /* Pass through to the selected entry. */
        entry = list->entries[list->selected];
        ret = entry->type->input(entry, key);

        /* Re-render the entry if requested. */
        if (ret == INPUT_RENDER_ENTRY) {
            render_entry(entry, list->selected - list->offset, true);
            ret = INPUT_HANDLED;
        }

        return ret;
    }
}

/** List window type. */
static ui_window_type_t ui_list_window_type = {
    .destroy = ui_list_destroy,
    .render = ui_list_render,
    .help = ui_list_help,
    .input = ui_list_input,
};

/** Create a list window.
 * @param title         Title for the window.
 * @param exitable      Whether the window can be exited.
 * @return              Pointer to created window. */
ui_window_t *ui_list_create(const char *title, bool exitable) {
    ui_list_t *list;

    list = malloc(sizeof(*list));
    list->window.type = &ui_list_window_type;
    list->window.title = title;
    list->exitable = exitable;
    list->entries = NULL;
    list->count = 0;
    list->offset = 0;
    list->selected = 0;
    return &list->window;
}

/** Insert an entry into a list window.
 * @param window        Window to insert into.
 * @param entry         Entry to insert.
 * @param selected      Whether the entry should be selected. */
void ui_list_insert(ui_window_t *window, ui_entry_t *entry, bool selected) {
    ui_list_t *list = (ui_list_t *)window;
    size_t pos;

    pos = list->count++;
    list->entries = realloc(list->entries, sizeof(*list->entries) * list->count);
    list->entries[pos] = entry;

    if (selected) {
        list->selected = pos;
        if (pos >= CONTENT_HEIGHT)
            list->offset = (pos - CONTENT_HEIGHT) + 1;
    }
}

/** Return whether a list is empty.
 * @param window        Window to check.
 * @return              Whether the list is empty. */
bool ui_list_empty(ui_window_t *window) {
    ui_list_t *list = (ui_list_t *)window;

    return list->count == 0;
}

/** Render a link.
 * @param entry         Entry to render. */
static void ui_link_render(ui_entry_t *entry) {
    ui_link_t *link = (ui_link_t *)entry;

    ui_printf("%s", link->window->title);
    console_set_cursor(ui_console, -2, 0, false);
    ui_printf("->");
}

/** Write the help text for a link.
 * @param entry         Entry to write for. */
static void ui_link_help(ui_entry_t *entry) {
    ui_print_action('\n', "Select");
}

/** Handle input on a link.
 * @param entry         Entry input was performed on.
 * @param key           Key that was pressed.
 * @return              Input handling result. */
static input_result_t ui_link_input(ui_entry_t *entry, uint16_t key) {
    ui_link_t *link = (ui_link_t *)entry;

    switch (key) {
    case '\n':
        ui_display(link->window, ui_console, 0);
        return INPUT_RENDER_WINDOW;
    default:
        return INPUT_HANDLED;
    }
}

/** Link entry type. */
static ui_entry_type_t ui_link_entry_type = {
    .render = ui_link_render,
    .help = ui_link_help,
    .input = ui_link_input,
};

/** Create an entry which opens another window.
 * @param window        Window that the entry should open.
 * @return              Pointer to entry. */
ui_entry_t *ui_link_create(ui_window_t *window) {
    ui_link_t *link;

    link = malloc(sizeof(*link));
    link->entry.type = &ui_link_entry_type;
    link->window = window;
    return &link->entry;
}

/** Create an entry appropriate to edit a value.
 * @param label         Label to give the entry.
 * @param value         Value to edit.
 * @return              Pointer to created entry. */
ui_entry_t *ui_entry_create(const char *label, value_t *value) {
    switch (value->type) {
    case VALUE_TYPE_BOOLEAN:
        return ui_checkbox_create(label, value);
    case VALUE_TYPE_STRING:
        return ui_textbox_create(label, value);
    default:
        assert(0 && "Unhandled value type");
        return NULL;
    }
}

/** Render a checkbox.
 * @param entry         Entry to render. */
static void ui_checkbox_render(ui_entry_t *entry) {
    ui_checkbox_t *box = (ui_checkbox_t *)entry;

    ui_printf("%s", box->label);
    console_set_cursor(ui_console, -3, 0, false);
    ui_printf("[%c]", (box->value->boolean) ? 'x' : ' ');
}

/** Write the help text for a checkbox.
 * @param entry         Entry to write for. */
static void ui_checkbox_help(ui_entry_t *entry) {
    ui_print_action('\n', "Toggle");
}

/** Handle input on a checkbox.
 * @param entry         Entry input was performed on.
 * @param key           Key that was pressed.
 * @return              Input handling result. */
static input_result_t ui_checkbox_input(ui_entry_t *entry, uint16_t key) {
    ui_checkbox_t *box = (ui_checkbox_t *)entry;

    switch (key) {
    case '\n':
    case ' ':
        box->value->boolean = !box->value->boolean;
        return INPUT_RENDER_ENTRY;
    default:
        return INPUT_HANDLED;
    }
}

/** Checkbox entry type. */
static ui_entry_type_t ui_checkbox_entry_type = {
    .render = ui_checkbox_render,
    .help = ui_checkbox_help,
    .input = ui_checkbox_input,
};

/** Create a checkbox entry.
 * @param label         Label for the checkbox (not duplicated).
 * @param value         Value to store state in (should be VALUE_TYPE_BOOLEAN).
 * @return              Pointer to created entry. */
ui_entry_t *ui_checkbox_create(const char *label, value_t *value) {
    ui_checkbox_t *box;

    assert(value->type == VALUE_TYPE_BOOLEAN);

    box = malloc(sizeof(*box));
    box->entry.type = &ui_checkbox_entry_type;
    box->label = label;
    box->value = value;
    return &box->entry;
}

/** Render a textbox editor window.
 * @param window        Window to render. */
static void ui_textbox_editor_render(ui_window_t *window) {
    ui_textbox_editor_t *editor = (ui_textbox_editor_t *)window;
    uint16_t x, y;

    for (size_t i = 0; i <= editor->len; i++) {
        if (i == editor->offset)
            console_get_cursor(ui_console, &x, &y, NULL);

        if (i < editor->len)
            console_putc(ui_console, editor->buf[i]);
    }

    console_set_cursor(ui_console, x, y, true);
}

/** Write the help text for a textbox editor window.
 * @param window        Window to write for. */
static void ui_textbox_editor_help(ui_window_t *window) {
    ui_print_action('\n', "Update");
    ui_print_action('\e', "Cancel");
}

/** Insert a character to the buffer at the current position.
 * @param editor        Editor being operated on.
 * @param ch            Character to insert. */
static void insert_textbox_char(ui_textbox_editor_t *editor, char ch) {
    /* Resize the buffer if this will go over a chunk boundary. We allocate in
     * chunks to avoid reallocating on every modification. */
    if (!(editor->len % TEXTBOX_EDITOR_CHUNK_SIZE))
        editor->buf = realloc(editor->buf, editor->len + TEXTBOX_EDITOR_CHUNK_SIZE);

    console_putc(ui_console, ch);

    if (editor->offset == editor->len) {
        editor->buf[editor->len++] = ch;
        editor->offset++;
    } else {
        size_t i;

        memmove(&editor->buf[editor->offset + 1], &editor->buf[editor->offset], editor->len - editor->offset);
        editor->buf[editor->offset++] = ch;
        editor->len++;

        /* Reprint the character plus everything after, maintaining the
         * current cursor position. */
        for (i = 0; i < editor->len - editor->offset; i++)
            console_putc(ui_console, editor->buf[editor->offset + i]);
        while (i--)
            console_putc(ui_console, '\b');
    }
}

/** Erase a character from the current position.
 * @param editor        Editor being operated on.
 * @param forward       If true, will erase the character at the current cursor
 *                      position, else will erase the previous one. */
static void erase_textbox_char(ui_textbox_editor_t *editor, bool forward) {
    size_t i;

    if (forward) {
        if (editor->offset == editor->len)
            return;
    } else {
        if (!editor->offset) {
            return;
        } else {
            /* Decrement position and fall through. */
            editor->offset--;
            console_putc(ui_console, '\b');
        }
    }

    editor->len--;
    memmove(&editor->buf[editor->offset], &editor->buf[editor->offset + 1], editor->len - editor->offset);

    /* If we're now on a chunk boundary, we can resize the buffer down a chunk. */
    if (!(editor->len % TEXTBOX_EDITOR_CHUNK_SIZE))
        editor->buf = realloc(editor->buf, editor->len);

    /* Reprint everything, maintaining cursor position. */
    for (i = 0; i < editor->len - editor->offset; i++)
        console_putc(ui_console, editor->buf[editor->offset + i]);
    console_putc(ui_console, ' ');
    i++;
    while (i--)
        console_putc(ui_console, '\b');
}

/** Handle input on a textbox editor window.
 * @param window        Window input was performed on.
 * @param key           Key that was pressed.
 * @return              Input handling result. */
static input_result_t ui_textbox_editor_input(ui_window_t *window, uint16_t key) {
    ui_textbox_editor_t *editor = (ui_textbox_editor_t *)window;

    /* TODO: Common line editing code, this is all pretty much the same as the
     * shell's editing code. */
    switch (key) {
    case '\n':
        editor->update = true;
    case '\e':
        return INPUT_CLOSE;
    case CONSOLE_KEY_LEFT:
        if (editor->offset) {
            console_putc(ui_console, '\b');
            editor->offset--;
        }

        break;
    case CONSOLE_KEY_RIGHT:
        if (editor->offset != editor->len) {
            console_putc(ui_console, editor->buf[editor->offset]);
            editor->offset++;
        }

        break;
    case CONSOLE_KEY_HOME:
        while (editor->offset) {
            console_putc(ui_console, '\b');
            editor->offset--;
        }

        break;
    case CONSOLE_KEY_END:
        while (editor->offset < editor->len) {
            console_putc(ui_console, editor->buf[editor->offset]);
            editor->offset++;
        }

        break;
    case '\b':
        erase_textbox_char(editor, false);
        break;
    case 0x7f:
        erase_textbox_char(editor, true);
        break;
    default:
        if (isprint(key))
            insert_textbox_char(editor, key);

        break;
    }

    return INPUT_HANDLED;
}

/** Text box editor window type. */
static ui_window_type_t ui_textbox_editor_window_type = {
    .render = ui_textbox_editor_render,
    .help = ui_textbox_editor_help,
    .input = ui_textbox_editor_input,
};

/** Render a textbox.
 * @param entry         Entry to render. */
static void ui_textbox_render(ui_entry_t *entry) {
    ui_textbox_t *box = (ui_textbox_t *)entry;
    size_t len, avail;

    ui_printf("%s", box->label);

    /* Work out the length available to put the string value in. */
    avail = CONTENT_WIDTH - strlen(box->label) - 3;
    len = strlen(box->value->string);
    if (len > avail) {
        ui_printf(" [");
        for (size_t i = 0; i < avail - 3; i++)
            console_putc(ui_console, box->value->string[i]);
        ui_printf("...]");
    } else {
        console_set_cursor(ui_console, 0 - len - 2, 0, false);
        ui_printf("[%s]", box->value->string);
    }
}

/** Write the help text for a textbox.
 * @param entry         Entry to write for. */
static void ui_textbox_help(ui_entry_t *entry) {
    ui_print_action('\n', "Edit");
}

/** Handle input on a textbox.
 * @param entry         Entry input was performed on.
 * @param key           Key that was pressed.
 * @return              Input handling result. */
static input_result_t ui_textbox_input(ui_entry_t *entry, uint16_t key) {
    ui_textbox_t *box = (ui_textbox_t *)entry;

    if (key == '\n') {
        ui_textbox_editor_t editor;
        size_t len;
        char *title __cleanup_free;

        /* Determine the window title. */
        len = strlen(box->label) + 8;
        title = malloc(len);
        snprintf(title, len, "Edit '%s'", box->label);

        editor.window.type = &ui_textbox_editor_window_type;
        editor.window.title = title;
        editor.len = strlen(box->value->string);
        editor.offset = editor.len;
        editor.update = false;

        if (editor.len) {
            editor.buf = malloc(round_up(editor.len, TEXTBOX_EDITOR_CHUNK_SIZE));
            memcpy(editor.buf, box->value->string, editor.len);
        } else {
            editor.buf = NULL;
        }

        ui_display(&editor.window, ui_console, 0);

        if (editor.update) {
            /* Resize buffer down to exact size. */
            free(box->value->string);
            box->value->string = realloc(editor.buf, editor.len + 1);
            box->value->string[editor.len] = 0;
        }

        return INPUT_RENDER_WINDOW;
    } else {
        return INPUT_HANDLED;
    }
}

/** Textbox entry type. */
static ui_entry_type_t ui_textbox_entry_type = {
    .render = ui_textbox_render,
    .help = ui_textbox_help,
    .input = ui_textbox_input,
};

/** Create a textbox entry.
 * @param label         Label for the textbox.
 * @param value         Value to store state in (should be VALUE_TYPE_STRING).
 * @return              Pointer to created entry. */
ui_entry_t *ui_textbox_create(const char *label, value_t *value) {
    ui_textbox_t *box;

    assert(value->type == VALUE_TYPE_STRING);

    box = malloc(sizeof(*box));
    box->entry.type = &ui_textbox_entry_type;
    box->label = label;
    box->value = value;
    return &box->entry;
}

/** Destroy a window.
 * @param window        Window to destroy. */
void ui_window_destroy(ui_window_t *window) {
    if (window->type->destroy)
        window->type->destroy(window);

    free(window);
}

/** Destroy a list entry.
 * @param entry         Entry to destroy. */
void ui_entry_destroy(ui_entry_t *entry) {
    if (entry->type->destroy)
        entry->type->destroy(entry);

    free(entry);
}
