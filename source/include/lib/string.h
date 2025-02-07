/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               String handling functions.
 */

#ifndef __LIB_STRING_H
#define __LIB_STRING_H

#include <types.h>

extern void *memcpy(void *__restrict dest, const void *__restrict src, size_t count);
extern void *memset(void *dest, int val, size_t count);
extern void *memmove(void *dest, const void *src, size_t count);
extern int memcmp(const void *p1, const void *p2, size_t count);
extern void *memchr(const void *src, int c, size_t count);
extern void *memdup(const void *src, size_t count);
extern size_t strlen(const char *str);
extern size_t strnlen(const char *str, size_t count);
extern int strcmp(const char *s1, const char *s2);
extern int strncmp(const char *s1, const char *s2, size_t count);
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t count);
extern char *strsep(char **stringp, const char *delim);
extern char *strchr(const char *s, int c);
extern char *strrchr(const char *s, int c);
extern char *strstr(const char *s, const char *what);
extern char *strstrip(char *str);
extern char *strcpy(char *__restrict dest, const char *__restrict src);
extern char *strncpy(char *__restrict dest, const char *__restrict src, size_t count);
extern char *strcat(char *__restrict dest, const char *__restrict src);
extern char *strdup(const char *src);
extern char *strndup(const char *src, size_t n);
extern unsigned long strtoul(const char *cp, char **endp, unsigned int base);
extern long strtol(const char *cp,char **endp,unsigned int base);
extern unsigned long long strtoull(const char *cp, char **endp, unsigned int base);
extern long long strtoll(const char *cp, char **endp, unsigned int base);
extern int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
extern int vsprintf(char *buf, const char *fmt, va_list args);
extern int snprintf(char *buf, size_t size, const char *fmt, ...);
extern int sprintf(char *buf, const char *fmt, ...);

extern char *basename(const char *path);
extern char *dirname(const char *path);

extern void split_cmdline(const char *str, char **_path, char **_cmdline);

extern bool str_ends_with(const char *str, const char *suffix);

#endif /* __LIB_STRING_H */
