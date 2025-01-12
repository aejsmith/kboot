/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Menu interface.
 */

#ifndef __MENU_H
#define __MENU_H

#include <config.h>

#ifdef CONFIG_TARGET_HAS_UI

extern environ_t *menu_select(environ_t *env);

extern void menu_cleanup(environ_t *env);

#else

static inline environ_t *menu_select(environ_t *env) { return env; }
static inline void menu_cleanup(environ_t *env) {}

#endif /* CONFIG_TARGET_HAS_UI */
#endif /* __MENU_H */
