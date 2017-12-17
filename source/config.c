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
 * @brief               Configuration system.
 *
 * The configuration system is based around commands: a configuration file is
 * parsed into a list of commands and their arguments, then the command list is
 * executed to yield an environment.
 *
 * An environment stores the state necessary to load an operating system: an OS
 * loader type and any internal data it needs, variables which influence the
 * behaviour of the loader, and a current device which is used when looking up
 * filesystem paths that do not specify a device name.
 *
 * Each OS loader type provides a configuration command. OS loader commands
 * should not directly load the OS, rather they just set the OS loader type in
 * the environment and save any data they require (arguments to the command,
 * etc.). The OS is loaded once the loader type's load method is called. This
 * split is mainly to allow OS loaders to provide a configuration UI. Commands
 * for a menu entry are executed immediately when loading the configuration
 * rather than when the menu entry is selected, which allows OS loader commands
 * to, for example, examine the kernel image before the menu is displayed and
 * change the UI based on it. The KBoot protocol uses this to build a
 * configuration UI from the option tags in the kernel image.
 *
 * The configuration parser is structured such that it can be used both when
 * reading from a configuration file and when in the shell. A helper function is
 * used by the parser to read characters from the input, which when reading a
 * file gets the next character from it, and when in the shell will read input
 * characters from the console.
 *
 * A separate set of output/error handling functions are provided for use within
 * config commands. This is because the output and error handling strategy are
 * different depending on whether reading from a configuration file or in the
 * shell. When reading a configuration file, errors should be handled by calling
 * error(), i.e. to display an error UI, while when in the shell, errors should
 * just be printed out to the console, and the command should just return to the
 * prompt.
 *
 * TODO:
 *  - The parser is a pretty terrible hand-written one. It should probably be
 *    replaced with a better one at some point in the future, particularly if
 *    the configuration format gets extended any more.
 *  - Should allow to escape the $ in a variable reference in a string. Slightly
 *    awkward interaction with the escape handling in parse_string() - we have
 *    to defer substitution until a command is executed so that it will get the
 *    current value of the variable, but at parse time parse_string() will have
 *    fiddled with the escape characters.
 */

#include <lib/ctype.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <config.h>
#include <console.h>
#include <device.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>
#include <menu.h>
#include <shell.h>

/** Structure containing details of a command to run. */
typedef struct command_list_entry {
    list_t header;                      /**< Link to command list. */
    char *name;                         /**< Name of the command. */
    value_list_t *args;                 /**< List of arguments. */
} command_list_entry_t;

/** Structure containing an environment entry. */
typedef struct environ_entry {
    list_t header;                      /**< Link to environment. */
    char *name;                         /**< Name of entry. */
    value_t value;                      /**< Value of the entry. */
} environ_entry_t;

static command_list_t *parse_command_list(void);

/** Length of the temporary buffer. */
#define TEMP_BUF_LEN 512

/** Temporary buffer to collect strings in. */
static char temp_buf[TEMP_BUF_LEN];
static size_t temp_buf_idx;

/** Current error handler function. */
static config_error_handler_t current_error_handler;

/** Current read helper function. */
static config_read_helper_t current_helper;

/** Name of the currently executing command. */
static const char *current_command;

/** Current parser state. */
static const char *current_path;        /**< Current configuration file path. */
static int current_line;                /**< Current line in the file. */
static int current_col;                 /**< Current column in the file (minus 1). */
static unsigned nesting_count;          /**< Parser nesting count. */
static int returned_char;               /**< Character returned with return_char() (0 is no char). */
static bool ignore_comments;            /**< Whether to ignore comments. */

/** Current file state used by config_load(). */
static char *current_file;              /**< Pointer to data for current file. */
static size_t current_file_offset;      /**< Current offset in the file. */
static size_t current_file_size;        /**< Size of the current file. */

/** Configuration file paths to try. */
static const char *config_file_paths[] = {
    "/boot/kboot.cfg",
    "/kboot.cfg",
};

/** Reserved environment variable names. */
static const char *reserved_environ_names[] = {
    "device",
    "device_label",
    "device_uuid",
};

/** Environment variable names to not inherit. */
static const char *no_inherit_environ_names[] = {
    "default",
    "gui",
    "gui_background",
    "gui_icon",
    "gui_selection",
    "hidden",
    "timeout",
};

/** Overridden configuration file path. */
char *config_file_override;

/** Root environment. */
environ_t *root_environ;

/** Current environment. */
environ_t *current_environ;

/**
 * Handle a configuration error.
 *
 * The action of this function depends on whether the error results from an
 * error in a configuration file, or if being executed from the shell. If the
 * error is in the file, the function will display an error UI and not return.
 * If the error occurred in the shell, it will print the error message to the
 * console and return.
 *
 * @param fmt           Format string used to create the error message.
 * @param ...           Arguments to substitute into format.
 */
void config_error(const char *fmt, ...) {
    va_list args;
    const char *cmd;

    va_start(args, fmt);

    /* Set current_command to NULL so that it will not be set if the handler
     * calls boot_error() and then the user goes into the shell. */
    cmd = current_command;
    current_command = NULL;

    if (current_error_handler) {
        current_error_handler(cmd, fmt, args);
    } else {
        vsnprintf(temp_buf, TEMP_BUF_LEN, fmt, args);
        boot_error("%s", temp_buf);
    }

    va_end(args);
}

/** Set the configuration error handler.
 * @param handler       Handler function.
 * @return              Previous handler. */
config_error_handler_t config_set_error_handler(config_error_handler_t handler) {
    swap(current_error_handler, handler);
    return handler;
}

/**
 * Value functions.
 */

/** Initialize a value to a default (empty) value.
 * @param value         Value to initialize.
 * @param type          Type of the value. */
void value_init(value_t *value, value_type_t type) {
    value->type = type;

    switch (type) {
    case VALUE_TYPE_INTEGER:
        value->integer = 0;
        break;
    case VALUE_TYPE_BOOLEAN:
        value->boolean = false;
        break;
    case VALUE_TYPE_STRING:
        value->string = strdup("");
        break;
    case VALUE_TYPE_LIST:
        value->list = malloc(sizeof(*value->list));
        value->list->values = NULL;
        value->list->count = 0;
        break;
    case VALUE_TYPE_COMMAND_LIST:
        value->cmds = malloc(sizeof(*value->cmds));
        list_init(value->cmds);
        break;
    default:
        assert(0 && "Setting invalid value type");
    }
}

/** Destroy a value.
 * @param value         Value to destroy. */
void value_destroy(value_t *value) {
    /* Pointers can be NULL on cleanup path from parse_value_list(). */
    switch (value->type) {
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_REFERENCE:
        free(value->string);
        break;
    case VALUE_TYPE_LIST:
        if (value->list)
            value_list_destroy(value->list);
        break;
    case VALUE_TYPE_COMMAND_LIST:
        if (value->cmds)
            command_list_destroy(value->cmds);
        break;
    default:
        break;
    }
}

/** Copy the contents of one value to another.
 * @param source        Source value.
 * @param dest          Destination value (should be uninitialized). */
void value_copy(const value_t *source, value_t *dest) {
    dest->type = source->type;

    switch (dest->type) {
    case VALUE_TYPE_INTEGER:
        dest->integer = source->integer;
        break;
    case VALUE_TYPE_BOOLEAN:
        dest->boolean = source->boolean;
        break;
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_REFERENCE:
        dest->string = strdup(source->string);
        break;
    case VALUE_TYPE_LIST:
        dest->list = value_list_copy(source->list);
        break;
    case VALUE_TYPE_COMMAND_LIST:
        dest->cmds = command_list_copy(source->cmds);
        break;
    }
}

/** Move the contents of one value to another.
 * @param source        Source value (will be invalidated, do not use except
 *                      for passing to value_destroy()).
 * @param dest          Destination value (should be uninitialized). */
void value_move(value_t *source, value_t *dest) {
    dest->type = source->type;

    switch (dest->type) {
    case VALUE_TYPE_INTEGER:
        dest->integer = source->integer;
        break;
    case VALUE_TYPE_BOOLEAN:
        dest->boolean = source->boolean;
        break;
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_REFERENCE:
        dest->string = source->string;
        source->string = NULL;
        break;
    case VALUE_TYPE_LIST:
        dest->list = source->list;
        source->list = NULL;
        break;
    case VALUE_TYPE_COMMAND_LIST:
        dest->cmds = source->cmds;
        source->cmds = NULL;
        break;
    }
}

/** Check if two values are equal.
 * @param value         First value.
 * @param other         Second value.
 * @return              Whether the values are equal. */
bool value_equals(const value_t *value, const value_t *other) {
    if (value->type == other->type) {
        switch (value->type) {
        case VALUE_TYPE_INTEGER:
            return value->integer == other->integer;
        case VALUE_TYPE_BOOLEAN:
            return value->boolean == other->boolean;
            break;
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_REFERENCE:
            return strcmp(value->string, other->string) == 0;
        default:
            assert(0 && "Comparison not implemented for lists");
            break;
        }
    }

    return false;
}

/**
 * Substitute variable references in a value.
 *
 * Substitutes variable references in a value (entirely replaces reference
 * values, and substitutes variables within strings). If the value is a list,
 * will recurse onto the values contained within the list. If an error occurs
 * while substituting variables, the error will be raised with config_error()
 * and the function will return false. For strings/references, if an error
 * occurs, the value will not be changed.
 *
 * @param value         Value to substitute in.
 * @param env           Environment to take variables from.
 *
 * @return              Whether variables were successfully substituted.
 */
bool value_substitute(value_t *value, environ_t *env) {
    const value_t *target;
    char *str;
    size_t i, start;

    switch (value->type) {
    case VALUE_TYPE_REFERENCE:
        /* Non-string variable reference, replace the whole value. */
        target = environ_lookup(env, value->string);
        if (!target) {
            config_error("Variable '%s' not found", value->string);
            return false;
        }

        free(value->string);
        value_copy(target, value);
        break;
    case VALUE_TYPE_STRING:
        /* Search for in-string variable references, which we substitute in the
         * string for a string representation of the variable. */
        str = strdup(value->string);
        i = 0;
        start = 0;
        while (str[i]) {
            if (start) {
                if (isalnum(str[i]) || str[i] == '_') {
                    i++;
                } else if (str[i] == '}') {
                    const char *name;
                    char *subst;
                    size_t prefix_len, var_len, len;

                    str[start - 2] = 0;
                    str[i] = 0;

                    /* We have a whole reference. */
                    name = &str[start];
                    target = environ_lookup(env, name);
                    if (!target) {
                        config_error("Variable '%s' not found", name);
                        free(str);
                        return false;
                    }

                    /* Stringify the target into the temporary buffer. */
                    switch (target->type) {
                    case VALUE_TYPE_INTEGER:
                        snprintf(temp_buf, TEMP_BUF_LEN, "%llu", target->integer);
                        break;
                    case VALUE_TYPE_BOOLEAN:
                        snprintf(temp_buf, TEMP_BUF_LEN, (target->boolean) ? "true" : "false");
                        break;
                    case VALUE_TYPE_STRING:
                        snprintf(temp_buf, TEMP_BUF_LEN, "%s", target->string);
                        break;
                    default:
                        config_error("Variable '%s' cannot be converted to string", name);
                        free(str);
                        return false;
                    }

                    /* Now allocate a new string. The start and end characters
                     * of the reference have been replaced with null terminators
                     * effectively splitting up the string into 3 parts. */
                    prefix_len = strlen(str);
                    var_len = strlen(temp_buf);
                    len = prefix_len + var_len + strlen(&str[i + 1]) + 1;
                    subst = malloc(len);
                    snprintf(subst, len, "%s%s%s", str, temp_buf, &str[i + 1]);

                    /* Replace the string and continue after the substituted
                     * portion. */
                    free(str);
                    str = subst;
                    i = prefix_len + var_len;
                    start = 0;
                } else {
                    start = 0;
                    i++;
                }
            } else {
                if (str[i] == '$' && str[i + 1] == '{') {
                    i += 2;
                    start = i;
                } else {
                    i++;
                }
            }
        }

        free(value->string);
        value->string = str;
        break;
    case VALUE_TYPE_LIST:
        for (i = 0; i < value->list->count; i++) {
            if (!value_substitute(&value->list->values[i], env))
                return false;
        }

        break;
    default:
        break;
    }

    return true;
}

/**
 * Value list functions.
 */

/** Destroy an argument list.
 * @param list          List to destroy. */
void value_list_destroy(value_list_t *list) {
    for (size_t i = 0; i < list->count; i++)
        value_destroy(&list->values[i]);

    free(list->values);
    free(list);
}

/** Copy a value list.
 * @param source        Source list.
 * @return              Pointer to destination list. */
value_list_t *value_list_copy(const value_list_t *source) {
    value_list_t *dest = malloc(sizeof(*dest));

    dest->count = source->count;

    if (source->count) {
        dest->values = malloc(sizeof(*dest->values) * source->count);
        for (size_t i = 0; i < source->count; i++)
            value_copy(&source->values[i], &dest->values[i]);
    } else {
        dest->values = NULL;
    }

    return dest;
}

/**
 * Command list functions.
 */

/** Destroy a command list.
 * @param list          List to destroy. */
void command_list_destroy(command_list_t *list) {
    list_foreach_safe(list, iter) {
        command_list_entry_t *command = list_entry(iter, command_list_entry_t, header);

        list_remove(&command->header);

        /* Can be NULL on the cleanup path from parse_command_list(). */
        if (command->args)
            value_list_destroy(command->args);

        free(command->name);
        free(command);
    }

    free(list);
}

/** Copy a command list.
 * @param source        Source list.
 * @return              Pointer to destination list. */
command_list_t *command_list_copy(const command_list_t *source) {
    command_list_t *dest = malloc(sizeof(*dest));

    list_init(dest);

    list_foreach(source, iter) {
        command_list_entry_t *entry = list_entry(iter, command_list_entry_t, header);
        command_list_entry_t *copy = malloc(sizeof(*copy));

        list_init(&copy->header);
        copy->name = strdup(entry->name);
        copy->args = value_list_copy(entry->args);

        list_append(dest, &copy->header);
    }

    return dest;
}

/** Execute a single command from a command list.
 * @param entry         Entry to execute.
 * @return              Whether successful. */
static bool command_exec(command_list_entry_t *entry) {
    value_t args;

    /* Recursively substitute variable references in the argument list. */
    args.type = VALUE_TYPE_LIST;
    args.list = entry->args;
    if (!value_substitute(&args, current_environ))
        return false;

    builtin_foreach(BUILTIN_TYPE_COMMAND, command_t, command) {
        if (strcmp(command->name, entry->name) == 0) {
            const char *prev = current_command;
            bool ret;

            current_command = command->name;
            ret = command->func(entry->args);
            current_command = prev;
            return ret;
        }
    }

    config_error("Unknown command '%s'", entry->name);
    return false;
}

/** Execute a command list.
 * @param list          List of commands.
 * @param env           Environment to execute commands under.
 * @return              Whether all of the commands completed successfully. */
bool command_list_exec(command_list_t *list, environ_t *env) {
    environ_t *prev;
    bool ret = false;

    /* Set the environment as the current. */
    prev = current_environ;
    current_environ = env;

    /* Execute each command. */
    list_foreach(list, iter) {
        command_list_entry_t *entry = list_entry(iter, command_list_entry_t, header);

        /* Loader command must be the last command in the list, prevent any
         * other commands from being run if we have a loader set. */
        if (current_environ->loader) {
            config_error("Loader command must be final command");
            goto out;
        }

        if (!command_exec(entry))
            goto out;
    }

    ret = true;

out:
    current_environ = prev;
    return ret;
}

/**
 * Environment management.
 */

/** Create a new environment.
 * @param parent        Parent environment.
 * @return              Pointer to created environment. */
environ_t *environ_create(environ_t *parent) {
    environ_t *env = malloc(sizeof(*env));

    list_init(&env->entries);
    list_init(&env->menu_entries);
    env->loader = NULL;
    env->loader_private = NULL;

    if (parent) {
        env->device = parent->device;

        env->directory = parent->directory;
        if (env->directory)
            fs_retain(env->directory);

        list_foreach(&parent->entries, iter) {
            const environ_entry_t *entry = list_entry(iter, environ_entry_t, header);
            environ_entry_t *clone;

            /* Check if this is a name we should not inherit. */
            for (size_t i = 0; i < array_size(no_inherit_environ_names); i++) {
                if (!strcmp(entry->name, no_inherit_environ_names[i])) {
                    entry = NULL;
                    break;
                }
            }

            if (!entry)
                continue;

            clone = malloc(sizeof(*clone));
            list_init(&clone->header);
            clone->name = strdup(entry->name);
            value_copy(&entry->value, &clone->value);
            list_append(&env->entries, &clone->header);
        }
    } else {
        env->device = NULL;
        env->directory = NULL;
    }

    return env;
}

/** Destroy an environment.
 * @param env           Environment to destroy. */
void environ_destroy(environ_t *env) {
    menu_cleanup(env);

    list_foreach_safe(&env->entries, iter) {
        environ_entry_t *entry = list_entry(iter, environ_entry_t, header);

        list_remove(&entry->header);
        free(entry->name);
        value_destroy(&entry->value);
        free(entry);
    }

    free(env);
}

/** Look up an entry in an environment.
 * @param env           Environment to look up in.
 * @param name          Name of entry to look up.
 * @return              Pointer to value if found, NULL if not. */
value_t *environ_lookup(environ_t *env, const char *name) {
    list_foreach(&env->entries, iter) {
        environ_entry_t *entry = list_entry(iter, environ_entry_t, header);

        if (strcmp(entry->name, name) == 0)
            return &entry->value;
    }

    return NULL;
}

/** Insert an entry into an environment.
 * @param env           Environment to insert into.
 * @param name          Name of entry to look up.
 * @param value         Value to insert. Will be copied.
 * @return              Pointer to inserted value. */
value_t *environ_insert(environ_t *env, const char *name, const value_t *value) {
    environ_entry_t *entry;

    /* Look for an existing entry with the same name. */
    list_foreach(&env->entries, iter) {
        entry = list_entry(iter, environ_entry_t, header);

        if (strcmp(entry->name, name) == 0) {
            value_destroy(&entry->value);
            value_copy(value, &entry->value);
            return &entry->value;
        }
    }

    /* Create a new entry. */
    entry = malloc(sizeof(*entry));
    list_init(&entry->header);
    entry->name = strdup(name);
    value_copy(value, &entry->value);
    list_append(&env->entries, &entry->header);

    return &entry->value;
}

/** Remove an entry from an environment.
 * @param env           Environment to remove from.
 * @param name          Name of entry to remove. */
void environ_remove(environ_t *env, const char *name) {
    list_foreach(&env->entries, iter) {
        environ_entry_t *entry = list_entry(iter, environ_entry_t, header);

        if (strcmp(entry->name, name) == 0) {
            list_remove(&entry->header);
            value_destroy(&entry->value);
            free(entry->name);
            free(entry);
            return;
        }
    }
}

/** Set the current device in an environment.
 * @param env           Environment to set in.
 * @param device        Device to set. */
void environ_set_device(environ_t *env, device_t *device) {
    value_t value;

    env->device = device;

    value.type = VALUE_TYPE_STRING;
    value.string = (char *)device->name;
    environ_insert(env, "device", &value);

    if (device->mount) {
        if (device->mount->label) {
            value.string = device->mount->label;
            environ_insert(env, "device_label", &value);
        }

        if (device->mount->uuid) {
            value.string = device->mount->uuid;
            environ_insert(env, "device_uuid", &value);
        }
    } else {
        environ_remove(env, "device_label");
        environ_remove(env, "device_uuid");
    }

    /* Change directory to the root (NULL indicates root to the FS code). */
    if (env->directory)
        fs_close(env->directory);

    env->directory = NULL;
}

/** Set the current directory in an environment.
 * @param env           Environment to set in.
 * @param handle        Handle to directory to set (must be on current device). */
void environ_set_directory(environ_t *env, fs_handle_t *handle) {
    assert(handle->type == FILE_TYPE_DIR);
    assert(handle->mount->device == env->device);

    if (env->directory)
        fs_close(env->directory);

    fs_retain(handle);
    env->directory = handle;
}

/**
 * Set the loader for an environment.
 *
 * Sets the loader for an environment. After this is called, no more commands
 * can be executed on the environment, which guarantees that the environment
 * cannot be further modified.
 *
 * @param env           Environment to set in.
 * @param ops           Loader operations.
 * @param private       Private data for the loader.
 */
void environ_set_loader(environ_t *env, loader_ops_t *ops, void *private) {
    assert(!env->loader);

    env->loader = ops;
    env->loader_private = private;
}

/** Boot the OS specified by an environment.
 * @param env           Environment to boot. */
void environ_boot(environ_t *env) {
    /* Should be checked beforehand. */
    assert(env->loader);

    current_environ = env;
    shell_enabled = false;
    env->loader->load(env->loader_private);
}

/**
 * Configuration parser.
 */

/** Read a character from the input.
 * @return              Character read. */
static int read_char(void) {
    bool in_comment = false;
    int ch;

    do {
        if (returned_char) {
            ch = returned_char;
            returned_char = 0;
        } else {
            ch = current_helper(nesting_count);
        }

        if (ch == '\n') {
            current_line++;
            current_col = 0;

            if (in_comment)
                in_comment = false;
        } else if (ch == '\t') {
            current_col += 8 - (current_col % 8);
        } else {
            if (!ignore_comments && ch == '#')
                in_comment = true;

            current_col++;
        }
    } while (in_comment);

    return ch;
}

/** Return an input character, which will be returned by the next read_char().
 * @param ch            Character to return. */
static void return_char(int ch) {
    assert(!returned_char);

    returned_char = ch;

    if (current_col > 0) {
        current_col--;
    } else if (current_line > 1) {
        current_line--;
    }
}

/** Error due to an unexpected character.
 * @param ch            Character that was unexpected. */
static void unexpected_char(int ch) {
    config_error(
        "%s:%d:%d: Unexpected %s",
        current_path, current_line, current_col, (ch == EOF) ? "end of file" : "character");
}

/** Consume a character and check that it is the expected character.
 * @param expect        Expected character.
 * @return              Whether the character was the expected character. */
static bool expect_char(int expect) {
    int ch = read_char();

    if (ch != expect) {
        unexpected_char(ch);
        return false;
    } else {
        return true;
    }
}

/** Parse an integer.
 * @param ch            Starting character.
 * @return              Integer parsed. */
static uint64_t parse_integer(int ch) {
    uint64_t result = 0, value;
    unsigned base;

    if (ch == '0') {
        ch = read_char();

        if (tolower(ch) == 'x') {
            ch = read_char();
            base = 16;
        } else {
            base = 8;
        }
    } else {
        base = 10;
    }

    while (isxdigit(ch) && (value = (isdigit(ch)) ? ch - '0' : tolower(ch) - 'a' + 10) < base) {
        result = (result * base) + value;
        ch = read_char();
    }

    /* This is not part of the digit, the caller should see this. */
    return_char(ch);
    return result;
}

/** Parse a string.
 * @return              Pointer to string on success, NULL on failure. */
static char *parse_string(void) {
    bool escaped = false;

    /* Shouldn't treat # as comment inside a string. */
    ignore_comments = true;

    while (true) {
        int ch = read_char();

        if (ch == EOF) {
            unexpected_char(ch);
            temp_buf_idx = 0;
            return NULL;
        } else if (!escaped && ch == '"') {
            ignore_comments = false;

            temp_buf[temp_buf_idx] = 0;
            temp_buf_idx = 0;
            return strdup(temp_buf);
        } else if (!escaped && ch == '\\' && !escaped) {
            escaped = true;
        } else {
            temp_buf[temp_buf_idx++] = ch;
            escaped = false;
        }
    }
}

/** Parse a variable name.
 * @return              Pointer to name string on success, NULL on failure. */
static char *parse_variable_name(void) {
    while (true) {
        int ch = read_char();

        if (!isalnum(ch) && ch != '_') {
            if (!temp_buf_idx) {
                unexpected_char(ch);
                return NULL;
            }

            return_char(ch);
            temp_buf[temp_buf_idx] = 0;
            temp_buf_idx = 0;
            return strdup(temp_buf);
        } else {
            temp_buf[temp_buf_idx++] = ch;
        }
    }
}

/** Parse an value list.
 * @param command       Whether this value list is a command's argument list.
 * @return              Pointer to list on success, NULL on failure. */
static value_list_t *parse_value_list(bool command) {
    value_list_t *list;
    bool escaped, need_space;

    list = malloc(sizeof(*list));
    list->values = NULL;
    list->count = 0;

    escaped = false;
    need_space = false;
    while (true) {
        int ch;
        value_t *value;

        ch = read_char();

        if (!escaped) {
            if (ch == '\\') {
                escaped = true;
                continue;
            } else if ((ch == '\n' || ch == EOF) && command) {
                return list;
            }
        }

        escaped = false;

        if (command && ch == '}') {
            return_char(ch);
            return list;
        } else if (!command && ch == ']') {
            return list;
        } else if (isspace(ch)) {
            need_space = false;
            continue;
        } else if (need_space) {
            unexpected_char(ch);
            break;
        }

        /* Start of a new value. */
        list->values = realloc(list->values, sizeof(*list->values) * (list->count + 1));
        value = &list->values[list->count];

        if (isdigit(ch)) {
            value->type = VALUE_TYPE_INTEGER;
            value->integer = parse_integer(ch);
        } else if (ch == 't') {
            value->type = VALUE_TYPE_BOOLEAN;
            value->boolean = true;

            if (!expect_char('r') || !expect_char('u') || !expect_char('e'))
                break;
        } else if (ch == 'f') {
            value->type = VALUE_TYPE_BOOLEAN;
            value->boolean = false;

            if (!expect_char('a') || !expect_char('l') || !expect_char('s') || !expect_char('e'))
                break;
        } else if (ch == '"') {
            value->type = VALUE_TYPE_STRING;

            nesting_count++;
            value->string = parse_string();
            nesting_count--;

            if (!value->string)
                break;
        } else if (ch == '[') {
            value->type = VALUE_TYPE_LIST;

            nesting_count++;
            value->list = parse_value_list(false);
            nesting_count--;

            if (!value->list)
                break;
        } else if (ch == '{') {
            value->type = VALUE_TYPE_COMMAND_LIST;

            nesting_count++;
            value->cmds = parse_command_list();
            nesting_count--;

            if (!value->cmds)
                break;
        } else if (ch == '$') {
            value->type = VALUE_TYPE_REFERENCE;
            value->string = parse_variable_name();

            if (!value->string)
                break;
        } else {
            unexpected_char(ch);
            break;
        }

        /* Increment count only upon success, so value_list_destroy() does not
         * attempt to clean up the failed entry. */
        list->count++;

        /* At least one space is required after each value. */
        need_space = true;
    }

    value_list_destroy(list);
    return NULL;
}

/** Parse a command list.
 * @return              Pointer to list on success, NULL on failure. */
static command_list_t *parse_command_list(void) {
    command_list_t *list;
    int endch;

    list = malloc(sizeof(*list));
    list_init(list);

    endch = (nesting_count) ? '}' : EOF;
    while (true) {
        int ch = read_char();

        if (ch == endch || isspace(ch)) {
            command_list_entry_t *command;

            if (temp_buf_idx == 0) {
                if (ch == endch) {
                    return list;
                } else {
                    continue;
                }
            }

            temp_buf[temp_buf_idx] = 0;
            temp_buf_idx = 0;

            /* End of command name, push it onto the list. */
            command = malloc(sizeof(*command));
            list_init(&command->header);
            command->name = strdup(temp_buf);
            list_append(list, &command->header);

            return_char(ch);

            /* Do not increase the nest count here as we are not expecting a ']'. */
            command->args = parse_value_list(true);
            if (!command->args)
                break;
        } else if (ch == EOF) {
            unexpected_char(ch);
            break;
        } else {
            temp_buf[temp_buf_idx++] = ch;
            continue;
        }
    }

    command_list_destroy(list);
    return NULL;
}

/** Parse configuration data.
 * @param path          Path of the file (used in error output).
 * @param helper        Helper to read from the file.
 * @return              Pointer to parsed command list on success, NULL on failure. */
command_list_t *config_parse(const char *path, config_read_helper_t helper) {
    command_list_t *list;

    current_helper = helper;
    current_path = path;
    current_line = 1;
    current_col = 0;
    nesting_count = 0;
    returned_char = 0;
    ignore_comments = false;

    list = parse_command_list();
    assert(!nesting_count);
    return list;
}

/**
 * Core commands.
 */

/** Sort comparison function for the command list. */
static int command_sort_compare(const void *a, const void *b) {
    const command_t *first = a;
    const command_t *second = b;

    return strcmp(first->name, second->name);
}

/** List available commands.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_help(value_list_t *args) {
    command_t *commands __cleanup_free = NULL;
    size_t count = 0;

    if (args->count != 0) {
        config_error("Invalid arguments");
        return false;
    }

    /* The builtin command list is not sorted. Build a copy and sort it. */
    builtin_foreach(BUILTIN_TYPE_COMMAND, command_t, command) {
        /* NULL description means it should be hidden from help. */
        if (command->description) {
            commands = realloc(commands, sizeof(*commands) * (count + 1));
            commands[count].name = command->name;
            commands[count].description = command->description;
            count++;
        }
    }

    qsort(commands, count, sizeof(*commands), command_sort_compare);

    printf("Command       Description\n");
    printf("-------       -----------\n");

    for (size_t i = 0; i < count; i++)
        printf("%-12s  %s\n", commands[i].name, commands[i].description);

    return true;
}

BUILTIN_COMMAND("help", "List available commands", config_cmd_help);

/** Display the KBoot version.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_version(value_list_t *args) {
    if (args->count != 0) {
        config_error("Invalid arguments");
        return false;
    }

    printf("KBoot version %s\n", kboot_loader_version);
    return true;
}

BUILTIN_COMMAND("version", "Display the KBoot version", config_cmd_version);

/** Print a list of environment variables.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_env(value_list_t *args) {
    if (args->count != 0) {
        config_error("Invalid arguments");
        return false;
    }

    list_foreach(&current_environ->entries, iter) {
        environ_entry_t *entry = list_entry(iter, environ_entry_t, header);
        const char *type = NULL;

        switch (entry->value.type) {
        case VALUE_TYPE_INTEGER:
            type = "integer";
            break;
        case VALUE_TYPE_BOOLEAN:
            type = "boolean";
            break;
        case VALUE_TYPE_STRING:
            type = "string";
            break;
        case VALUE_TYPE_LIST:
            type = "list";
            break;
        case VALUE_TYPE_COMMAND_LIST:
            type = "command list";
            break;
        case VALUE_TYPE_REFERENCE:
            type = "reference";
            break;
        }

        switch (entry->value.type) {
        case VALUE_TYPE_INTEGER:
            snprintf(temp_buf, TEMP_BUF_LEN, "%llu", entry->value.integer);
            break;
        case VALUE_TYPE_BOOLEAN:
            snprintf(temp_buf, TEMP_BUF_LEN, (entry->value.boolean) ? "true" : "false");
            break;
        case VALUE_TYPE_STRING:
            snprintf(temp_buf, TEMP_BUF_LEN, "\"%s\"", entry->value.string);
            break;
        default:
            /* TODO: other types. */
            temp_buf[0] = 0;
            break;
        }

        printf("%s = (%s) %s\n", entry->name, type, temp_buf);
    }

    return true;
}

BUILTIN_COMMAND("env", "List environment variables", config_cmd_env);

/** Check if a variable name is valid.
 * @param name          Name of environment variable.
 * @return              Whether the name was valid. */
static bool variable_name_valid(const char *name) {
    for (size_t i = 0; name[i]; i++) {
        if (!isalnum(name[i]) && name[i] != '_') {
            config_error("Invalid variable name '%s'", name);
            return false;
        }
    }

    /* Check that the name is not reserved. */
    for (size_t i = 0; i < array_size(reserved_environ_names); i++) {
        if (strcmp(name, reserved_environ_names[i]) == 0) {
            config_error("Variable name '%s' is reserved", name);
            return false;
        }
    }

    return true;
}

/** Set a variable in the environment.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_set(value_list_t *args) {
    const char *name;

    if (args->count != 2 || args->values[0].type != VALUE_TYPE_STRING) {
        config_error("Invalid arguments");
        return false;
    }

    /* Check whether the name is valid. */
    name = args->values[0].string;
    if (!variable_name_valid(name))
        return false;

    environ_insert(current_environ, name, &args->values[1]);
    return true;
}

BUILTIN_COMMAND("set", "Set an environment variable", config_cmd_set);

/** Unset a variable in the environment.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_unset(value_list_t *args) {
    const char *name;

    if (args->count != 1 || args->values[0].type != VALUE_TYPE_STRING) {
        config_error("Invalid arguments");
        return false;
    }

    /* Check whether the name is valid. */
    name = args->values[0].string;
    if (!variable_name_valid(name))
        return false;

    environ_remove(current_environ, name);
    return true;
}

BUILTIN_COMMAND("unset", "Unset an environment variable", config_cmd_unset);

/** Reboot the system.
 * @param private       Unused. */
static __noreturn void reboot_loader_load(void *private) {
    target_reboot();
}

/** Reboot loader operations. */
static loader_ops_t reboot_loader_ops = {
    .load = reboot_loader_load,
};

/** Reboot the system.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_reboot(value_list_t *args) {
    if (args->count != 0) {
        config_error("Invalid arguments");
        return false;
    }

    environ_set_loader(current_environ, &reboot_loader_ops, NULL);
    return true;
}

BUILTIN_COMMAND("reboot", "Reboot the system", config_cmd_reboot);

/** Exit the loader.
 * @param private       Unused. */
static __noreturn void exit_loader_load(void *private) {
    target_exit();
}

/** Exit loader operations. */
static loader_ops_t exit_loader_ops = {
    .load = exit_loader_load,
};

/** Exit the loader and return to firmware.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_exit(value_list_t *args) {
    if (args->count != 0) {
        config_error("Invalid arguments");
        return false;
    }

    environ_set_loader(current_environ, &exit_loader_ops, NULL);
    return true;
}

BUILTIN_COMMAND("exit", "Exit the loader and return to firmware", config_cmd_exit);

/**
 * Configuration loading.
 */

/** File input helper config_parse().
 * @param nest          Nesting count (unused).
 * @return              Character read, or EOF on end of file. */
static int file_read_helper(unsigned nest) {
    while (true) {
        if (current_file_offset < current_file_size) {
            char ch = current_file[current_file_offset++];

            /* Deal with CRLF line endings by just ignoring the CR. */
            if (ch == '\r')
                continue;

            return ch;
        } else {
            return EOF;
        }
    }
}

/** Parse a configuration file.
 * @param handle        Handle to file to parse.
 * @param path          Path of the file.
 * @return              Parsed command list, or NULL on failure. */
static command_list_t *parse_config_file(fs_handle_t *handle, const char *path) {
    command_list_t *list;
    status_t ret;

    dprintf("config: reading configuration file '%s'\n", path);

    current_file = malloc(handle->size + 1);

    ret = fs_read(handle, current_file, handle->size, 0);
    if (ret != STATUS_SUCCESS) {
        config_error("Error reading '%s': %pS", path, ret);
        return NULL;
    }

    current_file[handle->size] = 0;

    /* Use strlen() to set this in case there is a null byte in the middle of
     * the file somewhere. */
    current_file_size = strlen(current_file);
    current_file_offset = 0;

    list = config_parse(path, file_read_helper);

    free(current_file);
    return list;
}

/**
 * Attempt to load a configuration file.
 *
 * Attempts to replace load a new configuration file and execute its contents,
 * i.e. display the menu and/or load an OS. This function will return if the
 * specified file did not exist, or a config_error() occurred that did not
 * result in boot_error() being called.
 *
 * @param path          Path of the file.
 * @param must_exist    Whether the file must exist.
 */
static void load_config_file(const char *path, bool must_exist) {
    command_list_t *list;
    fs_handle_t *handle;
    status_t ret;
    char *dir __cleanup_free = NULL;
    environ_t *env, *target;
    bool ok;

    ret = fs_open(path, NULL, FILE_TYPE_REGULAR, 0, &handle);
    if (ret != STATUS_SUCCESS) {
        if (must_exist || ret != STATUS_NOT_FOUND)
            config_error("Error opening '%s': %pS", path, ret);

        return;
    }

    list = parse_config_file(handle, path);
    if (!list) {
        fs_close(handle);
        return;
    }

    env = environ_create(root_environ);

    /* Set the device in the environment to the one containing the config. */
    environ_set_device(env, handle->mount->device);

    fs_close(handle);

    /* Set the directory. Note this may fail on certain filesystems, e.g. PXE. */
    dir = dirname(path);
    ret = fs_open(dir, NULL, FILE_TYPE_DIR, 0, &handle);
    if (ret == STATUS_SUCCESS) {
        environ_set_directory(env, handle);
        fs_close(handle);
    }

    ok = command_list_exec(list, env);
    command_list_destroy(list);
    if (ok) {
        /* Select an environment to boot. */
        target = menu_select(env);

        /* And finally boot the OS. */
        if (target->loader) {
            environ_boot(target);
        } else {
            environ_destroy(env);
            boot_error("No operating system to boot");
        }
    } else {
        environ_destroy(env);
    }
}

/** Replace the current configuration with a new one.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_config(value_list_t *args) {
    if (args->count != 1 || args->values[0].type != VALUE_TYPE_STRING) {
        config_error("Invalid arguments");
        return false;
    }

    /* If this returns, an error occurred. */
    load_config_file(args->values[0].string, true);
    return false;
}

BUILTIN_COMMAND("config", "Replace the current configuration with a new one", config_cmd_config);

/** Include a single configuration file.
 * @param handle        Handle to file.
 * @param path          Path to file.
 * @return              Whether successful. */
static bool include_config_file(fs_handle_t *handle, const char *path) {
    command_list_t *list;
    bool ok;

    list = parse_config_file(handle, path);
    if (!list)
        return false;

    ok = command_list_exec(list, current_environ);
    command_list_destroy(list);
    return ok;
}

/** Directory iteration state for config_cmd_include(). */
typedef struct include_state {
    char **entries;                 /**< Array of entry names. */
    size_t count;                   /**< Number of entries. */
} include_state_t;

/** Clean up the include state structure. */
static void include_state_cleanup(include_state_t *state) {
    for (size_t i = 0; i < state->count; i++)
        free(state->entries[i]);

    free(state->entries);
}

/** Iteration callback for config_cmd_include(). */
static bool include_iterate_cb(const fs_entry_t *entry, void *_state) {
    include_state_t *state = _state;

    state->entries = realloc(state->entries, (state->count + 1) * sizeof(state->entries[0]));
    state->entries[state->count] = strdup(entry->name);
    state->count++;

    return true;
}

/** Sort comparison function for the include entries list. */
static int include_sort_compare(const void *a, const void *b) {
    const char *const *first = a;
    const char *const *second = b;

    return strcmp(*first, *second);
}

/** Include another configuration file into the current one.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_include(value_list_t *args) {
    fs_handle_t *handle __cleanup_close = NULL;
    status_t ret;

    if (args->count != 1 || args->values[0].type != VALUE_TYPE_STRING) {
        config_error("Invalid arguments");
        return false;
    }

    ret = fs_open(args->values[0].string, NULL, FILE_TYPE_NONE, 0, &handle);
    if (ret != STATUS_SUCCESS) {
        config_error("Error opening '%s': %pS", args->values[0].string, ret);
        return false;
    }

    if (handle->type == FILE_TYPE_DIR) {
        include_state_t state __cleanup(include_state_cleanup) = { NULL, 0 };

        /* We're including a directory of config files. We want the order in
         * which we include files to be alphabetically sorted (so there is a
         * guaranteed order in which files are included), however the FS does
         * not guarantee sorting. Therefore we must sort manually here. We get
         * a list of all entry names, sort it, then open them manually. */
        ret = fs_iterate(handle, include_iterate_cb, &state);
        if (ret != STATUS_SUCCESS) {
            config_error("Error iterating '%s': %pS", args->values[0].string, ret);
            return false;
        }

        qsort(state.entries, state.count, sizeof(state.entries[0]), include_sort_compare);

        for (size_t i = 0; i < state.count; i++) {
            char *path __cleanup_free;
            fs_handle_t *child __cleanup_close = NULL;

            path = malloc(strlen(args->values[0].string) + strlen(state.entries[i]) + 2);
            sprintf(path, "%s/%s", args->values[0].string, state.entries[i]);

            ret = fs_open(path, NULL, FILE_TYPE_REGULAR, 0, &child);
            if (ret != STATUS_SUCCESS) {
                if (ret != STATUS_NOT_FILE) {
                    config_error("Error opening '%s': %pS", path, ret);
                    return false;
                } else {
                    continue;
                }
            }

            if (!include_config_file(child, path))
                return false;
        }

        return true;
    } else {
        return include_config_file(handle, args->values[0].string);
    }
}

BUILTIN_COMMAND("include", "Include another configuration file into the current one", config_cmd_include);

/**
 * Initialization functions.
 */

/** Set up the configuration system. */
void config_init(void) {
    /* Create the root environment. */
    root_environ = environ_create(NULL);
    current_environ = root_environ;

    /* We can now use the shell. */
    shell_enabled = true;
}

/** Load the configuration. */
void config_load(void) {
    if (config_file_override) {
        load_config_file(config_file_override, false);
    } else {
        /* Try the boot directory. */
        if (boot_directory)
            load_config_file("kboot.cfg", false);

        /* Try the various default paths. */
        for (size_t i = 0; i < array_size(config_file_paths); i++)
            load_config_file(config_file_paths[i], false);
    }

    boot_error("Could not find configuration file");
}
