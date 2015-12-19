Installation
============

Once KBoot has been built, you need to install it onto a device or image to
make it bootable. This section covers the possible methods for doing this.
After you have installed using the appropriate method for your system, you must
proceed to create a [configuration file](configuration.md).

Warning
-------

While every effort has been made to test KBoot on systems available to the
author, it has not received very wide testing, and therefore you may run into
problems on your system.

Therefore, it is **highly** recommended that if you are installing onto a
production system, you keep around an alternative boot method or e.g. a live CD
that can be used to recover your system, in case you encounter any problems.

Installing to a Device
----------------------

**Note**: Installation to a device is only supported on Linux at present.

KBoot can be installed to a device (hard drive, USB stick, etc.) using the
`kboot-install` utility. The procedure for this differs depending on whether
you wish to boot via legacy BIOS or by UEFI.

### BIOS

On BIOS systems, a boot sector must be installed to the filesystem that you
wish to make bootable, and a copy of the boot loader binary must be placed on
it.

When installing to a device with a partition table (rather than just a
filesystem covering the whole device), KBoot's boot sector is intended to be
installed to the partition containing the boot loader. Therefore, if you wish
KBoot to be the primary boot loader on the device, you must ensure that the
installation partition is marked as active in the MBR, and that the MBR has
suitable boot code that will boot the active partition (such as the MBR
provided by [Syslinux](http://www.syslinux.org/wiki/index.php/Mbr)).

Several limitations apply to hard drive booting on BIOS systems at present:

 * The boot filesystem must be ext2/3/4. FAT is not currently supported, as no
   boot sector has been implemented.
 * The boot filesystem must be completely within the first 2 TiB of the device.

The filesystem you wish to install KBoot to must be mounted, and you must
create the directory you want the boot loader binary to be copied to. This
directory will also be where the boot loader will search for the configuration
file first.

With the installation directory created, KBoot can then be installed with:

    $ kboot-install --target=bios --dir=/path/to/dir

As a complete example:

    $ mount /dev/sdc1 /boot
    $ mkdir /boot/kboot
    $ kboot-install --target=bios --dir=/boot/kboot

This will result in the boot sector being installed on `/dev/sdc1`, and
`kboot.bin` being copied into `/boot/kboot`.

Note that the path to the boot loader binary relative to the root of the device
is hardcoded into the boot sector, therefore you must not move the file. If you
wish to do so then the boot sector must be reinstalled.

If you wish to update the main boot loader binary without updating the boot
sector (e.g. if you update your KBoot build), you can pass the `--update`
argument to `kboot-install` in addition to the other arguments.

### UEFI

**Note**: UEFI installation on Apple Macs is *not* currently supported. They
do not follow the UEFI standard in terms of boot manager behaviour.

On UEFI systems, KBoot must be installed to the EFI System Partition of the
device to make bootable. UEFI defines 2 ways a boot loader can be installed.

Firstly, it can be installed into a vendor-specific subdirectory of `EFI` in
the system partition, e.g. `EFI/kboot`, and an entry can be added to the
firmware's EFI boot manager (stored in NVRAM) that refers to the boot loader.
That entry should then be selectable as a boot option in the firmware. This is
the recommended way to install on internal hard drives.

Secondly, it can be installed to a fallback location, i.e.
`EFI/boot/boot<arch>.efi`. This is the location looked in if no boot manager
entries are available for a device. This should be used to install to a
removable device such as a USB stick, as boot manager entries created referring
to a removable device would not be present if it were booted on another system.

To install KBoot on a UEFI system, you must mount the EFI system partition, and
pass the path to its root to `kboot-install`.

Assuming the EFI system partition is mounted at `/boot/efi`, and using the
correct target name for your firmware architecture as described in
[Building](building.md), the following command will install the boot loader in
`EFI/kboot`, and create an EFI boot manager entry labelled "KBoot" referring to
it:

    $ kboot-install --target=efi-<arch> --dir=/boot/efi

The boot loader will search for the configuration file in the `EFI/kboot`
directory first.

You can optionally change both the vendor sub-directory name and the boot
manager entry label with the `--vendor-id` and `--label` arguments,
respectively. For example:

    $ kboot-install --target=efi-<arch> --dir=/boot/efi --vendor-id=foobar --label="Foo Bar"

will install to `EFI/foobar`, and label the boot manager entry as "Foo Bar".

If you wish to update the main boot loader binary (e.g. if you update your
KBoot build), you can pass the `--update` argument to `kboot-install` in
addition to the other arguments. This will copy the appropriate binary into
place, skipping the creation of a boot manager entry (which would result in
duplicates because one already exists).

To install to the fallback location, e.g. for an external device, assuming the
device is mounted at `/mnt` and again using the correct target name, run the
following:

    $ kboot-install --target=efi-<arch> --dir=/mnt --fallback

The boot loader will search for the configuration file in the fallback location,
i.e. `EFI/boot`, first.

Along with `--fallback`, `--update` is ignored, since the EFI boot manager is
not touched when installing to the fallback location.

Installing to a Disk Image
--------------------------

The `kboot-install` utility supports installation to a disk image, though the
procedure is a little more complicated as it cannot detect partition information
on the image. The procedure to make an image bootable differs depending on
whether it needs to be bootable by BIOS or UEFI systems.

### BIOS

To make a disk image bootable by BIOS systems, you need to manually copy
`kboot.bin` to a location in the filesystem, and then use `kboot-install` to
write the boot sector into the image, letting it know where the boot partition
is and the path to the boot sector on it.

As described in the previous section, if the image contains a partition table,
the boot partition must be made active, and the MBR should contain boot code
that will boot the active partition.

As an example, assume we have a disk image, `hd.img`, that has an ext2 partition
starting at block 2048, and has `kboot.bin` installed at `boot/kboot.bin`.
`kboot-install` expects a byte offset to the partition, so that would be
2048 * 512 = 1048576. The boot sector can then be installed to that partition
within the image with:

    $ kboot-install --target=bios --image=hd.img --offset=1048576 --path=boot/kboot.bin

The path specified should be **relative** to the root of the partition.

### UEFI

To make a disk image bootable by UEFI systems, you need to create a FAT
partition for use as a system partition, and copy the `kboot<arch>.efi` binary
for the correct architecture to `EFI/boot/boot<arch>.efi` in that filesystem.

Creating an ISO Image
---------------------

The `kboot-mkiso` utility can be used to create KBoot-bootable ISO images.
These images can be made bootable by both BIOS and UEFI systems.

Usage is simple. Simply create a directory containing everything you wish to
include in the ISO image, then create an image with:

    $ kboot-mkiso image.iso <dir>

The directory should contain a configuration file at either `boot/kboot.cfg`
or `kboot.cfg`.

If no directory is specified then an image will still be created containing
only the boot loader, which will error on boot due to the lack of a
configuration file, however the shell will still be accessible so it could be
used as a rescue medium.

By default, `kboot-mkiso` will include support for all BIOS/UEFI targets for
which it can find the binaries. This behaviour can be changed using the
`--targets` option. For example:

    $ kboot-mkiso --targets=bios image.iso <dir>

This will create an image which only supports BIOS systems.
