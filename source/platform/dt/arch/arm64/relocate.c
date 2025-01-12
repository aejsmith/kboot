/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 relocation function.
 */

#include <elf.h>

/** Relocate the loader.
 * @param load_base     Load base address.
 * @param rela_start    Start of RELA section.
 * @param rela_end      End of RELA section.
 * @return              0 on success, 1 on failure. */
int dt_arch_relocate(ptr_t load_base, elf_rela_t *rela_start, elf_rela_t *rela_end) {
    elf_rela_t *reloc;
    elf_addr_t *addr;

    if (((ptr_t)rela_end - (ptr_t)rela_start) % sizeof(elf_rela_t))
        return 1;

    for (reloc = rela_start; reloc < rela_end; reloc++) {
        addr = (elf_addr_t *)(load_base + reloc->r_offset);

        switch (ELF64_R_TYPE(reloc->r_info)) {
        case ELF_R_AARCH64_NONE:
            break;
        case ELF_R_AARCH64_RELATIVE:
            *addr = (elf_addr_t)load_base + reloc->r_addend;
            break;
        default:
            return 1;
        }
    }

    return 0;
}
