GUI Menu
========

Overview
--------

KBoot provides a basic graphical boot menu. An icon is assigned to each menu
entry, and the icons will be laid out horizontally on screen. A selection image
is drawn below the currently selected menu entry.

In addition to left/right to navigate the entries, the menu supports the same
inputs as the text menu:

* *F1* - Show configuration options for the currently selected entry.
* *F2* - Enter the shell.
* *F10* - Display the debug log.

Usage
-----

To use the GUI menu you must provide an icon for each menu entry, and an image
to use to highlight the current selection. Currently, only uncompressed TGA
images are supported, with their origin set as top left.

If you are using a platform that does not set a framebuffer video mode by
default (e.g. BIOS), you will need to add a "video" command into your
configuration file to set one.

Any errors in the GUI configuration will result in falling back on the text
menu. If this happens an error will be logged to the debug log to state what
was wrong, which can be viewed by pressing F10.

The example below illustrates configuration of the GUI menu:

    set "timeout" 5

    video "lfb:1024x768x32"

    set "gui" true
    set "gui_background" 0x000000
    set "gui_selection" "theme/selection.tga"

    entry "Arch" {
        set "gui_icon" "theme/os_arch.tga"
        device "hd0,0"
        linux \
            "boot/vmlinuz-linux root=UUID=${device_uuid} rw" \
            "boot/initramfs-linux.img"
    }

    entry "Windows" {
        set "gui_icon" "theme/os_win.tga"
        efi "/EFI/Microsoft/Boot/bootmgfw.efi"
    }

This results in the following menu:

![GUI Menu](https://raw.githubusercontent.com/aejsmith/kboot/master/documentation/images/gui.png)
