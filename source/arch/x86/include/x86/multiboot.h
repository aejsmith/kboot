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
 * @brief               Multiboot header.
 */

#ifndef __X86_MULTIBOOT_H
#define __X86_MULTIBOOT_H

/** How many bytes from the start of the file we search for the header. */
#define MULTIBOOT_SEARCH                8192

/** Required alignment of the header. */
#define MULTIBOOT_HEADER_ALIGN          4

/** Minimum header sizes. */
#define MULTIBOOT_HEADER_MIN_SIZE       12
#define MULTIBOOT_HEADER_AOUT_SIZE      32
#define MULTIBOOT_HEADER_VIDEO_SIZE     48

/** Magic value passed by the bootloader. */
#define MULTIBOOT_LOADER_MAGIC          0x2badb002

/** Magic value in the Multiboot header. */
#define MULTIBOOT_HEADER_MAGIC          0x1badb002

/** Flags for the Multiboot header. */
#define MULTIBOOT_PAGE_ALIGN            (1<<0)  /**< Align loaded modules on page boundaries. */
#define MULTIBOOT_MEMORY_INFO           (1<<1)  /**< Pass memory information to OS. */
#define MULTIBOOT_VIDEO_MODE            (1<<2)  /**< Pass video information to OS. */
#define MULTIBOOT_AOUT_KLUDGE           (1<<16) /**< Use address fields in header. */

/** Flags passed by the bootloader. */
#define MULTIBOOT_INFO_MEMORY           (1<<0)  /**< Bootloader provided memory info. */
#define MULTIBOOT_INFO_BOOTDEV          (1<<1)  /**< Bootloader provided boot device. */
#define MULTIBOOT_INFO_CMDLINE          (1<<2)  /**< Bootloader provided command line. */
#define MULTIBOOT_INFO_MODS             (1<<3)  /**< Bootloader provided module info. */
#define MULTIBOOT_INFO_AOUT_SYMS        (1<<4)  /**< Bootloader provided a.out symbols. */
#define MULTIBOOT_INFO_ELF_SHDR         (1<<5)  /**< Bootloader provided ELF section header table. */
#define MULTIBOOT_INFO_MEM_MAP          (1<<6)  /**< Bootloader provided memory map. */
#define MULTIBOOT_INFO_DRIVE_INFO       (1<<7)  /**< Bootloader provided drive information. */
#define MULTIBOOT_INFO_CONFIG_TABLE     (1<<8)  /**< Bootloader provided config table. */
#define MULTIBOOT_INFO_BOOT_LOADER_NAME (1<<9)  /**< Bootloader provided its name. */
#define MULTIBOOT_INFO_APM_TABLE        (1<<10) /**< Bootloader provided APM table. */
#define MULTIBOOT_INFO_VIDEO_INFO       (1<<11) /**< Bootloader provided video info. */

/** Size of the Multiboot structures. */
#define MULTIBOOT_INFO_SIZE             88
#define MULTIBOOT_MODULE_INFO_SIZE      16

/** Offsets into the info structure required in assembly code. */
#define MULTIBOOT_INFO_OFF_BOOT_DEVICE  12      /**< Offset of the boot device field. */
#define MULTIBOOT_INFO_OFF_CMDLINE      16      /**< Offset of the command line field. */
#define MULTIBOOT_INFO_OFF_MODS_COUNT   20      /**< Offset of the module count field. */
#define MULTIBOOT_INFO_OFF_MODS_ADDR    24      /**< Offset of the module address field. */

/** Offsets into the module structure required in assembly code. */
#define MULTIBOOT_MODULE_OFF_MOD_START  0       /**< Offset of the module start field. */
#define MULTIBOOT_MODULE_OFF_MOD_END    4       /**< Offset of the module end field. */
#define MULTIBOOT_MODULE_OFF_CMDLINE    8       /**< Offset of the command line field. */

#ifndef __ASM__

#include <config.h>
#include <elf.h>
#include <fs.h>
#include <video.h>

/** Multiboot header structure. */
typedef struct multiboot_header {
    uint32_t magic;                             /**< Magic number. */
    uint32_t flags;                             /**< Feature flags. */
    uint32_t checksum;                          /**< Header checksum. */
    uint32_t header_addr;                       /**< Physical address at which to load header. */
    uint32_t load_addr;                         /**< Physical address of text segment. */
    uint32_t load_end_addr;                     /**< Physical address of end of text segment. */
    uint32_t bss_end_addr;                      /**< Physical address of end of BSS segment. */
    uint32_t entry_addr;                        /**< Entry point address. */
    uint32_t mode_type;                         /**< Preferred graphics mode. */
    uint32_t width;                             /**< Framebuffer width or number of columns. */
    uint32_t height;                            /**< Framebuffer height or number of rows. */
    uint32_t depth;                             /**< Framebuffer bits per pixel. */
} multiboot_header_t;

/** Multiboot information structure. */
typedef struct multiboot_info {
    uint32_t flags;                             /**< Flags. */
    uint32_t mem_lower;                         /**< Bytes of lower memory. */
    uint32_t mem_upper;                         /**< Bytes of upper memory. */
    uint32_t boot_device;                       /**< Boot device. */
    uint32_t cmdline;                           /**< Address of kernel command line. */
    uint32_t mods_count;                        /**< Module count. */
    uint32_t mods_addr;                         /**< Address of module structures. */

    union {
        /** a.out symbol information. */
        struct {
            uint32_t tabsize;                   /**< Symbol table size. */
            uint32_t strsize;                   /**< String table size. */
            uint32_t addr;                      /**< Physical address of symbol table. */
            uint32_t reserved;
        } aout;

        /** ELF section information. */
        struct {
            uint32_t num;                       /**< Number of section headers. */
            uint32_t size;                      /**< Size of a section table entry. */
            uint32_t addr;                      /**< Physical address of section header table. */
            uint32_t shndx;                     /**< Index of shstrtab section. */
        } elf;
    };

    uint32_t mmap_length;                       /**< Memory map length. */
    uint32_t mmap_addr;                         /**< Address of memory map. */
    uint32_t drives_length;                     /**< Drive information length. */
    uint32_t drives_addr;                       /**< Address of drive information. */
    uint32_t config_table;                      /**< Configuration table. */
    uint32_t boot_loader_name;                  /**< Boot loader name. */
    uint32_t apm_table;                         /**< APM table. */
    uint32_t vbe_control_info;                  /**< VBE control information. */
    uint32_t vbe_mode_info;                     /**< VBE mode information. */
    uint16_t vbe_mode;                          /**< VBE mode. */
    uint16_t vbe_interface_seg;                 /**< VBE interface segment. */
    uint16_t vbe_interface_off;                 /**< VBE interface offset. */
    uint16_t vbe_interface_len;                 /**< VBE interface length. */
} __packed multiboot_info_t;

/** Multiboot module information structure. */
typedef struct multiboot_module_info {
    uint32_t mod_start;                         /**< Module start address. */
    uint32_t mod_end;                           /**< Module end address. */
    uint32_t cmdline;                           /**< Module command line. */
    uint32_t _pad;
} __packed multiboot_module_info_t;

/** Multiboot memory map entry. */
typedef struct multiboot_mmap_entry {
    uint32_t size;                              /**< Size of the entry. */
    uint64_t addr;                              /**< Base address. */
    uint64_t len;                               /**< Length of the range. */
    uint32_t type;                              /**< Type of the range. */
} __packed multiboot_mmap_entry_t;

/** Multiboot/E820 memory types. */
#define MULTIBOOT_MMAP_FREE             1       /**< Usable memory. */
#define MULTIBOOT_MMAP_RESERVED         2       /**< Reserved memory. */
#define MULTIBOOT_MMAP_ACPI_RECLAIM     3       /**< ACPI reclaimable. */
#define MULTIBOOT_MMAP_ACPI_NVS         4       /**< ACPI NVS. */
#define MULTIBOOT_MMAP_BAD              5       /**< Bad memory. New in ACPI 3.0. */
#define MULTIBOOT_MMAP_DISABLED         6       /**< Address range is disabled. New in ACPI 4.0. */

/**
 * Multiboot kernel loader definitions.
 */

/** Multiboot ELF type definitions. */
typedef Elf32_Ehdr multiboot_elf_ehdr_t;
typedef Elf32_Phdr multiboot_elf_phdr_t;
typedef Elf32_Shdr multiboot_elf_shdr_t;
typedef Elf32_Addr multiboot_elf_addr_t;

/** Multiboot loader internal data. */
typedef struct multiboot_loader {
    char *path;                             /**< Path to kernel image. */
    value_t args;                           /**< Arguments to kernel image. */
    fs_handle_t *handle;                    /**< Handle to kernel image. */
    multiboot_header_t header;              /**< Kernel image header. */
    size_t header_offset;                   /**< File offset at which header was found. */
    list_t modules;                         /**< List of modules to load. */
    size_t num_modules;                     /**< Number of modules. */
    multiboot_elf_ehdr_t ehdr;              /**< ELF header. */
    uint32_t entry;                         /**< Entry point address. */
    phys_ptr_t kernel_end;                  /**< End of kernel image. */
    void *info_base;                        /**< Information area base address. */
    size_t info_offset;                     /**< Current information area offset. */
    multiboot_info_t *info;                 /**< Main information structure. */
    video_mode_t *mode;                     /**< Video mode set for the OS, if any. */
} multiboot_loader_t;

/** Details of a module to load. */
typedef struct multiboot_module {
    list_t header;                          /**< Link to modules list. */

    fs_handle_t *handle;                    /**< Handle to module. */
    char *path;                             /**< Path to module. */
    char *basename;                         /**< Base name for display in UI. */
    value_t args;                           /**< Arguments to module. */
} multiboot_module_t;

extern void *multiboot_alloc_info(multiboot_loader_t *loader, size_t size, uint32_t *_phys);

extern void multiboot_platform_load(multiboot_loader_t *loader);

extern void multiboot_loader_enter(uint32_t entry, uint32_t info) __noreturn;

#endif /* __ASM__ */
#endif /* __X86_MULTIBOOT_H */
