/*
	gib_execute.c

	GIB runtime execution functions

	Copyright (C) 2002 Brian Koropoff

	Author: Brian Koropoff
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

#include <string.h>
#include <stdlib.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/gib_buffer.h"
#include "QF/gib_vars.h"
#include "QF/gib_process.h"
#include "QF/gib_builtin.h"
#include "QF/gib_function.h"
#include "QF/gib_execute.h"

static void
GIB_Execute_Generate_Composite (struct cbuf_s *cbuf)
{
	cbuf_args_t *args = cbuf->args;
	int i;
	
	dstring_clearstr (GIB_DATA (cbuf)->arg_composite);
	for (i = 0; i < args->argc; i++) {
		// ->str could be moved by realloc when a dstring is resized
		// so save the offset instead of the pointer
		args->args[i] = (const char *) strlen (GIB_DATA (cbuf)->arg_composite->str);
		dstring_appendstr (GIB_DATA (cbuf)->arg_composite, args->argv[i]->str);
		dstring_appendstr (GIB_DATA (cbuf)->arg_composite, " ");
	}
	
	// Get rid of trailing space
	GIB_DATA (cbuf)->arg_composite->str[strlen(GIB_DATA (cbuf)->arg_composite->str)-1] = 0;
	
	for (i = 0; i < args->argc; i++)
		// now that arg_composite is done we can add the pointer to the stored
		// offsets and get valid pointers. This *should* be portable.
		args->args[i] += (unsigned long int) GIB_DATA (cbuf)->arg_composite->str;
}		

static void
GIB_Execute_Split_Array (cbuf_t *cbuf)
{
	gib_var_t *var;
	unsigned int i;
	int start = 0, end = 0;
	char *c, *str = cbuf->args->argv[cbuf->args->argc-1]->str+1;
	void *m = cbuf->args->argm[cbuf->args->argc-1];

	i = strlen(str)-1;
	if (str[i] == ']')
		for (; i; i--)
			if (str[i] == '[') {
				str[i] = 0;
				start = atoi (str+i+1);
				if ((c = strchr (str+i+1, ':')))
					end = atoi (c+1);
				else
					end = start;
				break;
			}
	cbuf->args->argc--;
	if (!(var = GIB_Var_Get_Complex (&GIB_DATA(cbuf)->locals, &gib_globals, str, &i, false)))
		return;
	while (start < 0)
		start += var->size-1;
	while (end < 0)
		end += var->size;
	if (start >= var->size)
		return;
	if (end >= var->size || !end)
		end = var->size;
	for (i = start; i < end; i++) {
		if (var->array[i])
			Cbuf_ArgsAdd (cbuf->args, var->array[i]->str);
		else
			Cbuf_ArgsAdd (cbuf->args, "");
		cbuf->args->argm[cbuf->args->argc-1] = m;
	}
}

static int
GIB_Execute_Prepare_Line (cbuf_t *cbuf, gib_tree_t *line)
{
	gib_tree_t *cur;
	cbuf_args_t *args = cbuf->args;
	unsigned int pos;
	
	args->argc = 0;
	
	for (cur = line->children; cur; cur = cur->next) {
		if (cur->flags & TREE_CONCAT) {
			pos = args->argv[args->argc-1]->size-1;
			if (cur->flags & TREE_P_EMBED) {
				GIB_Process_Embedded (cur, cbuf->args);
			} else
				dstring_appendstr (args->argv[args->argc-1], cur->str);
		} else {
			pos = 0;
			if (cur->flags & TREE_P_EMBED) {
				Cbuf_ArgsAdd (args, "");
				GIB_Process_Embedded (cur, cbuf->args);
			} else
				Cbuf_ArgsAdd (args, cur->str);
			args->argm[args->argc-1] = cur;
		}
		if (cur->flags & TREE_P_MATH && GIB_Process_Math (args->argv[args->argc-1], pos))
			return -1;
		if (cur->flags & TREE_ASPLIT)
			GIB_Execute_Split_Array (cbuf);
	}
	return 0;
}

static void
GIB_Execute_Assign (cbuf_t *cbuf)
{
	cbuf_args_t *args = cbuf->args;
	gib_var_t *var;
	unsigned int index, i, len, el;

	// First, grab our variable
	var = GIB_Var_Get_Complex (&GIB_DATA(cbuf)->locals, &GIB_DATA(cbuf)->globals, args->argv[0]->str, &index, true);
	// Now, expand the array to the correct size
	len = args->argc-2 + index;
	if (len >= var->size) {
		var->array = realloc (var->array, len * sizeof (dstring_t *));
		memset (var->array+var->size, 0, (len-var->size) * sizeof (dstring_t *));
		var->size = len;
	} else if (len < var->size) {
		for (i = len; i < var->size; i++)
			if (var->array[i])
				dstring_delete (var->array[i]);
		var->array = realloc (var->array, len * sizeof (dstring_t *));
	}
	var->size = len;
	for (i = 2; i < args->argc; i++) {
		el = i-2+index;
		if (var->array[el])
			dstring_clearstr (var->array[el]);
		else
			var->array[el] = dstring_newstr ();
		dstring_appendstr (var->array[el], args->argv[i]->str);
	}
}
	
int
GIB_Execute_For_Next (cbuf_t *cbuf)
{
	unsigned int index;
	gib_var_t *var;
	struct gib_dsarray_s *array = GIB_DATA(cbuf)->stack.values+GIB_DATA(cbuf)->stack.p-1;
	if (array->size == 1) {
		GIB_Buffer_Pop_Sstack (cbuf);
		return 0;
	}
	array->size--;
	var = GIB_Var_Get_Complex (&GIB_DATA(cbuf)->locals, &GIB_DATA(cbuf)->globals, array->dstrs[0]->str, &index, true);
	dstring_clearstr (var->array[index]);
	dstring_appendstr (var->array[index], array->dstrs[array->size]->str);
	return 1;
}

void
GIB_Execute (cbuf_t *cbuf)
{
	gib_buffer_data_t *g = GIB_DATA (cbuf);
	gib_builtin_t *b;
	gib_function_t *f;
	
	if (g->waitret) {
			Cbuf_Error ("return", "Embedded function '%s' did not return a value.", cbuf->args->argv[0]->str);
			return;
	}
	if (!g->program) {
		return;
	}
	if (!g->ip)
		g->ip = g->program;
	while (!g->done) {
		if (GIB_Execute_Prepare_Line (cbuf, g->ip))
			return;
		if (g->ip->flags & TREE_COND) {
			if (!atoi(cbuf->args->argv[1]->str))
				g->ip = g->ip->jump;
		} else if (g->ip->flags & TREE_FORNEXT) {
			if (GIB_Execute_For_Next (cbuf))
				g->ip = g->ip->jump;
		} else if (g->ip->flags & TREE_END) {
			g->ip = g->ip->jump;
			if (g->ip->flags & TREE_COND)
				continue;
		} else if (cbuf->args->argc) {
			if (g->ip->flags & TREE_EMBED) {
				g->waitret = true;
				GIB_Buffer_Push_Sstack (cbuf); // Make room for return values
			}
			if (!strcmp (cbuf->args->argv[1]->str, "=") && ((gib_tree_t *)cbuf->args->argm[1])->delim == ' ')
				GIB_Execute_Assign (cbuf);
			else if ((b = GIB_Builtin_Find (cbuf->args->argv[0]->str))) {
				b->func ();
				// If there already was an error, don't override it
				if (g->ip->flags & TREE_EMBED && !cbuf->state) {
					if (!g->haveret) {
						Cbuf_Error ("return", "Embedded builtin '%s' did not return a value.", cbuf->args->argv[0]->str);
						return;
					}
					g->haveret = g->waitret = 0;
				}
			} else if ((f = GIB_Function_Find (cbuf->args->argv[0]->str))) {
				cbuf_t *new = Cbuf_New (&gib_interp);
				cbuf->down = new;
				new->up = cbuf;
				cbuf->state = CBUF_STATE_STACK;
				GIB_Function_Execute (new, f, cbuf->args);
				if (g->ip->flags & TREE_EMBED)
					g->waitret = true;
			} else {
				GIB_Execute_Generate_Composite (cbuf);
				Cmd_Command (cbuf->args);
			}
		}
		if (!(g->ip = g->ip->next)) // No more commands
				g->done = true;
		if (cbuf->state) // Let the stack walker figure out what to do
			return;
	}
	g->done = false;
	GIB_Tree_Free_Recursive (g->program, false);
	g->program = g->ip = 0;
}
