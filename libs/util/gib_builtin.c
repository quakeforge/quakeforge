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

#include "QF/sys.h"
#include "QF/cmd.h"
#include "QF/cbuf.h"
#include "QF/hash.h"
#include "QF/dstring.h"
#include "QF/gib_builtin.h"
#include "QF/gib_buffer.h"
#include "QF/gib_function.h"

hashtab_t *gib_builtins;

const char * 
GIB_Builtin_Get_Key (void *ele, void *ptr)
{
	return ((gib_builtin_t *)ele)->name->str;
}

void
GIB_Builtin_Free (void *ele, void *ptr)
{
	gib_builtin_t *b;
	b = (gib_builtin_t *) ele;
	dstring_delete (b->name);
	free (b);
}

void
GIB_Builtin_Add (const char *name, void (*func) (void))
{
	gib_builtin_t *new;
	
	if (!gib_builtins)
		gib_builtins = Hash_NewTable (1024, GIB_Builtin_Get_Key, GIB_Builtin_Free, 0);
	
	new = calloc (1, sizeof (gib_builtin_t));
	new->func = func;
	new->name = dstring_newstr();
	dstring_appendstr (new->name, name);
	Hash_Add (gib_builtins, new);
}

gib_builtin_t *
GIB_Builtin_Find (const char *name)
{
	if (!gib_builtins)
		return 0;
	return (gib_builtin_t *) Hash_Find (gib_builtins, name);
}

unsigned int
GIB_Argc (void)
{
	return cbuf_active->args->argc;
}

const char *
GIB_Argv (unsigned int arg)
{
	if (arg < cbuf_active->args->argc)
		return cbuf_active->args->argv[arg]->str;
	else
		return "";
}

void
GIB_Function_f (void)
{
	if (GIB_Argc () != 3)
		Cbuf_Error ("numargs", 
					"function: invalid number of arguments\n"
					"usage: function function_name {program}");
	else
		GIB_Function_Define (GIB_Argv(1), GIB_Argv(2));
}			

void
GIB_Lset_f (void)
{
	if (GIB_Argc () != 3)
		Cbuf_Error ("numargs",
					"lset: invalid number of arguments\n"
					"usage: lset variable value");
	else
		GIB_Local_Set (cbuf_active, GIB_Argv(1), GIB_Argv(2));
}

void
GIB_Return_f (void)
{
	if (GIB_Argc () > 2)
		Cbuf_Error ("numargs",
					"return: invalid number of arguments\n"
					"usage: return <value>");
	else {
		dstring_clearstr (cbuf_active->buf);
		if (GIB_Argc () == 1)
			return;
		if (!cbuf_active->up || !cbuf_active->up->up)
		 	Cbuf_Error ("return","return attempted at top of stack");
		if (GIB_DATA(cbuf_active->up->up)->ret.waiting) {
			dstring_clearstr (GIB_DATA(cbuf_active->up->up)->ret.retval);
			dstring_appendstr (GIB_DATA(cbuf_active->up->up)->ret.retval, GIB_Argv(1));
			GIB_DATA(cbuf_active->up->up)->ret.available = true;
		}
	}
}

void
GIB_Builtin_Init (void)
{
	GIB_Builtin_Add ("function", GIB_Function_f);
	GIB_Builtin_Add ("lset", GIB_Lset_f);
	GIB_Builtin_Add ("return", GIB_Return_f);
}
