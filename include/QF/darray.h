/*
	darray.h

	Dynamic arrays

	Copyright (C) 2020 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2020/02/17

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

#ifndef __QF_darray_h
#define __QF_darray_h

#include <stdlib.h>

#include "QF/sys.h"

/** \defgroup darray Dynamic Arrays
	\ingroup utils

	Dynamic array container object
*/
///@{

/**	The structure defs for a dynamic array with elements of the given type.

	This is just the defs of a struct delcaration: it is useless on its own.
	The intended usage is something like:

		typedef struct dynamic_array_s DARRAY_TYPE(int) dynamic_array_t;

	This allows full flexibility in just how the actual type is used.

	The \a size field is the number of elements currently in the array, and
	the \a maxSize field is the number of elements the array can hold without
	being resized.

	The \a grow field specifies the number of elements by which \a maxSize is
	to grow when the array needs to be resized. Setting this to 0 prevents
	resizing and any attempt to do so is a fatal error.

	\param ele_type	The type to use for the element array, which is accessed
					by the \a a field.
	\hideinitializer
*/
#define DARRAY_TYPE(ele_type)	\
	{							\
		size_t      size;		\
		size_t      maxSize;	\
		size_t      grow;		\
		ele_type   *a;			\
	}

#define DARRAY_STATIC_INIT(g) { .grow = g }

/**	Allocate a fixed-size array using the given allocator

	The allocated array is initilized to be ungrowable, and with both size
	and maxSize set to the given size.

	\param array_type	Expression acceptable by typeof for determining the
						type of the array.
	\param array_size   The size of the array.
	\param alloc		Allocator compatible with malloc (eg, alloca).
*/
#define DARRAY_ALLOCFIXED(array_type, array_size, alloc)				\
	({																	\
		__auto_type s = (array_size);									\
		typeof (array_type) *ar = alloc (sizeof(*ar)					\
										 + s * sizeof (*ar->a));		\
		ar->size = ar->maxSize = s;										\
		ar->grow = 0;													\
		ar->a = (typeof (ar->a)) (ar + 1);								\
		ar;																\
	})

/**	Allocate a fixed-size array using the given allocator

	The allocated array is initilized to be ungrowable, and with both size
	and maxSize set to the given size.

	\param array_type	Expression acceptable by typeof for determining the
						type of the array.
	\param array_size   The size of the array.
	\param alloc		Allocator taking (obj, size) where obj is allocator
						specific data (eg, a memory pool).
	\param obj			Additional data for the allocator.
*/
#define DARRAY_ALLOCFIXED_OBJ(array_type, array_size, alloc, obj)		\
	({																	\
		__auto_type s = (array_size);									\
		typeof (array_type) *ar = alloc ((obj),							\
										 sizeof(*ar)					\
										 + s * sizeof (*ar->a));		\
		ar->size = ar->maxSize = s;										\
		ar->grow = 0;													\
		ar->a = (typeof (ar->a)) (ar + 1);								\
		ar;																\
	})

/**	Initialized the array.

	The array will be initialized to be empty but with grow set to the
	specifed value.

	\param array	*Address* of the array to be modified (ie, pointer to the
					array struct instance, not the instance itself: use & for
					static instances of the array struct).
	\param growSize Number of elements by which the array is to grow when
					required.
	\hideinitializer
*/
#define DARRAY_INIT(array, growSize)									\
	do {																\
		__auto_type ar = (array);										\
		ar->size = ar->maxSize = 0;										\
		ar->grow = (growSize);											\
		ar->a = 0;														\
	} while (0)

/**	Clear the array.

	If the array can grow, its backing will be freed and maxSize and a reset,
	otherwise maxSize and a are left untouched.

	\param array	*Address* of the array to be modified (ie, pointer to the
					array struct instance, not the instance itself: use & for
					static instances of the array struct).
	\hideinitializer
*/
#define DARRAY_CLEAR(array)												\
	do {																\
		__auto_type ar = (array);										\
		ar->size = 0;													\
		if (ar->grow) {													\
			free (ar->a);												\
			ar->maxSize = 0;											\
			ar->a = 0;													\
		}																\
	} while (0)

/**	Set the size of the array.

	If the new size is greater than maxSize, and the array can grow (grow is
	non-zero), then maxSize will be increased to the smallest multiple of grow
	greater than or equal to size (ie, maxSize >= size, maxSize % grow == 0).

	Attempting to increase maxSize on an array that cannot grow is an error:
	it is assumed that the array struct does not own the backing memory.

	\param array	*Address* of the array to be modified (ie, pointer to the
					array struct instance, not the instance itself: use & for
					static instances of the array struct).
	\param newSize	The new size of the array: newly opened slots are
					uninitialized, but old slots retain their values.
	\hideinitializer
*/
#define DARRAY_RESIZE(array, newSize)									\
	do {																\
		__auto_type ar = (array);										\
		size_t ns = (newSize);											\
		if (__builtin_expect (ns > ar->maxSize, 0)) {					\
			if (__builtin_expect (!ar->grow, 0)) {						\
				Sys_Error ("Attempt to grow fixed-size darray: %s:%d",	\
						   __FILE__, __LINE__);							\
			}															\
			ar->maxSize = ar->grow * ((ns + ar->grow - 1) / ar->grow);	\
			ar->a = realloc (ar->a, ar->maxSize * sizeof (*ar->a));		\
			if (__builtin_expect (!ar->a, 0)) {							\
				Sys_Error ("failed to realloc darray: %s:%d",			\
						   __FILE__, __LINE__);							\
			}															\
		}																\
		ar->size = ns;													\
	} while (0)

/**	Append a value to the end of the array.

	The array is grown by one and the value written to the newly opened slot.

	If the new array size is greater than maxSize and the array can be grown,
	the array backing will be resized to the next multiple of grow.

	Attempting to grow an array that cannot grow is an error: it is assumed
	that the array struct does not own the backing memory.

	\param array	*Address* of the array to be modified (ie, pointer to the
					array struct instance, not the instance itself: use & for
					static instances of the array struct).
	\param value	The value to be appended to the array. Must be of a type
					compatible with that used for creating the array struct.
	\return			The appended value: can be assigned to another compatible
					value.
	\hideinitializer
*/
#define DARRAY_APPEND(array, value)										\
	({																	\
		__auto_type ar = (array);										\
		typeof(ar->a[0]) ob = (value);									\
		size_t      sz = ar->size;										\
		DARRAY_RESIZE (ar, ar->size + 1);								\
		ar->a[sz] = ob;													\
	})

/**	Open a hole in the array for bulk copying of data.

	The array is grown by the requested size, opening a hole at the specified
	index. Values beyond the index are copied to just after the newly opened
	hole.

	If the new array size is greater than maxSize and the array can be grown,
	the array backing will be resized to the next multiple of grow.

	Attempting to grow an array that cannot grow is an error: it is assumed
	that the array struct does not own the backing memory.

	\param array	*Address* of the array to be modified (ie, pointer to the
					array struct instance, not the instance itself: use & for
					static instances of the array struct).
	\param index	The index at which the hole will begin.
	\param space	The sized of the hole to be opened, in array elements.
	\return			The *address* of the newly opened hole: can be passed to
					memcpy and friends.

						memcpy (DARRAY_OPEN_AT(array, index, size), data,
								size * sizeof (*data));
	\hideinitializer
*/
#define DARRAY_OPEN_AT(array, index, space)								\
	({																	\
		__auto_type ar = (array);										\
		size_t po = (index);											\
		size_t sp = (space);											\
		if (__builtin_expect (po > ar->size, 0)) {						\
			Sys_Error ("Attempt to insert elements outside darray: "	\
					   "%s:%d", __FILE__, __LINE__);					\
		}																\
		DARRAY_RESIZE (ar, ar->size + sp);								\
		memmove (&ar->a[po + sp], &ar->a[po],							\
				 (ar->size - po - sp) * sizeof (*ar->a));				\
		&ar->a[po];														\
	})

/**	Insert a value into the array at the specified index.

	The array is grown by one at the specified index and the value written
	to the newly opened slot. Values beyond the index are copied to just
	after the newly opened slot.

	If the new array size is greater than maxSize and the array can be grown,
	the array backing will be resized to the next multiple of grow.

	Attempting to grow an array that cannot grow is an error: it is assumed
	that the array struct does not own the backing memory.

	Attempting to insert a value beyond one past the end of the array is an
	error (inserting at index = size is valid).

	\param array	*Address* of the array to be modified (ie, pointer to the
					array struct instance, not the instance itself: use & for
					static instances of the array struct).
	\param value	The value to be inserted into the array. Must be of a type
					compatible with that used for creating the array struct.
	\param index	The index at which the value will be inserted
	\return			The inserted value: can be assigned to another compatible
					value.
	\hideinitializer
*/
#define DARRAY_INSERT_AT(array, value, index)							\
	({																	\
		__auto_type ar = (array);										\
		typeof(ar->a[0]) ob = (value);									\
		*DARRAY_OPEN_AT (ar, index, 1) = ob;							\
	})

/**	Close a segment of an array.

	The values beyond the segment are copied to the beginning of the segment
	and the array size reduced by the size of the segment. All but the first
	one of the values previously in the segment are lost and gone forever.

	Attempting to close a segment that extends outside the array is an error.

	\param array	*Address* of the array to be modified (ie, pointer to the
					array struct instance, not the instance itself: use & for
					static instances of the array struct).
	\param index	The index of the beginning of the segment.
	\param count	The number of values in the segment.
	\return			The single value at the beginning of the segment: can be
					assigned to another compatible value.
	\hideinitializer
*/
#define DARRAY_CLOSE_AT(array, index, count)							\
	({																	\
		__auto_type ar = (array);										\
		size_t po = (index);											\
		size_t co = (count);											\
		if (__builtin_expect (po + co > ar->size						\
							  || po >= ar->size, 0)) {					\
			Sys_Error ("Attempt to remove elements outside darray: "	\
					   "%s:%d", __FILE__, __LINE__);					\
		}																\
		__auto_type ob = ar->a[po];										\
		memmove (&ar->a[po], &ar->a[po + co],							\
				 (ar->size - po - co) * sizeof (ob));					\
		ar->size -= co;													\
		ob;																\
	})

/**	Remove a value from an array at the specified index.

	The values beyond the index are moved down to fill the hole left by the
	single value and the array size reduced by one.

	Attempting to remove a value from beyond the array is an error.

	\param array	*Address* of the array to be modified (ie, pointer to the
					array struct instance, not the instance itself: use & for
					static instances of the array struct).
	\param index	The index of the value to be removed.
	\return			The removed value: can be assigned to another compatible
					value.
	\hideinitializer
*/
#define DARRAY_REMOVE_AT(array, index)									\
	({																	\
		__auto_type ar = (array);										\
		DARRAY_CLOSE_AT (ar, index, 1);									\
	})

/**	Remove (pop) a value from the end of an array.

	The size of the array size reduced by one.

	Attempting to remove a value from an empty array is an error.

	\param array	*Address* of the array to be modified (ie, pointer to the
					array struct instance, not the instance itself: use & for
					static instances of the array struct).
	\return			The removed value: can be assigned to another compatible
					value.
	\hideinitializer
*/
#define DARRAY_REMOVE(array)											\
	({																	\
		__auto_type ar = (array);										\
		DARRAY_CLOSE_AT (ar, ar->size - 1, 1);							\
	})

///@}

#endif//__QF_darray_h
