KBoot
=====

KBoot is a general-purpose boot loader, which supports both BIOS- and
UEFI-based PCs.

A short feature overview:

 * Text and GUI menu interfaces
 * Interactive shell
 * Serial console support
 * Ext2/3/4, FAT and ISO9660 filesystem support
 * MBR and GPT partition support
 * PXE network booting
 * Linux and
   [Multiboot](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)
   OS loaders
 * BIOS and EFI chain loaders (for booting OSes not natively supported, e.g.
   Windows)
 * [Custom boot protocol](documentation/kboot-protocol.md) targeting hobby OS
   projects

KBoot also supports use as a second-stage boot loader on additional platforms,
where its purpose is only to be able to load a kernel which uses the KBoot boot
protocol. The platforms which support this are:

 * QEMU ARM64 virt machine
 * Raspberry Pi 3 & 4 (ARM64)

Current limitations:

 * No FAT boot sector - cannot boot directly from a FAT filesystem on legacy
   BIOS systems.
 * Floppy drives are not supported on BIOS systems.
 * UEFI installation to disk on Macs is not supported (legacy BIOS, i.e. Boot
   Camp, is OK).

Screenshots
-----------

![Text Menu](documentation/guide/images/menu.png)
![GUI Menu](documentation/guide/images/gui.png)

Documentation
-------------

User documentation is available [here](documentation/guide).

Reporting Issues
----------------

Issues can be reported via the GitHub
[issue tracker](https://github.com/aejsmith/kboot/issues).

License
-------

KBoot is licensed under the terms of the [ISC license](documentation/license.md).

Credits
-------

KBoot is primarily authored by [Alex Smith](https://github.com/aejsmith).
Other major contributors include:

 * [froggey](https://github.com/froggey) - parts of ARM64 support, bug fixes.
