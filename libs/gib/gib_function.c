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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] =
        "$Id$";

#include <stdlib.h>

#include "QF/sys.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/cbuf.h"
#include "QF/gib_parse.h"
#include "QF/gib_buffer.h"
#include "QF/gib_function.h"
#include "QF/gib_vars.h"
#include "QF/va.h"

hashtab_t *gib_functions = 0;

/*
	GIB_Function_New
	
	Builds a new function struct and returns
	a pointer to it.
*/
static gib_function_t *
GIB_Function_New (void)
{
	gib_function_t *new = calloc (1, sizeof (gib_function_t));
	new->name = dstring_newstr();
	new->program = dstring_newstr();
	
	return new;
}

/* 
	Hashtable callbacks
*/
static const char *
GIB_Function_Get_Key (void *ele, void *ptr)
{
	return ((gib_function_t *)ele)->name->str;
}
static void
GIB_Function_Free (void *ele, void *ptr)
{
	gib_function_t *func = (gib_function_t *)ele;
	dstring_delete (func->name);
	dstring_delete (func->program);
	free (func);
}

/*
	GIB_Function_Define
	
	Sets the program text of a GIB function,
	allocating one and adding it to the functions
	hash if needed.
*/
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

/*
	GIB_Function_Find
	
	Looks up a function in the function hash
	and returns a pointer to it on success,
	0 otherwise
*/
gib_function_t *
GIB_Function_Find (const char *name)
{
	if (!gib_functions)
		return 0;
	return (gib_function_t *) Hash_Find (gib_functions, name);
}

void
GIB_Function_Prepare_Args (cbuf_t *cbuf, cbuf_args_t *args)
{
	int i;
	
	for (i = 0; i < args->argc; i++)
		GIB_Var_Set_Local (cbuf, va("%i", i), args->argv[i]->str);
	GIB_Var_Set_Local (cbuf, "argc", va("%i", args->argc));
}

/*
	GIB_Function_Execute
	
	Prepares a buffer to execute
	a GIB function with certain arguments
*/

void
GIB_Function_Execute (cbuf_t *cbuf, gib_function_t *func, cbuf_args_t *args)
{
	Cbuf_AddText (cbuf, func->program->str);
	GIB_Function_Prepare_Args (cbuf, args);
}
