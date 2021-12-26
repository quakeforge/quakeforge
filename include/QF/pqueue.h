/*
	pqueue.h

	Priority Queues

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/08/02

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#ifndef __QF_pqueue_h
#define __QF_pqueue_h

#include <stdlib.h>

#include "QF/heapsort.h"
#include "QF/sys.h"

/** \defgroup pqueue Priority Queues
	\ingroup utils

	Priority queue management
*/
///@{

/** The structure def for a priority queue with elements of the given type.

	This is just the def of a struct delcaration: it is useless on its own.
	The intended usage is something like:

		typedef struct priority_queue_s PQUEUE_TYPE (int) priority_queue_t;

	This allows full flexibility in just how the actual type is used.

	The \a size field is the number of elements currently in the queue, and
	the \a maxSize field is the number of elements the queue can hold. The
	priority queue functions will never resize the array. However, with a
	little glue logic, priority queues can be used with dynamic arrays.

	\param T		The type to use for the element array, which is accessed
					by the \a a field.
*/
#define PQUEUE_TYPE(T)												\
	{																\
		size_t      size;											\
		size_t      maxSize;										\
		T          *a;												\
		int       (*compare) (const T *a, const T *b, void *data);	\
		void       *data;											\
	}

#define PQUEUE_IS_EMPTY(queue) ((queue)->size == 0)

#define PQUEUE_IS_FULL(queue)		\
	({								\
		__auto_type q = (queue);	\
		q->size == q->maxSize;		\
	})

#define PQUEUE_PEEK(queue) ((queue)->a[0])

#define PQUEUE_INSERT(queue, value) 								\
	do {															\
		__auto_type q = (queue);									\
		if (PQUEUE_IS_FULL (q)) {									\
			Sys_Error ("Priority queue full");						\
		}															\
		q->a[q->size++] = (value);									\
		heap_swim_r (q->a, q->size - 1, q->size, sizeof (q->a[0]),	\
					 (__compar_d_fn_t) q->compare, q->data);		\
	} while (0)

#define PQUEUE_REMOVE(queue)									\
	({															\
		__auto_type q = (queue);								\
		if (PQUEUE_IS_EMPTY (q)) {								\
			Sys_Error ("Priority queue empty");					\
		}														\
		__auto_type value = q->a[0];							\
		q->a[0] = q->a[--q->size];								\
		heap_sink_r (q->a, 0, q->size, sizeof (q->a[0]),		\
					 (__compar_d_fn_t) q->compare, q->data);	\
		value;													\
	})

#define PQUEUE_ADJUST(queue, index)											\
	do {																	\
		__auto_type q = (queue);											\
		size_t      ind = (index);											\
		if (ind >= q->size) {												\
			Sys_Error ("Priority queue index error");						\
		}																	\
		if (ind>0 && q->compare(q->a + ind, q->a + (ind-1)/2, q->data)>0) {	\
			heap_swim_r (q->a, ind, q->size, sizeof (q->a[0]),				\
						 (__compar_d_fn_t) q->compare, q->data);			\
		} else {															\
			heap_sink_r (q->a, ind, q->size, sizeof (q->a[0]),				\
						 (__compar_d_fn_t) q->compare, q->data);			\
		}																	\
	} while (0)

///@}

#endif//__QF_pqueue_h
