/*
 * Copyright (C) 2010-2014 Alex Smith
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
 * @brief		Multiboot header.
 */

#ifndef __X86_MULTIBOOT_H
#define __X86_MULTIBOOT_H

/** Magic value passed by the bootloader. */
#define MULTIBOOT_LOADER_MAGIC		0x2badb002

/** Magic value in the Multiboot header. */
#define MULTIBOOT_HEADER_MAGIC		0x1badb002

/** Flags for the Multiboot header. */
#define MULTIBOOT_HEADER_MODALIGN	(1<<0)	/**< Align loaded modules on page boundaries. */
#define MULTIBOOT_HEADER_MEMINFO	(1<<1)	/**< Kernel wants a memory map. */
#define MULTIBOOT_HEADER_KLUDGE		(1<<16)	/**< Use a.out kludge. */

/** Flags passed by the bootloader. */
#define	MULTIBOOT_INFO_MEMINFO		(1<<0)	/**< Bootloader provided memory info. */
#define MULTIBOOT_INFO_BOOTDEV		(1<<1)	/**< Bootloader provided boot device. */
#define MULTIBOOT_INFO_CMDLINE		(1<<2)	/**< Bootloader provided command line. */
#define MULTIBOOT_INFO_MODULES		(1<<3)	/**< Bootloader provided module info. */
#define MULTIBOOT_INFO_AOUTSYMS		(1<<4)	/**< Bootloader provided a.out symbols. */
#define MULTIBOOT_INFO_ELFSYMS		(1<<5)	/**< Bootloader provided ELF symbols. */
#define MULTIBOOT_INFO_MMAP		(1<<6)	/**< Bootloader provided memory map. */
#define MULTIBOOT_INFO_DRIVES		(1<<7)	/**< Bootloader provided drive information. */
#define MULTIBOOT_INFO_CONFTABLE	(1<<8)	/**< Bootloader provided config table. */
#define MULTIBOOT_INFO_LDRNAME		(1<<9)	/**< Bootloader provided its name. */
#define MULTIBOOT_INFO_APMTABLE		(1<<10)	/**< Bootloader provided APM table. */
#define MULTIBOOT_INFO_VBEINFO		(1<<11)	/**< Bootloader provided VBE info. */

/** Size of the Multiboot structures. */
#define MULTIBOOT_INFO_SIZE		88
#define MULTIBOOT_MODULE_SIZE		16

/** Offsets into the info structure required in assembly code. */
#define MULTIBOOT_INFO_OFF_BOOT_DEVICE	12	/**< Offset of the boot device field. */
#define MULTIBOOT_INFO_OFF_CMDLINE	16	/**< Offset of the command line field. */
#define MULTIBOOT_INFO_OFF_MODS_COUNT	20	/**< Offset of the module count field. */
#define MULTIBOOT_INFO_OFF_MODS_ADDR	24	/**< Offset of the module address field. */

/** Offsets into the module structure required in assembly code. */
#define MULTIBOOT_MODULE_OFF_MOD_START	0	/**< Offset of the module start field. */
#define MULTIBOOT_MODULE_OFF_MOD_END	4	/**< Offset of the module end field. */
#define MULTIBOOT_MODULE_OFF_CMDLINE	8	/**< Offset of the command line field. */

#ifndef __ASM__

#include <types.h>

/** Multiboot information structure. */
typedef struct multiboot_info {
	uint32_t flags;				/**< Flags. */
	uint32_t mem_lower;			/**< Bytes of lower memory. */
	uint32_t mem_upper;			/**< Bytes of upper memory. */
	uint32_t boot_device;			/**< Boot device. */
	uint32_t cmdline;			/**< Address of kernel command line. */
	uint32_t mods_count;			/**< Module count. */
	uint32_t mods_addr;			/**< Address of module structures. */
	uint32_t elf_sec[4];			/**< ELF section headers. */
	uint32_t mmap_length;			/**< Memory map length. */
	uint32_t mmap_addr;			/**< Address of memory map. */
	uint32_t drives_length;			/**< Drive information length. */
	uint32_t drives_addr;			/**< Address of drive information. */
	uint32_t config_table;			/**< Configuration table. */
	uint32_t boot_loader_name;		/**< Boot loader name. */
	uint32_t apm_table;			/**< APM table. */
	uint32_t vbe_control_info;		/**< VBE control information. */
	uint32_t vbe_mode_info;			/**< VBE mode information. */
	uint16_t vbe_mode;			/**< VBE mode. */
	uint16_t vbe_interface_seg;		/**< VBE interface segment. */
	uint16_t vbe_interface_off;		/**< VBE interface offset. */
	uint16_t vbe_interface_len;		/**< VBE interface length. */
} __packed multiboot_info_t;

/** Multiboot module information structure. */
typedef struct multiboot_module {
	uint32_t mod_start;			/**< Module start address. */
	uint32_t mod_end;			/**< Module end address. */
	uint32_t cmdline;			/**< Module command line. */
	uint32_t _pad;
} __packed multiboot_module_t;

#endif /* __ASM__ */
#endif /* __X86_MULTIBOOT_H */
