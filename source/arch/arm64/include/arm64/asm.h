/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 assembly code definitions.
 */

#ifndef __ARM64_ASM_H
#define __ARM64_ASM_H

#ifndef __ASM__
#   error "What are you doing?"
#endif

/** Macro to define the beginning of a global function. */
#define FUNCTION_START(_name) \
    .global _name; \
    .type _name, @function; \
    _name:

/** Macro to define the beginning of a private function. */
#define PRIVATE_FUNCTION_START(_name) \
    .type _name, @function; \
    _name:

/** Macro to define the end of a function. */
#define FUNCTION_END(_name) \
    .size _name, . - _name

/** Macro to define a global symbol. */
#define SYMBOL(_name) \
    .global _name; \
    _name:

/** Macro to define a global symbol with alignment. */
#define SYMBOL_ALIGNED(_name, _align) \
    .balign _align; \
    .global _name; \
    _name:

#endif /* __ARM64_ASM_H */
