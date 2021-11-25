/*
	listener.h

	Listener objects

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/11/25

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

#ifndef __QF_listener_h
#define __QF_listener_h

#include "QF/darray.h"

/** \defgroup listener Listeners
	\ingroup utils

	Listener objects
*/
///@{

/** The structure defs for a listener set for an object of the given type.

	This is just the defs of a struct delcaration: it is useless on its own.
	The intended usage is something like:

		typedef struct obj_listenerset_s LISTENER_SET_TYPE(obj_t)
			obj_listenerset_t;

	This allows full flexibility in just how the actual type is used.

	The \a lfunc field is the function that will be called whenever the
	listener is invoked. \a ldata is an aribtrary pointer that is passed to
	the function as its \a data parameter. The function's \a obj parameter
	is the \a obj parameter passed to LISTENER_INVOKE(), and is intended to
	be the object being listened to.

	There can be any number of \a lfunc, \a ldata pairs, but they should be
	unique, though this is not enforced.

	\param type		The type of the listened objects. A pointer to an object
					being listend to is passed to LISTENER_INVOKE() and that
					pointer is passed on to each listener function in the
					listener set.
	\hideinitializer
*/
#define LISTENER_SET_TYPE(type) \
	DARRAY_TYPE(struct {									\
		void      (*lfunc) (void *data, const type *obj);	\
		void       *ldata;									\
	})

#define LISTENER_SET_STATIC_INIT(g) DARRAY_STATIC_INIT(g)

/** Initialize the listener set.

	The set is initialized to be emtpy.

	\param lset		The *address* of the listener set being initialized (ie,
					a pointer to the listener set).
	\param growSice	Amount by which the underlying array will grow.
	\hideinitializer
*/
#define LISTENER_SET_INIT(lset, growSice) DARRAY_INIT(lset, growSice)

/** Add a listener function/data par to the listener set.

	The function's first parameter is the \a data pointer in the \a func
	\a data pair.  The function's second parameter is the \a obj parameter
	of LISTENER_INVOKE(). Thus each added listener function will be called
	with its associated data pointer and the \a obj parameter from
	LISTENER_INVOKE().

	Each pair should be unique in order to avoid double-action and so
	listeners can be removed properly.

	\param lset		The *address* of the listener set being modified (ie,
					a pointer to the listener set).
	\param func		The function to be called when the listener is invoked.
	\param data		Arbitrary pointer passed on to the function as its first
					parameter when when the listener is invoked.
	\hideinitializer
*/
#define LISTENER_ADD(lset, func, data)	\
	do {																\
		__auto_type ls = (lset);										\
		typeof(ls->a[0]) l = {(func), (data)};							\
		DARRAY_APPEND (ls, l);											\
	} while (0)

/** Remove a listener function/data pair from the listener set.

	Each individual listener is identifed by its \a func \a data pair. Only
	the first instance of a pair is removed, thus only unique pairs should be
	added.

	\param lset		The *address* of the listener set being modified (ie,
					a pointer to the listener set).
	\param func		The listener function being removed.
	\param data		The function's associated data pointer.
	\hideinitializer
*/
#define LISTENER_REMOVE(lset, func, data)									\
	do {																	\
		__auto_type ls = (lset);											\
		typeof(ls->a[0]) l = {(func), (data)};								\
		for (size_t i = 0; i < ls->size; i++) {								\
			if (ls->a[i].lfunc == l.lfunc && ls->a[i].ldata == l.ldata) {	\
				DARRAY_REMOVE_AT (ls, i);									\
				break;														\
			}																\
		}																	\
	} while (0)

/** Call all listener functions in the listener set.

	Each listener function is passed its data pointer as the first parameter
	and \a obj as the second parameter.

	\param lset		The *address* of the listener set being invoked (ie,
					a pointer to the listener set).
	\param obj		Pointer passed to each listener function as its second
					parameter.
	\hideinitializer
*/
#define LISTENER_INVOKE(lset, obj)										\
	do {																\
		__auto_type ls = (lset);										\
		__auto_type o = (obj);											\
		for (size_t i = 0; i < ls->size; i++) {							\
			ls->a[i].lfunc (ls->a[i].ldata, o);							\
		}																\
	} while (0)

///@}

#endif//__QF_listener_h
