/*
 * Copyright (C) 2015 Alex Smith
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
 * @brief               Multiboot kernel loader.
 *
 * Reference:
 *  - Multiboot Specification
 *    https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
 */

#include <arch/page.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <x86/multiboot.h>

#include <loader.h>
#include <memory.h>
#include <ui.h>

/** Allocation parameters for Multiboot information. */
#define INFO_ALLOC_SIZE         PAGE_SIZE
#define INFO_ALLOC_MIN_ADDR     0x10000
#define INFO_ALLOC_MAX_ADDR     0x100000

/** Supported header flags. */
#define SUPPORTED_FLAGS         \
    (MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_MODE | MULTIBOOT_AOUT_KLUDGE)

/** Video mode types to support. */
#define MULTIBOOT_VIDEO_TYPES   (VIDEO_MODE_VGA | VIDEO_MODE_LFB)

/** Allocate from the information area.
 * @param loader        Loader internal data.
 * @param size          Size of information to allocate.
 * @param _phys         Where to store physical address of allocation. */
void *multiboot_alloc_info(multiboot_loader_t *loader, size_t size, uint32_t *_phys) {
    void *ret;

    size = round_up(size, 4);

    if (size > INFO_ALLOC_SIZE - loader->info_offset)
        internal_error("Exceeded maximum information size");

    ret = loader->info_base + loader->info_offset;
    loader->info_offset += size;

    if (_phys)
        *_phys = virt_to_phys((ptr_t)ret);

    return ret;
}

/** Load a Multiboot kernel using the a.out kludge.
 * @param loader        Loader internal data. */
static void load_kernel_kludge(multiboot_loader_t *loader) {
    multiboot_header_t *header = &loader->header;
    uint32_t offset, load_size, bss_size;
    phys_ptr_t alloc_base;
    phys_size_t alloc_size;
    void *dest;
    status_t ret;

    if (header->header_addr < header->load_addr) {
        boot_error("Invalid header address");
    } else if (header->load_end_addr && header->load_end_addr < header->load_addr) {
        boot_error("Invalid load end address");
    } else if (header->bss_end_addr && header->bss_end_addr < header->load_end_addr) {
        boot_error("Invalid BSS end address");
    }

    /* Determine file offset and load size. */
    offset = loader->header_offset - (header->header_addr - header->load_addr);
    if (header->load_end_addr) {
        load_size = header->load_end_addr - header->load_addr;
        if (load_size > loader->handle->size - offset)
            boot_error("Load size is larger than kernel image");
    } else {
        load_size = loader->handle->size - offset;
    }

    /* Get the BSS size. */
    bss_size = (header->bss_end_addr) ? header->bss_end_addr - header->load_addr - load_size : 0;

    dprintf(
        "multiboot: loading a.out kludge kernel (load_addr: 0x%x, load_size: 0x%x, bss_size: 0x%x)\n",
        header->load_addr, load_size, bss_size);

    /* Allocate space for it. Allocator expects page aligned addresses. */
    alloc_base = round_down(header->load_addr, PAGE_SIZE);
    alloc_size = round_up(header->load_addr + load_size + bss_size, PAGE_SIZE) - alloc_base;
    memory_alloc(alloc_size, 0, alloc_base, alloc_base + alloc_size, MEMORY_TYPE_ALLOCATED, 0, NULL);

    /* Read in the kernel image. */
    dest = (void *)phys_to_virt(header->load_addr);
    ret = fs_read(loader->handle, dest, load_size, offset);
    if (ret != STATUS_SUCCESS)
        boot_error("Error reading kernel image: %pS", ret);

    /* Clear the BSS section, if any. */
    if (bss_size)
        memset(dest + load_size, 0, bss_size);

    loader->entry = header->entry_addr;
    loader->kernel_end = alloc_base + alloc_size;
}

/** Load an ELF Multiboot kernel.
 * @param loader        Loader internal data. */
static void load_kernel_elf(multiboot_loader_t *loader) {
    multiboot_elf_phdr_t *phdrs __cleanup_free;
    size_t size;
    status_t ret;

    if (loader->ehdr.e_phentsize != sizeof(*phdrs))
        boot_error("Invalid ELF program header size");

    size = loader->ehdr.e_phnum * loader->ehdr.e_phentsize;
    phdrs = malloc(size);

    ret = fs_read(loader->handle, phdrs, size, loader->ehdr.e_phoff);
    if (ret != STATUS_SUCCESS)
        boot_error("Error reading kernel image: %pS", ret);

    /* Load in the image data. */
    loader->kernel_end = 0;
    for (size_t i = 0; i < loader->ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == ELF_PT_LOAD) {
            phys_ptr_t alloc_base;
            phys_size_t alloc_size;
            void *dest;

            if (!phdrs[i].p_memsz)
                continue;

            dprintf(
                "multiboot: loading ELF segment %zu to 0x%x (filesz: 0x%x, memsz: 0x%x)\n",
                i, phdrs[i].p_paddr, phdrs[i].p_filesz, phdrs[i].p_memsz);

            /* Try to allocate the load address for this segment. */
            alloc_base = round_down(phdrs[i].p_paddr, PAGE_SIZE);
            alloc_size = round_up(phdrs[i].p_paddr + phdrs[i].p_memsz, PAGE_SIZE) - alloc_base;
            memory_alloc(alloc_size, 0, alloc_base, alloc_base + alloc_size, MEMORY_TYPE_ALLOCATED, 0, NULL);
            dest = (void *)phys_to_virt(phdrs[i].p_paddr);

            /* Save highest address. */
            loader->kernel_end = max(loader->kernel_end, alloc_base + alloc_size);

            /* Read it in. */
            if (phdrs[i].p_filesz) {
                ret = fs_read(loader->handle, dest, phdrs[i].p_filesz, phdrs[i].p_offset);
                if (ret != STATUS_SUCCESS)
                    boot_error("Error reading kernel image: %pS", ret);
            }

            /* Clear zero-initialized sections. */
            memset(dest + phdrs[i].p_filesz, 0, phdrs[i].p_memsz - phdrs[i].p_filesz);
        }
    }

    /* Load section headers. */
    if (loader->ehdr.e_shnum) {
        multiboot_elf_shdr_t *shdrs;

        if (loader->ehdr.e_shentsize != sizeof(*shdrs))
            boot_error("Invalid ELF section header size");

        /* Allocate information area space, we pass them to the kernel. */
        size = loader->ehdr.e_shnum * loader->ehdr.e_shentsize;
        shdrs = multiboot_alloc_info(loader, size, &loader->info->elf.addr);
        loader->info->flags |= MULTIBOOT_INFO_ELF_SHDR;
        loader->info->elf.num = loader->ehdr.e_shnum;
        loader->info->elf.size = loader->ehdr.e_shentsize;
        loader->info->elf.shndx = loader->ehdr.e_shstrndx;

        ret = fs_read(loader->handle, shdrs, size, loader->ehdr.e_shoff);
        if (ret != STATUS_SUCCESS)
            boot_error("Error reading kernel image: %pS", ret);

        /* Load in all unloaded sections. */
        for (size_t i = 0; i < loader->ehdr.e_shnum; i++) {
            phys_size_t alloc_size, alloc_align;
            void *dest;
            phys_ptr_t phys;

            if (shdrs[i].sh_addr || !shdrs[i].sh_size)
                continue;

            /* Allocate space. */
            alloc_size = round_up(shdrs[i].sh_size, PAGE_SIZE);
            alloc_align = round_up(shdrs[i].sh_addralign, PAGE_SIZE);
            dest = memory_alloc(
                alloc_size, alloc_align, loader->kernel_end, 0,
                MEMORY_TYPE_ALLOCATED, 0, &phys);

            dprintf(
                "multiboot: loading ELF section %zu to 0x%" PRIxPHYS " (size: 0x%x)\n",
                i, phys, shdrs[i].sh_size);

            if (shdrs[i].sh_type == ELF_SHT_NOBITS) {
                memset(dest, 0, shdrs[i].sh_size);
            } else {
                ret = fs_read(loader->handle, dest, shdrs[i].sh_size, shdrs[i].sh_offset);
                if (ret != STATUS_SUCCESS)
                    boot_error("Error reading kernel image: %pS", ret);
            }

            shdrs[i].sh_addr = phys;
        }
    }

    /* Save entry point address. */
    loader->entry = loader->ehdr.e_entry;
}

/** Join a path and arguments into a command line string.
 * @param loader        Loader internal data.
 * @param path          Path string.
 * @param args          Arguments value.
 * @return              Physical address of string. */
static uint32_t join_cmdline(multiboot_loader_t *loader, char *path, value_t *args) {
    size_t path_len, args_len;
    uint32_t phys;
    char *str;

    path_len = strlen(path);
    args_len = strlen(args->string);
    str = multiboot_alloc_info(loader, path_len + args_len + ((args_len) ? 2 : 1), &phys);

    strcpy(str, path);

    if (args_len) {
        str[path_len] = ' ';
        strcpy(str + path_len + 1, args->string);
    }

    return phys;
}

/** Load a Multiboot kernel.
 * @param _loader       Pointer to loader internal data. */
static __noreturn void multiboot_loader_load(void *_loader) {
    multiboot_loader_t *loader = _loader;
    uint32_t info_phys;
    char *str;
    list_t memory_map;

    /* Allocate the information area. This area is where we allocate all bits
     * of information to pass to the kernel from. The Multiboot specification
     * doesn't say anything about where things should get allocated, so I was
     * really tempted to purposely try to fling things all over the place. I
     * decided to be nice in the end. */
    loader->info_base = memory_alloc(
        INFO_ALLOC_SIZE, 0, INFO_ALLOC_MIN_ADDR, INFO_ALLOC_MAX_ADDR,
        MEMORY_TYPE_RECLAIMABLE, 0, NULL);
    memset(loader->info_base, 0, INFO_ALLOC_SIZE);
    loader->info_offset = 0;

    /* Allocate the main information structure. */
    loader->info = multiboot_alloc_info(loader, sizeof(*loader->info), &info_phys);

    /* We always provide these. */
    loader->info->flags = MULTIBOOT_INFO_CMDLINE | MULTIBOOT_INFO_BOOT_LOADER_NAME;

    /* Set the loader name. */
    str = multiboot_alloc_info(loader, strlen("KBoot") + 1, &loader->info->boot_loader_name);
    strcpy(str, "KBoot");

    /* Set the command line string. */
    loader->info->cmdline = join_cmdline(loader, loader->path, &loader->args);

    /* Load the kernel image. */
    if (loader->header.flags & MULTIBOOT_AOUT_KLUDGE) {
        load_kernel_kludge(loader);
    } else {
        load_kernel_elf(loader);
    }

    if (loader->num_modules) {
        multiboot_module_info_t *modules;
        size_t i = 0;

        modules = multiboot_alloc_info(
            loader, sizeof(*modules) * loader->num_modules,
            &loader->info->mods_addr);
        loader->info->flags |= MULTIBOOT_INFO_MODS;
        loader->info->mods_count = loader->num_modules;

        /* Load each module. */
        list_foreach(&loader->modules, iter) {
            multiboot_module_t *module = list_entry(iter, multiboot_module_t, header);
            size_t size;
            void *dest;
            phys_ptr_t phys;
            status_t ret;

            /* We page-align modules regardless of the page-align header flag,
             * because our allocator works with page alignment. Some kernels
             * break if modules are not placed after the kernel. */
            size = round_up(module->handle->size, PAGE_SIZE);
            dest = memory_alloc(size, 0, loader->kernel_end, 0, MEMORY_TYPE_MODULES, 0, &phys);

            dprintf(
                "multiboot: loading module '%s' to 0x%" PRIxPHYS " (size: %" PRIu64 ")\n",
                module->path, phys, module->handle->size);

            ret = fs_read(module->handle, dest, module->handle->size, 0);
            if (ret != STATUS_SUCCESS)
                boot_error("Error reading '%s': %pS", module->path, ret);

            modules[i].mod_start = phys;
            modules[i].mod_end = phys + size;
            modules[i].cmdline = join_cmdline(loader, module->path, &module->args);

            i++;
        }
    }

    /* Set the video mode. */
    loader->mode = (loader->header.flags & MULTIBOOT_VIDEO_MODE)
        ? video_env_set(current_environ, "video_mode")
        : NULL;

    /* Print out a memory map for informational purposes. */
    dprintf("multiboot: final physical memory map:\n");
    memory_finalize(&memory_map);
    memory_map_dump(&memory_map);

    dprintf("multiboot: kernel entry point at 0x%" PRIx32 ", info at 0x%" PRIx32 "\n", loader->entry, info_phys);

    /* Perform pre-boot tasks. */
    loader_preboot();

    /* Do platform initialization. We pass mostly the same information per-
     * platform, but how we get it differs for BIOS/EFI. We may not be able to
     * output or do I/O after this point, as on EFI this will exit boot
     * services. */
    multiboot_platform_load(loader);

    /* Enter the kernel. */
    multiboot_loader_enter(loader->entry, info_phys);
}

/** Get a configuration window.
 * @param _loader       Pointer to loader internal data.
 * @param title         Title to give the window.
 * @return              Configuration window. */
static ui_window_t *multiboot_loader_configure(void *_loader, const char *title) {
    multiboot_loader_t *loader = _loader;
    ui_window_t *window;
    ui_entry_t *entry;

    window = ui_list_create(title, true);
    entry = ui_entry_create("Command line", &loader->args);
    ui_list_insert(window, entry, false);

    if (loader->header.flags & MULTIBOOT_VIDEO_MODE) {
        entry = video_env_chooser(current_environ, "video_mode", MULTIBOOT_VIDEO_TYPES);
        ui_list_insert(window, entry, false);
    }

    if (!list_empty(&loader->modules)) {
        ui_list_add_section(window, "Modules");

        list_foreach(&loader->modules, iter) {
            multiboot_module_t *module = list_entry(iter, multiboot_module_t, header);

            if (!module->basename)
                module->basename = basename(module->path);

            entry = ui_entry_create(module->basename, &module->args);
            ui_list_insert(window, entry, false);
        }
    }

    return window;
}

/** Multiboot loader operations. */
static loader_ops_t multiboot_loader_ops = {
    .load = multiboot_loader_load,
    .configure = multiboot_loader_configure,
};

/** Search for the Multiboot header in a kernel.
 * @param loader        Loader internal data.
 * @return              Whether header was found. */
static bool find_header(multiboot_loader_t *loader) {
    char *buf __cleanup_free;
    size_t size, required;
    status_t ret;

    /* Header must be contained within the first 8K of the file. */
    size = min(loader->handle->size, (offset_t)MULTIBOOT_SEARCH);
    buf = malloc(size);
    ret = fs_read(loader->handle, buf, size, 0);
    if (ret != STATUS_SUCCESS) {
        config_error("Error reading '%s': %pS", loader->path, ret);
        return false;
    }

    /* Search for the header. */
    required = MULTIBOOT_HEADER_MIN_SIZE;
    for (size_t offset = 0; size - offset >= required; offset += MULTIBOOT_HEADER_ALIGN) {
        multiboot_header_t *header = (multiboot_header_t *)(buf + offset);

        if (header->magic != MULTIBOOT_HEADER_MAGIC) {
            continue;
        } else if (header->magic + header->flags + header->checksum) {
            continue;
        }

        /* Looks like a Multiboot header, ensure the size is sufficient for the
         * flags that are set. */
        if (header->flags & MULTIBOOT_VIDEO_MODE) {
            required = MULTIBOOT_HEADER_VIDEO_SIZE;
        } else if (header->flags & MULTIBOOT_AOUT_KLUDGE) {
            required = MULTIBOOT_HEADER_AOUT_SIZE;
        }

        if (size - offset < required) {
            config_error("'%s' has short header", loader->path);
            return false;
        }

        memcpy(&loader->header, header, required);
        loader->header_offset = offset;
        return true;
    }

    config_error("'%s' is not a Multiboot kernel", loader->path);
    return false;
}

/** Load a Multiboot kernel.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_multiboot(value_list_t *args) {
    multiboot_loader_t *loader;
    status_t ret;

    if (args->count < 1 || args->count > 2 || args->values[0].type != VALUE_TYPE_STRING) {
        config_error("Invalid arguments");
        return false;
    }

    loader = malloc(sizeof(*loader));
    list_init(&loader->modules);
    loader->num_modules = 0;

    loader->args.type = VALUE_TYPE_STRING;
    split_cmdline(args->values[0].string, &loader->path, &loader->args.string);

    ret = fs_open(loader->path, NULL, FILE_TYPE_REGULAR, FS_OPEN_DECOMPRESS, &loader->handle);
    if (ret != STATUS_SUCCESS) {
        config_error("Error opening '%s': %pS", loader->path, ret);
        goto err_free;
    }

    /* Check if it is a valid Multiboot kernel. */
    if (!find_header(loader))
        goto err_close;

    if (loader->header.flags & ~SUPPORTED_FLAGS) {
        config_error(
            "'%s' has unsupported flags 0x%x",
            loader->path, loader->header.flags & ~SUPPORTED_FLAGS);
        goto err_close;
    }

    /* If not using the a.out kludge, check if this is a valid ELF file. */
    if (!(loader->header.flags & MULTIBOOT_AOUT_KLUDGE)) {
        ret = fs_read(loader->handle, &loader->ehdr, sizeof(loader->ehdr), 0);
        if (ret != STATUS_SUCCESS) {
            config_error("Error reading '%s': %pS", loader->path, ret);
            goto err_close;
        } else if (!elf_check(&loader->ehdr, ELFCLASS32, ELFDATA2LSB, ELF_EM_386, 0)) {
            config_error("'%s' is not a valid ELF image", loader->path);
            goto err_close;
        }
    }

    /* Get module information. */
    if (args->count == 2) {
        value_list_t *list = args->values[1].list;

        if (args->values[1].type != VALUE_TYPE_LIST) {
            config_error("Invalid arguments");
            goto err_free;
        }

        for (size_t i = 0; i < list->count; i++) {
            multiboot_module_t *module;

            if (list->values[i].type != VALUE_TYPE_STRING) {
                config_error("Invalid arguments");
                goto err_modules;
            }

            module = malloc(sizeof(*module));
            module->handle = NULL;
            module->basename = NULL;

            module->args.type = VALUE_TYPE_STRING;
            split_cmdline(list->values[i].string, &module->path, &module->args.string);

            list_init(&module->header);
            list_append(&loader->modules, &module->header);

            ret = fs_open(module->path, NULL, FILE_TYPE_REGULAR, FS_OPEN_DECOMPRESS, &module->handle);
            if (ret != STATUS_SUCCESS) {
                config_error("Error opening '%s': %pS", module->path, ret);
                goto err_modules;
            }

            loader->num_modules++;
        }
    }

    /* Set up the video mode environment variable. */
    if (loader->header.flags & MULTIBOOT_VIDEO_MODE) {
        video_mode_t *mode;

        if (loader->header.mode_type == 1) {
            /* Requesting a VGA text mode. */
            mode = video_find_mode(VIDEO_MODE_VGA, loader->header.width, loader->header.height, 0);
        } else {
            /* Requesting a linear framebuffer. */
            mode = video_find_mode(
                VIDEO_MODE_LFB, loader->header.width, loader->header.height,
                loader->header.depth);
        }

        video_env_init(current_environ, "video_mode", MULTIBOOT_VIDEO_TYPES, mode);
    }

    environ_set_loader(current_environ, &multiboot_loader_ops, loader);
    return true;

err_modules:
    while (!list_empty(&loader->modules)) {
        multiboot_module_t *module = list_first(&loader->modules, multiboot_module_t, header);

        list_remove(&module->header);

        if (module->handle)
            fs_close(module->handle);

        value_destroy(&module->args);
        free(module->path);
        free(module);
    }

err_close:
    fs_close(loader->handle);

err_free:
    value_destroy(&loader->args);
    free(loader->path);
    free(loader);
    return false;
}

BUILTIN_COMMAND("multiboot", "Load a Multiboot kernel", config_cmd_multiboot);
