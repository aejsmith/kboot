Target Support
==============

KBoot supports two modes of operation:

 * Primary boot loader with full user interface and disk device support.
 * Second-stage boot loader where its purpose is only to be able to load a
   kernel image, supplied from a primary boot loader, using the KBoot boot
   protocol.

Which modes of operation are supported depends on the target platform. Use as a
primary boot loader is generally only supported on platforms where there is
some firmware that can be relied upon to provide input, video and disk access.
This is because providing individual drivers for a wide range of hardware is
outside of the intended scope of KBoot.

The following platforms are supported for use as a primary boot loader:

 * BIOS- and UEFI-based x86 PCs

The following platforms are supported for use as a second-stage boot loader:

 * BIOS-based x86 PCs
 * QEMU ARM64 virt machine
 * Raspberry Pi 3 & 4 (ARM64)

Second-Stage Usage (BIOS PC)
----------------------------

Usage as a second-stage boot loader on BIOS-based PCs is supported via
[Multiboot](multiboot.md). See that page for full usage details.

Second-Stage Usage (ARM64)
--------------------------

All supported ARM64 platforms make use of a Device Tree to describe the
hardware to the boot loader and OS.

The generic `dt-arm64` target builds a single loader binary that works on all
supported ARM64 platforms.

Additionally, each supported platform has a build target specifically for that
platform. The single-platform targets may be used where support for only a
single platform is needed, which can reduce the size of the loader binary by
removing unnecessary hardware support code. They also allow for better
debugging of early loader initialization since they can use hardcoded serial
port details, rather than having to wait until the device tree is parsed to
discover serial ports.

On ARM64, the loader is built as a Linux kernel-compatible binary. This means
that it can be loaded by any firmware or primary boot loader that is capable of
loading a Linux kernel.

The kernel to boot is supplied to KBoot via a initrd from the primary boot
loader. The initrd is treated as a disk image, in the same way as the
`diskimage` configuration command (this also supports TAR archives). The
loader expects to find a configuration file in the image as it would do when
booted from a hard drive, and this file should load the kernel from the image.

### QEMU

KBoot can be run on QEMU's `virt` machine type, using the `-kernel` and
`-initrd` options. For example:

    $ qemu-system-aarch64 -M virt -serial stdio -kernel kboot.bin -initrd initrd.tar

### Raspberry Pi 3 & 4

KBoot can be loaded directly by the Raspberry Pi firmware, since it is a Linux
kernel-compatible binary.

Example config.txt entries to load KBoot and an initrd, with UART output:

    enable_uart=1
    arm_64bit=1
    kernel=kboot.bin
    initramfs initrd.tar followkernel
