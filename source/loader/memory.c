/*
 * Copyright (C) 2010-2014 Alex Smith
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
 * @brief		Memory management functions.
 */

#include <lib/list.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>

/** Structure representing an area on the heap. */
typedef struct heap_chunk {
	list_t header;			/**< Link to chunk list. */
	size_t size;			/**< Size of chunk including struct. */
	bool allocated;			/**< Whether the chunk is allocated. */
} heap_chunk_t;

/** Size of the heap (128KB). */
#define HEAP_SIZE		131072

/** Statically allocated heap. */
static uint8_t heap[HEAP_SIZE] __aligned(PAGE_SIZE);
static LIST_DECLARE(heap_chunks);

/** Allocate memory from the heap.
 * @note		An internal error will be raised if heap is full.
 * @param size		Size of allocation to make.
 * @return		Address of allocation. */
void *malloc(size_t size) {
	heap_chunk_t *chunk = NULL, *new;
	size_t total;

	if(size == 0)
		internal_error("Zero-sized allocation!");

	/* Align all allocations to 8 bytes. */
	size = round_up(size, 8);
	total = size + sizeof(heap_chunk_t);

	/* Create the initial free segment if necessary. */
	if(list_empty(&heap_chunks)) {
		chunk = (heap_chunk_t *)heap;
		chunk->size = HEAP_SIZE;
		chunk->allocated = false;
		list_init(&chunk->header);
		list_append(&heap_chunks, &chunk->header);
	} else {
		/* Search for a free chunk. */
		LIST_FOREACH(&heap_chunks, iter) {
			chunk = list_entry(iter, heap_chunk_t, header);
			if(!chunk->allocated && chunk->size >= total) {
				break;
			} else {
				chunk = NULL;
			}
		}

		if(!chunk)
			internal_error("Exhausted heap space (want %zu bytes)", size);
	}

	/* Resize the segment if it is too big. There must be space for a
	 * second chunk header afterwards. */
	if(chunk->size >= (total + sizeof(heap_chunk_t))) {
		new = (heap_chunk_t *)((char *)chunk + total);
		new->size = chunk->size - total;
		new->allocated = false;
		list_init(&new->header);
		list_add_after(&chunk->header, &new->header);
		chunk->size = total;
	}

	chunk->allocated = true;
	return ((char *)chunk + sizeof(heap_chunk_t));
}

/** Resize a memory allocation.
 * @param addr		Address of old allocation.
 * @param size		New size of allocation.
 * @return		Address of new allocation, or NULL if size is 0. */
void *realloc(void *addr, size_t size) {
	heap_chunk_t *chunk;
	void *new;

	if(size == 0) {
		free(addr);
		return NULL;
	} else {
		new = malloc(size);
		if(addr) {
			chunk = (heap_chunk_t *)((char *)addr - sizeof(heap_chunk_t));
			memcpy(new, addr, min(chunk->size - sizeof(heap_chunk_t), size));
			free(addr);
		}
		return new;
	}
}

/** Free memory allocated with free().
 * @param addr		Address of allocation. */
void free(void *addr) {
	heap_chunk_t *chunk, *adj;

	if(!addr)
		return;

	/* Get the chunk and free it. */
	chunk = (heap_chunk_t *)((char *)addr - sizeof(heap_chunk_t));
	if(!chunk->allocated)
		internal_error("Double free on address %p", addr);
	chunk->allocated = false;

	/* Coalesce adjacent free segments. */
	if(chunk->header.next != &heap_chunks) {
		adj = list_entry(chunk->header.next, heap_chunk_t, header);
		if(!adj->allocated) {
			assert(adj == (heap_chunk_t *)((char *)chunk + chunk->size));
			chunk->size += adj->size;
			list_remove(&adj->header);
		}
	}
	if(chunk->header.prev != &heap_chunks) {
		adj = list_entry(chunk->header.prev, heap_chunk_t, header);
		if(!adj->allocated) {
			assert(chunk == (heap_chunk_t *)((char *)adj + adj->size));
			adj->size += chunk->size;
			list_remove(&chunk->header);
		}
	}
}
