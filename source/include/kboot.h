/*
 * Copyright (C) 2009-2021 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief               KBoot boot protocol definitions.
 */

#ifndef __KBOOT_H
#define __KBOOT_H

/** Magic number passed to the entry point of a KBoot kernel. */
#define KBOOT_MAGIC                 0xb007cafe

/** Current KBoot version. */
#define KBOOT_VERSION               2

#ifndef __ASM__

#include <types.h>

/** Type used to store a physical address. */
typedef uint64_t kboot_paddr_t;

/** Type used to store a virtual address. */
typedef uint64_t kboot_vaddr_t;

/**
 * Information tags.
 */

/** KBoot information tag header structure. */
typedef struct kboot_tag {
    uint32_t type;                          /**< Type of the tag. */
    uint32_t size;                          /**< Total size of the tag data. */
} kboot_tag_t;

/** Possible information tag types. */
#define KBOOT_TAG_NONE              0       /**< End of tag list. */
#define KBOOT_TAG_CORE              1       /**< Core information tag (always present). */
#define KBOOT_TAG_OPTION            2       /**< Kernel option. */
#define KBOOT_TAG_MEMORY            3       /**< Physical memory range. */
#define KBOOT_TAG_VMEM              4       /**< Virtual memory range. */
#define KBOOT_TAG_PAGETABLES        5       /**< Page table information (architecture-specific). */
#define KBOOT_TAG_MODULE            6       /**< Boot module. */
#define KBOOT_TAG_VIDEO             7       /**< Video mode information. */
#define KBOOT_TAG_BOOTDEV           8       /**< Boot device information. */
#define KBOOT_TAG_LOG               9       /**< Kernel log buffer. */
#define KBOOT_TAG_SECTIONS          10      /**< ELF section information. */
#define KBOOT_TAG_BIOS_E820         11      /**< BIOS address range descriptor (BIOS-specific). */
#define KBOOT_TAG_EFI               12      /**< EFI firmware information. */
#define KBOOT_TAG_SERIAL            13      /**< Serial console information. */

/** Tag containing core information for the kernel. */
typedef struct kboot_tag_core {
    kboot_tag_t header;                     /**< Tag header. */

    kboot_paddr_t tags_phys;                /**< Physical address of the tag list. */
    uint32_t tags_size;                     /**< Total size of the tag list (rounded to 8 bytes). */
    uint32_t _pad;

    kboot_paddr_t kernel_phys;              /**< Physical address of the kernel image. */

    kboot_vaddr_t stack_base;               /**< Virtual address of the boot stack. */
    kboot_paddr_t stack_phys;               /**< Physical address of the boot stack. */
    uint32_t stack_size;                    /**< Size of the boot stack. */
} kboot_tag_core_t;

/** Tag containing an option passed to the kernel. */
typedef struct kboot_tag_option {
    kboot_tag_t header;                     /**< Tag header. */

    uint8_t type;                           /**< Type of the option. */
    uint32_t name_size;                     /**< Size of name string, including null terminator. */
    uint32_t value_size;                    /**< Size of the option value, in bytes. */
} kboot_tag_option_t;

/** Possible option types. */
#define KBOOT_OPTION_BOOLEAN        0       /**< Boolean. */
#define KBOOT_OPTION_STRING         1       /**< String. */
#define KBOOT_OPTION_INTEGER        2       /**< Integer. */

/** Tag describing a physical memory range. */
typedef struct kboot_tag_memory {
    kboot_tag_t header;                     /**< Tag header. */

    kboot_paddr_t start;                    /**< Start of the memory range. */
    kboot_paddr_t size;                     /**< Size of the memory range. */
    uint8_t type;                           /**< Type of the memory range. */
} kboot_tag_memory_t;

/** Possible memory range types. */
#define KBOOT_MEMORY_FREE           0       /**< Free, usable memory. */
#define KBOOT_MEMORY_ALLOCATED      1       /**< Kernel image and other non-reclaimable data. */
#define KBOOT_MEMORY_RECLAIMABLE    2       /**< Memory reclaimable when boot information is no longer needed. */
#define KBOOT_MEMORY_PAGETABLES     3       /**< Temporary page tables for the kernel. */
#define KBOOT_MEMORY_STACK          4       /**< Stack set up for the kernel. */
#define KBOOT_MEMORY_MODULES        5       /**< Module data. */

/** Tag describing a virtual memory range. */
typedef struct kboot_tag_vmem {
    kboot_tag_t header;                     /**< Tag header. */

    kboot_vaddr_t start;                    /**< Start of the virtual memory range. */
    kboot_vaddr_t size;                     /**< Size of the virtual memory range. */
    kboot_paddr_t phys;                     /**< Physical address that this range maps to. */
} kboot_tag_vmem_t;

/** Tag describing a boot module. */
typedef struct kboot_tag_module {
    kboot_tag_t header;                     /**< Tag header. */

    kboot_paddr_t addr;                     /**< Address of the module. */
    uint32_t size;                          /**< Size of the module. */
    uint32_t name_size;                     /**< Size of name string, including null terminator. */
} kboot_tag_module_t;

/** Structure describing an RGB colour. */
typedef struct kboot_colour {
    uint8_t red;                            /**< Red value. */
    uint8_t green;                          /**< Green value. */
    uint8_t blue;                           /**< Blue value. */
} kboot_colour_t;

/** Tag containing video mode information. */
typedef struct kboot_tag_video {
    kboot_tag_t header;                     /**< Tag header. */

    uint32_t type;                          /**< Type of the video mode set up. */
    uint32_t _pad;

    union {
        /** VGA text mode information. */
        struct {
            uint8_t cols;                   /**< Columns on the text display. */
            uint8_t lines;                  /**< Lines on the text display. */
            uint8_t x;                      /**< Cursor X position. */
            uint8_t y;                      /**< Cursor Y position. */
            uint32_t _pad;
            kboot_paddr_t mem_phys;         /**< Physical address of VGA memory. */
            kboot_vaddr_t mem_virt;         /**< Virtual address of VGA memory. */
            uint32_t mem_size;              /**< Size of VGA memory mapping. */
        } vga;

        /** Linear framebuffer mode information. */
        struct {
            uint32_t flags;                 /**< LFB properties. */
            uint32_t width;                 /**< Width of video mode, in pixels. */
            uint32_t height;                /**< Height of video mode, in pixels. */
            uint8_t bpp;                    /**< Number of bits per pixel. */
            uint32_t pitch;                 /**< Number of bytes per line of the framebuffer. */
            uint32_t _pad;
            kboot_paddr_t fb_phys;          /**< Physical address of the framebuffer. */
            kboot_vaddr_t fb_virt;          /**< Virtual address of a mapping of the framebuffer. */
            uint32_t fb_size;               /**< Size of the virtual mapping. */
            uint8_t red_size;               /**< Size of red component of each pixel. */
            uint8_t red_pos;                /**< Bit position of the red component of each pixel. */
            uint8_t green_size;             /**< Size of green component of each pixel. */
            uint8_t green_pos;              /**< Bit position of the green component of each pixel. */
            uint8_t blue_size;              /**< Size of blue component of each pixel. */
            uint8_t blue_pos;               /**< Bit position of the blue component of each pixel. */
            uint16_t palette_size;          /**< For indexed modes, length of the colour palette. */

            /** For indexed modes, the colour palette set by the loader. */
            kboot_colour_t palette[0];
        } lfb;
    };
} kboot_tag_video_t;

/** Video mode types. */
#define KBOOT_VIDEO_VGA             (1<<0)  /**< VGA text mode. */
#define KBOOT_VIDEO_LFB             (1<<1)  /**< Linear framebuffer. */

/** Linear framebuffer flags. */
#define KBOOT_LFB_RGB               (1<<0)  /**< Direct RGB colour format. */
#define KBOOT_LFB_INDEXED           (1<<1)  /**< Indexed colour format. */

/** Type used to store a MAC address. */
typedef uint8_t kboot_mac_addr_t[16];

/** Type used to store an IPv4 address. */
typedef uint8_t kboot_ipv4_addr_t[4];

/** Type used to store an IPv6 address. */
typedef uint8_t kboot_ipv6_addr_t[16];

/** Type used to store an IP address. */
typedef union kboot_ip_addr {
    kboot_ipv4_addr_t v4;                   /**< IPv4 address. */
    kboot_ipv6_addr_t v6;                   /**< IPv6 address. */
} kboot_ip_addr_t;

/** Tag containing boot device information. */
typedef struct kboot_tag_bootdev {
    kboot_tag_t header;                     /**< Tag header. */

    uint32_t type;                          /**< Boot device type. */

    union {
        /** Local file system information. */
        struct {
            uint32_t flags;                 /**< Behaviour flags. */
            uint8_t uuid[64];               /**< UUID of the boot filesystem. */
        } fs;

        /** Network boot information. */
        struct {
            uint32_t flags;                 /**< Behaviour flags. */
            kboot_ip_addr_t server_ip;      /**< Server IP address. */
            uint16_t server_port;           /**< UDP port number of TFTP server. */
            kboot_ip_addr_t gateway_ip;     /**< Gateway IP address. */
            kboot_ip_addr_t client_ip;      /**< IP used on this machine when communicating with server. */
            kboot_mac_addr_t client_mac;    /**< MAC address of the boot network interface. */
            uint8_t hw_type;                /**< Network interface type. */
            uint8_t hw_addr_size;           /**< Hardware address length. */
        } net;

        /** Other device information. */
        struct {
            uint32_t str_size;              /**< Size of device string (including null terminator). */
        } other;
    };
} kboot_tag_bootdev_t;

/** Boot device types. */
#define KBOOT_BOOTDEV_NONE          0       /**< No boot device (e.g. boot image). */
#define KBOOT_BOOTDEV_FS            1       /**< Booted from a local file system. */
#define KBOOT_BOOTDEV_NET           2       /**< Booted from the network. */
#define KBOOT_BOOTDEV_OTHER         3       /**< Other device (specified by string). */

/** Network boot behaviour flags. */
#define KBOOT_NET_IPV6              (1<<0)  /**< Given addresses are IPv6 addresses. */

/** Tag describing the kernel log buffer. */
typedef struct kboot_tag_log {
    kboot_tag_t header;                     /**< Tag header. */

    kboot_vaddr_t log_virt;                 /**< Virtual address of log buffer. */
    kboot_paddr_t log_phys;                 /**< Physical address of log buffer. */
    uint32_t log_size;                      /**< Size of log buffer. */
    uint32_t _pad;

    kboot_paddr_t prev_phys;                /**< Physical address of previous log buffer. */
    uint32_t prev_size;                     /**< Size of previous log buffer. */
} kboot_tag_log_t;

/** Structure of a log buffer. */
typedef struct kboot_log {
    uint32_t magic;                         /**< Magic value used by loader (should not be overwritten). */

    uint32_t start;                         /**< Offset in the buffer of the start of the log. */
    uint32_t length;                        /**< Number of characters in the log buffer. */

    uint32_t info[3];                       /**< Fields for use by the kernel. */
    uint8_t buffer[0];                      /**< Log data. */
} kboot_log_t;

/** Tag describing ELF section headers. */
typedef struct kboot_tag_sections {
    kboot_tag_t header;                     /**< Tag header. */

    uint32_t num;                           /**< Number of section headers. */
    uint32_t entsize;                       /**< Size of each section header. */
    uint32_t shstrndx;                      /**< Section name string table index. */

    uint32_t _pad;

    uint8_t sections[0];                    /**< Section data. */
} kboot_tag_sections_t;

/** Tag containing page table information (IA32). */
typedef struct kboot_tag_pagetables_ia32 {
    kboot_tag_t header;                     /**< Tag header. */

    kboot_paddr_t page_dir;                 /**< Physical address of the page directory. */
    kboot_vaddr_t mapping;                  /**< Virtual address of recursive mapping. */
} kboot_tag_pagetables_ia32_t;

/** Tag containing page table information (AMD64). */
typedef struct kboot_tag_pagetables_amd64 {
    kboot_tag_t header;                     /**< Tag header. */

    kboot_paddr_t pml4;                     /**< Physical address of the PML4. */
    kboot_vaddr_t mapping;                  /**< Virtual address of recursive mapping. */
} kboot_tag_pagetables_amd64_t;

/** Tag containing page table information (ARM). */
typedef struct kboot_tag_pagetables_arm {
    kboot_tag_t header;                     /**< Tag header. */

    kboot_paddr_t l1;                       /**< Physical address of the first level page table. */
    kboot_vaddr_t mapping;                  /**< Virtual address of temporary mapping region. */
} kboot_tag_pagetables_arm_t;

/** Tag containing page table information (ARM64). */
typedef struct kboot_tag_pagetables_arm64 {
    kboot_tag_t header;                     /**< Tag header. */

    kboot_paddr_t ttl0;                     /**< Physical address of the level 0 translation table. */
    kboot_vaddr_t mapping;                  /**< Virtual address of recursive mapping. */
} kboot_tag_pagetables_arm64_t;

/** Tag containing page table information. */
#if defined(__i386__)
    typedef kboot_tag_pagetables_ia32_t kboot_tag_pagetables_t;
#elif defined(__x86_64__)
    typedef kboot_tag_pagetables_amd64_t kboot_tag_pagetables_t;
#elif defined(__arm__)
    typedef kboot_tag_pagetables_arm_t kboot_tag_pagetables_t;
#elif defined(__aarch64__)
    typedef kboot_tag_pagetables_arm64_t kboot_tag_pagetables_t;
#endif

/** Tag containing the E820 memory map (BIOS-specific). */
typedef struct kboot_tag_bios_e820 {
    kboot_tag_t header;                     /**< Tag header. */

    uint32_t num_entries;                   /**< Number of entries. */
    uint32_t entry_size;                    /**< Size of each entry. */

    uint8_t entries[0];                     /**< Array of entries. */
} kboot_tag_bios_e820_t;

/** Tag containing EFI firmware information (EFI-specific). */
typedef struct kboot_tag_efi {
    kboot_tag_t header;                     /**< Tag header. */

    kboot_paddr_t system_table;             /**< Physical address of system table. */

    uint8_t type;                           /**< Type of the firmware. */

    uint32_t num_memory_descs;              /**< Number of memory descriptors. */
    uint32_t memory_desc_size;              /**< Size of each memory descriptor. */
    uint32_t memory_desc_version;           /**< Memory descriptor version. */

    uint8_t memory_map[0];                  /**< Firmware memory map. */
} kboot_tag_efi_t;

/** EFI firmware types. */
#define KBOOT_EFI_32                0       /**< Firmware is 32-bit. */
#define KBOOT_EFI_64                1       /**< Firmware is 64-bit. */

/** Tag containing serial console information. */
typedef struct kboot_tag_serial {
    kboot_tag_t header;                     /**< Tag header. */

    kboot_paddr_t addr;                     /**< Base address. */
    kboot_vaddr_t addr_virt;                /**< Virtual mapping (if MMIO). */
    uint8_t io_type;                        /**< I/O type. */

    uint32_t type;                          /**< Type of the serial port. */

    uint32_t baud_rate;                     /**< Baud rate. */
    uint8_t data_bits;                      /**< Number of data bits. */
    uint8_t stop_bits;                      /**< Number of stop bits. */
    uint8_t parity;                         /**< Parity mode. */
} kboot_tag_serial_t;

/** I/O types. */
#define KBOOT_IO_TYPE_MMIO              0   /**< Memory-mapped I/O. */
#define KBOOT_IO_TYPE_PIO               1   /**< Port I/O. */

/** Serial port types. */
#define KBOOT_SERIAL_TYPE_NS16550       0   /**< Standard 16550. */
#define KBOOT_SERIAL_TYPE_BCM2835_AUX   1   /**< BCM2835 auxiliary UART (16550-like). */
#define KBOOT_SERIAL_TYPE_PL011         2   /**< ARM PL011. */

/** Serial parity modes. */
#define KBOOT_SERIAL_PARITY_NONE        0   /**< No parity. */
#define KBOOT_SERIAL_PARITY_ODD         1   /**< Odd parity. */
#define KBOOT_SERIAL_PARITY_EVEN        2   /**< Even parity. */

/**
 * Image tags.
 */

/** KBoot ELF note name. */
#define KBOOT_NOTE_NAME             "KBoot"

/** Section type definition. */
#ifdef __arm__
#   define KBOOT_SECTION_TYPE       "%note"
#else
#   define KBOOT_SECTION_TYPE       "@note"
#endif

/** KBoot image tag types (used as ELF note type field). */
#define KBOOT_ITAG_IMAGE            0       /**< Basic image information (required). */
#define KBOOT_ITAG_LOAD             1       /**< Memory layout options. */
#define KBOOT_ITAG_OPTION           2       /**< Option description. */
#define KBOOT_ITAG_MAPPING          3       /**< Virtual memory mapping description. */
#define KBOOT_ITAG_VIDEO            4       /**< Requested video mode. */

/** Image tag containing basic image information. */
typedef struct kboot_itag_image {
    uint32_t version;                       /**< KBoot version that the image is using. */
    uint32_t flags;                         /**< Flags for the image. */
} kboot_itag_image_t;

/** Flags controlling optional features. */
#define KBOOT_IMAGE_SECTIONS        (1<<0)  /**< Load ELF sections and pass a sections tag. */
#define KBOOT_IMAGE_LOG             (1<<1)  /**< Enable the kernel log facility. */

/** Macro to declare an image itag. */
#define KBOOT_IMAGE(flags) \
    __asm__( \
        "   .pushsection \".note.kboot.image\", \"a\", " KBOOT_SECTION_TYPE "\n" \
        "   .long 1f - 0f\n" \
        "   .long 3f - 2f\n" \
        "   .long " XSTRINGIFY(KBOOT_ITAG_IMAGE) "\n" \
        "0: .asciz \"" KBOOT_NOTE_NAME "\"\n" \
        "1: .p2align 2\n" \
        "2: .long " XSTRINGIFY(KBOOT_VERSION) "\n" \
        "   .long " STRINGIFY(flags) "\n" \
        "   .p2align 2\n" \
        "3: .popsection\n")

/** Image tag specifying loading parameters. */
typedef struct kboot_itag_load {
    uint32_t flags;                         /**< Flags controlling load behaviour. */
    uint32_t _pad;
    kboot_paddr_t alignment;                /**< Requested physical alignment of kernel image. */
    kboot_paddr_t min_alignment;            /**< Minimum physical alignment of kernel image. */
    kboot_vaddr_t virt_map_base;            /**< Base of virtual mapping range. */
    kboot_vaddr_t virt_map_size;            /**< Size of virtual mapping range. */
} kboot_itag_load_t;

/** Flags controlling load behaviour. */
#define KBOOT_LOAD_FIXED            (1<<0)  /**< Load at a fixed physical address. */
#define KBOOT_LOAD_ARM64_EL2        (1<<1)  /**< Execute the kernel in EL2 if supported. */

/** Macro to declare a load itag. */
#define KBOOT_LOAD(flags, alignment, min_alignment, virt_map_base, virt_map_size) \
    __asm__( \
        "   .pushsection \".note.kboot.load\", \"a\", " KBOOT_SECTION_TYPE "\n" \
        "   .long 1f - 0f\n" \
        "   .long 3f - 2f\n" \
        "   .long " XSTRINGIFY(KBOOT_ITAG_LOAD) "\n" \
        "0: .asciz \"" KBOOT_NOTE_NAME "\"\n" \
        "1: .p2align 2\n" \
        "2: .long " STRINGIFY(flags) "\n" \
        "   .long 0\n" \
        "   .quad " STRINGIFY(alignment) "\n" \
        "   .quad " STRINGIFY(min_alignment) "\n" \
        "   .quad " STRINGIFY(virt_map_base) "\n" \
        "   .quad " STRINGIFY(virt_map_size) "\n" \
        "   .p2align 2\n" \
        "3: .popsection\n")

/** Image tag containing an option description. */
typedef struct kboot_itag_option {
    uint8_t type;                           /**< Type of the option. */
    uint32_t name_size;                     /**< Size of the option name. */
    uint32_t desc_size;                     /**< Size of the option description. */
    uint32_t default_size;                  /**< Size of the default value. */
} kboot_itag_option_t;

/** Macro to declare a boolean option itag. */
#define KBOOT_BOOLEAN_OPTION(name, desc, default) \
    __asm__( \
        "   .pushsection \".note.kboot.option." name "\", \"a\", " KBOOT_SECTION_TYPE "\n" \
        "   .long 1f - 0f\n" \
        "   .long 6f - 2f\n" \
        "   .long " XSTRINGIFY(KBOOT_ITAG_OPTION) "\n" \
        "0: .asciz \"" KBOOT_NOTE_NAME "\"\n" \
        "1: .p2align 2\n" \
        "2: .byte " XSTRINGIFY(KBOOT_OPTION_BOOLEAN) "\n" \
        "   .byte 0\n" \
        "   .byte 0\n" \
        "   .byte 0\n" \
        "   .long 4f - 3f\n" \
        "   .long 5f - 4f\n" \
        "   .long 1\n" \
        "3: .asciz \"" name "\"\n" \
        "4: .asciz \"" desc "\"\n" \
        "5: .byte " STRINGIFY(default) "\n" \
        "   .p2align 2\n" \
        "6: .popsection\n")

/** Macro to declare an integer option itag. */
#define KBOOT_INTEGER_OPTION(name, desc, default) \
    __asm__( \
        "   .pushsection \".note.kboot.option." name "\", \"a\", " KBOOT_SECTION_TYPE "\n" \
        "   .long 1f - 0f\n" \
        "   .long 6f - 2f\n" \
        "   .long " XSTRINGIFY(KBOOT_ITAG_OPTION) "\n" \
        "0: .asciz \"" KBOOT_NOTE_NAME "\"\n" \
        "1: .p2align 2\n" \
        "2: .byte " XSTRINGIFY(KBOOT_OPTION_INTEGER) "\n" \
        "   .byte 0\n" \
        "   .byte 0\n" \
        "   .byte 0\n" \
        "   .long 4f - 3f\n" \
        "   .long 5f - 4f\n" \
        "   .long 8\n" \
        "3: .asciz \"" name "\"\n" \
        "4: .asciz \"" desc "\"\n" \
        "5: .quad " STRINGIFY(default) "\n" \
        "   .p2align 2\n" \
        "6: .popsection\n")

/** Macro to declare an string option itag. */
#define KBOOT_STRING_OPTION(name, desc, default) \
    __asm__( \
        "   .pushsection \".note.kboot.option." name "\", \"a\", " KBOOT_SECTION_TYPE "\n" \
        "   .long 1f - 0f\n" \
        "   .long 6f - 2f\n" \
        "   .long " XSTRINGIFY(KBOOT_ITAG_OPTION) "\n" \
        "0: .asciz \"" KBOOT_NOTE_NAME "\"\n" \
        "1: .p2align 2\n" \
        "2: .byte " XSTRINGIFY(KBOOT_OPTION_STRING) "\n" \
        "   .byte 0\n" \
        "   .byte 0\n" \
        "   .byte 0\n" \
        "   .long 4f - 3f\n" \
        "   .long 5f - 4f\n" \
        "   .long 6f - 5f\n" \
        "3: .asciz \"" name "\"\n" \
        "4: .asciz \"" desc "\"\n" \
        "5: .asciz \"" default "\"\n" \
        "   .p2align 2\n" \
        "6: .popsection\n")

typedef struct kboot_itag_mapping_v1 {
    kboot_vaddr_t virt;
    kboot_paddr_t phys;
    kboot_vaddr_t size;
} kboot_itag_mapping_v1_t;

/** Image tag containing a virtual memory mapping description. */
typedef struct kboot_itag_mapping {
    kboot_vaddr_t virt;                     /**< Virtual address to map. */
    kboot_paddr_t phys;                     /**< Physical address to map to. */
    kboot_vaddr_t size;                     /**< Size of mapping to make. */
    uint32_t cache;                         /**< Cacheability flags for the mapping. */
} kboot_itag_mapping_t;

/** Cacheability flags. */
#define KBOOT_CACHE_DEFAULT         0       /**< Default caching behaviour. */
#define KBOOT_CACHE_WT              1       /**< Map as write-through. */
#define KBOOT_CACHE_UC              2       /**< Map as uncached. */

/** Macro to declare a virtual memory mapping itag. */
#define KBOOT_MAPPING(virt, phys, size, cache) \
    __asm__( \
        "   .pushsection \".note.kboot.mapping.b" # virt "\", \"a\", " KBOOT_SECTION_TYPE "\n" \
        "   .long 1f - 0f\n" \
        "   .long 3f - 2f\n" \
        "   .long " XSTRINGIFY(KBOOT_ITAG_MAPPING) "\n" \
        "0: .asciz \"" KBOOT_NOTE_NAME "\"\n" \
        "1: .p2align 2\n" \
        "2: .quad " STRINGIFY(virt) "\n" \
        "   .quad " STRINGIFY(phys) "\n" \
        "   .quad " STRINGIFY(size) "\n" \
        "   .long " STRINGIFY(cache) "\n" \
        "   .p2align 2\n" \
        "3: .popsection\n")

/** Image tag specifying the kernel's requested video mode. */
typedef struct kboot_itag_video {
    uint32_t types;                         /**< Supported video mode types. */
    uint32_t width;                         /**< Preferred LFB width. */
    uint32_t height;                        /**< Preferred LFB height. */
    uint8_t bpp;                            /**< Preferred LFB bits per pixel. */
} kboot_itag_video_t;

/** Macro to declare a video mode itag. */
#define KBOOT_VIDEO(types, width, height, bpp) \
    __asm__( \
        "   .pushsection \".note.kboot.video\", \"a\", " KBOOT_SECTION_TYPE "\n" \
        "   .long 1f - 0f\n" \
        "   .long 3f - 2f\n" \
        "   .long " XSTRINGIFY(KBOOT_ITAG_VIDEO) "\n" \
        "0: .asciz \"" KBOOT_NOTE_NAME "\"\n" \
        "1: .p2align 2\n" \
        "2: .long " STRINGIFY(types) "\n" \
        "   .long " STRINGIFY(width) "\n" \
        "   .long " STRINGIFY(height) "\n" \
        "   .byte " STRINGIFY(bpp) "\n" \
        "   .p2align 2\n" \
        "3: .popsection\n")

#endif /* __ASM__ */
#endif /* __KBOOT_H */
