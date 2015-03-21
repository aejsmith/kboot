/*
 * Copyright (C) 2010-2015 Alex Smith
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
 * @brief               KBoot ELF loading functions.
 */

#ifdef KBOOT_LOAD_ELF64
#   define kboot_elf_ehdr_t Elf64_Ehdr
#   define kboot_elf_phdr_t Elf64_Phdr
#   define kboot_elf_shdr_t Elf64_Shdr
#   define kboot_elf_addr_t Elf64_Addr
#   define ELF_BITS         64
#   define FUNC(name)       kboot_elf64_##name
#else
#   define kboot_elf_ehdr_t Elf32_Ehdr
#   define kboot_elf_phdr_t Elf32_Phdr
#   define kboot_elf_shdr_t Elf32_Shdr
#   define kboot_elf_addr_t Elf32_Addr
#   define ELF_BITS         32
#   define FUNC(name)       kboot_elf32_##name
#endif

/** Read in program headers. */
static status_t FUNC(identify)(kboot_loader_t *loader) {
    kboot_elf_ehdr_t *ehdr = loader->ehdr;
    size_t size;
    status_t ret;

    if (ehdr->e_phentsize != sizeof(kboot_elf_phdr_t))
        return STATUS_MALFORMED_IMAGE;

    size = ehdr->e_phnum * ehdr->e_phentsize;
    loader->phdrs = malloc(size);

    ret = fs_read(loader->handle, loader->phdrs, size, ehdr->e_phoff);
    if (ret != STATUS_SUCCESS) {
        free(loader->phdrs);
        free(loader->ehdr);
    }

    return ret;
}

/** Iterate over note sections in an ELF file. */
static status_t FUNC(iterate_notes)(kboot_loader_t *loader, kboot_note_cb_t cb) {
    kboot_elf_ehdr_t *ehdr = loader->ehdr;
    kboot_elf_phdr_t *phdrs = loader->phdrs;

    for (size_t i = 0; i < ehdr->e_phnum; i++) {
        char *buf __cleanup_free = NULL;
        size_t offset;
        status_t ret;

        if (phdrs[i].p_type != ELF_PT_NOTE)
            continue;

        buf = malloc(phdrs[i].p_filesz);

        ret = fs_read(loader->handle, buf, phdrs[i].p_filesz, phdrs[i].p_offset);
        if (ret != STATUS_SUCCESS)
            return ret;

        offset = 0;
        while (offset < phdrs[i].p_filesz) {
            elf_note_t *note;
            const char *name;
            void *desc;

            note = (elf_note_t *)(buf + offset);
            offset += sizeof(elf_note_t);
            if (offset >= phdrs[i].p_filesz)
                return STATUS_MALFORMED_IMAGE;

            name = (const char *)(buf + offset);
            offset += round_up(note->n_namesz, 4);
            if (offset > phdrs[i].p_filesz)
                return STATUS_MALFORMED_IMAGE;

            desc = buf + offset;
            offset += round_up(note->n_descsz, 4);
            if (offset > phdrs[i].p_filesz)
                return STATUS_MALFORMED_IMAGE;

            if (strcmp(name, KBOOT_NOTE_NAME) == 0) {
                if (!cb(loader, note, desc))
                    return STATUS_SUCCESS;
            }
        }
    }

    return STATUS_SUCCESS;
}

/** Load the kernel image. */
static void FUNC(load_kernel)(kboot_loader_t *loader) {
    kboot_elf_ehdr_t *ehdr = loader->ehdr;
    kboot_elf_phdr_t *phdrs = loader->phdrs;
    kboot_elf_addr_t virt_base, virt_end;
    void *load_base = NULL;

    /* Unless the kernel has a fixed load address, we allocate a single block of
     * physical memory to load at. This means that the offsets between segments
     * are the same in both the physical and virtual address spaces. */
    virt_base = virt_end = 0;
    if (!(loader->load->flags & KBOOT_LOAD_FIXED)) {
        /* Calculate the total load size of the kernel. */
        for (size_t i = 0; i < ehdr->e_phnum; i++) {
            if (phdrs[i].p_type == ELF_PT_LOAD) {
                if (!virt_base || virt_base > phdrs[i].p_vaddr)
                    virt_base = phdrs[i].p_vaddr;
                if (virt_end < phdrs[i].p_vaddr + phdrs[i].p_memsz)
                    virt_end = phdrs[i].p_vaddr + phdrs[i].p_memsz;
            }
        }

        load_base = allocate_kernel(loader, virt_base, virt_end);
    }

    /* Load in the image data. */
    for (size_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == ELF_PT_LOAD) {
            void *dest;
            status_t ret;

            /* If loading at a fixed location, we have to allocate space. */
            if (loader->load->flags & KBOOT_LOAD_FIXED) {
                dest = allocate_segment(loader, phdrs[i].p_vaddr, phdrs[i].p_paddr, phdrs[i].p_memsz, i);
            } else {
                dest = load_base + (phdrs[i].p_vaddr - virt_base);
            }

            ret = fs_read(loader->handle, dest, phdrs[i].p_filesz, phdrs[i].p_offset);
            if (ret != STATUS_SUCCESS)
                boot_error("Error reading kernel image: %pS", ret);

            /* Clear zero-initialized sections. */
            memset(dest + phdrs[i].p_filesz, 0, phdrs[i].p_memsz - phdrs[i].p_filesz);
        }
    }

    loader->entry = ehdr->e_entry;
}

/** Load additional ELF sections. */
static void FUNC(load_sections)(kboot_loader_t *loader) {
    kboot_elf_ehdr_t *ehdr = loader->ehdr;
    kboot_tag_sections_t *tag;
    size_t size;
    status_t ret;

    size = ehdr->e_shnum * ehdr->e_shentsize;

    tag = kboot_alloc_tag(loader, KBOOT_TAG_SECTIONS, sizeof(*tag) + size);
    tag->num = ehdr->e_shnum;
    tag->entsize = ehdr->e_shentsize;
    tag->shstrndx = ehdr->e_shstrndx;

    /* Read the section headers into the tag. */
    ret = fs_read(loader->handle, tag->sections, size, ehdr->e_shoff);
    if (ret != STATUS_SUCCESS)
        boot_error("Error reading kernel sections: %pS", ret);

    /* Iterate through the headers and load in additional loadable sections. */
    for (size_t i = 0; i < ehdr->e_shnum; i++) {
        kboot_elf_shdr_t *shdr = (kboot_elf_shdr_t *)&tag->sections[i * ehdr->e_shentsize];
        phys_ptr_t phys;
        void *dest;

        if (shdr->sh_flags & ELF_SHF_ALLOC || shdr->sh_addr || !shdr->sh_size)
            continue;

        switch (shdr->sh_type) {
        case ELF_SHT_PROGBITS:
        case ELF_SHT_NOBITS:
        case ELF_SHT_SYMTAB:
        case ELF_SHT_STRTAB:
            break;
        default:
            continue;
        }

        /* Allocate memory to load the section data to. */
        size = round_up(shdr->sh_size, PAGE_SIZE);
        dest = memory_alloc(size, 0, 0, 0, MEMORY_TYPE_ALLOCATED, MEMORY_ALLOC_HIGH, &phys);
        shdr->sh_addr = phys;

        dprintf("kboot: loading ELF section %zu to 0x%" PRIxPHYS " (size: %zu)\n", i, phys, (size_t)shdr->sh_size);

        /* Load in the section data. */
        if (shdr->sh_type == ELF_SHT_NOBITS) {
            memset(dest, 0, shdr->sh_size);
        } else {
            ret = fs_read(loader->handle, dest, shdr->sh_size, shdr->sh_offset);
            if (ret != STATUS_SUCCESS)
                boot_error("Error reading kernel sections: %pS", ret);
        }
    }
}

#undef kboot_elf_ehdr_t
#undef kboot_elf_phdr_t
#undef kboot_elf_shdr_t
#undef kboot_elf_addr_t
#undef ELF_BITS
#undef FUNC
