Multiboot Support
=================

In addition to being able to load
[Multiboot](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)
kernels, on the BIOS platform, the KBoot binary itself is a Multiboot kernel
image that can be loaded by another Multiboot-compliant boot loader.

When loaded as a Multiboot kernel, the boot loader can optionally be loaded
with a set of modules. When modules are present, the boot loader will use them
to populate a virtual filesystem which will be set as the initial boot device
in the environment.

In addition, if no file named `kboot.cfg` is present as a loaded module, a
default configuration file will be generated. This configuration will treat the
first loaded module as a KBoot-compliant kernel image, and the rest as modules
for that kernel. The command line to the kernel image module will be turned into
`set` commands, which can be used to set kernel options, etc. Boolean variables
can be set to true just be including their name on the command line, otherwise
you can use `name=value`, for instance `name="option value"`, `name=42`,
`name=false`.

The main reason why this functionality was implemented was to be able to load
a KBoot kernel image using QEMU's `-kernel` option, without having to go through
the process of creating a disk image. For example:

    $ qemu-system-x86_64 \
        -kernel kboot.bin \
        -initrd "kernel.elf bool_option video_mode=\"lfb:800x600x32\",module.elf"

This gets turned into the following configuration:

    set "bool_option" true
    set "video_mode" "lfb:800x600x32"
    kboot "kernel.elf" ["module.elf"]

As described in [Configuration](configuration.md), the boot loader can be
interrupted to bring up the configuration menu by pressing F8, or to bring up
the shell by pressing F10.

GRUB 2
------

The Multiboot support relies on the Multiboot boot loader that loads KBoot
passing the module file names as part of their command line, so that they can
be named appropriately in the virtual filesystem created from them.

GRUB 2 differs in behaviour from all other Multiboot loaders (including GRUB
Legacy) in that it does not include the file name in the module command lines.
Therefore, if you wish to use this functionality with GRUB 2, you must manually
include the module name on its command line, for example:

    multiboot /kboot.bin
    module /kernel.elf kernel.elf
    module /module.bin module.bin
