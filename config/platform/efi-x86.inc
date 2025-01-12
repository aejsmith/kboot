#
# SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
# SPDX-License-Identifier: ISC
#

include = {
    'includes': ['platform/efi'],
    'config': {
        'EFI_ARCH': 'x86',
        'EFI_ARCH_X86': True,
        'DRIVER_SERIAL_NS16550': True,
        'TARGET_NS16550_IO': True,
    },
}
