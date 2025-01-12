/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               BCM283x VideoCore mailbox driver.
 */

#ifndef __DRIVERS_PLATFORM_BCM283X_MBOX_H
#define __DRIVERS_PLATFORM_BCM283X_MBOX_H

#include <types.h>

typedef struct bcm283x_mbox bcm283x_mbox_t;

extern uint32_t bcm283x_mbox_read(bcm283x_mbox_t *mbox, uint8_t channel);
extern void bcm283x_mbox_write(bcm283x_mbox_t *mbox, uint8_t channel, uint32_t data);

extern bcm283x_mbox_t *bcm283x_mbox_get(uint32_t phandle);

#endif /* __DRIVERS_PLATFORM_BCM283X_MBOX_H */
