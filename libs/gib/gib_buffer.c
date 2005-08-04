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

static __attribute__ ((used))
const char  rcsid[] = "$Id$";

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "QF/sys.h"
#include "QF/dstring.h"
#include "QF/cbuf.h"
#include "QF/hash.h"
#include "QF/gib.h"
#include "QF/idparse.h"

#include "gib_tree.h"
#include "gib_parse.h"
#include "gib_vars.h"
#include "gib_execute.h"
#include "gib_buffer.h"

static void
GIB_Buffer_Construct (struct cbuf_s *cbuf)
{
	cbuf->data = calloc (1, sizeof (gib_buffer_data_t));
	GIB_DATA (cbuf)->arg_composite = dstring_newstr ();
	GIB_DATA (cbuf)->globals = gib_globals;
	cbuf->strict = true;
}

static void
GIB_Buffer_Destruct (struct cbuf_s *cbuf)
{
	gib_buffer_data_t *g = GIB_DATA (cbuf);
	unsigned int i, j;

	if (g->dnotify)
		g->dnotify (cbuf, g->ddata);

	dstring_delete (g->arg_composite);
	if (g->locals)
		Hash_DelTable (g->locals);
	if (g->program)
		GIB_Tree_Unref (&g->program);
	if (g->script && !(--g->script->refs)) {
		free ((void *) g->script->text);
		free ((void *) g->script->file);
		free (g->script);
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

static void
GIB_Buffer_Reset (struct cbuf_s *cbuf)
{
	gib_buffer_data_t *g = GIB_DATA (cbuf);

	// Being reset is nearly the same as being destroyed.
	// It just means the buffer is going to be reused.
	if (g->dnotify)
		g->dnotify (cbuf, g->ddata);
	
	if (g->locals)
		Hash_FlushTable (g->locals);
	g->globals = gib_globals;
	if (g->program)
		GIB_Tree_Unref (&g->program);
	if (g->script && !(--g->script->refs)) {
		free ((void *) g->script->text);
		free ((void *) g->script->file);
		free (g->script);
	}
	g->script = 0;
	g->program = g->ip = 0;
	g->stack.p = 0;
	g->waitret = false;
	g->dnotify = NULL;
	g->reply.obj = NULL;
}

void
GIB_Buffer_Set_Program (cbuf_t * cbuf, gib_tree_t * program)
{
	GIB_DATA (cbuf)->program = program;
}

static unsigned int
GIB_Buffer_Get_Line_Num (const char *program, unsigned int pos)
{
	unsigned int i, line;

	for (i = 0, line = 1; i < pos; i++)
		if (program[i] == '\n')
			line++;
	return line;
}

static void
GIB_Buffer_Add (cbuf_t * cbuf, const char *str)
{
	gib_buffer_data_t *g = GIB_DATA (cbuf);
	gib_tree_t **save, *cur;

	// AddText should only be used to populate a buffer before
	// executing it and shouldn't happen to a running GIB buffer,
	// but if it does, try to find somewhere else to put the text.
	if (g->ip) {
		for (;cbuf; cbuf = cbuf->up)
			if (cbuf->interpreter == &id_interp) {
				Cbuf_AddText (cbuf, str);
				return;
			}
		Sys_Printf (
				"-------------\n"
				"|GIB Warning|\n"
				"-------------\n"
				"Text added to running GIB buffer discarded.\n"
				"Text: %s\n",
				str
		);
		return;
	} else if (g->program) {
		for (cur = g->program; cur->next; cur = cur->next);
		save = &cur->next;
	} else
		save = &g->program;
	if (!(*save = GIB_Parse_Lines (str, 0)))
		Sys_Printf (
			"-----------------\n"
			"|GIB Parse Error|\n"
			"-----------------\n"
			"Parse error while adding text to GIB buffer.\n"
			"Line %u: %s\n", 
			GIB_Buffer_Get_Line_Num (str, GIB_Parse_ErrorPos ()),
			GIB_Parse_ErrorMsg ()
		);
}

static void
GIB_Buffer_Insert (cbuf_t * cbuf, const char *str)
{
	gib_buffer_data_t *g = GIB_DATA (cbuf);
	gib_tree_t *lines, *cur;

	// No GIB builtin should ever insert text into a running
	// GIB buffer, so create an idparse buffer to handle the
	// legacy console code.
	if (g->ip) {
		cbuf_t *new = Cbuf_New (&id_interp);
		new->up = cbuf;
		cbuf->down = new;
		cbuf->state = CBUF_STATE_STACK;
		Cbuf_InsertText (new, str);
		return;
	} else if ((lines = GIB_Parse_Lines (str, 0))) {
		for (cur = lines; cur; cur = cur->next);
		cur->next = g->program;
		g->program = lines;
	} else
		Sys_Printf (
			"-----------------\n"
			"|GIB Parse Error|\n"
			"-----------------\n"
			"Parse error while inserting text into GIB buffer.\n"
			"Line %u: %s\n",
			GIB_Buffer_Get_Line_Num (str, GIB_Parse_ErrorPos ()),
			GIB_Parse_ErrorMsg ()
		);
}

void
GIB_Buffer_Push_Sstack (struct cbuf_s *cbuf)
{
	gib_buffer_data_t *g = GIB_DATA (cbuf);

	if (++g->stack.p > g->stack.size) {
		g->stack.values =
			realloc (g->stack.values,
					 sizeof (struct gib_dsarray_s) * g->stack.p);
		g->stack.values[g->stack.p - 1].dstrs = 0;
		g->stack.values[g->stack.p - 1].size =
			g->stack.values[g->stack.p - 1].realsize = 0;
		g->stack.size = g->stack.p;
	}
	g->stack.values[g->stack.p - 1].size = 0;
}

void
GIB_Buffer_Pop_Sstack (struct cbuf_s *cbuf)
{
	GIB_DATA (cbuf)->stack.p--;
}

dstring_t *
GIB_Buffer_Dsarray_Get (struct cbuf_s *cbuf)
{
	struct gib_dsarray_s *vals =
		GIB_DATA (cbuf)->stack.values + GIB_DATA (cbuf)->stack.p - 1;
	if (++vals->size > vals->realsize) {
		vals->dstrs = realloc (vals->dstrs, sizeof (dstring_t *) * vals->size);
		vals->dstrs[vals->size - 1] = dstring_newstr ();
		vals->realsize = vals->size;
	} else
		dstring_clearstr (vals->dstrs[vals->size - 1]);
	return vals->dstrs[vals->size - 1];
}


static int
GIB_Buffer_Get_Line_Info (cbuf_t * cbuf, char **line)
{
	const char *text;
	unsigned int ofs, i, start, linenum;

	// Do we have a copy of the original program this buffer comes from?
	if (GIB_DATA (cbuf)->script) {
		text = GIB_DATA (cbuf)->script->text;
		for (ofs = GIB_DATA (cbuf)->ip->start, start = 0, i = 0, linenum = 1;
			 i <= ofs; i++)
			if (text[i] == '\n') {
				start = i + 1;
				linenum++;
			}
		while (text[i] != '\n')
			i++;
		*line = malloc (i - start + 1);
		memcpy (*line, text + start, i - start);
		(*line)[i - start] = 0;
		return linenum;
	} else {
		*line = strdup (GIB_DATA (cbuf)->ip->str);
		return -1;
	}
}

void
GIB_Buffer_Reply_Callback (int argc, const char **argv, void *data)
{
	cbuf_t *cbuf = (cbuf_t *) data;
	int i;

	for (i = 0; i < argc; i++)
		dstring_copystr (GIB_Buffer_Dsarray_Get (cbuf), argv[i]);
	if (cbuf->state == CBUF_STATE_BLOCKED)
		cbuf->state = CBUF_STATE_NORMAL;
}

void
GIB_Buffer_Error (cbuf_t * cbuf, const char *type, const char *fmt,
				  va_list args)
{
	char       *line;
	int         linenum;
	dstring_t  *message = dstring_newstr ();

	dvsprintf (message, fmt, args);
	va_end (args);
	Sys_Printf (
			"---------------------\n"
			"|GIB Execution Error|\n"
			"---------------------\n"
			"Type: %s\n",
			type
		);
	if ((linenum = GIB_Buffer_Get_Line_Info (cbuf, &line)) != -1)
		Sys_Printf (
			"%s:%i: %s\n"
			"-->%s\n",
			GIB_DATA (cbuf)->script->file, linenum, message->str, line
		);
	else
		Sys_Printf (
			"%s\n"
			"-->%s\n",
			message->str,
			line
		);
	cbuf->state = CBUF_STATE_ERROR;
	dstring_delete (message);
	free (line);
}


cbuf_interpreter_t gib_interp = {
	GIB_Buffer_Construct,
	GIB_Buffer_Destruct,
	GIB_Buffer_Reset,
	GIB_Buffer_Add,
	GIB_Buffer_Insert,
	GIB_Execute,
	GIB_Execute,
};

cbuf_interpreter_t *
GIB_Interpreter (void)
{
	return &gib_interp;
}
