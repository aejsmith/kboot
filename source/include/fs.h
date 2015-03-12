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
 * @brief               Filesystem support.
 */

#ifndef __FS_H
#define __FS_H

#include <loader.h>

struct device;
struct fs_entry;
struct fs_handle;
struct fs_mount;

/** Length of a standard UUID string (including null terminator). */
#define UUID_STR_LEN    37

/** Type of a fs_iterate() callback.
 * @param entry         Details of the entry that was found (only valid in the
 *                      scope of this function).
 * @param data          Data argument passed to fs_iterate().
 * @return              Whether to continue iteration. */
typedef bool (*fs_iterate_cb_t)(const struct fs_entry *entry, void *arg);

/** Structure containing operations for a filesystem. */
typedef struct fs_ops {
    const char *name;                   /**< Name of the filesystem type. */

    /** Mount an instance of this filesystem.
     * @param device        Device to mount.
     * @param _mount        Where to store pointer to mount structure. Should be
     *                      allocated by malloc(). ops and device will be set
     *                      upon return.
     * @return              Status code describing the result of the operation.
     *                      Return STATUS_UNKNOWN_FS to indicate that the
     *                      device does not contain a filesystem of this type. */
    status_t (*mount)(struct device *device, struct fs_mount **_mount);

    /** Open an entry on the filesystem.
     * @param entry         Entry to open (obtained via iterate()).
     * @param _handle       Where to store pointer to opened handle.
     * @return              Status code describing the result of the operation. */
    status_t (*open_entry)(const struct fs_entry *entry, struct fs_handle **_handle);

    /** Open a path on the filesystem.
     * @note                If not provided, a generic implementation will be
     *                      used that uses iterate().
     * @note                This function will always be passed a relative path,
     *                      if fs_open() was called with an absolute path then
     *                      from will be set to mount->root.
     * @param mount         Mount to open from.
     * @param path          Path to file/directory to open (can be modified).
     * @param from          Handle on this FS to open relative to.
     * @param _handle       Where to store pointer to opened handle.
     * @return              Status code describing the result of the operation. */
    status_t (*open_path)(struct fs_mount *mount, char *path, struct fs_handle *from, struct fs_handle **_handle);

    /** Close a handle (optional).
     * @param handle        Handle to close. The handle will be freed after this
     *                      returns, this only needs to clear up any additional
     *                      allocated data. */
    void (*close)(struct fs_handle *handle);

    /** Read from a file.
     * @param handle        Handle to the file.
     * @param buf           Buffer to read into.
     * @param count         Number of bytes to read.
     * @param offset        Offset into the file.
     * @return              Status code describing the result of the operation. */
    status_t (*read)(struct fs_handle *handle, void *buf, size_t count, offset_t offset);

    /** Iterate over directory entries.
     * @param handle        Handle to directory.
     * @param cb            Callback to call on each entry.
     * @param arg           Data to pass to callback.
     * @return              Status code describing the result of the operation. */
    status_t (*iterate)(struct fs_handle *handle, fs_iterate_cb_t cb, void *arg);
} fs_ops_t;

/** Define a builtin filesystem operations structure. */
#define BUILTIN_FS_OPS(name)   \
    static fs_ops_t name; \
    DEFINE_BUILTIN(BUILTIN_TYPE_FS, name); \
    static fs_ops_t name

/** Structure representing a mounted filesystem. */
typedef struct fs_mount {
    fs_ops_t *ops;                      /**< Operations structure for the filesystem. */
    struct device *device;              /**< Device that the filesystem is on. */
    struct fs_handle *root;             /**< Handle to root of FS (not needed if open() implemented). */
    bool case_insensitive;              /**< Whether the filesystem is case insensitive. */
    char *label;                        /**< Label of the filesystem. */
    char *uuid;                         /**< UUID of the filesystem. */
} fs_mount_t;

/** File type enumeration. */
typedef enum file_type {
    FILE_TYPE_NONE,                     /**< No type (invalid in handle, used by fs_open()). */
    FILE_TYPE_REGULAR,                  /**< Regular file. */
    FILE_TYPE_DIR,                      /**< Directory. */
} file_type_t;

/** Structure representing a handle to a filesystem entry. */
typedef struct fs_handle {
    fs_mount_t *mount;                  /**< Mount the entry is on. */
    file_type_t type;                   /**< Type of the entry. */
    offset_t size;                      /**< Size of the file. */
    unsigned count;                     /**< Reference count. */
} fs_handle_t;

/** Filesystem entry information structure. */
typedef struct fs_entry {
    fs_handle_t *owner;                 /**< Directory containing this entry. */
    const char *name;                   /**< Name of the entry. */
} fs_entry_t;

extern fs_mount_t *fs_probe(struct device *device);

/** Increase the reference count of a filesystem handle.
 * @param handle        Handle to retain. */
static inline void fs_retain(fs_handle_t *handle) {
    handle->count++;
}

extern status_t fs_open_entry(const fs_entry_t *entry, file_type_t type, fs_handle_t **_handle);
extern status_t fs_open(const char *path, fs_handle_t *from, file_type_t type, fs_handle_t **_handle);
extern void fs_close(fs_handle_t *handle);

extern status_t fs_read(fs_handle_t *handle, void *buf, size_t count, offset_t offset);
extern status_t fs_iterate(fs_handle_t *handle, fs_iterate_cb_t cb, void *arg);

/** Helper for __cleanup_close. */
static inline void fs_closep(void *p) {
    fs_handle_t *handle = *(fs_handle_t **)p;

    if (handle)
        fs_close(handle);
}

/** Variable attribute to release a handle when it goes out of scope. */
#define __cleanup_close __cleanup(fs_closep)

#endif /* __FS_H */
