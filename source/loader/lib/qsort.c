/* $Id: qsort.c 271 2008-05-28 06:16:15Z solar $ */

/* qsort( void *, size_t, size_t, int(*)( const void *, const void * ) )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <lib/utility.h>

/* This implementation is taken from Paul Edward's PDPCLIB.

   Original code is credited to Raymond Gardner, Englewood CO.
   Minor mods are credited to Paul Edwards.
   Some reformatting and simplification done by Martin Baute.
   All code is still Public Domain.
*/

static inline void memswp( char * item1, char * item2, size_t size )
{
	char ch;
	size_t i;

	for(i = 0; i < size; i++, item1++, item2++) {
		ch = *item1;

		*item1 = *item2;
		*item2 = ch;
	}
}

/* For small sets, insertion sort is faster than quicksort.
   T is the threshold below which insertion sort will be used.
   Must be 3 or larger.
*/
#define T 7

/* Macros for handling the QSort stack */
#define PREPARE_STACK char * stack[STACKSIZE]; char * * stackptr = stack
#define PUSH( base, limit ) stackptr[0] = base; stackptr[1] = limit; stackptr += 2
#define POP( base, limit ) stackptr -= 2; base = stackptr[0]; limit = stackptr[1]
/* TODO: This is platform-dependent */
#define STACKSIZE 40

void qsort( void * base, size_t nmemb, size_t size, int (*compar)( const void *, const void * ) )
{
    char * i;
    char * j;
    ptrdiff_t thresh  = T * size;
    char * base_              = (char *)base;
    char * limit              = base_ + nmemb * size;
    PREPARE_STACK;

    for ( ;; )
    {
        if ( limit - base_ > thresh ) /* QSort for more than T elements. */
        {
            /* We work from second to last - first will be pivot element. */
            i = base_ + size;
            j = limit - size;
            /* We swap first with middle element, then sort that with second
               and last element so that eventually first element is the median
               of the three - avoiding pathological pivots.
               TODO: Instead of middle element, chose one randomly.
            */
            memswp( ( ( ( (size_t)( limit - base_ ) ) / size ) / 2 ) * size + base_, base_, size );
            if ( compar( i, j ) > 0 ) memswp( i, j, size );
            if ( compar( base_, j ) > 0 ) memswp( base_, j, size );
            if ( compar( i, base_ ) > 0 ) memswp( i, base_, size );
            /* Now we have the median for pivot element, entering main Quicksort. */
            for ( ;; )
            {
                do
                {
                    /* move i right until *i >= pivot */
                    i += size;
                } while ( compar( i, base_ ) < 0 );
                do
                {
                    /* move j left until *j <= pivot */
                    j -= size;
                } while ( compar( j, base_ ) > 0 );
                if ( i > j )
                {
                    /* break loop if pointers crossed */
                    break;
                }
                /* else swap elements, keep scanning */
                memswp( i, j, size );
            }
            /* move pivot into correct place */
            memswp( base_, j, size );
            /* larger subfile base / limit to stack, sort smaller */
            if ( j - base_ > limit - i )
            {
                /* left is larger */
                PUSH( base_, j );
                base_ = i;
            }
            else
            {
                /* right is larger */
                PUSH( i, limit );
                limit = j;
            }
        }
        else /* insertion sort for less than T elements              */
        {
            for ( j = base_, i = j + size; i < limit; j = i, i += size )
            {
                for ( ; compar( j, j + size ) > 0; j -= size )
                {
                    memswp( j, j + size, size );
                    if ( j == base_ )
                    {
                        break;
                    }
                }
            }
            if ( stackptr != stack )           /* if any entries on stack  */
            {
                POP( base_, limit );
            }
            else                       /* else stack empty, done   */
            {
                break;
            }
        }
    }
}
