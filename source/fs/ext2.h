/*
 * Copyright (C) 2008-2015 Alex Smith
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
 * @brief               Ext2 filesystem types/definitions.
 */

#ifndef __EXT2_H
#define __EXT2_H

/** Ext2 filesystem magic number. */
#define EXT2_MAGIC              0xef53

/** Ext4 extent header magic number. */
#define EXT4_EXT_MAGIC          0xf30a

/** Special block numbers. */
#define EXT2_NDIR_BLOCKS        12          /**< Direct blocks. */
#define EXT2_IND_BLOCK          12          /**< Indirect block. */
#define EXT2_DIND_BLOCK         13          /**< Double indirect block. */
#define EXT2_TIND_BLOCK         14          /**< Triple indirect block. */
#define EXT2_N_BLOCKS           15          /**< Total number of blocks. */

/** EXT2 revision numbers. */
#define EXT2_GOOD_OLD_REV       0
#define EXT2_DYNAMIC_REV        1

/** Filesystem status flags. */
#define EXT2_ERROR_FS           0
#define EXT2_VALID_FS           1

/** File type definitions. */
#define EXT2_S_IFMT             0xf000      /**< Format mask. */
#define EXT2_S_IFSOCK           0xc000      /**< Socket. */
#define EXT2_S_IFLNK            0xa000      /**< Symbolic link. */
#define EXT2_S_IFREG            0x8000      /**< Regular file. */
#define EXT2_S_IFBLK            0x6000      /**< Block device. */
#define EXT2_S_IFDIR            0x4000      /**< Directory. */
#define EXT2_S_IFCHR            0x2000      /**< Character device. */
#define EXT2_S_IFIFO            0x1000      /**< FIFO. */

/** Access rights. */
#define EXT2_S_ISUID            04000       /**< Set-UID. */
#define EXT2_S_ISGID            02000       /**< Set-GID. */
#define EXT2_S_ISVTX            01000       /**< Sticky bit. */
#define EXT2_S_IRWXU            00700       /**< User access mask. */
#define EXT2_S_IRUSR            00400       /**< User can read. */
#define EXT2_S_IWUSR            00200       /**< User can write. */
#define EXT2_S_IXUSR            00100       /**< User can execute. */
#define EXT2_S_IRWXG            00070       /**< Group access mask. */
#define EXT2_S_IRGRP            00040       /**< Group can read. */
#define EXT2_S_IWGRP            00020       /**< Group can write. */
#define EXT2_S_IXGRP            00010       /**< Group can execute. */
#define EXT2_S_IRWXO            00007       /**< Others access mask. */
#define EXT2_S_IROTH            00004       /**< Others can read. */
#define EXT2_S_IWOTH            00002       /**< Others can write. */
#define EXT2_S_IXOTH            00001       /**< Others can execute. */

/** File types in directory entries. */
#define EXT2_FT_UNKNOWN         0
#define EXT2_FT_REG_FILE        1
#define EXT2_FT_DIR             2
#define EXT2_FT_CHRDEV          3
#define EXT2_FT_BLKDEV          4
#define EXT2_FT_FIFO            5
#define EXT2_FT_SOCK            6
#define EXT2_FT_SYMLINK         7
#define EXT2_FT_MAX             8

/** Reserved inode numbers. */
#define EXT2_BAD_INO            0x1         /**< Bad blocks inode. */
#define EXT2_ROOT_INO           0x2         /**< Root directory inode. */
#define EXT2_ACL_IDX_INO        0x3         /**< ACL index inode. */
#define EXT2_ACL_DATA_IN        0x4         /**< ACL data inode. */
#define EXT2_BOOT_LOADER_INO    0x5         /**< Boot loader inode. */
#define EXT2_UNDEL_DIR_INO      0x6         /**< Undelete directory inode. */

/** Limitations. */
#define EXT2_NAME_MAX           256         /**< Maximum file name length. */

/** Inode flags. */
#define EXT4_EXTENTS_FL         0x80000     /**< Inode uses extents. */

/** Structure sizes (for use in ASM code). */
#define EXT2_SUPERBLOCK_SIZE    1024
#define EXT2_INODE_SIZE         128
#define EXT2_GROUP_DESC_SIZE    32
#define EXT2_DIRENT_SIZE        8
#define EXT4_EXTENT_HEADER_SIZE 12
#define EXT4_EXTENT_IDX_SIZE    12
#define EXT4_EXTENT_SIZE        12

/** Structure offsets (for use in ASM code). */
#define EXT2_SUPERBLOCK_OFF_FIRST_DATA_BLOCK    20
#define EXT2_SUPERBLOCK_OFF_LOG_BLOCK_SIZE      24
#define EXT2_SUPERBLOCK_OFF_INODES_PER_GROUP    40
#define EXT2_SUPERBLOCK_OFF_MAGIC               56
#define EXT2_SUPERBLOCK_OFF_INODE_SIZE          88
#define EXT2_GROUP_DESC_OFF_INODE_TABLE         8
#define EXT2_INODE_OFF_SIZE                     4
#define EXT2_INODE_OFF_FLAGS                    32
#define EXT2_INODE_OFF_BLOCK                    40
#define EXT4_EXTENT_HEADER_OFF_MAGIC            0
#define EXT4_EXTENT_HEADER_OFF_ENTRIES          2
#define EXT4_EXTENT_HEADER_OFF_DEPTH            6
#define EXT4_EXTENT_IDX_OFF_BLOCK               0
#define EXT4_EXTENT_IDX_OFF_LEAF                4
#define EXT4_EXTENT_OFF_BLOCK                   0
#define EXT4_EXTENT_OFF_LEN                     4
#define EXT4_EXTENT_OFF_START                   8

#ifndef __ASM__

#include <types.h>

/** Superblock of an Ext2 filesystem. */
typedef struct ext2_superblock {
    uint32_t s_inodes_count;                /**< Inodes count. */
    uint32_t s_blocks_count;                /**< Blocks count. */
    uint32_t s_r_blocks_count;              /**< Reserved blocks count. */
    uint32_t s_free_blocks_count;           /**< Free blocks count. */
    uint32_t s_free_inodes_count;           /**< Free inodes count. */
    uint32_t s_first_data_block;            /**< First Data Block. */
    uint32_t s_log_block_size;              /**< Block size. */
    uint32_t s_log_frag_size;               /**< Fragment size. */
    uint32_t s_blocks_per_group;            /**< Number of blocks per group. */
    uint32_t s_frags_per_group;             /**< Number of fragments per group. */
    uint32_t s_inodes_per_group;            /**< Number of inodes per group. */
    uint32_t s_mtime;                       /**< Mount time. */
    uint32_t s_wtime;                       /**< Write time. */
    uint16_t s_mnt_count;                   /**< Mount count. */
    uint16_t s_max_mnt_count;               /**< Maximal mount count. */
    uint16_t s_magic;                       /**< Magic signature. */
    uint16_t s_state;                       /**< File system state. */
    uint16_t s_errors;                      /**< Behaviour when detecting errors. */
    uint16_t s_minor_rev_level;             /**< Minor revision level. */
    uint32_t s_lastcheck;                   /**< Time of last check. */
    uint32_t s_checkinterval;               /**< Max time between checks. */
    uint32_t s_creator_os;                  /**< OS. */
    uint32_t s_rev_level;                   /**< Revision level. */
    uint16_t s_def_resuid;                  /**< Default uid for reserved blocks. */
    uint16_t s_def_resgid;                  /**< Default gid for reserved blocks. */

    /* EXT2_DYNAMIC_REV superblocks only. */
    uint32_t s_first_ino;                   /**< First non-reserved inode. */
    uint16_t s_inode_size;                  /**< Size of inode structure. */
    uint16_t s_block_group_nr;              /**< Block group number of this superblock. */
    uint32_t s_feature_compat;              /**< Compatible feature set. */
    uint32_t s_feature_incompat;            /**< Incompatible feature set. */
    uint32_t s_feature_ro_compat;           /**< Readonly-compatible feature set. */
    uint8_t  s_uuid[16];                    /**< 128-bit uuid for volume. */
    char     s_volume_name[16];             /**< Volume name. */
    char     s_last_mounted[64];            /**< Directory where last mounted. */
    uint32_t s_algorithm_usage_bitmap;      /**< For compression. */

    /** Performance hints. */
    uint8_t  s_prealloc_blocks;             /**< Number of blocks to try to preallocate. */
    uint8_t  s_prealloc_dir_blocks;         /**< Number to preallocate for dirs. */
    uint16_t s_padding1;

    /** Journaling support (Ext3 compatibility). */
    uint8_t  s_journal_uuid[16];            /**< UUID of journal superblock. */
    uint32_t s_journal_inum;                /**< Inode number of journal file. */
    uint32_t s_journal_dev;                 /**< Device number of journal file. */
    uint32_t s_last_orphan;                 /**< Start of list of inodes to delete. */
    uint32_t s_hash_seed[4];                /**< HTREE hash seed. */
    uint8_t  s_def_hash_version;            /**< Default hash version to use. */
    uint8_t  s_jnl_backup_type;
    uint16_t s_reserved_word_pad;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;               /**< First metablock block group. */
    uint32_t s_mkfs_time;                   /**< When the filesystem was created. */
    uint32_t s_jnl_blocks[17];              /**< Backup of the journal inode. */
    uint32_t s_reserved[172];               /**< Padding to the end of the block. */
} __packed ext2_superblock_t;

/** Group descriptor table. */
typedef struct ext2_group_desc {
    uint32_t bg_block_bitmap;               /**< Blocks bitmap block. */
    uint32_t bg_inode_bitmap;               /**< Inode bitmap block. */
    uint32_t bg_inode_table;                /**< Inode table block. */
    uint16_t bg_free_blocks_count;          /**< Number of free blocks. */
    uint16_t bg_free_inodes_count;          /**< Number of free inodes. */
    uint16_t bg_used_dirs_count;            /**< Number of used directories. */
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} __packed ext2_group_desc_t;

/** Ext2 inode structure. */
typedef struct ext2_inode {
    uint16_t i_mode;                        /**< File mode. */
    uint16_t i_uid;                         /**< Lower 16-bits of owner's UID. */
    uint32_t i_size;                        /**< File size. */
    uint32_t i_atime;                       /**< Last access time. */
    uint32_t i_ctime;                       /**< Creation time. */
    uint32_t i_mtime;                       /**< Last modification time. */
    uint32_t i_dtime;                       /**< Deletion time. */
    uint16_t i_gid;                         /**< Lower 16-bits of owning GID. */
    uint16_t i_links_count;                 /**< Number of links to this inode. */
    uint32_t i_blocks;                      /**< Number of blocks. */
    uint32_t i_flags;                       /**< File flags. */
    uint32_t i_osd1;                        /**< OS-dependent data 1, we don't need this. */
    uint32_t i_block[EXT2_N_BLOCKS];        /**< Pointers to blocks. */
    uint32_t i_generation;                  /**< File version (NFS). */
    uint32_t i_file_acl;                    /**< File access control list. */
    uint32_t i_dir_acl;                     /**< Directory access control list. */
    uint32_t i_faddr;                       /**< Fragment address. */
    union {
        struct {
            uint8_t  l_i_frag;              /**< Fragment number. */
            uint8_t  l_i_fsize;             /**< Fragment size. */
            uint16_t l_i_pad1;
            uint16_t l_i_uid_high;          /**< Higher 16-bits of owner's UID. */
            uint16_t l_i_gid_high;          /**< Higher 16-bits of owning GID. */
            uint32_t l_i_reserved2;
        } linux2;
        struct {
            uint8_t  h_i_frag;              /**< Fragment number. */
            uint8_t  h_i_fsize;             /**< Fragment size. */
            uint16_t h_i_mode_high;
            uint16_t h_i_uid_high;
            uint16_t h_i_gid_high;
            uint32_t h_i_author;
        } hurd2;
        struct {
            uint8_t  m_i_frag;              /**< Fragment number. */
            uint8_t  m_i_fsize;             /**< Fragment size. */
            uint16_t m_i_pad1;
            uint32_t m_i_reserved2[2];
        } masix2;
    } osd2;                                 /**< OS-dependent data 2. */
} __packed ext2_inode_t;

/** Ext2 directory entry. */
typedef struct ext2_dir_entry {
    uint32_t inode;                         /**< Inode number. */
    uint16_t rec_len;                       /**< Length of the structure. */
    uint8_t name_len;                       /**< Name length. */
    uint8_t file_type;                      /**< File type. */
    char name[];                            /**< Name of the file. */
} __packed ext2_dir_entry_t;

/* Ext4 on-disk extent structure. */
typedef struct ext4_extent {
    uint32_t ee_block;                      /**< First logical block extent covers. */
    uint16_t ee_len;                        /**< Number of blocks covered by extent. */
    uint16_t ee_start_hi;                   /**< High 16 bits of physical block. */
    uint32_t ee_start;                      /**< Low 32 bits of physical block. */
} __packed ext4_extent_t;

/* Ext4 on-disk index structure. */
typedef struct ext4_extent_idx {
    uint32_t ei_block;                      /**< Index covers logical blocks from 'block'. */
    uint32_t ei_leaf;                       /**< Pointer to the physical block of the next level. */
    uint16_t ei_leaf_hi;                    /**< High 16 bits of physical block. */
    uint16_t ei_unused;                     /**< Unused. */
} __packed ext4_extent_idx_t;

/* Ext4 extent header structure. */
typedef struct ext4_extent_header {
    uint16_t eh_magic;                      /**< Magic number. */
    uint16_t eh_entries;                    /**< Number of valid entries. */
    uint16_t eh_max;                        /**< Capacity of store in entries. */
    uint16_t eh_depth;
    uint32_t eh_generation;
} __packed ext4_extent_header_t;

#endif /* __ASM__ */
#endif /* __EXT2_H */
