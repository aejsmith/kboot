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
 * The inode number of the loader binary should be written to offset 506.
 * Partition location is determined using the partition table entry passed by
 * the MBR.
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
 *  - Only supports up to the indirect block. I doubt any greater will be
 *    needed here, unless we have to deal with very large directories or
 *    something.
 */

#include <fs/ext2.h>

#include <platform/loader.h>

#include <x86/asm.h>

/** Address of the stack. */
#define STACK_OFFSET        0x6000

/** Segment that inode data is loaded to. */
#define LOAD_SEGMENT        (LOADER_LOAD_ADDR >> 4)

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
     * to figure out whether we're booted from a partition and where it is. The
     * standard MBR passes the partition table entry structure in DS:SI, we try
     * to determine whether this looks valid (won't be if we're not on a
     * partition and booted directly by the BIOS). We get partition LBA into
     * ECX, begin with this as zero for all the failure cases. */
    xorl    %ecx, %ecx

    /* First check if the address is within a reasonable range. */
    movw    %ds, %ax
    movw    %si, %di
    shrw    $4, %di
    addw    %di, %ax
    cmpw    $0x50, %ax
    jb      1f
    cmpw    $0x9fc0, %ax
    jae     1f

    /* Check if the partition is marked active. */
    movb    %ds:(%si), %al
    cmpb    $0x80, %al
    jne     1f

    /* Looks like we're a partition. Get the LBA. */
    movl    %ds:8(%si), %ecx
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

    /* Compute and save the block size. */
    addb    $9, %cl
    xorl    %eax, %eax
    incb    %al
    shll    %cl, %eax
    movl    %eax, block_size

    /* Read in our inode. */
    movl    inode_number, %eax
    call    read_inode

    /* Read in the inode data. */
    call    read_inode_data

    PRINT(msg_ok)

    /* Jump to the boot loader. */
    movb    drive_id, %dl
    movl    partition_lba, %ebx
    xorl    %ecx, %ecx
    ljmp    $(LOADER_LOAD_ADDR >> 4), $0

.Lbad_fs:
    ERROR(msg_not_ext2)
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
    movw    $dap_size, %si
    movb    $0x42, %ah
    int     $0x13
    jc      .Lfailed
    popal
    ret

.Lfailed:
    ERROR(msg_read_failed)
FUNCTION_END(read_disk_sectors)

/** Read an inode into the inode buffer.
 * @param %eax          Inode to read. */
PRIVATE_FUNCTION_START(read_inode)
    pushal

    /* Compute the group number in EAX, and inode table offset in EDX. */
    decl    %eax
    xorl    %edx, %edx
    divl    (superblock + EXT2_SUPERBLOCK_OFF_INODES_PER_GROUP)
    pushl   %eax
    movl    %edx, %eax
    movzxw  (superblock + EXT2_SUPERBLOCK_OFF_INODE_SIZE), %ecx
    mull    %ecx
    popl    %edx
    pushl   %eax

    /* Read in the group table entry. Work out which sector of the table it is
     * in (EAX), and the offset into that entry (EDX), and read it in. */
    shll    $EXT2_GROUP_DESC_SHIFT, %edx
    movl    %edx, %eax
    shrl    $9, %eax
    andl    $511, %edx
    addl    group_table_offset, %eax
    movw    $temp_buffer, %di
    call    read_disk_sector

    /* Retrieve the inode table block, then work out the sector that the inode
     * is in (EAX) and the offset into that sector (EDX), and read it in. */
    movl    (temp_buffer + EXT2_GROUP_DESC_OFF_INODE_TABLE)(%edx), %ebx
    movb    block_to_sector, %cl
    shll    %cl, %ebx
    popl    %eax
    movl    %eax, %edx
    shrl    $9, %eax
    andl    $511, %edx
    addl    %ebx, %eax
    movw    $temp_buffer, %di
    call    read_disk_sector

    /* Copy the inode to the inode buffer. */
    cld
    movw    $EXT2_INODE_SIZE, %cx
    movw    $inode, %di
    movw    $temp_buffer, %si
    addw    %dx, %si
    rep movsb

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
    movl    block_size, %ecx
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
    pushal

    /* Set the new segment and start data address. */
    pushw   %es
    pushw   $(LOADER_LOAD_ADDR >> 4)
    popw    %es
    xorw    %di, %di

    /* Get total inode data size, divide it by block size and increment if there
     * is a remainder, to get the total number of blocks we need to read. */
    movl    (inode + EXT2_INODE_OFF_SIZE), %eax
    xorl    %edx, %edx
    divl    block_size
    testl   %edx, %edx
    jz      1f
    incl    %eax
1:
    /* Read each block to the destination.
     * EAX = current block number, ECX = total blocks to read. */
    xorl    %ecx, %ecx
    xchgl   %eax, %ecx
2:  pushl   %eax
    call    get_raw_block
    call    read_block

    /* Increment data buffer, handle overflowing into the next segment. */
    addw    block_size, %di
    jnc     3f
    movw    %es, %dx
    addw    $0x1000, %dx
    movw    %dx, %es
3:  pop     %eax
    incl    %eax
    decl    %ecx
    jnz     2b

    popw    %es
    popal
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
msg_loading:        .asciz "Loading: "
msg_read_failed:    .asciz "Failed"
msg_not_ext2:       .asciz "Bad FS"
msg_ok:             .asciz "OK\r\n"

first_half_end:

/* Pad up to the end of the first 512 bytes, with space for the inode number
 * and boot signature at the end. We default to EXT2_BOOT_LOADER_INO, which
 * means no change is required when we have the ability to install the boot
 * loader into that inode. */
. = _start + 0x1fa
inode_number:       .long EXT2_BOOT_LOADER_INO
boot_signature:     .word 0xaa55

second_half:

/** Look up a raw block number in the current inode.
 * @param %eax      Block number.
 * @return          Raw block number in %eax. Trashes %edx and %ebx. */
PRIVATE_FUNCTION_START(get_raw_block)
    /* Test for extents. */
    testl   $EXT4_EXTENTS_FL, (inode + EXT2_INODE_OFF_FLAGS)
    jnz     .Lextents

    cmpl    $EXT2_NDIR_BLOCKS, %eax
    jae     .Lindirect

    /* Raw block number is stored in the inode. */
    shll    $2, %eax
    movl    (inode + EXT2_INODE_OFF_BLOCK)(%eax), %eax
    ret
.Lindirect:
    subl    $EXT2_NDIR_BLOCKS, %eax
    shll    $2, %eax
    cmpl    block_size, %eax
    jae     .Lbi_indirect

    /* Read in the indirect block. */
    pushl   %eax
    movl    (inode + EXT2_INODE_OFF_BLOCK + (EXT2_IND_BLOCK * 4)), %eax
    call    read_block
    popl    %eax
    pushw   %di
    addw    %ax, %di
    movl    %es:(%di), %eax
    popw    %di
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
    movw    %es:EXT4_EXTENT_HEADER_OFF_ENTRIES(%di), %bx
    addw    $EXT4_EXTENT_HEADER_SIZE, %di
2:  cmpw    %bx, %dx
    jae     3f

    cmpl    %es:EXT4_EXTENT_IDX_OFF_BLOCK(%di), %eax
    jb      3f

    addw    $EXT4_EXTENT_IDX_SIZE, %di
    incw    %dx
    jmp     2b

3:  /* Count of 0 means the block was not covered by any index. */
    testw   %dx, %dx
    jz      .Lbad_extent

    /* Get the block number to read. index[i - 1]. */
    movl    %es:(EXT4_EXTENT_IDX_OFF_LEAF - EXT4_EXTENT_IDX_SIZE)(%di), %ebx

    /* Restore the base address and read the next block (would be nice if %sp
     * could be used as a base register...). File block number is preserved in
     * %ebx. */
    popw    %di
    popw    %es
    xchgl   %eax, %ebx
    call    read_block
    movl    %ebx, %eax
    pushw   %es
    pushw   %di
    xorw    %dx, %dx
    jmp     1b
.Lfound_extent:
    /* %es:%di points at the header of the leaf. Loop through to find the extent
     * covering the block (%dx == 0). TODO: Could possibly merge this with the
     * similar loop above, but I'm lazy. Should be done if we need to free up
     * some space in the future. */
    movw    %es:EXT4_EXTENT_HEADER_OFF_ENTRIES(%di), %bx
    addw    $EXT4_EXTENT_HEADER_SIZE, %di
1:  cmpw    %bx, %dx
    jae     2f

    cmpl    %es:EXT4_EXTENT_OFF_BLOCK(%di), %eax
    jb      2f

    addw    $EXT4_EXTENT_SIZE, %di
    incw    %dx
    jmp     1b

2:  /* Count of 0 means the block was not covered by any extent. */
    testw   %dx, %dx
    jz      .Lbad_extent

    /* Subtract start block from block number. extent[i - 1]. */
    subl    %es:(EXT4_EXTENT_OFF_BLOCK - EXT4_EXTENT_SIZE)(%di), %eax

    /* If the offset is greater than the length of the extent, return 0, else
     * add on the physical start block. */
    movzxw  %es:(EXT4_EXTENT_OFF_LEN - EXT4_EXTENT_SIZE)(%di), %edx
    movl    %eax, %ebx
    xorl    %eax, %eax
    cmpl    %edx, %ebx
    jae     3f
    movl    %es:(EXT4_EXTENT_OFF_START - EXT4_EXTENT_SIZE)(%di), %eax
    add     %ebx, %eax

    /* Restore old base address and return. */
3:  popw    %di
    popw    %es
    ret
.Lbad_extent:
    ERROR(msg_bad_extent)
FUNCTION_END(get_raw_block)

/** More error strings. */
msg_too_large:      .asciz "Too large"
msg_bad_extent:     .asciz "Bad extent"

second_half_end:

/** Pad up to 1KB. */
. = _start + 0x3ff
.byte 0

.section .bss

/** Superblock. This is read into memory along with our second half. */
superblock:         .fill EXT2_SUPERBLOCK_SIZE

/** Current inode buffer. */
inode:              .fill EXT2_INODE_SIZE

/** Temporary buffer. */
temp_buffer:        .fill 512

/** Disk address packet. */
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
block_size:         .long 0
group_table_offset: .long 0
block_to_sector:    .byte 0

/** Saved BIOS drive ID. */
drive_id:           .byte 0
