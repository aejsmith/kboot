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
 * @brief               ext2/3/4 boot sector.
 *
 * This code will load the loader binary from an ext2/3/4 filesystem. It is
 * intended to be written to the first 1024 bytes of an ext* disk/partition and
 * used as its boot sector.
 *
 * Since the BIOS/MBR only loads 512 bytes for us, we are split into 2 sections,
 * and we load the second half early on in the main function (along with the FS
 * superblock).
 *
 * The path to the loader binary (null-terminated, separated with single '/'
 * characters, with no leading '/', all redundant elements removed and no
 * symlinks involved) should be written at offset 992, a maximum of 32 bytes
 * including the null terminator.
 *
 * Current limitations:
 *  - Only uses INT13 Extensions. This is also the case with the loader itself,
 *    if anyone feels like adding support for the legacy interface, go ahead.
 *  - Uses 32-bit LBAs, meaning we only support disks up to 2TB. When adding
 *    support for 64-bit LBAs, we should also support hybrid GPT MBRs, such as
 *    the one provided by syslinux. This passes the partition boot sector a GPT
 *    entry in addition to a generated legacy MBR structure.
 *  - Assumes EXT2_DYNAMIC_REV. The loader itself requires this at the moment,
 *    if that changes we should add support for it here too.
 *  - Only supports up to the indirect block. *May* have to support bi-indirect
 *    in the future if we have to deal with very large directories, but we're
 *    getting towards hundreds of thousands of entries at that point...
 */

#include <fs/ext2.h>

#include <partition/mbr.h>

#include <platform/loader.h>

#include <x86/asm.h>

/** Address of the stack. */
#define STACK_OFFSET        0x6000

/** Print a message. */
#define PRINT(s) \
    movw    $s, %si; \
    call    puts

/** Print an error message and halt. */
#define ERROR(s) \
    movw    $s, %si; \
    jmp     error

.section .text
.code16

/** Main function. */
FUNCTION_START(_start)
    /* First things first before we start futzing with the registers: attempt
     * to figure out whether we're booted from a partition and where it is, and
     * store the LBA in ECX. The standard MBR usually passes the partition table
     * entry structure in DS:SI, which we use where possible. However, partition
     * information may not always be passed to us (for instance Apple's BIOS
     * emulation does not provide it), so we also have a LBA baked into the boot
     * sector which we fall back on if the partition information does not look
     * valid. If we are not installed to a partition and being booted directly
     * by the BIOS, the partition information will be invalid and the baked LBA
     * will be 0. Start off by getting the baked LBA so it will be set for all
     * the failure cases below. */
    movl    def_partition_lba, %ecx

    /* First check if the address is within a reasonable range. */
    movw    %ds, %ax
    movw    %si, %di
    shrw    $4, %di
    addw    %di, %ax
    cmpw    $0x50, %ax
    jb      1f
    cmpw    $0x9fc0, %ax
    jae     1f

    /* Active flag should either be 0x80 or 0. */
    testb   $0x7f, %ds:MBR_PARTITION_OFF_BOOTABLE(%si)
    jnz     1f

    /* Type should be non-zero. */
    cmpb    %cl, %ds:MBR_PARTITION_OFF_TYPE(%si)
    je      1f

    /* Looks like we're a partition. Get the LBA. */
    movl    %ds:MBR_PARTITION_OFF_START_LBA(%si), %ecx
1:
    /* Set up the stack and data segment. */
    xorw    %ax, %ax
    movw    %ax, %ss
    movw    $STACK_OFFSET, %sp
    movw    %ax, %ds

    PRINT(msg_loading)

    /* Save drive ID and partition LBA. */
    movb    %dl, drive_id
    movl    %ecx, partition_lba

    /* Read in our second half and the superblock (3 sectors total). */
    movb    $3, %cl
    xorl    %eax, %eax
    movw    %ax, %es
    incw    %ax
    movw    $second_half, %di
    call    read_disk_sectors

    /* Check whether this is a valid ext2 filesystem. */
    cmpw    $EXT2_MAGIC, (superblock + EXT2_SUPERBLOCK_OFF_MAGIC)
    jne     .Lbad_fs

    /* Compute how many disk sectors there are per filesystem block. Although
     * s_log_block_size is a 32-bit field in the superblock, in reality it'll
     * never even be greater than 10, so just treat it as a byte. */
    movb    (superblock + EXT2_SUPERBLOCK_OFF_LOG_BLOCK_SIZE), %cl
    incb    %cl
    movb    %cl, block_to_sector

    /* Compute and save the group table start LBA. */
    movl    (superblock + EXT2_SUPERBLOCK_OFF_FIRST_DATA_BLOCK), %eax
    incl    %eax
    shll    %cl, %eax
    movl    %eax, group_table_offset

    /* Compute and save the block size. Maximum of 32K supported. */
    addb    $9, %cl
    cmpb    $15, %cl
    ja      .Lunsupported
    xorw    %ax, %ax
    incb    %al
    shlw    %cl, %ax
    movw    %ax, block_size

    /* Reject non-EXT2_DYNAMIC_REV filesystems. */
    cmpl    $EXT2_DYNAMIC_REV, (superblock + EXT2_SUPERBLOCK_OFF_REV_LEVEL)
    jne     .Lunsupported

    /* Check for a dynamic group descriptor size, if not put in a default. */
    testb   $EXT4_FEATURE_INCOMPAT_64BIT, (superblock + EXT2_SUPERBLOCK_OFF_FEATURE_INCOMPAT)
    jnz     .Lfind_inode
    movw    $EXT2_MIN_GROUP_DESC_SIZE, (superblock + EXT2_SUPERBLOCK_OFF_DESC_SIZE)

.Lfind_inode:
    /* Find the boot loader inode. */
    call    find_inode

    /* Read in the inode data. */
    call    read_inode_data

    PRINT(msg_ok)

    /* Jump to the boot loader. */
    movb    drive_id, %dl
    movl    partition_lba, %ebx
    xorl    %ecx, %ecx
    movw    $path, %si
    ljmp    $LOADER_LOAD_SEGMENT, $0

.Lbad_fs:
    ERROR(msg_not_ext2)

.Lunsupported:
    ERROR(msg_unsupported)
FUNCTION_END(_start)

/** Read a single sector from the boot partition.
 * @param %eax          Start offset.
 * @param %es:%di       Destination buffer. */
PRIVATE_FUNCTION_START(read_disk_sector)
    /* Set count and fall through. */
    movb    $1, %cl
FUNCTION_END(read_disk_sector)

/** Read sectors from the boot partition.
 * @param %cl           Sector count.
 * @param %eax          Start offset.
 * @param %es:%di       Destination buffer. */
PRIVATE_FUNCTION_START(read_disk_sectors)
    pushal

    /* Get the real offset. */
    addl    partition_lba, %eax

    /* Initialise DAP. */
    movl    %eax, dap_lba_low
    xorl    %eax, %eax
    movl    %eax, dap_lba_high
    movb    $0x10, dap_size
    movl    %eax, dap_reserved
    movb    %cl, dap_count
    movw    %di, dap_buffer_offset
    movw    %es, dap_buffer_segment

    /* Perform the read. */
    movb    drive_id, %dl
    movw    $dap, %si
    movb    $0x42, %ah
    int     $0x13
    jc      .Lfailed
    popal
    ret

.Lfailed:
    ERROR(msg_read_failed)
FUNCTION_END(read_disk_sectors)

/** Read an inode into the inode buffer.
 * @param %eax          Inode to read.
 * @param %es:%di       Temporary location for reads. */
PRIVATE_FUNCTION_START(read_inode)
    pushal

    /* Compute the group number in EBX, and push the inode table offset. */
    decl    %eax
    xorl    %edx, %edx
    divl    (superblock + EXT2_SUPERBLOCK_OFF_INODES_PER_GROUP)
    movl    %eax, %ebx
    movl    %edx, %eax
    movzxw  (superblock + EXT2_SUPERBLOCK_OFF_INODE_SIZE), %ecx
    mull    %ecx
    pushl   %eax

    /* Read in the group table entry. Work out which sector of the table it is
     * in (EAX), and the offset into that entry (EBX), and read it in. */
    movzxw  (superblock + EXT2_SUPERBLOCK_OFF_DESC_SIZE), %ecx
    movl    %ebx, %eax
    mull    %ecx
    movl    %eax, %ebx
    shrl    $9, %eax
    andw    $511, %bx
    addl    group_table_offset, %eax
    call    read_disk_sector

    /* Retrieve the inode table block, then work out the sector that the inode
     * is in (EAX) and the offset into that sector (EBX), and read it in. */
    movl    %es:EXT2_GROUP_DESC_OFF_INODE_TABLE(%bx,%di), %edx
    movb    block_to_sector, %cl
    shll    %cl, %edx
    popl    %eax
    movl    %eax, %ebx
    shrl    $9, %eax
    andw    $511, %bx
    addl    %edx, %eax
    call    read_disk_sector

    /* Copy the inode to the inode buffer. Do it manually rather than using
     * rep movsb because it avoids having to swap around ES/DS/SI/DI. */
    addw    %bx, %di
    xorw    %bx, %bx
1:  movb    %es:(%bx,%di), %al
    movb    %al, %ds:inode(%bx)
    incw    %bx
    cmpw    $EXT2_INODE_SIZE, %bx
    jb      1b

    popal
    ret
FUNCTION_END(read_inode)

/** Read a filesystem block.
 * @param %eax          Raw block number to read.
 * @param %es:%di       Location to read to. */
PRIVATE_FUNCTION_START(read_block)
    pushl   %eax
    pushl   %ecx

    /* Check for sparse blocks (raw block number 0). */
    testl   %eax, %eax
    jz      .Lsparse_block

    /* Read sectors in from the disk. */
    movb    block_to_sector, %cl
    shll    %cl, %eax
    movl    $1, %edx
    shll    %cl, %edx
    movl    %edx, %ecx
    call    read_disk_sectors
    jmp     .Lret

.Lsparse_block:
    pushw   %es
    pushw   %di

    /* Fill block with zeros. */
    movw    block_size, %cx
    xorb    %al, %al
    rep stosb

    popw    %di
    popw    %es

.Lret:
    popl    %ecx
    popl    %eax
    ret
FUNCTION_END(read_block)

/** Read all data for the inode to LOADER_LOAD_ADDR. */
PRIVATE_FUNCTION_START(read_inode_data)
    /* Get total number of blocks we need to read. */
    call    get_total_blocks

    /* Set the new segment and start data address. */
    pushw   $LOADER_LOAD_SEGMENT
    popw    %es
    xorw    %di, %di

    /* Read each block to the destination.
     * EBX = current block number, ECX = total blocks to read. */
    xorl    %ebx, %ebx
1:  call    get_raw_block
    call    read_block

    /* Increment data buffer, handle overflowing into the next segment. */
    addw    block_size, %di
    jnc     2f
    movw    %es, %dx
    addw    $0x1000, %dx
    movw    %dx, %es
2:  incl    %ebx
    decl    %ecx
    jnz     1b

    ret
FUNCTION_END(read_inode_data)

/** Display a string on the screen.
 * @param %ds:%si       String to display. */
PRIVATE_FUNCTION_START(puts)
    pushw   %ax
1:  lodsb
    testb   %al, %al
    jz      2f
    pushw   %bx
    movb    $0xe, %ah
    movw    $1, %bx
    int     $0x10
    popw    %bx
    jmp     1b
2:  popw    %ax
    ret
FUNCTION_END(puts)

/* Print an error and stop.
 * @param %ds:%si       String to display. */
PRIVATE_FUNCTION_START(error)
    call    puts
1:  hlt
    jmp 1b
FUNCTION_END(error)

/** Error strings. */
msg_loading:        .asciz "Loading... "
msg_read_failed:    .asciz "Failed"
msg_not_ext2:       .asciz "Bad FS"

first_half_end:

/* Pad up to the end of the first 512 bytes. */
. = _start + 0x1fa
def_partition_lba:  .long 0
boot_signature:     .word MBR_SIGNATURE

second_half:

/** Get total number of blocks in the current inode.
 * @return %ecx         Total number of blocks.
 * @return %eax         Trashed.
 * @return %edx         Remainder of data size in final block. */
PRIVATE_FUNCTION_START(get_total_blocks)
    movl    (inode + EXT2_INODE_OFF_SIZE), %eax
    xorl    %edx, %edx
    movzxw  block_size, %ecx
    divl    %ecx

    /* Increment if there is a remainder to cover final chunk of data. */
    testl   %edx, %edx
    jz      1f
    incl    %eax
1:  movl    %eax, %ecx
    ret
FUNCTION_END(get_total_blocks)

/** Look up a raw block number in the current inode.
 * @param %ebx      Block number (preserved).
 * @return %eax     Raw block number.
 * @return %edx     Trashed.
 * @return %ebp     Trashed. */
PRIVATE_FUNCTION_START(get_raw_block)
    /* We need to modify the block number but we want to preserve it for the
     * caller. We modify it in EBP. */
    movl    %ebx, %ebp

    /* Test for extents. */
    testb   $(EXT4_EXTENTS_FL >> 16), (inode + EXT2_INODE_OFF_FLAGS + 2)
    jnz     .Lextents

    cmpl    $EXT2_NDIR_BLOCKS, %ebp
    jae     .Lindirect

    /* Raw block number is stored in the inode. */
    shlw    $2, %bp
    movl    (inode + EXT2_INODE_OFF_BLOCK)(%bp), %eax
    ret

.Lindirect:
    subl    $EXT2_NDIR_BLOCKS, %ebp
    shll    $2, %ebp
    movzxw  block_size, %edx
    cmpl    %edx, %ebp
    jae     .Lbi_indirect

    /* Read in the indirect block. ES:DI should point to the location where
     * the caller is going to read to. We use this as a temporary location to
     * read the block to. */
    movl    (inode + EXT2_INODE_OFF_BLOCK + (EXT2_IND_BLOCK * 4)), %eax
    call    read_block
    movl    %es:(%bp,%di), %eax
    ret

.Lbi_indirect:
    /* Don't bother supporting this. We only have to load the loader here,
     * which shouldn't be too big. */
    ERROR(msg_too_large)

.Lextents:
    /* Find the leaf. First iteration we want to operate on i_block. All other
     * iterations we want to operate on the current buffer location. Safe for us
     * to trash %dx here to use as a loop counter. */
    pushw   %es
    pushw   %di
    xorw    %dx, %dx
    movw    %dx, %es
    movw    $inode + EXT2_INODE_OFF_BLOCK, %di

1:  /* Check whether the extent header is valid. */
    cmpw    $EXT4_EXT_MAGIC, %es:EXT4_EXTENT_HEADER_OFF_MAGIC(%di)
    jne     .Lbad_extent

    /* If the depth is 0, this is the leaf. */
    cmpw    $0, %es:EXT4_EXTENT_HEADER_OFF_DEPTH(%di)
    je      .Lfound_extent

    /* Loop through to find the index covering the block (%dx == 0). */
    movw    %es:EXT4_EXTENT_HEADER_OFF_ENTRIES(%di), %ax
    addw    $EXT4_EXTENT_HEADER_SIZE, %di
2:  cmpw    %ax, %dx
    jae     3f

    cmpl    %es:EXT4_EXTENT_IDX_OFF_BLOCK(%di), %ebp
    jb      3f

    addw    $EXT4_EXTENT_IDX_SIZE, %di
    incw    %dx
    jmp     2b

3:  /* Count of 0 means the block was not covered by any index. */
    testw   %dx, %dx
    jz      .Lbad_extent

    /* Get the block number to read. index[i - 1]. */
    movl    %es:(EXT4_EXTENT_IDX_OFF_LEAF - EXT4_EXTENT_IDX_SIZE)(%di), %eax

    /* Restore the base address and read the next block. */
    popw    %di
    popw    %es
    call    read_block
    pushw   %es
    pushw   %di
    xorw    %dx, %dx
    jmp     1b

.Lfound_extent:
    /* %es:%di points at the header of the leaf. Loop through to find the extent
     * covering the block (%dx == 0). TODO: Could possibly merge this with the
     * similar loop above, but I'm lazy. Should be done if we need to free up
     * some space in the future. */
    movw    %es:EXT4_EXTENT_HEADER_OFF_ENTRIES(%di), %ax
    addw    $EXT4_EXTENT_HEADER_SIZE, %di
1:  cmpw    %ax, %dx
    jae     2f

    cmpl    %es:EXT4_EXTENT_OFF_BLOCK(%di), %ebp
    jb      2f

    addw    $EXT4_EXTENT_SIZE, %di
    incw    %dx
    jmp     1b

2:  /* Count of 0 means the block was not covered by any extent. */
    testw   %dx, %dx
    jz      .Lbad_extent

    /* Subtract start block from block number. extent[i - 1]. */
    subl    %es:(EXT4_EXTENT_OFF_BLOCK - EXT4_EXTENT_SIZE)(%di), %ebp

    /* If the offset is greater than the length of the extent, return 0 (sparse
     * block), else add on the physical start block. */
    movzxw  %es:(EXT4_EXTENT_OFF_LEN - EXT4_EXTENT_SIZE)(%di), %edx
    xorl    %eax, %eax
    cmpl    %edx, %ebp
    jae     3f
    movl    %es:(EXT4_EXTENT_OFF_START - EXT4_EXTENT_SIZE)(%di), %eax
    addl    %ebp, %eax

    /* Restore old base address and return. */
3:  popw    %di
    popw    %es
    ret

.Lbad_extent:
    ERROR(msg_bad_extent)
FUNCTION_END(get_raw_block)

/** Find and read in the boot loader inode. */
PRIVATE_FUNCTION_START(find_inode)
    /* Point ES:DI to the loader load address, which is used as a temporary
     * location to read directory contents to. */
    pushw   $LOADER_LOAD_SEGMENT
    popw    %es
    xorw    %di, %di

    /* Point DS:SI to the loader path string. */
    movw    $path, %si

    /* Start at the root inode. */
    xorl    %eax, %eax
    movb    $EXT2_ROOT_INO, %al
    call    read_inode

.Linode_begin:
    /* Read through each block of the directory in turn. An entry cannot span
     * a block boundary. Current block number is stored in EBX, ECX is the
     * total number of blocks in the directory, decremented as we go to check
     * when we reach the end of the directory. */
    call    get_total_blocks
    xorl    %ebx, %ebx

.Lblock_begin:
    /* Read in this block. */
    call    get_raw_block
    call    read_block

.Lentry_begin:
    /* Check if the current directory entry is used. */
    cmpl    $0, %es:EXT2_DIR_ENTRY_OFF_INODE(%di)
    je      .Lnext_entry
    cmpb    $EXT2_FT_UNKNOWN, %es:EXT2_DIR_ENTRY_OFF_FILE_TYPE(%di)
    je      .Lnext_entry
    movb    %es:EXT2_DIR_ENTRY_OFF_NAME_LEN(%di), %al
    testb   %al, %al
    jz      .Lnext_entry

    /* It is, lets try to compare the name string. */
    pushw   %di
    pushw   %si
    addw    $EXT2_DIR_ENTRY_OFF_NAME, %di
    cld

.Lcompare_begin:
    /* Check if we are at the end of the current path component. */
    movb    (%si), %ah
    testb   %ah, %ah
    jz      .Lcompare_end
    cmpb    $'/', %ah
    je      .Lcompare_end

    /* Compare the current character. Increments DI/SI. */
    cmpsb
    jne     .Lcompare_fail
    decb    %al
    jmp     .Lcompare_begin

.Lcompare_end:
    /* We're at the end of the path component. If name length (AL) is 0, we have
     * a match. */
    testb   %al, %al
    jz      .Lcompare_match

.Lcompare_fail:
    /* Get back to the beginning of the path component and directory entry, fall
     * through to next entry. */
    popw    %si
    popw    %di

.Lnext_entry:
    /* Increment past this entry and check if we have crossed a block boundary. */
    addw    %es:EXT2_DIR_ENTRY_OFF_REC_LEN(%di), %di
    cmpw    block_size, %di
    jb      .Lentry_begin

    /* Advance to the next block, or error if we've reached the end. */
    incl    %ebx
    decl    %ecx
    jz      .Lnot_found

    /* Reset the buffer address for the next iteration. */
    xorw    %di, %di
    jmp     .Lblock_begin

.Lcompare_match:
    /* Discard the previous path component start, get back the directory entry. */
    addw    $2, %sp
    popw    %di

    /* Get the inode number and read it in. Clear DI as read_inode() uses it, so
     * we want it to point to the bottom of the temporary buffer. Also needs to
     * be clear for the start of the next iteration. */
    movl    %es:EXT2_DIR_ENTRY_OFF_INODE(%di), %eax
    xorw    %di, %di
    call    read_inode

    /* Get the inode type for type comparisons. */
    movw    (inode + EXT2_INODE_OFF_MODE), %ax
    andw    $EXT2_S_IFMT, %ax

    /* If we're at the end of the path string, we have our inode. */
    cmpb    $0, (%si)
    je      .Lfound

    /* Not the end of the path string, must be a directory. */
    cmpw    $EXT2_S_IFDIR, %ax
    jne     .Lbad_type

    /* Increment over the null terminator and start over. */
    incw    %si
    jmp     .Linode_begin

.Lfound:
    /* Must be a regular file. */
    cmpw    $EXT2_S_IFREG, %ax
    jne     .Lbad_type
    ret

.Lnot_found:
    ERROR(msg_not_found)

.Lbad_type:
    ERROR(msg_bad_type)
FUNCTION_END(find_inode)

/** More error strings. */
msg_unsupported:    .asciz "Unsupported"
msg_too_large:      .asciz "Too large"
msg_bad_extent:     .asciz "Bad extent"
msg_not_found:      .asciz "Not found"
msg_bad_type:       .asciz "Bad type"
msg_ok:             .asciz "OK\r\n"

second_half_end:

/** Pad up to 1KB - 32 bytes for the path string buffer. */
. = _start + 0x3e0
path:               .fill 32

.section .bss

/** Superblock. This is read into memory along with our second half. */
superblock:         .fill EXT2_SUPERBLOCK_SIZE

/** Current inode buffer. */
inode:              .fill EXT2_INODE_SIZE

/** Disk address packet. */
dap:
dap_size:           .byte 0
dap_reserved:       .byte 0
dap_count:          .word 0
dap_buffer_offset:  .word 0
dap_buffer_segment: .word 0
dap_lba_low:        .long 0
dap_lba_high:       .long 0

/** Saved partition LBA. */
partition_lba:      .long 0

/** Pre-computed FS information. */
group_table_offset: .long 0
block_size:         .word 0
block_to_sector:    .byte 0

/** Saved BIOS drive ID. */
drive_id:           .byte 0
