/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2002 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/gib_function.h"

hashtab_t *gib_functions = 0;

gib_function_t *
GIB_Function_New (void)
{
	gib_function_t *new = calloc (1, sizeof (gib_function_t));
	new->name = dstring_newstr();
	new->program = dstring_newstr();
	
	return new;
}

const char *
GIB_Function_Get_Key (void *ele, void *ptr)
{
	return ((gib_function_t *)ele)->name->str;
}

void
GIB_Function_Free (void *ele, void *ptr)
{
	gib_function_t *func = (gib_function_t *)ele;
	dstring_delete (func->name);
	dstring_delete (func->program);
}

void
GIB_Function_Define (const char *name, const char *program)
{
	gib_function_t *func;
	
	if (!gib_functions)
		gib_functions = Hash_NewTable (1024, GIB_Function_Get_Key, GIB_Function_Free, 0);
	func = Hash_Find(gib_functions, name);
	if (func)
		dstring_clearstr (func->program);
	else {
		func = GIB_Function_New ();
		dstring_appendstr (func->name, name);
		Hash_Add (gib_functions, func);
	}
	dstring_appendstr (func->program, program);
}

gib_function_t *
GIB_Function_Find (const char *name)
{
	if (!gib_functions)
		return 0;
	return (gib_function_t *) Hash_Find (gib_functions, name);
}
