/*
	object.c

	Provides a primitive object framework to back objects
	in higher level languages of QF so that they can share
	objects.  For example, Ruamoko and GIB would be able to
	pass String objects to each other, even if the higher
	-level implementations of String in each language differ.

	Copyright (C) 2003 Brian Koropoff

	Author: Brian Koropoff
	Date: November 28, 2003

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

#include "QF/dstring.h"
#include "QF/sys.h"
#include "QF/object.h"
#include "QF/va.h"

static String *
Object_ToString_f (Object *self) 
{
	return newFloat(String, va("%s@%p", self->cl->name, self));
}

static void
Object_Init_f (Object *self) 
{
	self->refs = -1; // Floating reference
	self->toString = Object_ToString_f;
	Sys_DPrintf("%s@%p initing...\n", self->cl->name, self);
}

static void
Object_Deinit_f (Object *self)
{
	Sys_DPrintf("%s@%p deiniting...\n", self->cl->name, self);
	free (self);
}

static String *
Class_ToString_f (Object *self)
{
	return newFloat(String, CLASS(self)->name);
}	

static Object *
Class_Init_f (Object *self, const char *name, unsigned int size, Class *parent, void *init, void *deinit, qboolean abstract)
{
	superInit(Class, self);
	CLASS(self)->name = strdup(name);
	CLASS(self)->size = size;
	CLASS(self)->parent = parent;
	CLASS(self)->init = (Object_Init_t) init;
	CLASS(self)->deinit = (Object_Deinit_t) deinit;
	self->toString = Class_ToString_f;
	return self;
}

static void
Class_Deinit_f (Object *self) {
	free ((void *)CLASS(self)->name);
}

static String *
String_ToString_f (Object *self)
{
	return STRING(self);
}

static Object *
String_Init_f (Object *self, const char *value)
{
	superInit(String, self);
	self->toString = String_ToString_f;
	STRING(self)->str = strdup (value);
	return self;
}

static void
String_Deinit_f (Object *self)
{
	free((void *)STRING(self)->str);
}

Object *
Object_Create (Class *cl)
{
	Object *new = malloc (cl->size);
	new->cl = cl;
	return new;
}

void
Object_Delete (Object *obj)
{
	Class *c;
	for (c = obj->cl; c; c = c->parent)
		c->deinit (obj);
}

Object *
Object_Retain (Object *obj)
{
	if (obj->refs == -1) {
		obj->refs = 1;
	} else {
		obj->refs++;
	}
	return obj;
}

Object *
Object_Release (Object *obj)
{
	if (obj->refs == -1)
		Sys_Error ("%s@%p with floating reference released.", obj->cl->name, obj);
	if (--obj->refs < 1) {
		Object_Delete (obj);
		return NULL;
	}
	return obj;
}

qboolean
Object_InstanceOf (Object *obj, Class *cl)
{
	Class *c;
	for (c = obj->cl; c; c = c->parent)
		if (c == cl)
			return true;
	return false;
}

static String *
List_ToString_f (Object *self)
{
	List *list = LIST(self);
	String *str;
	unsigned int i;
	dstring_t *dstr = dstring_newstr();

	dstring_appendstr (dstr, "{");
	
	for (i = 0; i < list->count; i++) {
		str = methodCall(methodCall(list, get, i), toString);
		retain(str);
		dstring_appendstr (dstr, str->str);
		release(str);
		if (i < list->count - 1)
			dstring_appendstr (dstr, ", ");
	}
	dstring_appendstr (dstr, "}");
	str = newFloat(String, dstr->str);
	dstring_delete (dstr);
	return str;
}
		
static Object *
List_Init_f (Object *self)
{
	superInit(List, self);
	LIST(self)->count = 0;
	LIST(self)->get = NULL;
	LIST(self)->add = NULL;
	self->toString = List_ToString_f;
	return self;
}

static void
List_Deinit_f (Object *self)
{
}

static Object *
ArrayList_Get_f (List *self, unsigned int index)
{
	if (index >= self->count)
		return NULL;
	return ARRAYLIST(self)->elements[index];
}

static qboolean
ArrayList_Add_f (List *self, Object *o)
{
	self->count++;
	ARRAYLIST(self)->elements = realloc (ARRAYLIST(self)->elements, self->count);
	ARRAYLIST(self)->elements[self->count-1] = o;
	retain (o);
	return true;
}

static Object *
ArrayList_Init_f (Object *self)
{
	superInit (ArrayList, self);
	ARRAYLIST(self)->elements = NULL;
	LIST(self)->get = ArrayList_Get_f;
	LIST(self)->add = ArrayList_Add_f;
	return self;
}

static void
ArrayList_Deinit_f (Object *self)
{
	unsigned int i;
	Object **elements = ARRAYLIST(self)->elements;

	for (i = 0; i < LIST(self)->count; i++)
		release (elements[i]);
	if (elements)
		free (elements);
}

static Object *
Number_Init_f (Object *self)
{
	superInit (Number, self);
	NUMBER(self)->intValue = NULL;
	NUMBER(self)->doubleValue = NULL;
	return self;
}

static void
Number_Deinit_f (Object *self)
{
}

static String *
Integer_ToString_f (Object *self)
{
	return newFloat(String, va("%i", INTEGER(self)->value));
}

static int
Integer_IntValue_f (Number *self)
{
	return INTEGER(self)->value;
}

static double
Integer_DoubleValue_f (Number *self)
{
	return (double) INTEGER(self)->value;
}

static Object *
Integer_Init_f (Object *self, int value)
{
	superInit (Integer, self);
	INTEGER(self)->value = value;
	NUMBER(self)->intValue = Integer_IntValue_f;
	NUMBER(self)->doubleValue = Integer_DoubleValue_f;
	self->toString = Integer_ToString_f;
	return self;
}

static void
Integer_Deinit_f (Object *self)
{
}

Class *Object_class;
Class *Class_class;
Class *String_class;
Class *List_class;
Class *ArrayList_class;
Class *Number_class;
Class *Integer_class;

static void
Object_Test (void)
{
	List *l = new(ArrayList);
	String *str;

	methodCall(l, add, newFloat(Integer, 5));
	methodCall(l, add, newFloat(String, "Daisy"));
	methodCall(l, add, newFloat(Integer, 42));

	str = methodCall(OBJECT(l), toString);
	retain(str);
	Sys_DPrintf("List: %s\n", str->str);
	release(str);
	release(l);
}

void
Object_Init (void)
{
	/* There is somewhat of a chicken and egg problem
	   here.
	*/
	Object_class = malloc (sizeof (struct Class_s));
	Class_class = malloc (sizeof (struct Class_s));
	OBJECT(Object_class)->cl = Class_class;
	OBJECT(Class_class)->cl = Class_class;
	Class_class->parent = Object_class;
	Object_class->init = (Object_Init_t) Object_Init_f; 
	
	Class_Init_f (OBJECT(Object_class), "Object", sizeof(Object), NULL, Object_Init_f, Object_Deinit_f, true);
	Class_Init_f (OBJECT(Class_class), "Class", sizeof(Class), Object_class, Class_Init_f, Class_Deinit_f, false);
	retain(Object_class);
	retain(Class_class);
	/* Phew... Object and Class are now bootstrapped,
	   classes can now be created by instantiating
	   Class
	*/

	/* Create String class normally */
	String_class = new(Class, "String", sizeof(String), Object_class, String_Init_f, String_Deinit_f, false);
	/* Etc... */
	List_class = new(Class, "List", sizeof(List), Object_class, List_Init_f, List_Deinit_f, true);
	ArrayList_class = new(Class, "ArrayList", sizeof(ArrayList), List_class, ArrayList_Init_f, ArrayList_Deinit_f, false);
	Number_class = new(Class, "Number", sizeof(Number), Object_class, Number_Init_f, Number_Deinit_f, true);
	Integer_class = new(Class, "Integer", sizeof(Integer), Number_class, Integer_Init_f, Integer_Deinit_f, false);

	/* Run a test! */
	Object_Test();
}
