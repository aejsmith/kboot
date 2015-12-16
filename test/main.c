/*
 * Copyright (C) 2011-2015 Alex Smith
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
 * @brief               KBoot test kernel.
 */

#include <test.h>

KBOOT_IMAGE(KBOOT_IMAGE_SECTIONS | KBOOT_IMAGE_LOG);
KBOOT_BOOLEAN_OPTION("bool_option", "Boolean option", true);
KBOOT_STRING_OPTION("string_option", "String option", "Default Value");

/** Dump a core tag. */
static void dump_core_tag(kboot_tag_core_t *tag) {
    printf("KBOOT_TAG_CORE:\n");
    printf("  tags_phys   = 0x%" PRIx64 "\n", tag->tags_phys);
    printf("  tags_size   = %" PRIu32 "\n", tag->tags_size);
    printf("  kernel_phys = 0x%" PRIx64 "\n", tag->kernel_phys);
    printf("  stack_base  = 0x%" PRIx64 "\n", tag->stack_base);
    printf("  stack_phys  = 0x%" PRIx64 "\n", tag->stack_phys);
    printf("  stack_size  = %" PRIu32 "\n", tag->stack_size);
}

/** Dump an option tag. */
static void dump_option_tag(kboot_tag_option_t *tag) {
    const char *name;
    void *value;

    printf("KBOOT_TAG_OPTION:\n");
    printf("  type       = %" PRIu8 "\n", tag->type);
    printf("  name_size  = %" PRIu32 "\n", tag->name_size);
    printf("  value_size = %" PRIu32 "\n", tag->value_size);

    name = (const char *)round_up((ptr_t)tag + sizeof(kboot_tag_option_t), 8);
    printf("  name       = `%s'\n", name);

    value = (void *)round_up((ptr_t)name + tag->name_size, 8);
    switch (tag->type) {
    case KBOOT_OPTION_BOOLEAN:
        printf("  value      = boolean: %d\n", *(bool *)value);
        break;
    case KBOOT_OPTION_STRING:
        printf("  value      = string: `%s'\n", (const char *)value);
        break;
    case KBOOT_OPTION_INTEGER:
        printf("  value      = integer: %" PRIu64 "\n", *(uint64_t *)value);
        break;
    default:
        printf("  <unknown type>\n");
        break;
    }
}

/** Get a memory range tag type. */
static const char *memory_tag_type(uint32_t type) {
    switch (type) {
    case KBOOT_MEMORY_FREE:
        return "Free";
    case KBOOT_MEMORY_ALLOCATED:
        return "Allocated";
    case KBOOT_MEMORY_RECLAIMABLE:
        return "Reclaimable";
    case KBOOT_MEMORY_PAGETABLES:
        return "Pagetables";
    case KBOOT_MEMORY_STACK:
        return "Stack";
    case KBOOT_MEMORY_MODULES:
        return "Modules";
    default:
        return "???";
    }
}

/** Dump a memory tag. */
static void dump_memory_tag(kboot_tag_memory_t *tag) {
    printf("KBOOT_TAG_MEMORY:\n");
    printf("  start = 0x%" PRIx64 "\n", tag->start);
    printf("  size  = 0x%" PRIx64 "\n", tag->size);
    printf("  end   = 0x%" PRIx64 "\n", tag->start + tag->size);
    printf("  type  = %u (%s)\n", tag->type, memory_tag_type(tag->type));
}

/** Dump a virtual memory tag. */
static void dump_vmem_tag(kboot_tag_vmem_t *tag) {
    printf("KBOOT_TAG_VMEM:\n");
    printf("  start = 0x%" PRIx64 "\n", tag->start);
    printf("  size  = 0x%" PRIx64 "\n", tag->size);
    printf("  end   = 0x%" PRIx64 "\n", tag->start + tag->size);
    printf("  phys  = 0x%" PRIx64 "\n", tag->phys);
}

/** Dump a pagetables tag. */
static void dump_pagetables_tag(kboot_tag_pagetables_t *tag) {
    printf("KBOOT_TAG_PAGETABLES:\n");
    #ifdef __x86_64__
        printf("  pml4    = 0x%" PRIx64 "\n", tag->pml4);
        printf("  mapping = 0x%" PRIx64 "\n", tag->mapping);
    #elif defined(__i386__)
        printf("  page_dir = 0x%" PRIx64 "\n", tag->page_dir);
        printf("  mapping  = 0x%" PRIx64 "\n", tag->mapping);
    #elif defined(__arm__)
        printf("  l1      = 0x%" PRIx64 "\n", tag->l1);
        printf("  mapping = 0x%" PRIx64 "\n", tag->mapping);
    #endif
}

/** Dump a module tag. */
static void dump_module_tag(kboot_tag_module_t *tag) {
    const char *name;

    printf("KBOOT_TAG_MODULE:\n");
    printf("  addr      = 0x%" PRIx64 "\n", tag->addr);
    printf("  size      = %" PRIu32 "\n", tag->size);
    printf("  name_size = %" PRIu32 "\n", tag->name_size);

    name = (const char *)round_up((ptr_t)tag + sizeof(kboot_tag_module_t), 8);
    printf("  name      = `%s'\n", name);
}

/** Dump a video tag. */
static void dump_video_tag(kboot_tag_video_t *tag) {
    printf("KBOOT_TAG_VIDEO:\n");

    switch (tag->type) {
    case KBOOT_VIDEO_VGA:
        printf("  type     = %u (KBOOT_VIDEO_VGA)\n", tag->type);
        printf("  cols     = %u\n", tag->vga.cols);
        printf("  lines    = %u\n", tag->vga.lines);
        printf("  x        = %u\n", tag->vga.x);
        printf("  y        = %u\n", tag->vga.y);
        printf("  mem_phys = 0x%" PRIx64 "\n", tag->vga.mem_phys);
        printf("  mem_virt = 0x%" PRIx64 "\n", tag->vga.mem_virt);
        printf("  mem_size = 0x%" PRIx32 "\n", tag->vga.mem_size);
        break;
    case KBOOT_VIDEO_LFB:
        printf("  type       = %u (KBOOT_VIDEO_LFB)\n", tag->type);
        printf("  flags      = 0x%" PRIx32 "\n", tag->lfb.flags);
        if (tag->lfb.flags & KBOOT_LFB_RGB)
            printf("    KBOOT_LFB_RGB\n");
        if (tag->lfb.flags & KBOOT_LFB_INDEXED)
            printf("    KBOOT_LFB_INDEXED\n");
        printf("  width      = %" PRIu32 "\n", tag->lfb.width);
        printf("  height     = %" PRIu32 "\n", tag->lfb.height);
        printf("  bpp        = %" PRIu8 "\n", tag->lfb.bpp);
        printf("  pitch      = %" PRIu32 "\n", tag->lfb.pitch);
        printf("  fb_phys    = 0x%" PRIx64 "\n", tag->lfb.fb_phys);
        printf("  fb_virt    = 0x%" PRIx64 "\n", tag->lfb.fb_virt);
        printf("  fb_size    = 0x%" PRIx32 "\n", tag->lfb.fb_size);

        if (tag->lfb.flags & KBOOT_LFB_RGB) {
            printf("  red_size   = %" PRIu8 "\n", tag->lfb.red_size);
            printf("  red_pos    = %" PRIu8 "\n", tag->lfb.red_pos);
            printf("  green_size = %" PRIu8 "\n", tag->lfb.green_size);
            printf("  green_pos  = %" PRIu8 "\n", tag->lfb.green_pos);
            printf("  blue_size  = %" PRIu8 "\n", tag->lfb.blue_size);
            printf("  blue_pos   = %" PRIu8 "\n", tag->lfb.blue_pos);
        } else if (tag->lfb.flags & KBOOT_LFB_INDEXED) {
            printf("  palette (%" PRIu16 " entries):\n", tag->lfb.palette_size);
            for (uint16_t i = 0; i < tag->lfb.palette_size; i++) {
                printf("    r = %-3u, g = %-3u, b = %-3u\n",
                    tag->lfb.palette[i].red, tag->lfb.palette[i].green,
                    tag->lfb.palette[i].blue);
            }
        }

        break;
    default:
        printf("  type = %u (unknown)\n", tag->type);
        break;
    }
}

/** Dump a boot device tag. */
static void dump_bootdev_tag(kboot_tag_bootdev_t *tag) {
    const char *str;

    printf("KBOOT_TAG_BOOTDEV:\n");

    switch (tag->type) {
    case KBOOT_BOOTDEV_NONE:
        printf("  type = %" PRIu32 " (KBOOT_BOOTDEV_NONE)\n", tag->type);
        break;
    case KBOOT_BOOTDEV_FS:
        printf("  type  = %" PRIu32 " (KBOOT_BOOTDEV_FS)\n", tag->type);
        printf("  flags = 0x%" PRIx32 "\n", tag->fs.flags);
        printf("  uuid  = `%s'\n", tag->fs.uuid);
        break;
    case KBOOT_BOOTDEV_NET:
        printf("  type         = %" PRIu32 " (KBOOT_BOOTDEV_NET)\n", tag->type);
        printf("  flags        = 0x%" PRIx32 "\n", tag->net.flags);

        if (tag->net.flags & KBOOT_NET_IPV6) {
            printf("    KBOOT_NET_IPV6\n");
            printf("  client_ip    = %pI6\n", &tag->net.client_ip.v6);
            printf("  gateway_ip   = %pI6\n", &tag->net.gateway_ip.v6);
            printf("  server_ip    = %pI6\n", &tag->net.server_ip.v6);
        } else {
            printf("  client_ip    = %pI4\n", &tag->net.client_ip.v4);
            printf("  gateway_ip   = %pI4\n", &tag->net.gateway_ip.v4);
            printf("  server_ip    = %pI4\n", &tag->net.server_ip.v4);
        }

        printf("  server_port  = %" PRIu16 "\n", tag->net.server_port);
        printf("  client_mac   = %pM\n", tag->net.client_mac);
        printf("  hw_addr_size = %u\n", tag->net.hw_addr_size);
        printf("  hw_type      = %u\n", tag->net.hw_type);
        break;
    case KBOOT_BOOTDEV_OTHER:
        printf("  type         = %" PRIu32 " (KBOOT_BOOTDEV_OTHER)\n", tag->type);
        printf("  str_size     = %" PRIu32 "\n", tag->other.str_size);

        str = (const char *)round_up((ptr_t)tag + sizeof(*tag), 8);
        printf("  str          = `%s'\n", str);
        break;
    default:
        printf("  type = %" PRIu32 " (unknown)\n", tag->type);
        break;
    }
}

/** Dump a log tag. */
static void dump_log_tag(kboot_tag_log_t *tag) {
    kboot_log_t *log;

    printf("KBOOT_TAG_LOG:\n");
    printf("  log_virt  = 0x%" PRIx64 "\n", tag->log_virt);
    printf("  log_phys  = 0x%" PRIx64 "\n", tag->log_phys);
    printf("  log_size  = %" PRIu32 "\n", tag->log_size);
    printf("  prev_phys = 0x%" PRIx64 "\n", tag->prev_phys);
    printf("  prev_size = %" PRIu32 "\n", tag->prev_size);

    log = (kboot_log_t *)((ptr_t)tag->log_virt);
    printf("  magic     = 0x%" PRIx32 "\n", log->magic);
}

/** Get a section by index.
 * @param tag           Tag to get from.
 * @param index         Index to get. */
static elf_shdr_t *get_elf_section(kboot_tag_sections_t *tag, uint32_t index) {
    return (elf_shdr_t *)&tag->sections[index * tag->entsize];
}

/** Dump a sections tag. */
static void dump_sections_tag(kboot_tag_sections_t *tag) {
    const char *strtab;
    elf_shdr_t *shdr;

    printf("KBOOT_TAG_SECTIONS:\n");
    printf("  num      = %" PRIu32 "\n", tag->num);
    printf("  entsize  = %" PRIu32 "\n", tag->entsize);
    printf("  shstrndx = %" PRIu32 "\n", tag->shstrndx);

    shdr = get_elf_section(tag, tag->shstrndx);
    strtab = phys_map(shdr->sh_addr, round_up(shdr->sh_size, PAGE_SIZE));
    printf("  shstrtab = 0x%lx (%p)\n", shdr->sh_addr, strtab);

    for (uint32_t i = 0; i < tag->num; i++) {
        shdr = get_elf_section(tag, i);

        printf("  section %u (`%s'):\n", i, (shdr->sh_name) ? strtab + shdr->sh_name : "");
        printf("    sh_type  = %" PRIu32 "\n", shdr->sh_type);
        printf("    sh_flags = 0x%" PRIx32 "\n", shdr->sh_flags);
        printf("    sh_addr  = %p\n", shdr->sh_addr);
        printf("    sh_size  = %" PRIu32 "\n", shdr->sh_size);
    }
}

/** E820 memory types. */
static const char *e820_memory_types[] = {
    "???",
    "Free",
    "Reserved",
    "ACPI Reclaimable",
    "ACPI NVS",
    "Bad",
    "Disabled",
};

/** E820 entry structure. */
typedef struct e820_entry {
    uint64_t start;
    uint64_t length;
    uint32_t type;
} __packed e820_entry_t;

/** Dump an E820 tag. */
static void dump_bios_e820_tag(kboot_tag_bios_e820_t *tag) {
    printf("KBOOT_TAG_BIOS_E820:\n");
    printf("  num_entries = %" PRIu32 "\n", tag->num_entries);
    printf("  entry_size  = %" PRIu32 "\n", tag->entry_size);

    for (uint32_t i = 0; i < tag->num_entries; i++) {
        e820_entry_t *entry = (e820_entry_t *)&tag->entries[i * tag->entry_size];
        const char *name;

        name = (entry->type < array_size(e820_memory_types))
            ? e820_memory_types[entry->type]
            : e820_memory_types[0];

        printf("  entry %u:\n", i);
        printf("    start  = 0x%" PRIx64 "\n", entry->start);
        printf("    length = 0x%" PRIx64 "\n", entry->length);
        printf("    type   = %" PRIu32 " (%s)\n", entry->type, name);
    }
}

/** EFI memory type strings.. */
static const char *efi_memory_types[] = {
    "Reserved",
    "Loader Code",
    "Loader Data",
    "Boot Services Code",
    "Boot Services Data",
    "Runtime Services Code",
    "Runtime Services Data",
    "Free",
    "Unusable",
    "ACPI Reclaimable",
    "ACPI NVS",
    "MMIO",
    "MMIO Port Space",
    "PAL Code",
};

/** Memory range descriptor. */
typedef struct efi_memory_descriptor {
    uint32_t type;
    uint32_t _pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t num_pages;
    uint64_t attribute;
} efi_memory_descriptor_t;

/** Dump an EFI tag. */
static void dump_efi_tag(kboot_tag_efi_t *tag) {
    const char *name;

    switch (tag->type) {
    case KBOOT_EFI_64:
        name = "KBOOT_EFI_64";
        break;
    case KBOOT_EFI_32:
        name = "KBOOT_EFI_32";
        break;
    default:
        name = "???";
        break;
    }

    printf("KBOOT_TAG_EFI:\n");
    printf("  type                = %" PRIu8 " (%s)\n", tag->type, name);
    printf("  system_table        = 0x%" PRIx64 "\n", tag->system_table);
    printf("  num_memory_descs    = %" PRIu32 "\n", tag->num_memory_descs);
    printf("  memory_desc_size    = %" PRIu32 "\n", tag->memory_desc_size);
    printf("  memory_desc_version = %" PRIu32 "\n", tag->memory_desc_version);

    for (uint32_t i = 0; i < tag->num_memory_descs; i++) {
        efi_memory_descriptor_t *desc = (efi_memory_descriptor_t *)&tag->memory_map[i * tag->memory_desc_size];

        name = (desc->type < array_size(efi_memory_types)) ? efi_memory_types[desc->type] : "???";

        printf("  descriptor %u:\n", i);
        printf("    type           = %" PRIu32 " (%s)\n", desc->type, name);
        printf("    physical_start = 0x%" PRIx64 "\n", desc->physical_start);
        printf("    virtual_start  = 0x%" PRIx64 "\n", desc->virtual_start);
        printf("    num_pages      = %" PRIu64 " (0x%" PRIx64 ")\n", desc->num_pages, desc->num_pages * 0x1000);
        printf("    attribute      = 0x%" PRIx64 "\n", desc->attribute);
    }
}

/** Entry point of the test kernel.
 * @param magic         KBoot magic number.
 * @param tags          Tag list pointer. */
void kmain(uint32_t magic, kboot_tag_t *tags) {
    debug_console_init();

    if (magic != KBOOT_MAGIC) {
        printf("Incorrect magic number 0x%x\n", magic);
        while (true)
            arch_pause();
    }

    mm_init(tags);
    primary_console_init(tags);

    printf("Test kernel loaded: magic: 0x%x, tags: %p\n", magic, tags);

    while (tags->type != KBOOT_TAG_NONE) {
        switch (tags->type) {
        case KBOOT_TAG_CORE:
            dump_core_tag((kboot_tag_core_t *)tags);
            break;
        case KBOOT_TAG_OPTION:
            dump_option_tag((kboot_tag_option_t *)tags);
            break;
        case KBOOT_TAG_MEMORY:
            dump_memory_tag((kboot_tag_memory_t *)tags);
            break;
        case KBOOT_TAG_VMEM:
            dump_vmem_tag((kboot_tag_vmem_t *)tags);
            break;
        case KBOOT_TAG_PAGETABLES:
            dump_pagetables_tag((kboot_tag_pagetables_t *)tags);
            break;
        case KBOOT_TAG_MODULE:
            dump_module_tag((kboot_tag_module_t *)tags);
            break;
        case KBOOT_TAG_VIDEO:
            dump_video_tag((kboot_tag_video_t *)tags);
            break;
        case KBOOT_TAG_BOOTDEV:
            dump_bootdev_tag((kboot_tag_bootdev_t *)tags);
            break;
        case KBOOT_TAG_LOG:
            dump_log_tag((kboot_tag_log_t *)tags);
            break;
        case KBOOT_TAG_SECTIONS:
            dump_sections_tag((kboot_tag_sections_t *)tags);
            break;
        case KBOOT_TAG_BIOS_E820:
            dump_bios_e820_tag((kboot_tag_bios_e820_t *)tags);
            break;
        case KBOOT_TAG_EFI:
            dump_efi_tag((kboot_tag_efi_t *)tags);
            break;
        }

        tags = (kboot_tag_t *)round_up((ptr_t)tags + tags->size, 8);
    }

    printf("Tag list dump complete\n");

    #if defined(__i386__) || defined(__x86_64__)
        __asm__ volatile("wbinvd");
    #endif

    while (true)
        arch_pause();
}
