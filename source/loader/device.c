/*
 * Copyright (C) 2014 Alex Smith
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

#include <device.h>
#include <loader.h>
#include <memory.h>

/** List of all registered devices. */
static LIST_DECLARE(device_list);

/** Read from a device.
 * @param device        Device to read from.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @param offset        Offset in the device to read from.
 * @return              Status code describing the result of the read. */
status_t device_read(device_t *device, void *buf, size_t count, offset_t offset) {
    if (!device->ops || !device->ops->read)
        return STATUS_NOT_SUPPORTED;

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
    list_foreach(&device_list, iter) {
        device_t *device = list_entry(iter, device_t, header);

        // TODO: UUID/labels
        if (strcmp(device->name, name) == 0)
            return device;
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
}
