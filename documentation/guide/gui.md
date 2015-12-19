GUI Menu
========

KBoot provides a basic graphical boot menu. An icon is assigned to each menu
entry, and the icons will be laid out horizontally on screen, on top of a
background image or colour. A selection colour or image is drawn over the
currently selected menu entry.

In addition to left/right to navigate the entries, the menu supports the same
inputs as the text menu:

* *F1*: Show configuration options for the currently selected entry.
* *F9*: Display the debug log.
* *F10*: Enter the shell.

Plus any other functions defined as nested menus for the currently selected
entry.

Usage
-----

To enable the GUI menu, set the `gui` environment variable to true at the top
level scope of the menu. There are 3 customisable elements to the GUI menu, all
controlled by environment variables:

* `gui_icon`: Specifies the path to an icon for an entry, set inside the entry
  scope.
* `gui_selection`: Specifies how to highlight a selected entry, set at the top
  level scope of the menu. This can either be set to an integer, in which case
  it is interpreted as an RGB colour that will be filled in the area behind a
  selected entry's icon, or to a string, in which case it is a path to an image
  that is drawn behind the icon (centred on it). Defaults to `0xffffff`.
* `gui_background`: Specifies the background of the menu, set at the top level
  scope of the menu. Can either be an integer RGB colour, or a path to an image
  that is drawn centred on the screen (no scaling is done). Any area not covered
  by the image is filled in black. Defaults to `0x000000`.

Currently, only TGA images are supported (uncompressed, with origin set to top
left).

At minimum each entry must have `gui_icon` set.

If you are using a platform that does not set a framebuffer video mode by
default (e.g. BIOS), you will need to add a "video" command into your
configuration file to set one.

Any errors in the GUI configuration will result in falling back on the text
menu. If this happens an error will be logged to the debug log to state what
was wrong, which can be viewed by pressing F9.

The example below illustrates configuration of the GUI menu:

    set "timeout" 5

    video "lfb:800x600x32"

    set "gui" true
    set "gui_background" 0x000000
    set "gui_selection" "theme/selection.tga"

    entry "Linux" {
        set "gui_icon" "theme/os_linux.tga"

        device "uuid:..."
        linux "boot/vmlinuz root=UUID=${device_uuid} rw" "boot/initramfs.img"
    }

    entry "Windows" {
        set "gui_icon" "theme/os_windows.tga"

        efi "/EFI/Microsoft/Boot/bootmgfw.efi"
    }

This results in the following menu:

![GUI Menu](images/gui.png)
