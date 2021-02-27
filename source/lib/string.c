/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               String handling functions.
 */

#include <lib/ctype.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <loader.h>
#include <memory.h>

#ifndef TARGET_HAS_MEMCPY

/**
 * Copy data in memory.
 *
 * Copies bytes from a source memory area to a destination memory area,
 * where both areas may not overlap.
 *
 * @param dest          The memory area to copy to.
 * @param src           The memory area to copy from.
 * @param count         The number of bytes to copy.
 *
 * @return              Destination location.
 */
void *memcpy(void *__restrict dest, const void *__restrict src, size_t count) {
    const char *s = (const char *)src;
    const unsigned long *ns;
    char *d = (char *)dest;
    unsigned long *nd;

#if !TARGET_SUPPORTS_UNALIGNED_ACCESS
    /* Both pointers must have the same alignment. */
    if (((ptr_t)d & (sizeof(unsigned long) - 1)) == ((ptr_t)s & (sizeof(unsigned long) - 1))) {
#endif
        /* Align the destination. */
        while ((ptr_t)d & (sizeof(unsigned long) - 1)) {
            if (count--) {
                *d++ = *s++;
            } else {
                return dest;
            }
        }

        /* Write in native-sized blocks if we can. */
        if (count >= sizeof(unsigned long)) {
            nd = (unsigned long *)d;
            ns = (const unsigned long *)s;

            /* Unroll the loop if possible. */
            while (count >= (sizeof(unsigned long) * 4)) {
                *nd++ = *ns++;
                *nd++ = *ns++;
                *nd++ = *ns++;
                *nd++ = *ns++;
                count -= sizeof(unsigned long) * 4;
            }
            while (count >= sizeof(unsigned long)) {
                *nd++ = *ns++;
                count -= sizeof(unsigned long);
            }

            d = (char *)nd;
            s = (const char *)ns;
        }
#if !TARGET_SUPPORTS_UNALIGNED_ACCESS
    }
#endif

    /* Write remaining bytes. */
    while (count--)
        *d++ = *s++;

    return dest;
}

#endif /* TARGET_HAS_MEMCPY */

#ifndef TARGET_HAS_MEMSET

/** Fill a memory area.
 * @param dest          The memory area to fill.
 * @param val           The value to fill with (converted to an unsigned char).
 * @param count         The number of bytes to fill.
 * @return              Destination location. */
void *memset(void *dest, int val, size_t count) {
    unsigned char c = val & 0xff;
    unsigned long *nd, nval;
    char *d = (char *)dest;

    /* Align the destination. */
    while ((ptr_t)d & (sizeof(unsigned long) - 1)) {
        if (count--) {
            *d++ = c;
        } else {
            return dest;
        }
    }

    /* Write in native-sized blocks if we can. */
    if (count >= sizeof(unsigned long)) {
        nd = (unsigned long *)d;

        /* Compute the value we will write. */
        #ifdef __LP64__
            nval = c * 0x0101010101010101ul;
        #else
            nval = c * 0x01010101ul;
        #endif

        /* Unroll the loop if possible. */
        while (count >= (sizeof(unsigned long) * 4)) {
            *nd++ = nval;
            *nd++ = nval;
            *nd++ = nval;
            *nd++ = nval;
            count -= sizeof(unsigned long) * 4;
        }
        while (count >= sizeof(unsigned long)) {
            *nd++ = nval;
            count -= sizeof(unsigned long);
        }

        d = (char *)nd;
    }

    /* Write remaining bytes. */
    while (count--)
        *d++ = val;

    return dest;
}

#endif /* TARGET_HAS_MEMSET */

/**
 * Copy overlapping data in memory.
 *
 * Copies bytes from a source memory area to a destination memory area,
 * where both areas may overlap.
 *
 * @param dest          The memory area to copy to.
 * @param src           The memory area to copy from.
 * @param count         The number of bytes to copy.
 *
 * @return              Destination location.
 */
void *memmove(void *dest, const void *src, size_t count) {
    const unsigned char *s;
    unsigned char *d;

    if (src != dest) {
        if (src > dest) {
            memcpy(dest, src, count);
        } else {
            d = (unsigned char *)dest + (count - 1);
            s = (const unsigned char *)src + (count - 1);
            while (count--)
                *d-- = *s--;
        }
    }

    return dest;
}

/** Compare 2 chunks of memory.
 * @param p1            Pointer to the first chunk.
 * @param p2            Pointer to the second chunk.
 * @param count         Number of bytes to compare.
 * @return              An integer less than, equal to or greater than 0 if
 *                      p1 is found, respectively, to be less than, to match,
 *                      or to be greater than p2. */
int memcmp(const void *p1, const void *p2, size_t count) {
    const unsigned char *s1 = (const unsigned char *)p1;
    const unsigned char *s2 = (const unsigned char *)p2;

    while (count--) {
        if (*s1 != *s2)
            return *s1 - *s2;

        s1++;
        s2++;
    }

    return 0;
}

/** Scan memory for a character.
 * @param src           Memory to scan.
 * @param c             Character to scan for.
 * @param count         Number of bytes to scan.
 * @return              Pointer to match or NULL if not found. */
void *memchr(const void *src, int c, size_t count) {
    const unsigned char *buf = src;
    unsigned char ch = c & 0xff;

    for (size_t i = 0; i < count; i++) {
        if (buf[i] == ch)
            return (void *)&buf[i];
    }

    return NULL;
}

/**
 * Duplicate memory.
 *
 * Allocates a block of memory big enough and copies the source to it. The
 * memory returned should be freed with free().
 *
 * @param src           Memory to duplicate.
 * @param count         Number of bytes to duplicate.
 *
 * @return              Pointer to duplicated memory.
 */
void *memdup(const void *src, size_t count) {
    char *dest;

    if (!count)
        return NULL;

    dest = malloc(count);
    if (dest)
        memcpy(dest, src, count);

    return dest;
}

/** Get the length of a string.
 * @param str           Pointer to the string.
 * @return              Length of the string. */
size_t strlen(const char *str) {
    size_t ret;

    for (ret = 0; *str; str++, ret++)
        ;

    return ret;
}

/** Get length of a string with limit.
 * @param str           Pointer to the string.
 * @param count         Maximum length of the string.
 * @return              Length of the string. */
size_t strnlen(const char *str, size_t count) {
    size_t ret;

    for (ret = 0; *str && ret < count; str++, ret++)
        ;

    return ret;
}

/** Compare two strings.
 * @param s1            Pointer to the first string.
 * @param s2            Pointer to the second string.
 * @return              An integer less than, equal to or greater than 0 if
 *                      s1 is found, respectively, to be less than, to match,
 *                      or to be greater than s2. */
int strcmp(const char *s1, const char *s2) {
    unsigned char c1, c2;

    while (true) {
        c1 = *s1++;
        c2 = *s2++;

        if (c1 != c2 || !c1)
            return (int)c1 - (int)c2;
    }
}

/** Compare two strings with a length limit.
 * @param s1            Pointer to the first string.
 * @param s2            Pointer to the second string.
 * @param count         Maximum number of bytes to compare.
 * @return              An integer less than, equal to or greater than 0 if
 *                      s1 is found, respectively, to be less than, to match,
 *                      or to be greater than s2. */
int strncmp(const char *s1, const char *s2, size_t count) {
    unsigned char c1, c2;

    while (count) {
        c1 = *s1++;
        c2 = *s2++;

        if (c1 != c2 || !c1)
            return (int)c1 - (int)c2;

        count--;
    }

    return 0;
}

/** Compare two strings ignorning case.
 * @param s1            Pointer to the first string.
 * @param s2            Pointer to the second string.
 * @return              An integer less than, equal to or greater than 0 if
 *                      s1 is found, respectively, to be less than, to match,
 *                      or to be greater than s2. */
int strcasecmp(const char *s1, const char *s2) {
    unsigned char c1, c2;

    while (true) {
        c1 = tolower(*s1++);
        c2 = tolower(*s2++);

        if (c1 != c2 || !c1)
            return (int)c1 - (int)c2;
    }
}

/** Compare two strings with a length limit ignoring case.
 * @param s1            Pointer to the first string.
 * @param s2            Pointer to the second string.
 * @param count         Maximum number of bytes to compare.
 * @return              An integer less than, equal to or greater than 0 if
 *                      s1 is found, respectively, to be less than, to match,
 *                      or to be greater than s2. */
int strncasecmp(const char *s1, const char *s2, size_t count) {
    unsigned char c1, c2;

    while (count) {
        c1 = tolower(*s1++);
        c2 = tolower(*s2++);

        if (c1 != c2 || !c1)
            return (int)c1 - (int)c2;

        count--;
    }

    return 0;
}

/**
 * Separate a string.
 *
 * Finds the first occurrence of a symbol in the string delim in *stringp.
 * If one is found, the delimeter is replaced by a NULL byte and the pointer
 * pointed to by stringp is updated to point past the string. If no delimeter
 * is found *stringp is made NULL and the token is taken to be the entire
 * string.
 *
 * @param stringp       Pointer to a pointer to the string to separate.
 * @param delim         String containing all possible delimeters.
 *
 * @return              NULL if stringp is NULL, otherwise a pointer to the
 *                      token found.
 */
char *strsep(char **stringp, const char *delim) {
    char *s;
    const char *spanp;
    int c, sc;
    char *tok;

    s = *stringp;
    if (!s)
        return NULL;

    for (tok = s;;) {
        c = *s++;
        spanp = delim;
        do {
            sc = *spanp++;
            if (sc == c) {
                if (c == 0) {
                    s = NULL;
                } else {
                    s[-1] = 0;
                }

                *stringp = s;
                return tok;
            }
        } while (sc != 0);
    }
}

/** Find first occurrence of a character in a string.
 * @param s             Pointer to the string to search.
 * @param c             Character to search for.
 * @return              NULL if token not found, otherwise pointer to token. */
char *strchr(const char *s, int c) {
    char ch = c;

    while (true) {
        if (*s == ch) {
            break;
        } else if (!*s) {
            return NULL;
        } else {
            s++;
        }
    }

    return (char *)s;
}

/** Find last occurrence of a character in a string.
 * @param s             Pointer to the string to search.
 * @param c             Character to search for.
 * @return              NULL if token not found, otherwise pointer to token. */
char *strrchr(const char *s, int c) {
    const char *l = NULL;

    while (true) {
        if (*s == c)
            l = s;
        if (!*s)
            return (char *)l;
        s++;
    }

    return (char *)l;
}

/** Find the first occurrence of a substring in a string.
 * @param s             String to search.
 * @param what          Substring to search for.
 * @return              Pointer to start of match if found, null if not. */
char *strstr(const char *s, const char *what) {
    size_t len = strlen(what);

    while (*s) {
        if (strncmp(s, what, len) == 0)
            return (char *)s;
        s++;
    }

    return NULL;
}

/**
 * Strip whitespace from a string.
 *
 * Strips whitespace from the start and end of a string. The string is modified
 * in-place.
 *
 * @param str           String to remove from.
 *
 * @return              Pointer to new start of string.
 */
char *strstrip(char *str) {
    size_t len;

    /* Strip from beginning. */
    while (isspace(*str))
        str++;

    /* Strip from end. */
    len = strlen(str);
    while (len--) {
        if (!isspace(str[len]))
            break;
    }

    str[++len] = 0;
    return str;
}

/**
 * Copy a string.
 *
 * Copies a string from one place to another. Assumes that the destination
 * is big enough to hold the string.
 *
 * @param dest          Pointer to the destination buffer.
 * @param src           Pointer to the source buffer.
 *
 * @return              The value specified for dest.
 */
char *strcpy(char *__restrict dest, const char *__restrict src) {
    char *d = dest;

    while ((*d++ = *src++))
        ;

    return dest;
}

/**
 * Copy a string with a length limit.
 *
 * Copies a string from one place to another. Will copy at most the number
 * of bytes specified.
 *
 * @param dest          Pointer to the destination buffer.
 * @param src           Pointer to the source buffer.
 * @param count         Maximum number of bytes to copy.
 *
 * @return              The value specified for dest.
 */
char *strncpy(char *__restrict dest, const char *__restrict src, size_t count) {
    size_t i;

    for (i = 0; i < count; i++) {
        dest[i] = src[i];
        if (!src[i])
            break;
    }

    return dest;
}

/**
 * Concatenate two strings.
 *
 * Appends one string to another. Assumes that the destination string has
 * enough space to store the contents of both strings and the NULL terminator.
 *
 * @param dest          Pointer to the string to append to.
 * @param src           Pointer to the string to append.
 *
 * @return              Pointer to dest.
 */
char *strcat(char *__restrict dest, const char *__restrict src) {
    size_t len = strlen(dest);
    char *d = dest + len;

    while ((*d++ = *src++))
        ;

    return dest;
}

/**
 * Duplicate a string.
 *
 * Allocates a buffer big enough to hold the given string and copies the
 * string to it. The memory returned should be freed with free().
 *
 * @param src           Pointer to the source buffer.
 *
 * @return              Pointer to the allocated buffer containing the string.
 */
char *strdup(const char *src) {
    size_t len = strlen(src) + 1;
    char *dup;

    dup = malloc(len);
    if (dup)
        memcpy(dup, src, len);

    return dup;
}

/**
 * Duplicate a string with a length limit.
 *
 * Allocates a buffer either as big as the string or the maximum length
 * given, and then copies at most the number of bytes specified of the string
 * to it. If the string is longer than the limit, a null byte will be added
 * to the end of the duplicate. The memory returned should be freed with
 * free().
 *
 * @param src           Pointer to the source buffer.
 * @param n             Maximum number of bytes to copy.
 *
 * @return              Pointer to the allocated buffer containing the string.
 */
char *strndup(const char *src, size_t n) {
    size_t len;
    char *dup;

    len = strnlen(src, n);
    dup = malloc(len + 1);
    if (dup) {
        memcpy(dup, src, len);
        dup[len] = '\0';
    }

    return dup;
}

/** Macro to implement strtoul() and strtoull(). */
#define __strtoux(type, cp, endp, base) \
    __extension__ \
    ({ \
        type result = 0, value; \
        if (!base) { \
            if (*cp == '0') { \
                if ((tolower(*(++cp)) == 'x') && isxdigit(cp[1])) { \
                    cp++; \
                    base = 16; \
                } else { \
                    base = 8; \
                } \
            } else { \
                base = 10; \
            } \
        } else if (base == 16) { \
            if (cp[0] == '0' && tolower(cp[1]) == 'x') \
                cp += 2; \
        } \
        \
        while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp - '0' : tolower(*cp) - 'a' + 10) < base) { \
            result = result * base + value; \
            cp++; \
        } \
        \
        if (endp) \
            *endp = (char *)cp; \
        result; \
    })

/**
 * Convert a string to an unsigned long.
 *
 * Converts a string to an unsigned long using the specified number base.
 *
 * @param cp            The start of the string.
 * @param endp          Pointer to the end of the parsed string placed here.
 * @param base          The number base to use (if zero will guess).
 *
 * @return              Converted value.
 */
unsigned long strtoul(const char *cp, char **endp, unsigned int base) {
    return __strtoux(unsigned long, cp, endp, base);
}

/**
 * Convert a string to a signed long.
 *
 * Converts a string to an signed long using the specified number base.
 *
 * @param cp            The start of the string.
 * @param endp          Pointer to the end of the parsed string placed here.
 * @param base          The number base to use.
 *
 * @return              Converted value.
 */
long strtol(const char *cp, char **endp, unsigned int base) {
    if (*cp == '-')
        return -strtoul(cp + 1, endp, base);

    return strtoul(cp, endp, base);
}

/**
 * Convert a string to an unsigned long long.
 *
 * Converts a string to an unsigned long long using the specified number base.
 *
 * @param cp            The start of the string.
 * @param endp          Pointer to the end of the parsed string placed here.
 * @param base          The number base to use.
 *
 * @return              Converted value.
 */
unsigned long long strtoull(const char *cp, char **endp, unsigned int base) {
    return __strtoux(unsigned long long, cp, endp, base);
}

/**
 * Convert a string to an signed long long.
 *
 * Converts a string to an signed long long using the specified number base.
 *
 * @param cp            The start of the string.
 * @param endp          Pointer to the end of the parsed string placed here.
 * @param base          The number base to use.
 *
 * @return              Converted value.
 */
long long strtoll(const char *cp, char **endp, unsigned int base) {
    if (*cp == '-')
        return -strtoull(cp + 1, endp, base);

    return strtoull(cp, endp, base);
}

/** Data used by vsnprintf_helper(). */
struct vsnprintf_data {
    char *buf;                      /**< Buffer to write to. */
    size_t size;                    /**< Total size of buffer. */
    size_t off;                     /**< Current number of bytes written. */
};

/** Helper for vsnprintf().
 * @param ch            Character to place in buffer.
 * @param _data         Data.
 * @param total         Pointer to total character count. */
static void vsnprintf_helper(char ch, void *_data, int *total) {
    struct vsnprintf_data *data = _data;

    if (data->off < data->size) {
        data->buf[data->off++] = ch;
        *total = *total + 1;
    }
}

/**
 * Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given.
 *
 * @param buf           The buffer to place the result into.
 * @param size          The size of the buffer, including the trailing NULL.
 * @param fmt           The format string to use.
 * @param args          Arguments for the format string.
 *
 * @return              The number of characters generated, excluding the
 *                      trailing NULL.
 */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
    struct vsnprintf_data data;
    int ret;

    data.buf = buf;
    data.size = size - 1;
    data.off = 0;

    ret = do_vprintf(vsnprintf_helper, &data, fmt, args);
    data.buf[min(data.off, data.size)] = 0;
    return ret;
}

/**
 * Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given.
 *
 * @param buf           The buffer to place the result into.
 * @param fmt           The format string to use.
 * @param args          Arguments for the format string.
 *
 * @return              The number of characters generated, excluding the
 *                      trailing NULL.
 */
int vsprintf(char *buf, const char *fmt, va_list args) {
    return vsnprintf(buf, (size_t)-1, fmt, args);
}

/**
 * Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given.
 *
 * @param buf           The buffer to place the result into.
 * @param size          The size of the buffer, including the trailing NULL.
 * @param fmt           The format string to use.
 *
 * @return              The number of characters generated, excluding the
 *                      trailing NULL, as per ISO C99.
 */
int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vsnprintf(buf, size, fmt, args);
    va_end(args);

    return ret;
}

/**
 * Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given.
 *
 * @param buf           The buffer to place the result into.
 * @param fmt           The format string to use.
 *
 * @return              The number of characters generated, excluding the
 *                      trailing NULL, as per ISO C99.
 */
int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vsprintf(buf, fmt, args);
    va_end(args);

    return ret;
}

/**
 * Get the last component of a path.
 *
 * Returns an allocated string buffer containing the last component of the
 * given path.
 *
 * @param path          Pathname to parse.
 *
 * @return              Pointer to string containing last component of path.
 *                      The string returned is allocated via malloc(), so
 *                      should be freed using free().
 */
char *basename(const char *path) {
    char *ptr, *dup, *ret;
    size_t len;

    if (!path || !path[0] || (path[0] == '.' && !path[1])) {
        return strdup(".");
    } else if (path[0] == '.' && path[1] == '.' && !path[2]) {
        return strdup("..");
    }

    dup = strdup(path);

    /* Strip off trailing '/' characters. */
    len = strlen(dup);
    while (len && dup[len - 1] == '/')
        dup[--len] = 0;

    /* If length is now 0, the entire string was '/' characters. */
    if (!len) {
        free(dup);
        return strdup("/");
    }

    ptr = strrchr(dup, '/');
    if (!ptr) {
        /* No '/' character in the string, that means what we have is correct
         * correct. Resize the allocation to the new length. */
        return realloc(dup, len + 1);
    } else {
        ret = strdup(ptr + 1);
        free(dup);
        return ret;
    }
}

/**
 * Get the part of a path preceding the last separator.
 *
 * Returns an allocated string buffer containing everything preceding the last
 * component of the given path.
 *
 * @param path          Pathname to parse.
 *
 * @return              Pointer to string. The string returned is allocated via
 *                      malloc(), so should be freed using free().
 */
char *dirname(const char *path) {
    char *ptr, *dup;
    size_t len;

    if (!path || !path[0]|| (path[0] == '.' && !path[1])) {
        return strdup(".");
    } else if (path[0] == '.' && path[1] == '.' && !path[2]) {
        return strdup(".");
    }

    dup = strdup(path);

    /* Strip off trailing '/' characters. */
    len = strlen(dup);
    while (len && dup[len - 1] == '/')
        dup[--len] = 0;

    /* Look for last '/' character. */
    ptr = strrchr(dup, '/');
    if (!ptr) {
        free(dup);
        return strdup(".");
    }

    /* Strip off the character and any extras. */
    len = (ptr - dup) + 1;
    while (len && dup[len - 1] == '/')
        dup[--len] = 0;

    if (!len) {
        free(dup);
        return strdup("/");
    } else {
        return realloc(dup, len + 1);
    }
}

/** Split a command line string into path and arguments.
 * @param str           String to split.
 * @param _path         Where to store malloc()'d path string.
 * @param _args         Where to store malloc()'d arguments string. */
void split_cmdline(const char *str, char **_path, char **_args) {
    size_t len = 0;
    bool escaped = false;
    char *path;

    for (size_t i = 0; str[i]; i++) {
        if (!escaped && str[i] == '\\') {
            escaped = true;
        } else if (!escaped && str[i] == ' ') {
            break;
        } else {
            len++;
            escaped = false;
        }
    }

    path = malloc(len + 1);
    path[len] = 0;

    escaped = false;
    for (size_t i = 0; i < len; str++) {
        if (!escaped && *str == '\\') {
            escaped = true;
        } else if (!escaped && *str == ' ') {
            break;
        } else {
            path[i++] = *str;
            escaped = false;
        }
    }

    /* Skip a space. */
    if (*str)
        str++;

    *_path = path;
    *_args = strdup(str);
}

/** Test if a string ends with a given suffix.
 * @param str           String to test.
 * @param suffix        Suffix to search for.
 * @return              Whether the string ends with the given suffix. */
bool str_ends_with(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len)
        return false;

    return strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}
