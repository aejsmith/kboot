/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               EFI network device support.
 */

#ifndef __EFI_NET_H
#define __EFI_NET_H

#include <net.h>

extern bool efi_net_is_net_device(efi_handle_t handle);
extern efi_handle_t efi_net_get_handle(net_device_t *net);

extern void efi_net_init(void);

#endif /* __EFI_NET_H */
