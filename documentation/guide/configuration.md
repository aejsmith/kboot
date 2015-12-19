Configuration/Usage
===================

This section covers the configuration and usage of KBoot. KBoot requires a
configuration file to describe how to boot your system.

Location
--------

When booted from a hard drive or external device, KBoot will by default search
for a configuration file named `kboot.cfg` in the same directory as the main
loader binary (see [Installation](installation.md)). This should be the
preferred location for the configuration file.

If such a file is not found, or if booted from a CD or over the network, then
the configuration file will be looked for in `/boot/kboot.cfg`, followed by
`/kboot.cfg`. If neither of these are found either, then an error will be
raised, however the shell will be accessible.

Structure
---------

### Commands

The structure of a KBoot configuration file is a basically a set of commands.
A full list of commands and details of their arguments is available in
[Command Reference](commands.md); this page is more concerned with the
structure of the configuration file and basic examples.

A command is given as a command name, followed by a set of arguments. The end
of a line (that is not inside a string, list or command list - see below)
indicates the end of a command, unless the line ends with a `\` character. For
example:

    hello "world" 42
    hello \
        "world" 1234

Arguments are typed, so you must use the correct types as expected by a
command, and arbitrary text (aside from the command name) is not valid (i.e.
the quotation marks around "world" in the above example are required).

The following is a list of types supported and their syntax:

 * **String**: Arbitrary text, given by `"some string"`. Quotation marks can be
   included within a string by escaping them with a `\` character.
 * **Integer**: Decimal, hexadecimal or octal numbers, given by `42`, `0x42`,
   and `042`, respectively.
 * **Boolean**: `true` or `false`.
 * **List**: A list of values of arbitrary types grouped together, given by
   `[value1 value2]`, e.g. `["hello world" 42 true]` is a list containing a
   string, an integer and a boolean.
 * **Command List**: A list of commands, enclosed in curly braces (`{ }`), each
   separated by new lines.

Commands are executed inside an environment, which contains a set of state
affecting the behaviour of commands and information about the OS to load. Each
command list executes in its own environment, which inherits initial state from
the environment of the command list it was defined in (at the point that it was
defined, changes made afterward to the parent environment do not affect the
child).

Operating system loaders are special types of commands. They determine the
actual operating system that will be booted. They must be the last command in
a command list. Compared to other commands, which are executed immediately as
the configuration file is read (even within a child command list for a menu
entry), the main action of the OS loader is deferred until later, e.g. when a
menu entry is selected to boot.

### Variables

Environments contains a set of variables. Variables are used both to control
the behaviour of the boot loader, and for substitution within the configuration
file.

Variables can be set with the `set` command, and unset with `unset`. They are
typed in the same way that command arguments are (their value is taken from the
`set` command argument). For example:

    set "foobar" 1234

This sets an integer variable named *foobar*.

The value of variables can be obtained in two ways. Firstly, in place of a whole
command argument, you can write `$variable_name` to pass the value of the
variable (with its type) to a command. For example:

    set "kernel_path" "/vmlinuz"
    linux $kernel_path

This passes the string `/vmlinuz` to the `linux` command.

Secondly, for string, integer, and boolean variables, their values can be
substituted into a string by writing `"... ${variable_name} ..."`. For example:

    set "kernel_path" "/vmlinuz"
    set "kernel_args" "root=/dev/sda3 ro"
    linux "${kernel_path} ${kernel_args}"

This passes the string `/vmlinuz root=/dev/sda3 ro` to the `linux` command.

Devices and Paths
-----------------

Devices (hard disks, CD drives, etc) in KBoot are given a name that they can be
referred to with. For hard disks, the name takes the form `hdX`, where X is the
number of the drive, and partitions on the disk are named `hdX,Y`, where Y is
the partition number. CDs are named `cdromX`, floppies `floppyX` and network
boot servers `netX`. A list of all detected devices on a system can be obtained
using the `lsdevice` command in the shell.

In addition, where supported by the filesystem, devices can be referred to by
UUID and by label. These should be used where possible, as device names are not
always stable, they depend on the order the firmware presents them. UUIDs and
labels can be used in place of a device name by using `uuid:...` and
`label:...`, respectively.

An environment has a current device. This can be changed using the `device`
command. The `device`, `device_uuid` and `device_label` variables are set in
the environment to reflect the name, UUID and label, respectively, of the
current device (the latter two may not be set depending on whether the device's
filesystem supports them). This can be used, for example, to avoid duplication
in a configuration file:

    device "uuid:599802a2-bc78-405e-9d14-756c75ebea88"
    linux "/boot/vmlinuz root=UUID=${device_uuid} ..."

Paths are used to refer to locations on the filesystem. KBoot uses UNIX-style
paths, i.e. separated with a `/` character. An environment has a current
directory (located on the current device), which is used for relative path
lookups. Absolute paths, i.e. beginning with a `/`, are looked up from the root
of the current device. In addition, absolute paths can be qualified with a
device identifier to refer to a location on another device without changing the
current device, e.g. `(hd0,1)/foo/bar`, `(uuid:...)/foo/bar`, etc.

Single-OS Configuration
-----------------------

For the simple case where you only have a single OS to boot, it is not
necessary to define a boot menu. For example:

    device "uuid:599802a2-bc78-405e-9d14-756c75ebea88"
    linux "/boot/vmlinuz rw root=UUID=${device_uuid}" "/boot/initramfs.img"

This configuration will boot a Linux kernel and initramfs. See the
[Command Reference](commands.md) for full details on the arguments to the
`linux` command.

When using a configuration without a menu, the boot loader will add a short
delay in the boot process to allow you to press F8 to open the OS loader's
configuration menu, e.g. to change kernel command line arguments, or F10 to
open the shell.

Multi-OS Configuration
----------------------

When you have multiple OSes that you wish to choose between, you can use a boot
menu. To use a boot menu, rather than having an OS loader command in the top
level of the configuration, you define a set of entries with the `entry`
command. The `entry` command is passed an entry name, and a command list to
execute for that entry. For example:

    set "timeout" 5

    entry "Linux" {
        device "uuid:..."
        linux "/boot/vmlinuz rw root=UUID=${device_uuid}" "/boot/initramfs.img"
    }

    entry "Windows" {
        efi "/EFI/Microsoft/Boot/bootmgfw.efi"
    }

This configuration presents a menu to choose between booting Linux and Windows.

There are a number of environment variables that can be set to control the
behaviour of the menu, which should be defined at the same scope as the entry
commands:

 * `default`: Sets the initially selected entry. Can either be an integer index
   (first entry has index 0), or a string matching the title of an entry.
 * `hidden`: If this is set to true, the menu will not display by default. As
   with the single-OS configuration, a short delay will be added during boot to
   allow you to press F8 in order to bring up the menu, or F10 to bring up the
   shell. If no key press is detected, then the boot loader will boot the
   default entry.
 * `timeout`: If set to an integer value, when the menu is first displayed, a
   countdown will begin for this many seconds. If no key presses are made during
   this time, the loader will boot the default entry.

### Nested Menus

The menu system also allows for more complex, nested menu structures, by placing
entries within other entries. This can work in two ways.

Firstly, entries inside an entry which does not have its own OS loader will
result in the parent entry being a link which, when selected, will open another
menu containing the entries inside it. For example:

    entry "Linux" {
        device "uuid:..."
        linux "/boot/vmlinuz rw root=UUID=${device_uuid}" "/boot/initramfs.img"
    }

    entry "Other OSes" {
        entry "Windows" {
            efi "/EFI/Microsoft/Boot/bootmgfw.efi"
        }
    }

Here, the main menu has two entries, "Linux" and "Other OSes". When "Other OSes"
is selected, another menu will display containing a "Windows" entry.

Secondly, entries inside an entry which *does* have its own OS loader will be
presented as additional functions that can be accessed through the F* keys, and
the parent entry itself will still boot the OS when it is selected by pressing
Enter. An example use for this is that several Linux distributions keep older
kernel versions around when a new version is installed, and typically fill up
the boot menu with these. Or, they keep a number of alternate boot options, for
example for using a fallback initramfs. Using nested menus, it would be possible
to relegate entries like these to a separate menu, and keep only one entry for
the distribution in the main boot menu. For example:

    entry "Linux" {
        device "uuid:..."

        set "kernel_cmdline" "/boot/vmlinuz rw root=UUID=${device_uuid}"

        entry "Options" {
            entry "Fallback" {
                linux $kernel_cmdline "/boot/initramfs-fallback.img"
            }
        }

        linux $kernel_cmdline "/boot/initramfs.img"
    }

This results in a "Linux" entry in the main menu that has an "Options" menu
accessible by pressing F2 while the entry is highlighted. The "Options" menu has
a "Fallback" entry which boots using an alternate initramfs.

Note that environment variables related to menu behaviour (the ones described
above, as well as the `gui*` variables for the GUI menu) are *not* inherited
inside child environments. This is because the settings for the top level menu
are typically not desired for child menus, so special-casing them to not be
inherited avoids having to manually unset them.

### GUI Menu

See [GUI Menu](gui.md) for details on GUI menu configuration.

Video Mode Configuration
------------------------

KBoot supports setting video modes for use within the boot loader itself and
for the OS being booted (depending on whether the OS supports it). Video modes
are referred to by a mode string of the form `type[:WxH[xD]]`, where the width
and height are optional, and depth is optional in addition to that. Type is
currently either `lfb` for a high-resolution framebuffer mode, or `vga` for VGA
text mode. A list of available mode strings can be obtained using the `lsvideo`
command from the shell.

On the BIOS platform, KBoot defaults to a VGA text mode console. On UEFI, it
defaults to a framebuffer console as configured by the firmware. You can set a
custom video mode to use within the boot loader by using the `video` command.
Note that on higher resolutions, the text UI is constrained to a maximum
1024x768 area in the centre, because it simply looks too spread out if it takes
up the whole screen past this size. Higher resolutions are more of use for the
GUI menu.

For OSes that support video mode selection in the boot loader (e.g. Linux,
some Multiboot/KBoot kernels), setting the `video_mode` environment variable
before the OS loader command will determine the video mode that will be set for
that OS. This setting can also be overridden from the configuration menu.

Shell
-----

The shell is essentially an interactive frontend into the configuration system,
which allows you to execute commands exactly as you would place them in the
configuration file. In addition, some shell-oriented commands are provided
with output, e.g. the `ls*` commands, which allow you to examine the boot
loader's view of the system, as well as look at filesystem contents.

The shell can be entered by pressing F10 at the boot menu, or at certain error
screens. You could use the shell, for example, if you make an error in your
configuration file that prevents it from being loaded, in order to manually
load your OS.

Debug Log
---------

KBoot maintains an internal debug log, which may contain useful information to
help solve problems. On debug builds (the default when built from git, or
selected by passing `DEBUG=1` on the SCons command line), the debug log is by
default directed to the first serial port. It is also possible to access the
log by pressing F9 from the boot menu or error screens, or using the `log`
command in the shell.

Examples
--------

This section includes some full example configurations.

### Linux/Windows Dual Boot (EFI)

    set "timeout" 5

    video "lfb:1920x1080x32"

    entry "Arch Linux" {
        device "uuid:599802a2-bc78-405e-9d14-756c75ebea88"

        set "kernel_cmdline" "/boot/vmlinuz-linux rw root=UUID=${device_uuid}"

        entry "Options" {
            entry "Fallback" {
                linux \
                    $kernel_cmdline \
                    ["/boot/intel-ucode.img" "/boot/initramfs-linux-fallback.img"]
            }
        }

        linux \
            "${kernel_cmdline} quiet rd.udev.log-priority=3 loglevel=3" \
            ["/boot/intel-ucode.img" "/boot/initramfs-linux.img"]
    }

    entry "Windows 10" {
        efi "/EFI/Microsoft/Boot/bootmgfw.efi"
    }

### Linux/Windows Dual Boot (BIOS)

    set "timeout" 5

    video "lfb:1920x1080x32"

    entry "Arch Linux" {
        device "uuid:599802a2-bc78-405e-9d14-756c75ebea88"

        set "kernel_cmdline" "/boot/vmlinuz-linux rw root=UUID=${device_uuid}"

        entry "Options" {
            entry "Fallback" {
                linux \
                    $kernel_cmdline \
                    ["/boot/intel-ucode.img" "/boot/initramfs-linux-fallback.img"]
            }
        }

        linux \
            "${kernel_cmdline} quiet rd.udev.log-priority=3 loglevel=3" \
            ["/boot/intel-ucode.img" "/boot/initramfs-linux.img"]
    }

    entry "Windows 10" {
        device "hd0,1"
        chain
    }
