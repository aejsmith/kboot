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
 * @brief               BCM283x VideoCore mailbox driver.
 *
 * The mailbox is used for communication with the firmware running on the
 * VideoCore. Note that this is NOT the "ARM Mailbox" described in the "BCM2*
 * Peripherals" documents.
 * 
 * Mailbox 0 is used for VideoCore to ARM communication, mailbox 1 is used for
 * ARM to VideoCore. ARM should never write mailbox 0 or read mailbox 1.
 *
 * Reference:
 *  - https://github.com/raspberrypi/firmware/wiki/Mailboxes
 *  - https://github.com/raspberrypi/firmware/wiki/Accessing-mailboxes
 *  - https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
 */

#include <arch/io.h>

#include <drivers/platform/bcm283x/mbox.h>

#include <assert.h>
#include <dt.h>
#include <loader.h>
#include <memory.h>
#include <video.h>

struct bcm283x_mbox {
    uint32_t *regs;
};

/** Mailbox register definitions. */
#define BCM283X_MBOX_REG_RW0        0       /**< Mailbox 0 read/write. */
#define BCM283X_MBOX_REG_STATUS0    6       /**< Mailbox 0 status. */
#define BCM283X_MBOX_REG_RW1        8       /**< Mailbox 1 read/write. */
#define BCM283X_MBOX_REG_STATUS1    14      /**< Mailbox 1 status. */

/** Mailbox status register bits. */
#define BCM283X_MBOX_STATUS_FULL    (1<<31) /**< Mailbox Full. */
#define BCM283X_MBOX_STATUS_EMPTY   (1<<30) /**< Mailbox Empty. */

/** Read a value from the mailbox.
 * @param mbox          Mailbox to read.
 * @param channel       Mailbox channel to read from.
 * @return              Value read from the mailbox. */
uint32_t bcm283x_mbox_read(bcm283x_mbox_t *mbox, uint8_t channel) {
    assert(!(channel & 0xf0));

    while (true) {
        /* Wait for data. */
        while (true) {
            if (!(read32(&mbox->regs[BCM283X_MBOX_REG_STATUS0]) & BCM283X_MBOX_STATUS_EMPTY))
                break;
        }

        uint32_t value = read32(&mbox->regs[BCM283X_MBOX_REG_RW0]);

        if ((value & 0xf) != channel)
            continue;

        return (value & 0xfffffff0);
    }
}

/** Write a value to the mailbox.
 * @param mbox          Mailbox to write.
 * @param channel       Mailbox channel to write to.
 * @param data          Data to write. */
void bcm283x_mbox_write(bcm283x_mbox_t *mbox, uint8_t channel, uint32_t data) {
    assert(!(channel & 0xf0));
    assert(!(data & 0xf));

    /* Drain any pending responses. */
    while (true) {
        if (read32(&mbox->regs[BCM283X_MBOX_REG_STATUS0]) & BCM283X_MBOX_STATUS_EMPTY)
            break;

        read32(&mbox->regs[BCM283X_MBOX_REG_RW0]);
    }

    /* Wait for space. */
    while (true) {
        if (!(read32(&mbox->regs[BCM283X_MBOX_REG_STATUS1]) & BCM283X_MBOX_STATUS_FULL))
            break;
    }

    write32(&mbox->regs[BCM283X_MBOX_REG_RW1], data | (uint32_t)channel);
}

static status_t bcm283x_mbox_init(dt_device_t *device) {
    phys_ptr_t base;
    phys_ptr_t size;
    if (!dt_get_reg(device->node_offset, 0, &base, &size))
        return STATUS_INVALID_ARG;

    bcm283x_mbox_t *mbox = malloc(sizeof(*mbox));
    mbox->regs = (uint32_t *)phys_to_virt(base);

    dprintf("bcm283x: mailbox: initialized at 0x%" PRIxPHYS "\n", base);

    device->private = mbox;
    return STATUS_SUCCESS;
}

static const char *bcm283x_mbox_match[] = {
    "brcm,bcm2835-mbox",
};

BUILTIN_DT_DRIVER(bcm283x_mbox_driver) = {
    .matches = DT_MATCH_TABLE(bcm283x_mbox_match),
    .init = bcm283x_mbox_init,
};

/** Get the mailbox from a phandle.
 * @param phandle       Handle to mailbox.
 * @return              Pointer to mailbox or NULL if invalid reference. */
bcm283x_mbox_t *bcm283x_mbox_get(uint32_t phandle) {
    dt_device_t *device = dt_device_get_by_phandle(phandle, &bcm283x_mbox_driver);
    return (device) ? (bcm283x_mbox_t *)device->private : NULL;
}
