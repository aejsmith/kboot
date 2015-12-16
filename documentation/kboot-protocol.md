KBoot Boot Protocol
===================

License
-------

Copyright &copy; 2012-2014 Alex Smith

This document is distributed under the terms of the [Creative Commons
Attribution-ShareAlike 3.0 Unported](http://creativecommons.org/licenses/
by-sa/3.0/) license.

Introduction
------------

The purpose of this document is to specify the KBoot Boot Protocol, a method
for loading an operating system kernel and providing information to it.

The following sections describe the format of the information provided to the
boot loader in the kernel image, the format of the information that the boot
loader provides to the kernel, and the machine state upon entering the kernel.

Basic Definitions
-----------------

This specification uses C syntax to define the KBoot structures. Structures are
assumed to be implicitly padded by the compiler to achieve natural alignment
for each field. However, in some cases padding has been explicitly added to
structures to ensure that 64-bit fields are aligned to an 8 byte boundary
regardless of architecture (e.g. on 32-bit x86, 64-bit fields need only be
aligned to 4 bytes). Only the standard C fixed-width integer types are used, to
ensure that it is clear what size each field should be. The following
additional type aliases are defined:

    typedef uint64_t kboot_paddr_t;
    typedef uint64_t kboot_vaddr_t;

These types are used to store physical and virtual addresses, respectively.
They are fixed to 64 bits in order to keep the format of all structures the
same across architectures.

Kernel Image
------------

A KBoot kernel is an ELF32 or ELF64 image annotated with ELF note sections to
specify to the boot loader how it wishes to be loaded, options that can be
edited by the user to control kernel behaviour, among other things. These
sections are referred to as "image tags". All integer values contained within
the image tags are stored in the byte order specified by the ELF header.

ELF notes have the following format:

 * _Name Size_: 32-bit integer. Size of the Name field, in bytes.
 * _Desc Size_: 32-bit integer. Size of the Desc field, in bytes.
 * _Type_: 32-bit integer. Type of the note.
 * _Name_: String identifying the vendor who defined the format of the note.
   Padded to a 4 byte boundary.
 * _Desc_: Data contained in the note. Padded to a 4 byte boundary.

For a KBoot image tag, the Name field is set to `"KBoot\0"`, the Type field is
set to the identifier of the image tag type, and the Desc field contains the
tag data.

The KBoot header file included with the KBoot boot loader defines macros for
creating all image tag types that expand to an inline ASM code chunk that
correctly defines the note section.

The following image tag types are defined (given as "Name (type ID)"):

### `KBOOT_ITAG_IMAGE` (`0`)

This image tag is the only tag that is required to be present in a kernel image.
All other tags are optional. It is used to identify the image as a KBoot kernel.
There must only be one `KBOOT_ITAG_IMAGE` tag in a kernel image.

    typedef struct kboot_itag_image {
        uint32_t version;
        uint32_t flags;
    } kboot_itag_image_t;

Fields:

 * `version`: Number defining the KBoot protocol version that the kernel is
   using. The version number can be used by a boot loader to determine whether
   additions in later versions of this specification are present. The current
   version number is 1.
 * `flags`: Flags controlling whether certain optional features should be
   enabled. The following flags are currently defined:
    - `KBOOT_IMAGE_SECTIONS` (bit 0): Load additional ELF sections and pass
      section header table to the kernel (see `KBOOT_TAG_SECTIONS`).
    - `KBOOT_IMAGE_LOG` (bit 1): Enable the kernel log facility (see
      `KBOOT_TAG_LOG`).

### `KBOOT_ITAG_LOAD` (`1`)

This tag can be used for more control over the physical and virtual memory
layout set up by the boot loader. There must be at most one `KBOOT_ITAG_LOAD`
tag in a kernel image.

    typedef struct kboot_itag_load {
        uint32_t      flags;
        uint32_t      _pad;
        kboot_paddr_t alignment;
        kboot_paddr_t min_alignment;
        kboot_vaddr_t virt_map_base;
        kboot_vaddr_t virt_map_size;
    } kboot_itag_load_t;

Fields:

 * `flags`: Flags controlling load behaviour. The following flags are currently
   defined:
    - `KBOOT_LOAD_FIXED` (bit 0): When this bit is set, the loader will load
      each segment in the ELF image at the exact physical address specified by
      the `p_paddr` field in the program headers. The `alignment` and
      `min_alignment` fields will be ignored. When unset, the kernel image will
      be loaded at a physical address allocated by the boot loader.
 * `alignment`: Requested alignment of the kernel image in physical memory. If
   this is set to 0, the alignment will be chosen automatically by the boot
   loader. Otherwise, it must be a power of 2 greater than or equal to the
   machine's page size.
 * `min_alignment`: Minimum physical alignment. If non-zero and less than the
   `alignment` value, if the loader is unable to allocate at `alignment` it
   will repeatedly try to allocate at decreasing powers of 2 down to this value
   until it is able to make an allocation. If 0, `alignment` will also be the
   minimum alignment.
 * `virt_map_base`/`virt_map_size`: The boot loader allocates regions of the
   virtual address space to map certain things into, such as the tag list for
   the kernel and the video framebuffer. These fields specify the region of
   virtual address space that these mappings should be made in. If both 0, the
   boot loader may place its mappings at any location in the virtual address
   space.

If this tag is not present in the kernel image, all fields will default to 0.

### `KBOOT_ITAG_OPTION` (`2`)

Multiple option tags can be included in a kernel image. Each describes an
option for the kernel that can be configured by the user via the boot loader.
The boot loader can use the information given by these tags, for example, to
display a configuration menu.

    typedef struct kboot_itag_option {
        uint8_t  type;
        uint32_t name_size;
        uint32_t desc_size;
        uint32_t default_size;
    } kboot_itag_option_t;

This structure is followed by 3 variable-length fields: `name`, `desc` and
`default`.

Fields:

 * `type`: Type of the option. What is in the `default` field depends on the
   option type. The following option types are defined:
   - `KBOOT_OPTION_BOOLEAN` (0): Boolean, 1 byte, 0 or 1.
   - `KBOOT_OPTION_STRING` (1): String, variable-length, null-terminated string
     (size includes null terminator).
   - `KBOOT_OPTION_INTEGER` (2): Integer, 8 bytes, 64-bit integer.
 * `name_size`: Size of the `name` field, including null terminator.
 * `desc_size`: Size of the `desc` field, including null terminator.
 * `default_size`: Size of the `default` field. Dependent on `type`, see above.
 * `name`: String name of the option. Must not contain spaces, or any of the
   following characters: `" '`. Names of the format `option_name` are
   recommended.
 * `desc`: Human readable description for the option, for use in configuration
   menus in the boot loader.
 * `default`: Default value of the option. Dependent on `type`, see above.

### `KBOOT_ITAG_MAPPING` (`3`)

This tag specifies a range of physical memory to map in to the kernel virtual
address space.

    typedef struct kboot_itag_mapping {
        kboot_vaddr_t virt;
        kboot_paddr_t phys;
        kboot_vaddr_t size;
    } kboot_itag_mapping_t;

Fields:

 * `virt`: The virtual address to map at. If this is set to the special value
   `0xffffffffffffffff`, then the boot loader will allocate an address to map
   at. The allocated address can be found in the kernel by iterating over the
   provided virtual address space map to find an entry with matching physical
   address and size. Otherwise, the exact address specified (must be aligned to
   the page size) is used.
 * `phys`: The physical address to map to (must be aligned to the page size).
 * `size`: The size of the range to map (must be a multiple of the page size).

For more information on the procedure used to build the virtual address space
for the kernel, see the _Kernel Environment_ section.

### `KBOOT_ITAG_VIDEO` (`4`)

This tag is used to specify to the boot loader what video types are supported
and what display mode it should attempt to set. There must be at most one
`KBOOT_ITAG_VIDEO` tag in a kernel image.

    typedef struct kboot_itag_video {
        uint32_t types;
        uint32_t width;
        uint32_t height;
        uint8_t  bpp;
    } kboot_itag_video_t;

Fields:

 * `types`: Bitfield of supported video types. The following video types are
   currently defined:
    - `KBOOT_VIDEO_VGA` (bit 0): VGA text mode.
    - `KBOOT_VIDEO_LFB` (bit 1): Linear framebuffer.
 * `width`/`height`/`bpp`: Preferred mode for `KBOOT_VIDEO_LFB`. If this mode
   is available it will be set, otherwise one as close as possible will be set.
   If all fields are 0, a mode will be chosen automatically.

What types are supported, the default video setup if this tag is not present,
and the precedence of types all depend on the platform. Furthermore, some
platforms may not support video mode setting at all. On such platforms the
kernel should use other means of output, such as a UART. See the _Platform
Specifics_ section for details of the video support on each platform.

Kernel Environment
------------------

The kernel entry point takes 2 arguments:

    void kmain(uint32_t magic, kboot_tag_t *tags);

The first argument is the magic value, `0xb007cafe`, which can be used by the
kernel to ensure that it has been loaded by a KBoot boot loader. The second
argument is a pointer to the first tag in the information tag list, described
in the _Kernel Information_ section.

The kernel is entered with the MMU enabled. A virtual address space is set up
by the boot loader which maps the kernel and certain other things, as well as
an architecture-dependent method to be able to manipulate the virtual address
space (see below). The following procedure is used to build the address space:

 * Map the kernel image.
 * Map ranges specified by `KBOOT_ITAG_MAPPING` tags.
 * Allocate space for other mappings (e.g. tag list, framebuffer, log buffer)
   as well as the stack. Address space for these mappings is allocated within
   the address range specified by the `KBOOT_ITAG_LOAD` tag. They are all
   allocated contiguously, however padding is permitted between mappings only
   as necessary to satisfy any alignment requirements.

A map of the virtual address space is provided in the tag list, see
`KBOOT_TAG_VMEM`. Note that the page tables set up by the boot loader are
intended to only be temporary. The kernel should build its own page tables using
the information provided by the boot loader as soon as possible. The physical
memory used by the page tables created by boot loader is marked as
`KBOOT_MEMORY_PAGETABLES` in the memory map, allowing it to be identified and
made free for use once the kernel has switched to its own page tables.

On all architectures, the kernel will be entered with a valid stack set up,
mapped in the virtual address space. Details of the stack mapping are contained
in the `KBOOT_TAG_CORE` information tag. The physical memory used by the stack
is marked in the memory map as `KBOOT_MEMORY_STACK`; if the kernel switches away
from this stack as part of its initialization it should be sure to free the
physical memory used by it.

The following sections describe the environment upon entering the kernel for
each supported architecture.

### IA32

The arguments to the kernel entry point are passed on the stack. The machine
state upon entry to the kernel is as follows:

 * In 32-bit protected mode, paging enabled. A20 line is enabled.
 * CS is set to a flat 32-bit read/execute code segment. The exact value is
   undefined.
 * DS/ES/FS/GS/SS are set to a flat 32-bit read/write data segment. The exact
   values are undefined.
 * All bits in EFLAGS are clear except for bit 1, which must always be 1.
 * EBP is 0.
 * ESP points to a valid stack mapped in the virtual address space set up by
   the boot loader.
    - KBoot magic value at offset 4.
    - Tag list pointer at offset 8.

Other machine state is not defined. In particular, the state of GDTR and IDTR
may not be valid, the kernel should set up its own GDT and IDT.

The boot loader may make use of large (4MB) pages within a single virtual
mapping when constructing the virtual address space, however separate mappings
should not be mapped together on a single large page. If large pages have been
used then the kernel will be entered with CR4.PSE set to 1, however this is not
guaranteed to be set and the kernel should explicitly enable it should it wish
to use large pages.

The address space set up by the boot loader will _not_ have the global flag set
on any mappings.

To allow the kernel to manipulate the virtual address space, the page directory
is recursively mapped. A 4MB (sized and aligned) region of address space is
allocated, and the page directory entry for that range is set to point to
itself. This region is allocated as high as possible _outside_ of the virtual
map range specified by the `KBOOT_ITAG_LOAD` tag and any mappings specified by
`KBOOT_ITAG_MAPPING` tags. The range is additionally not included in any
`KBOOT_TAG_VMEM` tags passed to the kernel. This region is kept separate like
this because it is intended to be temporary to assist the kernel in setting up
its own page tables, and should be hidden from architecture-independent kernel
code.

The format of the `KBOOT_TAG_PAGETABLES` tag for IA32 is as follows:

    typedef struct kboot_tag_pagetables {
        kboot_tag_t   header;
    
        kboot_paddr_t page_dir;
        kboot_vaddr_t mapping;
    } kboot_tag_pagetables_t;

Fields:

 * `page_dir`: Physical address of the page directory.
 * `mapping`: Virtual address of the recursive page directory mapping.

### AMD64

The arguments to the kernel entry point are passed as per the AMD64 ABI. The
machine state upon entry to the kernel is as follows:

 * RDI contains the KBoot magic value.
 * RSI contains the tag list pointer.
 * In 64-bit long mode, paging enabled, A20 line is enabled.
 * CS is set to a flat 64-bit read/execute code segment. The exact value is
   undefined.
 * DS/ES/FS/GS/SS are set to 0.
 * All bits in RFLAGS are clear except for bit 1, which must always be 1.
 * RBP is 0.
 * RSP points to a valid stack mapped in the virtual address space set up by
   the boot loader.

Other machine state is not defined. In particular, the state of GDTR and IDTR
may not be valid, the kernel should set up its own GDT and IDT.

The boot loader may make use of large (2MB) pages within a single virtual
mapping when constructing the virtual address space, however separate mappings
should not be mapped together on a single large page.

The address space set up by the boot loader will _not_ have the global flag set
on any mappings.

To allow the kernel to manipulate the virtual address space, the PML4 is
recursively mapped. A 512GB (sized and aligned) region of the virtual address
space is allocated, and the PML4 entry for that range is set to point to itself.
The same rules for allocation of this region as for IA32 apply here.

The format of the `KBOOT_TAG_PAGETABLES` tag for AMD64 is as follows:

    typedef struct kboot_tag_pagetables {
        kboot_tag_t   header;
    
        kboot_paddr_t pml4;
        kboot_vaddr_t mapping;
    } kboot_tag_pagetables_t;

Fields:

 * `pml4`: Physical address of the PML4.
 * `mapping`: Virtual address of the recursive PML4 mapping.

### ARM

The arguments to the kernel are passed as per the ARM Procedure Call Standard.
The machine state upon entry to the kernel is as follows:

 * R0 contains the KBoot magic value.
 * R1 contains the tag list pointer.
 * Both IRQs and FIQs are disabled.
 * MMU is enabled (TTBR0 = address of first level table, TTBCR.N = 0).
 * Instruction and data caches are enabled.
 * R11 is 0.
 * R13 (SP) points to a valid stack mapped in the virtual address space set up
   by the boot loader.

Other machine state is not defined.

The boot loader may make use of sections (1MB page mappings) within a single
virtual mapping when constructing the virtual address space, however separate
mappings should not be mapped together on a single section.

Unlike IA32 and AMD64, on ARM it is not possible to recursively map the paging
structures as the first and second level tables have different formats.
Instead, a 1MB (sized and aligned) region of the virtual address space is
allocated, and the last page within it is mapped to the second level table that
covers that region. The kernel can then set up temporary mappings within this
region by modifying the page table. The same rules for allocation of this region
as for IA32 apply here.

The format of the `KBOOT_TAG_PAGETABLES` tag for ARM is as follows:

    typedef struct kboot_tag_pagetables {
        kboot_tag_t   header;
    
        kboot_paddr_t l1;
        kboot_vaddr_t mapping;
    } kboot_tag_pagetables_t;

Fields:

 * `l1`: Physical address of the first level translation table.
 * `mapping`: Virtual address of the 1MB temporary mapping region.

Kernel Information
------------------

Once the kernel has been loaded, the boot loader provides a set of information
to it describing the environment that it is running in, options that were set by
the user, modules that were loaded, etc. This information is provided as a list
of "information tags". The tag list is a contiguous list of structures, each
with an identical header identifying the type and size of that tag. The start
of the tag list is aligned to the page size, and the start of each tag is
aligned on an 8 byte boundary after the end of the previous tag. The tag list
is terminated with an `KBOOT_TAG_NONE` tag. The tag list is mapped in a virtual
memory range, and the physical memory containing it is marked in the memory map
as `KBOOT_MEMORY_RECLAIMABLE`. Multiple tags of the same type are guaranteed to
be grouped together in the tag list.

The header of each tag is in the following format:

    typedef struct kboot_tag {
        uint32_t type;
        uint32_t size;
    } kboot_tag_t;

Fields:

 * `type`: Type ID for this tag.
 * `size`: Total size of the tag data, including the header. The location of
   the next tag is given by `ROUND_UP(current_tag + current_tag->size, 8)`.

The following sections define all of the tag types that can appear in the tag
list (given as "Name (type ID)").

### `KBOOT_TAG_NONE` (`0`)

This tag signals the end of the tag list. It contains no extra data other than
the header.

### `KBOOT_TAG_CORE` (`1`)

This tag is always present in the tag list, and it is always the first tag in
the list.

    typedef struct kboot_tag_core {
        kboot_tag_t   header;
    
        kboot_paddr_t tags_phys;
        uint32_t      tags_size;
        uint32_t      _pad;

        kboot_paddr_t kernel_phys;

        kboot_vaddr_t stack_base;
        kboot_paddr_t stack_phys;
        uint32_t      stack_size;
    } kboot_tag_core_t;

Fields:

 * `tags_phys`: Physical address of the tag list.
 * `tags_size`: Total size of the tag data in bytes, rounded up to an 8 byte
   boundary.
 * `kernel_phys`: The physical load address of the kernel image. This field is
   only valid when the `KBOOT_ITAG_LOAD` tag does not have `KBOOT_LOAD_FIXED`
   set.
 * `stack_base`: The virtual address of the base of the boot stack.
 * `stack_phys`: The physical address of the base of the boot stack.
 * `stack_size`: The size of the boot stack in bytes.

### `KBOOT_TAG_OPTION` (`2`)

This tag gives details of the value of a kernel option. For each option defined
by a `KBOOT_ITAG_OPTION` image tag, a `KBOOT_TAG_OPTION` will be present to
give the option value.

    typedef struct kboot_tag_option {
        kboot_tag_t header;
    
        uint8_t     type;
        uint32_t    name_size;
        uint32_t    value_size;
    } kboot_tag_option_t;

This structure is followed by 2 variable-length fields, `name` and `value`.

Fields:

 * `type`: Type of the option (see `KBOOT_ITAG_OPTION`).
 * `name_size`: Size of the name string, including null terminator.
 * `value_size`: Size of the option value, in bytes.
 * `name`: Name of the option. The start of this field is the end of the tag
   structure, aligned up to an 8 byte boundary.
 * `value`: Value of the option, dependent on the type (see `KBOOT_ITAG_OPTION`).
   The start of this field is the end of the name string, aligned up to an 8
   byte boundary.

### `KBOOT_TAG_MEMORY` (`3`)

The set of `KBOOT_TAG_MEMORY` tags in the tag list describe the physical memory
available for use by the kernel. This memory map is derived from a platform-
specific memory map and also details which ranges of memory contain the kernel,
modules, KBoot data, etc. All ranges are page-aligned, and no ranges will
overlap. The minimum set of ranges possible are given, i.e. adjacent ranges of
the same type are coalesced into a single range. Note that this memory map does
_not_ describe platform-specific memory regions such as memory used for ACPI
data. It only contains information about usable RAM. Depending on the platform,
additional information may be included in the tag list about such regions. See
the _Platform Specifics_ section for more information. All memory tags are
sorted in the tag list by lowest start address first.

    typedef struct kboot_tag_memory {
        kboot_tag_t   header;
    
        kboot_paddr_t start;
        kboot_paddr_t size;
        uint8_t       type;
    } kboot_tag_memory_t;

Fields:

 * `start`: Start address of the range. Aligned to the page size.
 * `size`: Size of the range. Multiple of the page size.
 * `type`: Type of the range. The following types are currently defined:
    - `KBOOT_MEMORY_FREE` (0): Free memory available for the kernel to use.
    - `KBOOT_MEMORY_ALLOCATED` (1): Allocated memory not available for use by
      the kernel. Such ranges contain the kernel image and the log buffer (if
      any).
    - `KBOOT_MEMORY_RECLAIMABLE` (2): Memory that currently contains KBoot data
      (e.g. the tag list) but can be freed after kernel initialization is
      complete.
    - `KBOOT_MEMORY_PAGETABLES` (3): Memory containing the page tables set up
      by the boot loader.
    - `KBOOT_MEMORY_STACK` (4): Memory containing the stack set up by the boot
      loader.
    - `KBOOT_MEMORY_MODULES` (5): Memory containing module data.

### `KBOOT_TAG_VMEM` (`4`)

The set of `KBOOT_TAG_VMEM` tags describe all virtual memory mappings that
exist in the kernel virtual address space. All virtual memory tags are sorted
in the tag list by lowest start address first.

    typedef struct kboot_tag_vmem {
        kboot_tag_t   header;
    
        kboot_vaddr_t start;
        kboot_vaddr_t size;
        kboot_paddr_t phys;
    } kboot_tag_vmem_t;

Fields:

 * `start`: Start address of the virtual memory range. Aligned to the page size.
 * `size`: Size of the virtual memory range. Multiple of the page size.
 * `phys`: Start of the physical memory range that this range maps to.

### `KBOOT_TAG_PAGETABLES` (`5`)

This tag contains information required to allow the kernel to manipulate virtual
address mappings. The content of this tag is architecture-specific. For more
information see the _Kernel Environment_ section.

### `KBOOT_TAG_MODULE` (`6`)

KBoot allows "modules" to be loaded and passed to the kernel. As far as KBoot
and the boot loader is aware, a module is simply a regular file that is loaded
into memory and passed to the kernel. Interpretation of the loaded data is up
to the kernel. Possible uses are loading drivers necessary for the kernel to
be able to access the boot filesystem, or loading a file system image to run
the OS from. Each instance of this tag describes one of the modules that has
been loaded.

    typedef struct kboot_tag_module {
        kboot_tag_t   header;
    
        kboot_paddr_t addr;
        uint32_t      size;
        uint32_t      name_size;
    } kboot_tag_module_t;

This structure is followed by a variable-length `name` field.

Fields:

 * `addr`: Physical address at which the module data is located, aligned to the
   page size. The data is not mapped into the virtual address space. The memory
   containing module data is marked as `KBOOT_MEMORY_MODULES`.
 * `size`: Size of the module data, in bytes.
 * `name_size`: Size of the name string, including null terminator.
 * `name`: Name of the module. This is the base name of the file that the module
   was loaded from.

### `KBOOT_TAG_VIDEO` (`7`)

This tag describes the current video mode. This tag may not always be present,
whether it is is platform dependent. For more details, see `KBOOT_ITAG_VIDEO`
and the _Platform Specifics_ section.

    typedef struct kboot_colour {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } kboot_colour_t;
    
    typedef struct kboot_tag_video {
        kboot_tag_t header;
    
        uint32_t     type;
        uint32_t    _pad;
    
        union {
            struct {
                uint8_t        cols;
                uint8_t        lines;
                uint8_t        x;
                uint8_t        y;
                uint32_t       _pad;
                kboot_paddr_t  mem_phys;
                kboot_vaddr_t  mem_virt;
                uint32_t       mem_size;
            } vga;
    
            struct {
                uint32_t       flags;
                uint32_t       width;
                uint32_t       height;
                uint8_t        bpp;
                uint32_t       pitch;
                uint32_t       _pad;
                kboot_paddr_t  fb_phys;
                kboot_vaddr_t  fb_virt;
                uint32_t       fb_size;
                uint8_t        red_size;
                uint8_t        red_pos;
                uint8_t        green_size;
                uint8_t        green_pos;
                uint8_t        blue_size;
                uint8_t        blue_pos;
                uint16_t       palette_size;
                kboot_colour_t palette[0];
            } lfb;
        };
    } kboot_tag_video_t;

Fields:

 * `type`: One of the available video mode type bits defined in the
   `KBOOT_ITAG_VIDEO` section will be set in this field to indicate the type
   of the video mode that has been set. The remainder of the structure
   content is dependent on the video mode type.

Fields (`KBOOT_VIDEO_VGA`):

 * `cols`: Number of columns on the text display (in characters).
 * `lines`: Number of lines on the text display (in characters).
 * `x`: Current X position of the cursor.
 * `y`: Current Y position of the cursor.
 * `mem_phys`: Physical address of VGA memory (0xb8000 on PC).
 * `mem_virt`: Virtual address of a mapping of the VGA memory.
 * `mem_size`: Size of the virtual mapping (multiple of page size). The size of
   the mapping will be sufficient to access the entire screen (i.e. at least
   `cols * rows * 2`).

Fields (`KBOOT_VIDEO_LFB`);

 * `flags`: Flags indicating properties of the framebuffer. The lower 8 bits
   contain the colour The following flags
   are currently defined:
    - `KBOOT_LFB_RGB` (bit 0): The framebuffer is in direct RGB colour format.
    - `KBOOT_LFB_INDEXED` (bit 1): The framebuffer is in indexed colour format.
   The `KBOOT_LFB_RGB` and `KBOOT_LFB_INDEXED` flags are mutually exclusive.
 * `width`: Width of the video mode, in pixels.
 * `height`: Height of the video mode, in pixels.
 * `bpp`: Bits per pixel.
 * `pitch`: Number of bytes per line of the framebuffer.
 * `fb_phys`: Physical address of the framebuffer.
 * `fb_virt`: Virtual address of a mapping of the framebuffer.
 * `fb_size`: Size of the virtual mapping (multiple of the page size). The size
   of the mapping will be sufficient to access the entire screen (i.e. at least
   `pitch * height`).
 * `red_size`/`green_size`/`blue_size`: For `KBOOT_LFB_RGB` modes, these fields
   give the size, in bits, of the red, green and blue components of each pixel.
 * `red_pos`/`green_pos`/`blue_pos`: For `KBOOT_LFB_RGB` modes, these fields
   give the bit position within each pixel of the least significant bits of the
   red, green and blue components.
 * `palette_size`: For `KBOOT_LFB_INDEXED`, the number of colours in the given
   palette.
 * `palette`: For `KBOOT_LFB_INDEXED`, a variable-length colour palette that
   has been set up by the boot loader.

### `KBOOT_TAG_BOOTDEV` (`8`)

This tag describes the device that the system was booted from.

    typedef uint8_t kboot_mac_addr_t[16];
    typedef uint8_t kboot_ipv4_addr_t[4];
    typedef uint8_t kboot_ipv6_addr_t[16];
    typedef union kboot_ip_addr {
        kboot_ipv4_addr_t v4;
        kboot_ipv6_addr_t v6;
    } kboot_ip_addr_t;

The above types are used to store network address information. A MAC address is
stored in the `kboot_mac_addr_t` type, formatted according to the information
provided in the tag. IP addresses are stored in the `kboot_ip_addr_t` union,
in network byte order.

    typedef struct kboot_tag_bootdev {
        kboot_tag_t header;
    
        uint32_t type;
    
        union {
            struct {
                uint32_t         flags;
                uint8_t          uuid[64];
            } fs;
    
            struct {
                uint32_t         flags;
                kboot_ip_addr_t  server_ip;
                uint16_t         server_port;
                kboot_ip_addr_t  gateway_ip;
                kboot_ip_addr_t  client_ip;
                kboot_mac_addr_t client_mac;
                uint8_t          hw_type;
                uint8_t          hw_addr_size;
            } net;

            struct {
                uint32_t         str_size;
            } other;
        };
    } kboot_tag_bootdev_t;

Fields:

 * `type`: Specifies the type of the boot device. The following device types are
   currently defined:
    - `KBOOT_BOOTDEV_NONE` (0): No boot device. This is the case when booted
      from a boot image.
    - `KBOOT_BOOTDEV_FS` (1): Booted from a local file system.
    - `KBOOT_BOOTDEV_NET` (2): Booted from the network.
    - `KBOOT_BOOTDEV_OTHER` (3): Booted from some other device, specified by a
      string, the meaning of which is OS-defined.
   
   The remainder of the structure is dependent on the device type.

Fields (`KBOOT_BOOTDEV_FS`):

 * `flags`: Behaviour flags. No flags are currently defined.
 * `uuid`: The UUID of the boot filesystem. The UUID is given as a string. See
   the section _Filesystem UUIDs_ for more information about this field.

Fields (`KBOOT_BOOTDEV_NET`):

 * `flags`: Behaviour flags. The following flags are currently defined:
    - `KBOOT_NET_IPV6` (bit 0): The given IP addresses are IPv6 addresses,
      rather than IPv4 addresses.
 * `server_ip`: IP address of the server that was booted from.
 * `server_port`: UDP port number of the TFTP server.
 * `gateway_ip`: Gateway IP address.
 * `client_ip`: IP address that was used on this machine when communicating
   with the server.
 * `client_mac`: MAC address of the boot network interface.
 * `hw_type`: Type of the network interface, as specified by the _Hardware Type_
   section of RFC 1700.
 * `hw_addr_size`: Size of the client MAC address.

Fields (`KBOOT_BOOTDEV_OTHER`):

 * `str_size`: Size of the device specifier string following the structure,
   including null terminator.
 * `str`: Device specifier string. The start of this field is the end of the
   tag structure, aligned up to an 8 byte boundary.

### `KBOOT_TAG_LOG` (`9`)

This tag is part of an optional feature that enables the boot loader to display
a log from the kernel in the event of a crash. As long as the memory that
contains the log buffer is not cleared when the machine is reset, it can be
recovered by the boot loader. Whether this is supported is platform-dependent
(currently PC only). It is a rather rudimentary system, however it is useful to
assist in debugging, particularly on real hardware if no other mechanism is
available to obtain output from a kernel before a crash. A kernel indicates
that it wishes to use this feature by setting the `KBOOT_IMAGE_LOG` flag in the
`KBOOT_ITAG_IMAGE` tag. If the boot loader supports this feature, this tag type
will be included in the tag list.

    typedef struct kboot_tag_log {
        kboot_tag_t   header;
    
        kboot_vaddr_t log_virt;
        kboot_paddr_t log_phys;
        uint32_t      log_size;
        uint32_t      _pad;
    
        kboot_paddr_t prev_phys;
        uint32_t      prev_size;
    } kboot_tag_log_t;

Fields:

 * `log_virt`: Virtual address of the log buffer (aligned to page size).
 * `log_phys`: Physical address of the log buffer.
 * `log_size`: Total size of the log buffer (multiple of page size).
 * `prev_phys`: Physical address of the log buffer from the previous session.
 * `prev_size`: Total size of previous session log buffer. If 0, no previous
   log buffer was available.

The main log buffer will be contained in physical memory marked as
`KBOOT_MEMORY_ALLOCATED`, while the previous log buffer, if any, will be
contained in physical memory marked as `KBOOT_MEMORY_RECLAIMABLE`.

The log buffer is a circular buffer. It includes a header at the start, any
remaining space contains the log itself. The header has the following format:

    typedef struct kboot_log {
        uint32_t magic;
    
        uint32_t start;
        uint32_t length;
    
        uint32_t info[3];
        uint8_t  buffer[0];
    } kboot_log_t;

Fields:

 * `magic`: This field is used by the boot loader to identify a log buffer. Its
   exact value is not specified, it should not be modified by the kernel.
 * `start`: Offset in the buffer of the start of the log. A new log buffer will
   have this field initialized to 0. Must not exceed
   `(tag->log_size - sizeof(kboot_log_t))`.
 * `length`: Number of characters in the log buffer. A new log buffer will have
   this field initialized to 0. Must not exceed
   `(tag->log_size - sizeof(kboot_log_t))`.
 * `info`: These fields are free for use by the kernel. A new log buffer will
   have them initialized to 0, the previous session log buffer will contain
   the values written by the previous kernel.

The kernel should modify the `start` and `length` fields as it writes into the
buffer. An algorithm for writing to the buffer is shown below:

    log_size = tag->log_size - sizeof(kboot_log_t);
    log->buffer[(log->start + log->length) % log_size] = ch;
    if(log->length < log_size) {
        log->length++;
    } else {
        log->start = (log->start + 1) % log_size;
    }

A point to bear in mind is that when the CPU is reset, the CPU cache may not be
written back to memory. Therefore, should you wish to ensure that a set of data
written to the log can be seen by the boot loader, invalidate the CPU cache
after writing.

### `KBOOT_TAG_SECTIONS` (`10`)

If the `KBOOT_IMAGE_SECTIONS` flag is specified in the `KBOOT_ITAG_IMAGE` image
tag, additional loadable sections not normally loaded (`SHT_PROGBITS`,
`SHT_NOBITS`, `SHT_SYMTAB` and `SHT_STRTAB` sections without the `SHF_ALLOC`
flag set, e.g. the symbol table and debug information) will be loaded, and this
tag will be passed containing the section header table. The additional sections
will be loaded in physical memory marked as `KBOOT_MEMORY_ALLOCATED`, and each
will have its header modified to contain the allocated physical address in the
`sh_addr` field.

    typedef struct kboot_tag_sections {
        kboot_tag_t header;
    
        uint32_t    num;
        uint32_t    entsize;
        uint32_t    shstrndx;
    
        uint32_t    _pad;
    
        uint8_t     sections[0];
    } kboot_tag_sections_t;

Fields:

 * `num`/`entsize`/`shstrndx`: These fields correspond to the fields with the
   same name in the ELF executable header.
 * `sections`: Array of section headers, each `entsize` bytes long.

Platform Specifics
------------------

### PC BIOS

If a video mode image tag is not included, VGA text mode will be used. If both
`KBOOT_VIDEO_LFB` and `KBOOT_VIDEO_VGA` are set, the boot loader will attempt
to set a framebuffer mode and fall back on VGA. It is recommended that a
kernel supports VGA text mode rather than exclusively using an LFB mode, as it
may not always be possible to set a LFB mode.

There are some information tags which are specific to the PC BIOS platform.
These are detailed below.

#### `KBOOT_TAG_BIOS_E820` (`11`)

The PC BIOS provides more memory information than is included in the KBoot
physical memory map. Some of this information is required to support ACPI,
therefore the boot loader will provide an unmodified copy of the BIOS E820
memory map to the kernel. A single tag will be provided containing all entries
returned by the BIOS, in the order in which they were returned.

    typedef struct kboot_tag_bios_e820 {
        kboot_tag_t header;
    
        uint32_t    num_entries;
        uint32_t    entry_size;
    
        uint8_t     entries[0];
    } kboot_tag_bios_e820_t;

Fields:

 * `num_entries`: The number of entries present.
 * `entry_size`: The size of each entry in the array.
 * `entries`: Array of entries, each `entry_size` bytes long.

### EFI

EFI firmwares provide two sets of services, Boot Services and Runtime Services.
When the system is executing in Boot Services mode, the firmware is in control
of the system memory map and hardware. Before entering the kernel, the boot
loader must call `ExitBootServices`. Any ranges in the EFI memory map marked
as types `EfiBootServicesCode` or `EfiBootServicesData` should be made free in
the main KBoot memory map.

Runtime Services must be called with all ranges in the EFI memory map marked
with the `EFI_MEMORY_RUNTIME` attribute identity mapped, until a call to
`SetVirtualAddressMap` is made. Even though the boot loader sets up a virtual
address space for the kernel, it will _not_ map runtime services sections or
call `SetVirtualAddressMap`, it is left up to the kernel to do so. This is done
because `SetVirtualAddressMap` is a one-shot operation, it cannot be called
multiple times, so it is left to the kernel to give it maximum flexibility in
mapping things how it wants.

Only `KBOOT_VIDEO_LFB` is supported on EFI platforms. If a video mode image tag
is not included, a default video mode will be set (if the platform has video
output support).

There are some information tags which are specific to EFI-based platforms. These
are detailed below.

#### `KBOOT_TAG_EFI` (`12`)

This tag provides the information needed to access EFI runtime services, as well
as a copy of the EFI memory map at the time of the call to `ExitBootServices`.

    typedef struct kboot_tag_efi {
        kboot_tag_t   header;
    
        kboot_paddr_t system_table;

        uint8_t       type;
    
        uint32_t      num_memory_descs;
        uint32_t      memory_desc_size;
        uint32_t      memory_desc_version;
    
        uint8_t       memory_map[0];
    } kboot_tag_efi_t;

Fields:

 * `system_table`: Physical address of the EFI system table.
 * `type`: Type of the EFI firmware, one of:
    - `KBOOT_EFI_32` (0): Firmware is 32-bit.
    - `KBOOT_EFI_64` (1): Firmware is 64-bit.
 * `num_memory_descs`: Number of memory descriptors.
 * `memory_desc_size`: Size of each descriptor in the memory map.
 * `memory_desc_version`: Memory map descriptor version.
 * `memory_map`: Array of memory descriptors, each `memory_desc_size` bytes long.

Filesystem UUIDs
----------------

Some filesystems, such as ext2/3/4, have their own UUID. For these, the UUID
field of the `KBOOT_TAG_BOOTDEV` tag contains a string representation of the
filesystem's UUID. There are others that do not, however for some it is
possible to give a reasonably unique identifier based on other information.
This section describes what a boot loader should give as a UUID for certain
filesystems. These should follow the behaviour of the `libblkid` library.

### ISO9660

If the filesystem has a modification date set, that is used, else the creation
date is used, and formatted into the following:

    yyyy-mm-dd-hh-mm-ss-cc
