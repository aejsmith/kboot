Building
========

Prerequisites
-------------

KBoot uses SCons as its build system. You will need to install this.

If you wish to install KBoot to a UEFI system, you will need to install
efibootmgr.

If you wish to make use of the `kboot-mkiso` utility to generate a bootable ISO
image, you will need to install xorriso, as well as dosfstools and mtools if
you wish to create a UEFI bootable image.

Targets
-------

KBoot must be built separately for each target system type you require. A full
list of available targets can be obtained by running `scons -h` in the root of
the source tree.

If you wish to boot via the legacy BIOS, you will need to build the `bios`
target.

If you wish to boot via UEFI, you will need to build either the `efi-amd64` or
`efi-ia32` target, depending on whether your firmware is 64- or 32-bit. Most
systems use 64-bit firmware, but some older ones are 32-bit. You can determine
the type of your firmware from Linux (if the running kernel has been booted via
UEFI) by running

    $ cat /sys/firmware/efi/fw_platform_size

This will output either 64 or 32.

Building
--------

For each required target, build with:

    $ scons CONFIG=<target>

This will produce a set of binaries appropriate to the target in
`build/<target>/bin`.

Note that SCons will remember the target selected until it is overriden, so
after the first run you do not need to specify `CONFIG=<target>` on the command
line, unless you wish to change the target.

If you wish to install these binaries into your filesystem, you can do so by
running:

    $ scons install

By default, this will install to `/usr/local/lib/kboot/<target>`. You can
install them elsewhere by setting `PREFIX`, which defaults to `/usr/local`. So,
for example, to install to `/usr/lib/kboot/<target>`, use:

    $ scons PREFIX=/usr install

Like `CONFIG`, the value of `PREFIX` is remembered until is overridden.

If you install KBoot onto your filesystem, it is also recommended that you
install a copy of the binary with debugging information for your build. The
debug binary may be useful should you encounter a crash in KBoot and wish to
report it to the developers. You can install this binary with:

    $ scons install-debug

If you have not already set `PREFIX` and wish to do so, you must specify it on
that command line.

Building Utilities
------------------

A number of utilities are provided to assist in making a device bootable with
KBoot. These must be built separately from the boot loader itself as they are
independent of the selected target. They can be built by running:

    $ scons utilities

If you wish to install these binaries into your filesystem, you can do so with:

    $ scons install-utilities

As described above, you can control the installation location with `PREFIX`.
The utilities will be installed in `PREFIX/bin`.

All required targets should also be installed for the installed utilities to
work.

If you do not wish to install to your filesystem and instead work out of the
source tree, you can do so. Running the utilities from `build/host/bin` will
search for the target binaries under the `build` directory, rather than from
an installed location on the filesystem.
