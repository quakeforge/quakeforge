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
#include <string.h>

#include "QF/dstring.h"
#include "QF/cbuf.h"
#include "QF/hash.h"
#include "QF/gib_parse.h"
#include "QF/gib_buffer.h"
#include "QF/gib_tree.h"
#include "QF/gib_vars.h"
#include "QF/gib_execute.h"

void
GIB_Buffer_Construct (struct cbuf_s *cbuf)
{
	cbuf->data = calloc (1, sizeof (gib_buffer_data_t));
	GIB_DATA (cbuf)->arg_composite = dstring_newstr ();
	GIB_DATA (cbuf)->globals = gib_globals;
	cbuf->strict = true;
}

void
GIB_Buffer_Destruct (struct cbuf_s *cbuf)
{
	gib_buffer_data_t *g = GIB_DATA(cbuf);
	unsigned int i, j;
	
	dstring_delete (g->arg_composite);
	if (g->locals)
		Hash_DelTable (g->locals);
	if (g->program) {
		g->program->refs--;
		GIB_Tree_Free_Recursive (g->program);
	}
	for (i = 0; i < g->stack.size; i++) {
		for (j = 0; j < g->stack.values[i].realsize; j++)
			dstring_delete (g->stack.values[i].dstrs[j]);
		if (g->stack.values[i].dstrs)
			free (g->stack.values[i].dstrs);
	}
	if (g->stack.values)
		free (g->stack.values);
	free (cbuf->data);
}

void
GIB_Buffer_Set_Program (cbuf_t *cbuf, gib_tree_t *program)
{
	GIB_DATA(cbuf)->program = program;
}

void
GIB_Buffer_Add (cbuf_t *cbuf, const char *str)
{
	gib_buffer_data_t *g = GIB_DATA (cbuf);
	gib_tree_t **save, *cur;

	if (g->program) {
		for (cur = g->program; cur->next; cur = cur->next);
		save = &cur->next;
	} else
		save = &g->program;
	*save = GIB_Parse_Lines (str, TREE_NORMAL);
	if (gib_parse_error)
		Cbuf_Error ("parse", "Parse error in program!");
}

void
GIB_Buffer_Insert (cbuf_t *cbuf, const char *str)
{
	gib_buffer_data_t *g = GIB_DATA (cbuf);
	gib_tree_t *lines, *cur;

	if ((lines = GIB_Parse_Lines (str, TREE_NORMAL))) {
		for (cur = lines; cur; cur = cur->next);
		//if (g->ip) { // This buffer is already running!
			
		if (g->program)
			g->program->refs--;
		cur->next = g->program;
		g->program = lines;
	}
	if (gib_parse_error)
		Cbuf_Error ("parse", "Parse error in program!");
}

void
GIB_Buffer_Push_Sstack (struct cbuf_s *cbuf)
{
	gib_buffer_data_t *g = GIB_DATA(cbuf);
	if (++g->stack.p > g->stack.size) {
		g->stack.values = realloc(g->stack.values, sizeof (struct gib_dsarray_s) * g->stack.p);
		g->stack.values[g->stack.p-1].dstrs = 0;
		g->stack.values[g->stack.p-1].size = g->stack.values[g->stack.p-1].realsize = 0;
		g->stack.size = g->stack.p;
	}
	g->stack.values[g->stack.p-1].size = 0;
}

void
GIB_Buffer_Pop_Sstack (struct cbuf_s *cbuf)
{
	GIB_DATA(cbuf)->stack.p--;
}

dstring_t *
GIB_Buffer_Dsarray_Get (struct cbuf_s *cbuf)
{
	struct gib_dsarray_s *vals = GIB_DATA(cbuf)->stack.values+GIB_DATA(cbuf)->stack.p-1;
	if (++vals->size > vals->realsize) {
		vals->dstrs = realloc (vals->dstrs, sizeof (dstring_t *) * vals->size);
		vals->dstrs[vals->size-1] = dstring_newstr ();
		vals->realsize = vals->size;
	} else
		dstring_clearstr (vals->dstrs[vals->size-1]);
	return vals->dstrs[vals->size-1];
}

cbuf_interpreter_t gib_interp = {
	GIB_Buffer_Construct,
	GIB_Buffer_Destruct,
	GIB_Buffer_Add,
	GIB_Buffer_Insert,
	GIB_Execute,
	GIB_Execute,
};
