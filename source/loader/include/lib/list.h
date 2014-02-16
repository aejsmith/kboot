/*
 * Copyright (C) 2008-2014 Alex Smith
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
 * @brief		Circular doubly-linked list implementation.
 */

#ifndef __LIB_LIST_H
#define __LIB_LIST_H

#include <types.h>

/** Doubly linked list node structure. */
typedef struct list {
	struct list *prev;		/**< Pointer to previous entry. */
	struct list *next;		/**< Pointer to next entry. */
} list_t;

/** Iterate over a list.
 * @param list		Head of list to iterate.
 * @param iter		Variable name to set to node pointer on each iteration. */
#define LIST_FOREACH(list, iter) \
	for(list_t *iter = (list)->next; iter != (list); iter = iter->next)

/** Iterate over a list in reverse.
 * @param list		Head of list to iterate.
 * @param iter		Variable name to set to node pointer on each iteration. */
#define LIST_FOREACH_REVERSE(list, iter) \
	for(list_t *iter = (list)->prev; iter != (list); iter = iter->prev)

/** Iterate over a list safely.
 * @note		Safe to use when the loop may modify the list - caches
 *			the next pointer from the entry before the loop body.
 * @param list		Head of list to iterate.
 * @param iter		Variable name to set to node pointer on each iteration. */
#define LIST_FOREACH_SAFE(list, iter) \
	for(list_t *iter = (list)->next, *_##iter = iter->next; \
		iter != (list); iter = _##iter, _##iter = _##iter->next)

/** Iterate over a list in reverse.
 * @note		Safe to use when the loop may modify the list.
 * @param list		Head of list to iterate.
 * @param iter		Variable name to set to node pointer on each iteration. */
#define LIST_FOREACH_REVERSE_SAFE(list, iter) \
	for(list_t *iter = (list)->prev, *_##iter = iter->prev; \
		iter != (list); iter = _##iter, _##iter = _##iter->prev)

/** Initializes a statically declared linked list. */
#define LIST_INITIALIZER(_var) \
	{ \
		.prev = &_var, \
		.next = &_var, \
	}

/** Statically declares a new linked list. */
#define LIST_DECLARE(_var) \
	list_t _var = LIST_INITIALIZER(_var)

/** Get a pointer to the structure containing a list node.
 * @param entry		List node pointer.
 * @param type		Type of the structure.
 * @param member	Name of the list node member in the structure.
 * @return		Pointer to the structure. */
#define list_entry(entry, type, member) \
	((type *)((char *)entry - offsetof(type, member)))

/** Get a pointer to the next structure in a list.
 * @note		Does not check if the next entry is the head.
 * @param entry		Current entry.
 * @param member	Name of the list node member in the structure.
 * @return		Pointer to the next structure. */
#define list_next(entry, member) \
	(list_entry((entry)->member.next, typeof(*(entry)), member))

/** Get a pointer to the previous structure in a list.
 * @note		Does not check if the previous entry is the head.
 * @param entry		Current entry.
 * @param member	Name of the list node member in the structure.
 * @return		Pointer to the previous structure. */
#define list_prev(entry, member) \
	(list_entry((entry)->member.prev, typeof(*(entry)), member))

/** Get a pointer to the first structure in a list.
 * @note		Does not check if the list is empty.
 * @param list		Head of the list.
 * @param type		Type of the structure.
 * @param member	Name of the list node member in the structure.
 * @return		Pointer to the first structure. */
#define list_first(list, type, member) \
	(list_entry((list)->next, type, member))

/** Get a pointer to the last structure in a list.
 * @note		Does not check if the list is empty.
 * @param list		Head of the list.
 * @param type		Type of the structure.
 * @param member	Name of the list node member in the structure.
 * @return		Pointer to the last structure. */
#define list_last(list, type, member) \
	(list_entry((list)->prev, type, member))

/** Checks whether the given list is empty.
 * @param list		List to check. */
static inline bool list_empty(const list_t *list) {
	return (list->prev == list && list->next == list);
}

/** Check if a list has only a single entry.
 * @param list		List to check. */
static inline bool list_is_singular(const list_t *list) {
	return (!list_empty(list) && list->next == list->prev);
}

/** Internal part of list_remove(). */
static inline void list_real_remove(list_t *entry) {
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
}

/** Initializes a linked list.
 * @param list		List to initialize. */
static inline void list_init(list_t *list) {
	list->prev = list->next = list;
}

/** Add an entry to a list before the given entry.
 * @param exist		Existing entry to add before.
 * @param entry		Entry to append. */
static inline void list_add_before(list_t *exist, list_t *entry) {
	list_real_remove(entry);

	exist->prev->next = entry;
	entry->next = exist;
	entry->prev = exist->prev;
	exist->prev = entry;
}

/** Add an entry to a list after the given entry.
 * @param exist		Existing entry to add after.
 * @param entry		Entry to append. */
static inline void list_add_after(list_t *exist, list_t *entry) {
	list_real_remove(entry);

	exist->next->prev = entry;
	entry->next = exist->next;
	entry->prev = exist;
	exist->next = entry;
}

/** Append an entry to a list.
 * @param list		List to append to.
 * @param entry		Entry to append. */
static inline void list_append(list_t *list, list_t *entry) {
	list_add_before(list, entry);
}

/** Prepend an entry to a list.
 * @param list		List to prepend to.
 * @param entry		Entry to prepend. */
static inline void list_prepend(list_t *list, list_t *entry) {
	list_add_after(list, entry);
}

/** Remove a list entry from its containing list.
 * @param entry		Entry to remove. */
static inline void list_remove(list_t *entry) {
	list_real_remove(entry);
	list_init(entry);
}

/** Splice the contents of one list onto another.
 * @param position	Entry to insert before.
 * @param list		Head of list to insert. Will become empty after the
 *			operation. */
static inline void list_splice_before(list_t *position, list_t *list) {
	if(!list_empty(list)) {
		list->next->prev = position->prev;
		position->prev->next = list->next;
		position->prev = list->prev;
		list->prev->next = position;

		list_init(list);
	}
}

/** Splice the contents of one list onto another.
 * @param position	Entry to insert after.
 * @param list		Head of list to insert. Will become empty after the
 *			operation. */
static inline void list_splice_after(list_t *position, list_t *list) {
	if(!list_empty(list)) {
		list->prev->next = position->next;
		position->next->prev = list->prev;
		position->next = list->next;
		list->next->prev = position;

		list_init(list);
	}
}

#endif /* __LIB_LIST_H */
