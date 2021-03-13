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
 * @brief               BCM283x VideoCore firmware interface.
 */

#include <drivers/platform/bcm283x/firmware.h>

#include <assert.h>
#include <dt.h>
#include <loader.h>

/** Mailbox channel definitions.*/
#define MBOX_CHANNEL_PROP       8   /**< Property tags (ARM to VC). */

/** Make a request to the firmware property interface.
 * @param mbox          Firmware mailbox.
 * @param buffer        Request data buffer (overwritten with response).
 * @return              Whether the request succeeded. */
bool bcm283x_firmware_request(bcm283x_mbox_t *mbox, void *buffer) {
    phys_ptr_t addr = virt_to_phys((ptr_t)buffer);
    assert(!(addr & ~0xfffffff0ull));

    bcm283x_mbox_write(mbox, MBOX_CHANNEL_PROP, (uint32_t)addr);
    uint32_t val = bcm283x_mbox_read(mbox, MBOX_CHANNEL_PROP);

    if (val != (uint32_t)addr) {
        dprintf(
            "firmware: request returned mismatching buffer address 0x%" PRIx32 ", should be 0x%" PRIx32 "\n",
            val, (uint32_t)addr);
        return false;
    }

    bcm283x_firmware_message_header_t *header = (bcm283x_firmware_message_header_t *)buffer;

    if (header->code != BCM283X_FIRMWARE_STATUS_SUCCESS) {
        dprintf("bcm283x: firmware: request failed, status: 0x%" PRIx32 "\n", header->code);
        return false;
    }

    return true;
}

/** Get the firmware mailbox from a DT phandle.
 * @param phandle       Handle to the firmware DT node.
 * @return              Pointer to mailbox or NULL if not available. */
bcm283x_mbox_t *bcm283x_firmware_get(uint32_t phandle) {
    int firmware_offset = fdt_node_offset_by_phandle(fdt_address, phandle);
    if (firmware_offset < 0)
        return NULL;

    uint32_t mbox_handle;
    if (!dt_get_prop_u32(firmware_offset, "mboxes", &mbox_handle))
        return NULL;

    return bcm283x_mbox_get(mbox_handle);
}
