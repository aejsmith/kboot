/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               IA32 relocation function.
 */

#include <efi/efi.h>

/** Relocate the loader.
 * @param load_base     Load base address.
 * @param dyn           Pointer to dynamic section.
 * @return              Status code describing result of the operation. */
efi_status_t efi_arch_relocate(ptr_t load_base, elf_dyn_t *dyn) {
    elf_rel_t *reloc = NULL;
    elf_addr_t *addr;
    size_t size = 0, ent = 0, i;

    for (i = 0; dyn[i].d_tag != ELF_DT_NULL; i++) {
        switch (dyn[i].d_tag) {
        case ELF_DT_REL:
            reloc = (elf_rel_t *)(dyn[i].d_un.d_ptr + load_base);
            break;
        case ELF_DT_RELSZ:
            size = dyn[i].d_un.d_val;
            break;
        case ELF_DT_RELENT:
            ent = dyn[i].d_un.d_val;
            break;
        }
    }

    if (!reloc || !ent)
        return EFI_LOAD_ERROR;

    for (i = 0; i < size / ent; i++, reloc = (elf_rel_t *)((ptr_t)reloc + ent)) {
        addr = (elf_addr_t *)(load_base + reloc->r_offset);

        switch (ELF32_R_TYPE(reloc->r_info)) {
        case ELF_R_386_NONE:
            break;
        case ELF_R_386_RELATIVE:
            *addr += (elf_addr_t)load_base;
            break;
        default:
            return EFI_LOAD_ERROR;
        }
    }

    return EFI_SUCCESS;
}
