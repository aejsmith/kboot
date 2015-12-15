Command Reference
=================

This page contains a list of all commands supported by the boot loader's
configuration system.

The syntax used to describe a commands arguments is
`command <arg1> [<arg2> [<arg3>...]]`. Here, `arg1` is a required argument,
`arg2` is an optional argument, and `arg3` is a further repeated argument.

OS Loaders
----------

These commands specify the main action for a configuration file or a menu entry.
Once one of these commands has been run, no more commands are allowed in the
scope that they are specified in.

### `chain`

Executes another boot sector from a device or file (BIOS-only).

**Usage**: `chain [<path>]`

**Arguments**:

 * `path` (string): Optional path to file containing boot sector.

If a path is specified, then the boot sector (i.e. the first 512 bytes) will be
loaded from that file, and executed with its boot device set to the device
containing the file.

Otherwise, the boot sector will be loaded from the current device, and executed
with its boot device set to that device.

**Examples**:

Chain load from hd1,0:

    device "hd1,0"
    chain

Chain load from a file:

    device "(hd0,1)/bootsect.bin"

### `efi`

Loads another EFI application (EFI-only).

**Usage**: `efi <cmdline>`

**Arguments**:

 * `cmdline` (string): Command line for the application. Everything before the
   first non-escaped space is treated as the file path, everything after is
   passed to the application as arguments. The file path must be absolute.

The configuration menu for this loader will allow the arguments passed to the
binary to be modified.

**Examples**:

Boot an EFI-based Windows installation:

    efi "/EFI/Microsoft/Boot/bootmgfw.efi"

### `exit`

Exits the loader and returns to the firmware.

**Usage**: `exit`

What this does depends on the platform. On EFI, it will return to the firmware
and try the next boot option, or if KBoot was run from another boot loader or
the EFI shell, it will return to that. On BIOS, it will just reboot the system.

### `kboot`

Loads a KBoot kernel.

**Usage**: `kboot <path> [<modules>]`

**Arguments**:

 * `path` (string): Path to the kernel image.
 * `modules` (string, or string list): Specifies the modules to load. If a
   string, treated as a path to a directory, from which all files will be
   loaded as modules. If a list, treated as a list of paths to modules to load.

**Environment Variables**:

 * `root_device`: By default, the boot device information passed to the kernel
   will be set to the device containing the kernel image. This variable can be
   set to an alternate device specifier which will be passed to the kernel as
   the boot device instead. If set to a UUID specifier (`uuid:...`), a UUID can
   be used which is not known to the boot loader. Additionally, arbitrary OS-
   specific boot device strings may be used by specifying `other:...`, which
   will be passed (minus the prefix) to the OS as `KBOOT_BOOTDEV_OTHER` and
   interpreted however it wishes.
 * `video_mode`: Sets the video mode, if supported by the OS.

In addition to these variables, OS-specific options can be configured by setting
environment variables corresponding to the option name.

The configuration menu for this loader will allow any options defined by the OS
to be configured, as well as the video mode if this is supported by the OS.

**Examples**:

Load a kernel and two modules, with a video mode set, the root device set to
the string "ramdisk", and the boolean kernel option `disable_smp` set to true.

    set "video_mode" "lfb:1920x1080x32"
    set "root_device" "other:ramdisk"
    set "disable_smp" true
    kboot "/boot/kernel.elf" ["/boot/module.bin" "/boot/ramdisk.img"]

### `linux`

Loads a Linux kernel.

**Usage**: `linux <cmdline> [<initrds>]`

**Arguments**:

 * `cmdline` (string): Command line for the kernel. Everything before the first
   non-escaped space is treated as the file path, everything after is passed to
   the kernel as its command line.
 * `initrds` (string, or string list): Specifies one or more initrds to load.
   If a string, treated as a path to a single initrd. If a list, treated as a
   list of paths to initrds to load.

**Environment Variables**:

 * `video_mode`: Sets the video mode to use for the kernel console. If not
   specified, on BIOS systems the kernel will be given a VGA text mode console,
   and on EFI will be given a framebuffer console in the same resolution the
   boot loader's console is using.

Note that the boot loader will not attempt to pass an appropriate `root=`
option on the kernel command line, you must do this manually. You can make use
of the `device_*` environment variables to do this.

The configuration menu for this loader will allow the kernel command line
arguments to be edited, and the video mode for the kernel to be configured.

**Examples**:

Load a kernel with the root command line argument set based on the current
device's UUID, with a framebuffer console, and two initrds, one containing a
microcode update and the other a real initramfs:

    set "video_mode" "lfb:1920x1080x32"
    linux \
        "/boot/vmlinuz rw root=UUID=${device_uuid}" \
        ["/boot/intel-ucode.img" "/boot/initramfs.img"]

### `multiboot`

Loads a Multiboot-compliant kernel.

**Usage**: `multiboot <cmdline> [<modules>]`

**Arguments**:

 * `cmdline` (string): Command line for the kernel. Everything before the first
   non-escaped space is treated as the file path. The whole string is passed to
   the kernel as its command line.
 * `modules` (string list): Specifies one or more modules to load. Each entry
   is a string containing a module command line. In each, everything before the
   first non-escaped space is treated as the file path, and the whole string is
   passed to the kernel as the module command line.

**Environment Variables**:

 * `video_mode`: Sets the video mode, if supported by the OS.

The configuration menu for this loader will allow the kernel command line
arguments to be edited, as well as the command line arguments for each module.

**Examples**:

Load a kernel and two modules, with a video mode set:

    set "video_mode" "lfb:1920x1080x32"
    multiboot "/boot/kernel.elf arg1 arg2" [
        "/boot/module1.bin arg3 arg4"
        "/boot/module2.bin arg5 arg6"
    ]

### `reboot`

Reboots the system.

**Usage**: `reboot`

Other Commands
--------------

### `cd`

Sets the current directory.

**Usage**: `cd dir`

**Arguments**:

 * `dir` (string): Path to directory to change to. Must be on current device.

### `console`

Sets the current console for shell and UI output.

**Usage**: `console <name>`

**Arguments**:

 * `name` (string): Console to set as the current. A list of consoles can be
   obtained with `lsconsole`.

### `debug`

Sets the current console for debug output.

**Usage**: `debug <name>`

**Arguments**:

 * `name` (string): Console to set as the debug console. A list of consoles can
   be obtained with `lsconsole`.

### `device`

Sets the current device.

**Usage**: `device <name>`

**Arguments**:

 * `name` (string): Device specifier string, see
   [Devices and Paths](configuration.md#devices-and-paths).

This command will set the current directory to the root of the new device, and
will set the `device`, `device_uuid` and `device_label` environment variables
to reflect the new device.

### `include`

Includes another configuration file into the current one.

**Usage**: `include <files>`

**Arguments**:

 * `files` (string): Path to either a directory or a single configuration file.
   If a directory, all files in that directory will be included in alphabetical
   order.

The effect of this command is as though the contents of the specified file(s)
were directly pasted into the current configuration file in place of the
`include` command. However, any parsing error which occurs in the new file(s)
will be treated as an error of the command, rather than causing the parsing of
the file that contains the command to fail as well.

### `serial`

Configures a serial port.

**Usage**: `serial <console> [<baud_rate> [<data_bits> [<parity> [<stop_bits>]]]]`

**Arguments**:

 * `console` (string): Name of the console to configure, must be a serial port.
 * `baud_rate` (integer): Baud rate, defaults to 115200.
 * `data_bits` (integer): Number of data bits, between 5 and 8, defaults to 8.
 * `parity` (string): Type of parity, either `none`, `even`, or `odd`, defaults
   to `none`.
 * `stop_bits` (integer): Number of stop bits, either 1 or 2, defaults to 1.

This command allows the configuration of serial port parameters, however it
does not put a serial port into use. To do that, the `console` command must be
used.

### `set`

Sets an environment variable.

**Usage**: `set <name> <value>`

**Arguments**:

 * `name` (string): Name of the variable to set.
 * `value` (any): Value to set the variable to.

### `unset`

Unsets an environment variable.

**Usage**: `unset <name>`

**Arguments**:

 * `name` (string): Name of the variable to unset.

### `video`

Sets the current video mode used by the boot loader.

**Usage**: `video <mode>`

**Arguments**:

 * `mode` (string): Video mode specifier string, see
   [Video Mode Configuration](configuration.md#video-mode-configuration).

Note that this sets the mode used in the boot loader. It does not affect the
mode used by the OS that is loaded, unless otherwise specified for the OS
loader type.

Shell Commands
--------------

These commands are primarily for use in the shell. They provide output which is
not useful in a configuration file, and are mainly used to examine the state of
the boot loader.

### `cat`

Outputs the contents of one or more files.

**Usage**: `cat <path> [<path>...]`

**Arguments**:

 * `path` (string): Path(s) to file(s) to output.

### `config`

Loads a new configuration file.

**Usage**: `config <path>`

**Arguments**:

 * `path` (string): Path to new configuration file.

### `env`

Lists environment variables.

**Usage**: `env`

### `help`

Lists available commands.

**Usage**: `help`

### `log`

Outputs the contents of the debug log.

**Usage**: `log`

### `lsconsole`

Lists available consoles and shows the currently active console.

**Usage**: `lsconsole`

### `lsdevice`

Lists available devices, or properties of a single device.

**Usage**: `lsdevice [<name>]`

**Arguments**:

 * `name` (string): Optional device specifier string. If specified, will output
   details about the specified device, otherwise will list all available
   devices.

### `ls`

Lists the contents of a directory.

**Usage**: `ls [<path>]`

**Arguments**:

 * `path` (string): Optional path string. If specified, will list the contents
   of that directory, otherwise will list the current directory.

### `lsmemory`

Lists known memory ranges and shows current memory usage.

**Usage**: `lsmemory`

### `lsvideo`

Lists available video modes.

**Usage**: `lsvideo`

### `version`

Displays the KBoot version.

**Usage**: `version`
