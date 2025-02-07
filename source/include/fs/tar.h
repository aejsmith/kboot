/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               TAR filesystem support.
 */

#ifndef __FS_TAR_H
#define __FS_TAR_H

/** Header for a TAR file. */
typedef struct tar_header {
    char name[100];             /**< Name of entry. */
    char mode[8];               /**< Mode of entry. */
    char uid[8];                /**< User ID. */
    char gid[8];                /**< Group ID. */
    char size[12];              /**< Size of entry. */
    char mtime[12];             /**< Modification time. */
    char chksum[8];             /**< Checksum. */
    char typeflag;              /**< Type flag. */
    char linkname[100];         /**< Symbolic link name. */
    char magic[6];              /**< Magic string. */
    char version[2];            /**< TAR version. */
    char uname[32];             /**< User name. */
    char gname[32];             /**< Group name. */
    char devmajor[8];           /**< Device major. */
    char devminor[8];           /**< Device minor. */
    char prefix[155];           /**< Prefix. */
} tar_header_t;

/** TAR entry types. */
#define REGTYPE     '0'         /**< Regular file (preferred code). */
#define AREGTYPE    '\0'        /**< Regular file (alternate code). */
#define LNKTYPE     '1'         /**< Hard link. */
#define SYMTYPE     '2'         /**< Symbolic link (hard if not supported). */
#define CHRTYPE     '3'         /**< Character special. */
#define BLKTYPE     '4'         /**< Block special. */
#define DIRTYPE     '5'         /**< Directory.  */
#define FIFOTYPE    '6'         /**< Named pipe.  */
#define CONTTYPE    '7'         /**< Contiguous file. */

/** TAR mode bits. */
#define TSUID       04000
#define TSGID       02000
#define TSVTX       01000
#define TUREAD      00400
#define TUWRITE     00200
#define TUEXEC      00100
#define TGREAD      00040
#define TGWRITE     00020
#define TGEXEC      00010
#define TOREAD      00004
#define TOWRITE     00002
#define TOEXEC      00001

#endif /* __FS_TAR_H */
