/*
 * Copyright (C) 2012 Alex Smith
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
 * @brief               x86 Linux boot protocol definitions.
 */

#ifndef __X86_LINUX_H
#define __X86_LINUX_H

#include <loader/linux.h>

/** Where to load a bzImage kernel to. */
#define LINUX_BZIMAGE_ADDR          0x100000

/** Linux kernel image header. */
typedef struct linux_header {
    uint8_t  setup_sects;
    uint16_t root_flags;
    uint32_t syssize;
    uint16_t ram_size;
    uint16_t vid_mode;
    uint16_t root_dev;
    uint16_t boot_flag;
    uint8_t  jump;
    uint8_t  relative_end;
    uint32_t header;
    uint16_t version;
    uint32_t realmode_swtch;
    uint16_t start_sys;
    uint16_t kernel_version;
    uint8_t  type_of_loader;
    uint8_t  loadflags;
    uint16_t setup_move_size;
    uint32_t code32_start;
    uint32_t ramdisk_image;
    uint32_t ramdisk_size;
    uint32_t bootsect_kludge;
    uint16_t heap_end_ptr;
    uint8_t  ext_loader_ver;
    uint8_t  ext_loader_type;
    uint32_t cmd_line_ptr;
    uint32_t initrd_addr_max;
    uint32_t kernel_alignment;
    uint8_t  relocatable_kernel;
    uint8_t  min_alignment;
    uint16_t xloadflags;
    uint32_t cmdline_size;
    uint32_t hardware_subarch;
    uint64_t hardware_subarch_data;
    uint32_t payload_offset;
    uint32_t payload_length;
    uint64_t setup_data;
    uint64_t pref_address;
    uint32_t init_size;
    uint32_t handover_offset;
} __packed linux_header_t;

/** Offset of the header in the kernel image. */
#define LINUX_HEADER_OFFSET                 0x1f1

/** Linux magic signature ("HdrS"). */
#define LINUX_MAGIC_SIGNATURE               0x53726448

/** Bits in linux_header_t::loadflags. */
#define LINUX_LOAD_LOADED_HIGH              (1<<0)
#define LINUX_LOAD_QUIET                    (1<<5)
#define LINUX_LOAD_KEEP_SEGMENTS            (1<<6)
#define LINUX_LOAD_CAN_USE_HEAP             (1<<7)

/** Bits in linux_header_t::xloadflags. */
#define LINUX_XLOAD_KERNEL_64               (1<<0)
#define LINUX_XLOAD_CAN_BE_LOADED_ABOVE_4G  (1<<1)
#define LINUX_XLOAD_EFI_HANDOVER_32         (1<<2)
#define LINUX_XLOAD_EFI_HANDOVER_64         (1<<3)
#define LINUX_XLOAD_EFI_KEXEC               (1<<4)

/** Boot parameters structure (so-called "zero page"). */
typedef struct linux_params {
    struct {
        uint8_t  orig_x;
        uint8_t  orig_y;
        uint16_t ext_mem_k;
        uint16_t orig_video_page;
        uint8_t  orig_video_mode;
        uint8_t  orig_video_cols;
        uint8_t  flags;
        uint8_t  unused2;
        uint16_t orig_video_ega_bx;
        uint16_t unused3;
        uint8_t  orig_video_lines;
        uint8_t  orig_video_isVGA;
        uint16_t orig_video_points;
        uint16_t lfb_width;
        uint16_t lfb_height;
        uint16_t lfb_depth;
        uint32_t lfb_base;
        uint32_t lfb_size;
        uint16_t cl_magic;
        uint16_t cl_offset;
        uint16_t lfb_linelength;
        uint8_t  red_size;
        uint8_t  red_pos;
        uint8_t  green_size;
        uint8_t  green_pos;
        uint8_t  blue_size;
        uint8_t  blue_pos;
        uint8_t  rsvd_size;
        uint8_t  rsvd_pos;
        uint16_t vesapm_seg;
        uint16_t vesapm_off;
        uint16_t pages;
        uint16_t vesa_attributes;
        uint32_t capabilities;
        uint8_t  _reserved[6];
    } __packed screen_info;

    struct {
        uint16_t version;
        uint16_t cseg;
        uint32_t offset;
        uint16_t cseg_16;
        uint16_t dseg;
        uint16_t flags;
        uint16_t cseg_len;
        uint16_t cseg_16_len;
        uint16_t dseg_len;
    } __packed apm_bios_info;

    uint8_t  _pad2[4];
    uint64_t tboot_addr;

    struct {
        uint32_t signature;
        uint32_t command;
        uint32_t event;
        uint32_t perf_level;
    } __packed ist_info;

    uint8_t  _pad3[16];
    uint8_t  hd0_info[16];
    uint8_t  hd1_info[16];

    struct {
        uint16_t length;
        uint8_t  table[14];
    } __packed sys_desc_table;

    struct {
        uint32_t ofw_magic;
        uint32_t ofw_version;
        uint32_t cif_handler;
        uint32_t irq_desc_table;
    } __packed olpc_ofw_header;

    uint32_t ext_ramdisk_image;
    uint32_t ext_ramdisk_size;
    uint32_t ext_cmd_line_ptr;
    uint8_t  _pad4[116];
    uint8_t  edid_info[128];

    struct {
        uint32_t efi_loader_signature;
        uint32_t efi_systab;
        uint32_t efi_memdesc_size;
        uint32_t efi_memdesc_version;
        uint32_t efi_memmap;
        uint32_t efi_memmap_size;
        uint32_t efi_systab_hi;
        uint32_t efi_memmap_hi;
    } __packed efi_info;

    uint32_t alt_mem_k;
    uint32_t scratch;
    uint8_t  e820_entries;
    uint8_t  eddbuf_entries;
    uint8_t  edd_mbr_sig_buf_entries;
    uint8_t  kbd_status;
    uint8_t  _pad5[3];
    uint8_t  sentinel;
    uint8_t  _pad6;
    linux_header_t hdr;
    uint8_t  _pad7[0x290 - 0x1f1 - sizeof(linux_header_t)];
    uint32_t edd_mbr_sig_buffer[16];

    struct {
        uint64_t addr;
        uint64_t size;
        uint32_t type;
    } __packed e820_map[128];

    uint8_t  _pad8[0x1000 - 0xcd0];
} __packed linux_params_t;

/** Values of the orig_video_isVGA field. */
#define LINUX_VIDEO_TYPE_VGA        0x1
#define LINUX_VIDEO_TYPE_VESA       0x23

extern void linux_platform_load(linux_loader_t *loader, linux_params_t *params) __noreturn;

#endif /* __X86_LINUX_H */
