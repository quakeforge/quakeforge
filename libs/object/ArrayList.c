/*
	ArrayList.c

	Implements List by storing elements in a dynamic array

	Copyright (C) 2003 Brian Koropoff

	Author: Brian Koropoff
	Date: December 03, 2003

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/classes/ArrayList.h"

static Object *
ArrayListIterator_Next_f (Iterator *self)
{
	/* Was list structurally modified while we are iterating? */
	if (ARRAYLISTITERATOR(self)->smods != LIST(ARRAYLISTITERATOR(self)->list)->smods) {
		ARRAYLISTITERATOR(self)->alive = false;
		// FIXME: Throw an exception or something
	}
	if (ARRAYLISTITERATOR(self)->alive) {
		Object *o = ARRAYLISTITERATOR(self)->list->elements[ARRAYLISTITERATOR(self)->pos++];
		if (ARRAYLISTITERATOR(self)->pos >= COLLECTION(ARRAYLISTITERATOR(self)->list)->count)
			ARRAYLISTITERATOR(self)->alive = false;
		return o;
	}
	return NULL;
}

static qboolean
ArrayListIterator_HasNext_f (Iterator *self)
{
	if (ARRAYLISTITERATOR(self)->smods != LIST(ARRAYLISTITERATOR(self)->list)->smods)
		ARRAYLISTITERATOR(self)->alive = false;
	return ARRAYLISTITERATOR(self)->alive;
}

static Object *
ArrayListIterator_Init_f (Object *self, ArrayList *list)
{
	superInit (ArrayListIterator, self);
	ITERATOR(self)->next = ArrayListIterator_Next_f;
	ITERATOR(self)->hasNext = ArrayListIterator_HasNext_f;
	ARRAYLISTITERATOR(self)->list = ARRAYLIST(retain(list));
	ARRAYLISTITERATOR(self)->smods = LIST(list)->smods;
	ARRAYLISTITERATOR(self)->pos = 0;
	ARRAYLISTITERATOR(self)->alive = COLLECTION(list)->count ? true : false;
	return self;
}

static void
ArrayListIterator_Deinit_f (Object *self)
{
	release(ARRAYLISTITERATOR(self)->list);
}

static void
ArrayList_EnsureCapacity (ArrayList *list, unsigned int size)
{
	if (list->realsize < size) {
		unsigned int newsize = size | (list->realsize + list->realsize/2);
		list->elements = realloc (list->elements, newsize);
		memset (list->elements + list->realsize, 0, newsize - list->realsize);
		list->realsize = newsize;
	}
}

static qboolean
ArrayList_Set_f (List *self, unsigned int index, Object *o)
{
	ArrayList *list = ARRAYLIST(self);
	if (o) {
		if (!Object_InstanceOf (o, COLLECTION(list)->type))
			return false;
		ArrayList_EnsureCapacity (list, index+1);
		retain(o);
		if (list->elements[index])
			release (list->elements[index]);
		list->elements[index] = o;
	} else if (index < COLLECTION(list)->count) {
		if (list->elements[index])
			release (list->elements[index]);
		list->elements[index] = NULL;
	}
	if (COLLECTION(list)->count < index+1)
		COLLECTION(list)->count = index+1;
	return true;
}

static Object *
ArrayList_Get_f (List *self, unsigned int index)
{
	ArrayList *list = ARRAYLIST(self);
	if (index >= COLLECTION(list)->count || index >= list->realsize)
		return NULL;
	else
		return list->elements[index];
}

static void
ArrayList_MakeRoomAt (ArrayList *list, unsigned int index)
{
	ArrayList_EnsureCapacity(list, ++COLLECTION(list)->count);
	memmove(list->elements+index+1, list->elements+index, sizeof(Object *) * (COLLECTION(list)->count-index-1));
	LIST(list)->smods++;
}

static qboolean
ArrayList_InsertAt_f (List *self, unsigned int index, Object *o)
{
	ArrayList *list = ARRAYLIST(self);
	if (index >= COLLECTION(list)->count)
		return ArrayList_Set_f (self, index, o);
	else if (o && !Object_InstanceOf(o, COLLECTION(list)->type))
		return false;
	else {
		ArrayList_MakeRoomAt (list, index);
		retain(o);
		list->elements[index] = o;
		return true;
	}
}
		
static Object *
ArrayList_RemoveAt_f (List *self, unsigned int index)
{
	ArrayList *list = ARRAYLIST(self);
	if (index >= COLLECTION(list)->count)
		return NULL;
	else {
		Object *o = list->elements[index];
		if (o)
			release(o);
		COLLECTION(list)->count--;
		memmove(list->elements+index, list->elements+index+1, sizeof(Object *) * (COLLECTION(list)->count-index));
		LIST(list)->smods++;
		return o;
	}
}

static qboolean
ArrayList_Add_f (Collection *self, Object *o)
{
	ArrayList *list = ARRAYLIST(self);
	if (o && !Object_InstanceOf(o, COLLECTION(list)->type))
		return false;
	else {
		ArrayList_EnsureCapacity (list, ++COLLECTION(list)->count);
		list->elements[COLLECTION(list)->count-1] = o;
		if (o)
			retain(o);
		return true;
	}
}

static Iterator *
ArrayList_Iterator_f (Collection *self)
{
	return newFloat(ArrayListIterator, ARRAYLIST(self));
}

static Object *
ArrayList_Init_f (Object *self, Class *type, Collection *source)
{
	ARRAYLIST(self)->realsize = 32;
	ARRAYLIST(self)->elements = calloc(32, sizeof(Object *));
	LIST(self)->set = ArrayList_Set_f;
	LIST(self)->get = ArrayList_Get_f;
	LIST(self)->insertAt = ArrayList_InsertAt_f;
	LIST(self)->removeAt = ArrayList_RemoveAt_f;
	COLLECTION(self)->add = ArrayList_Add_f;
	COLLECTION(self)->iterator = ArrayList_Iterator_f;
	superInit(List, self, type, source);
	return self;
}

static void
ArrayList_Deinit_f (Object *self)
{
	unsigned int i;
	for (i = 0; i < COLLECTION(self)->count; i++)
		if (ARRAYLIST(self)->elements[i])
			release(ARRAYLIST(self)->elements[i]);
	free(ARRAYLIST(self)->elements);
}

classInitFunc(ArrayListIterator) {
	classObj (ArrayListIterator) = new (Class, 
			"ArrayListIterator", sizeof(ArrayListIterator), classObj(Iterator), 
			ArrayListIterator_Init_f, ArrayListIterator_Deinit_f, false);
}

classInitFunc(ArrayList) {
	classInit (ArrayListIterator);
	classObj (ArrayList) = new (Class, 
			"ArrayList", sizeof(ArrayList), classObj(List), 
			ArrayList_Init_f, ArrayList_Deinit_f, false);
}
