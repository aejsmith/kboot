/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               BCM283x VideoCore firmware interface.
 */

#include <drivers/platform/bcm283x/firmware.h>

#include <drivers/video/bcm283x.h>

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

static status_t bcm283x_firmware_init(dt_device_t *device) {
    uint32_t mbox_handle;
    if (!dt_get_prop_u32(device->node_offset, "mboxes", &mbox_handle))
        return STATUS_INVALID_ARG;

    bcm283x_mbox_t *mbox = bcm283x_mbox_get(mbox_handle);
    if (!mbox)
        return STATUS_INVALID_ARG;

    device->private = mbox;

    #ifdef CONFIG_DRIVER_VIDEO_BCM283X
        bcm283x_video_init(mbox);
    #endif

    return STATUS_SUCCESS;
}

static const char *bcm283x_firmware_match[] = {
    "raspberrypi,bcm2835-firmware",
};

BUILTIN_DT_DRIVER(bcm283x_firmware_driver) = {
    .matches = DT_MATCH_TABLE(bcm283x_firmware_match),
    .init = bcm283x_firmware_init,
};

/** Get the firmware mailbox from a DT phandle.
 * @param phandle       Handle to the firmware DT node.
 * @return              Pointer to mailbox or NULL if not available. */
bcm283x_mbox_t *bcm283x_firmware_get(uint32_t phandle) {
    dt_device_t *device = dt_device_get_by_phandle(phandle, &bcm283x_firmware_driver);
    return (device) ? (bcm283x_mbox_t *)device->private : NULL;
}
