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
 * @brief               Device management.
 */

#include <lib/string.h>

#include <config.h>
#include <device.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>

/** List of all registered devices. */
static LIST_DECLARE(device_list);

/** Boot device. */
device_t *boot_device;

/** Read from a device.
 * @param device        Device to read from.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @param offset        Offset in the device to read from.
 * @return              Status code describing the result of the read. */
status_t device_read(device_t *device, void *buf, size_t count, offset_t offset) {
    if (!device->ops || !device->ops->read)
        return STATUS_NOT_SUPPORTED;

    if (!count)
        return STATUS_SUCCESS;

    return device->ops->read(device, buf, count, offset);
}

/**
 * Look up a device.
 *
 * Looks up a device. If given a string in the format "uuid:<uuid>", the device
 * will be looked up by filesystem UUID. If given a string in the format
 * "label:<label>", will be looked up by filesystem label. Otherwise, will be
 * looked up by the device name.
 *
 * @param name          String to look up.
 *
 * @return              Matching device, or NULL if no matches found.
 */
device_t *device_lookup(const char *name) {
    bool uuid = false, label = false;

    if (strncmp(name, "uuid:", 5) == 0) {
        uuid = true;
        name += 5;
    } else if (strncmp(name, "label:", 6) == 0) {
        label = true;
        name += 6;
    }

    if (!name[0])
        return NULL;

    list_foreach(&device_list, iter) {
        device_t *device = list_entry(iter, device_t, header);

        if (uuid || label) {
            if (!device->mount)
                continue;

            if (strcmp((uuid) ? device->mount->uuid : device->mount->label, name) == 0)
                return device;
        } else {
            if (strcmp(device->name, name) == 0)
                return device;
        }
    }

    return NULL;
}

/** Register a device.
 * @param device        Device to register (details should be filled in).
 * @param name          Name to give the device (string will be duplicated). */
void device_register(device_t *device, const char *name) {
    if (device_lookup(name))
        internal_error("Device named '%s' already exists", name);

    device->name = strdup(name);

    list_init(&device->header);
    list_append(&device_list, &device->header);

    /* Probe for filesystems. */
    if (!device->mount)
        device->mount = fs_probe(device);
}

/** Set the device in an environment.
 * @param env           Environment to set in.
 * @param device        Device to set. */
static void set_environ_device(environ_t *env, device_t *device) {
    value_t value;

    env->device = device;

    value.type = VALUE_TYPE_STRING;
    value.string = device->name;
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

/** Set the current device.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_device(value_list_t *args) {
    device_t *device;

    if (args->count != 1 || args->values[0].type != VALUE_TYPE_STRING) {
        config_error("Invalid arguments");
        return false;
    }

    device = device_lookup(args->values[0].string);
    if (!device) {
        config_error("Device '%s' not found", args->values[0].string);
        return false;
    }

    set_environ_device(current_environ, device);
    return true;
}

BUILTIN_COMMAND("device", "Set the current device", config_cmd_device);

/** Print a list of devices.
 * @param console       Console to write to.
 * @param indent        Indentation level. */
static void print_device_list(console_t *console, size_t indent) {
    list_foreach(&device_list, iter) {
        device_t *device = list_entry(iter, device_t, header);
        size_t child = 0;
        char buf[128];

        /* Figure out how much to indent the string (so we get a tree-like view
         * with child devices indented). */
        for (size_t i = 0; device->name[i]; i++) {
            if (device->name[i] == ',')
                child++;
        }

        snprintf(buf, sizeof(buf), "Unknown");
        if (device->ops->identify)
            device->ops->identify(device, DEVICE_IDENTIFY_SHORT, buf, sizeof(buf));

        console_printf(console, "%-*s%-*s -> %s\n", indent + child, "", 7 - child, device->name, buf);
    }
}

/** Print a list of devices.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_lsdev(value_list_t *args) {
    if (args->count == 0) {
        print_device_list(config_console, 0);
        return true;
    } else if (args->count == 1 && args->values[0].type == VALUE_TYPE_STRING) {
        device_t *device;
        char buf[256];

        device = device_lookup(args->values[0].string);
        if (!device) {
            config_error("Device '%s' not found", args->values[0].string);
            return false;
        }

        config_printf("name       = %s\n", device->name);

        snprintf(buf, sizeof(buf), "Unknown");
        if (device->ops->identify)
            device->ops->identify(device, DEVICE_IDENTIFY_SHORT, buf, sizeof(buf));

        config_printf("identity   = %s\n", buf);

        buf[0] = 0;
        if (device->ops->identify) {
            device->ops->identify(device, DEVICE_IDENTIFY_LONG, buf, sizeof(buf));
            config_printf("%s", buf);
        }

        if (device->mount) {
            config_printf("fs         = %s\n", device->mount->ops->name);
            if (device->mount->uuid)
                config_printf("uuid       = %s\n", device->mount->uuid);
            if (device->mount->label)
                config_printf("label      = \"%s\"\n", device->mount->label);
        }

        return true;
    } else {
        config_error("Invalid arguments");
        return false;
    }
}

BUILTIN_COMMAND("lsdev", "List available devices", config_cmd_lsdev);

/** Initialize the device manager. */
void device_init(void) {
    target_device_probe();

    /* Print out a list of all devices. */
    dprintf("device: detected devices:\n");
    print_device_list(&debug_console, 1);

    /* Set the device in the environment. */
    if (boot_device) {
        dprintf("device: boot device is %s\n", (boot_device) ? boot_device->name : "unknown");
        set_environ_device(root_environ, boot_device);
    }

    if (!boot_device || !boot_device->mount)
        boot_error("Unable to find boot filesystem");
}
